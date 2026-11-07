#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdio>

#include "main.h"
#include "util/link_test.hpp"

// Funk-Link-Test (L-01, ADR-041): Hilfsprogramm fuer beide Roboter, aktiv nur mit config::link_test_enabled (dann startet
// initialize() keinen Steuer-Task). Der Pinger (config::link_is_pinger) schickt alle config::link_period_ms einen Ping, der
// Reflektor schickt jeden Ping sofort zurueck. Der Pinger schreibt je Ping eine Zeile nach /usd/linklog_<roboter>.csv
// (frostbyte-linklog v1; Auswertung mit tools/link_report.py) und zeigt Zaehler aufs LCD. Eigene Schleife mit delay():
// nie zusammen mit dem Steuer-Task, und nie im Match (der Link ist dort fuer den Betrieb da).
// TODO(HW): Funk-Port, Link-Id und Rollen (Sender/Empfaenger) stehen in config/ und sind ungeprueft; ob receive() ohne
// Daten PROS_ERR liefert, ob raw_receivable_size() im Paketmodus brauchbar ist und ob ein EBADMSG den Puffer zum Neu-
// Synchronisieren leeren muss, am Roboter pruefen (L-04). Build-Kennung folgt mit dem Release-Ticket (L-03).
namespace frostbyte {

inline void link_test_run() {
    pros::lcd::print(1, "%s Link-Test", config::robot_name);
    if (config::link_port < 1 || config::link_port > 21) {
        pros::lcd::print(2, "link_port fehlt");
        pros::lcd::print(3, "(TODO(HW) in config/)");
        while (true)
            pros::delay(100);
    }
    pros::Link link(static_cast<std::uint8_t>(config::link_port), config::link_id,
                    config::link_is_transmitter ? pros::E_LINK_TX : pros::E_LINK_RX, true);
    const bool pinger_role = config::link_is_pinger;
    LinkPinger pinger(config::link_period_ms, config::link_timeout_ms);
    LinkReflector reflector;

    char path[40];
    std::snprintf(path, sizeof path, "/usd/linklog_%s.csv", config::robot_name);
    std::FILE* log = pinger_role ? std::fopen(path, "w") : nullptr;
    char line[96];
    auto write_line = [&](std::size_t n) {
        if (n == 0)
            return;
        std::fputs(line, stdout);
        if (log)
            std::fputs(line, log);
    };
    if (pinger_role) {
        write_line(link_log_header(line, sizeof line, config::robot_name, "ping", "-"));
        write_line(link_log_columns(line, sizeof line));
        if (!log)
            pros::lcd::print(7, "keine SD-Karte: nur Terminal");
    }

    std::uint32_t last_flush_ms = pros::millis();
    std::uint32_t last_show_ms = last_flush_ms;
    std::uint32_t bad_packets = 0;
    while (true) {
        const std::uint32_t now_ms = pros::millis();
        const bool up = link.connected();
        if (up) {
            if (pinger_role) {
                LinkPacket ping;
                if (pinger.next_ping(now_ms, ping)) {
                    std::uint8_t buf[LinkPacket::wire_size];
                    ping.encode(buf);
                    link.transmit(buf, sizeof buf);
                }
            }
            // Alles abholen, was da ist (hoechstens 8 je Durchlauf, damit die Schleife nie haengt).
            for (int i = 0; i < 8 && link.raw_receivable_size() > 0; ++i) {
                std::uint8_t buf[LinkPacket::wire_size];
                errno = 0;
                const std::uint32_t got = link.receive(buf, sizeof buf);
                LinkPacket p;
                if (got == sizeof buf && LinkPacket::decode(buf, sizeof buf, p)) {
                    const std::uint32_t rx_ms = pros::millis();
                    if (pinger_role) {
                        pinger.on_packet(p, rx_ms);
                    } else {
                        LinkPacket pong;
                        if (reflector.reflect(p, rx_ms, pong)) {
                            pong.encode(buf);
                            link.transmit(buf, sizeof buf);
                        }
                    }
                } else {
                    ++bad_packets;
                    if (errno == EBADMSG)
                        link.clear_receive_buf();
                }
            }
        }
        if (pinger_role) {
            pinger.update(now_ms);
            LinkRecord r;
            while (pinger.take_record(r))
                write_line(link_log_row(line, sizeof line, r));
        }
        if (now_ms - last_show_ms >= 250) {
            last_show_ms = now_ms;
            pros::lcd::print(2, up ? "Link verbunden" : "Link NICHT verbunden");
            if (pinger_role) {
                const LinkStats& s = pinger.stats();
                pros::lcd::print(3, "gesendet %u", static_cast<unsigned>(s.sent));
                pros::lcd::print(4, "ok %u  verloren %u", static_cast<unsigned>(s.received),
                                 static_cast<unsigned>(s.lost));
                pros::lcd::print(5, "spaet %u dopp %u umg %u", static_cast<unsigned>(s.late),
                                 static_cast<unsigned>(s.duplicate), static_cast<unsigned>(s.reordered));
            } else {
                pros::lcd::print(3, "zurueckgeschickt %u", static_cast<unsigned>(reflector.reflected()));
            }
            pros::lcd::print(6, "kaputte Pakete %u", static_cast<unsigned>(bad_packets));
        }
        if (log && now_ms - last_flush_ms >= 1000) {
            last_flush_ms = now_ms;
            std::fflush(log);
        }
        pros::delay(1);
    }
}

}  // namespace frostbyte
