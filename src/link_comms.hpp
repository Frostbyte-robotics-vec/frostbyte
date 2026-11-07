#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "hal/pros/pros_link_transport.hpp"
#include "main.h"
#include "util/link_protocol.hpp"

// Funk zum anderen Roboter im Match (L-02, ADR-043), aktiv nur mit config::link_enabled. Ein niedrig priorisierter Task
// (comms_task) spricht als Einziger mit pros::Link; der Steuer-Task und die Route benutzen nur LinkShared, und das nur mit
// Try-Lock ohne Wartezeit: klappt die Sperre nicht, ist der Aufruf wirkungslos und meldet busy; der Aufrufer versucht es im
// naechsten Takt wieder (siehe LinkSend). Die Sperre haelt der Comms-Task nur fuer reine Rechnung, nie waehrend receive() oder
// transmit(). Ausfall des Links stoppt nichts: Signale laufen in die Frist, wait_for in sein Timeout (RoutePlayer, ADR-029).
// link_shared() beim Start (initialize()) einmal aufrufen, damit Mutex und Endpunkt nicht im Steuer-Task entstehen.
// TODO(HW): Port, Link-Id, Rollen und Paketgroesse stehen in config/ und sind ungeprueft (L-04).
static_assert(frostbyte::LinkFrame::max_wire_size <= config::link_max_packet_bytes,
              "Funk-Frame passt nicht in link_max_packet_bytes (TODO(HW) in config/)");

namespace frostbyte {

// Ergebnis von try_send_signal: queued = vorgemerkt; busy = Sperre belegt, nichts passiert, im naechsten Takt erneut versuchen;
// rejected = ungueltiger Name oder Warteschlange voll (dauerhaft, nicht erneut versuchen ohne Aenderung).
enum class LinkSend { queued, busy, rejected };

struct LinkSnapshot {
    bool valid = false;  // false: Sperre war belegt, Werte nicht aktuell
    bool connected = false;
    std::size_t pending = 0;
    std::uint32_t acked = 0;
    std::uint32_t expired = 0;
    std::uint32_t frames_in = 0;
    std::uint32_t frames_bad = 0;
    bool selftest_seen = false;  // Signal config::link_selftest_name vom Partner gesehen
};

class LinkShared {
public:
    explicit LinkShared(const LinkConfig& cfg) : ep_(self_robot(), 0, cfg) {}

    // Steuer-Task und Routen: alle ohne Wartezeit.
    LinkSend try_send_signal(std::string_view name, std::uint32_t now_ms) {
        if (!mutex_.take(0))
            return LinkSend::busy;
        const bool queued = ep_.send_signal(name, now_ms);
        mutex_.give();
        return queued ? LinkSend::queued : LinkSend::rejected;
    }
    // false auch, wenn die Sperre belegt war (dann im naechsten Takt erneut fragen).
    bool try_signal_seen(std::string_view name) {
        if (!mutex_.take(0))
            return false;
        const bool seen = ep_.signal_seen(name);
        mutex_.give();
        return seen;
    }
    void try_set_status(const LinkStatus& s) {
        if (mutex_.take(0)) {
            ep_.set_status(s);
            mutex_.give();
        }
    }
    void try_set_goals(const std::array<std::uint8_t, LinkFrame::kGoals>& g) {
        if (mutex_.take(0)) {
            ep_.set_goals(g);
            mutex_.give();
        }
    }
    bool try_connected(std::uint32_t now_ms) {
        if (!mutex_.take(0))
            return false;
        const bool c = ep_.connected(now_ms);
        mutex_.give();
        return c;
    }
    LinkSnapshot snapshot(std::uint32_t now_ms) {
        LinkSnapshot s;
        if (!mutex_.take(0))
            return s;
        s.valid = true;
        s.connected = ep_.connected(now_ms);
        s.pending = ep_.pending_signals();
        s.acked = ep_.stats().signals_acked;
        s.expired = ep_.stats().signals_expired;
        s.frames_in = ep_.stats().frames_in;
        s.frames_bad = ep_.stats().frames_bad;
        s.selftest_seen = ep_.signal_seen(config::link_selftest_name);
        mutex_.give();
        return s;
    }

    // Nur der Comms-Task. Kurze Sperre fuer reine Rechnung (kein pros::Link darin).
    bool lock() { return mutex_.take(10); }
    void unlock() { mutex_.give(); }
    LinkEndpoint& endpoint() { return ep_; }
    void restart(std::uint8_t session, const LinkConfig& cfg) { ep_ = LinkEndpoint(self_robot(), session, cfg); }

private:
    static LinkRobot self_robot() { return config::link_self_is_stacker ? LinkRobot::stacker : LinkRobot::runner; }

    LinkEndpoint ep_;
    pros::Mutex mutex_;
};

inline LinkConfig link_config() {
    return LinkConfig{config::link_status_period_ms, config::link_goals_period_ms, config::link_retry_ms,
                      config::link_signal_deadline_ms, config::link_loss_ms};
}

inline LinkShared& link_shared() {
    static LinkShared s{link_config()};
    return s;
}

inline void comms_task(void*) {
    if (config::link_port < 1 || config::link_port > 21) {
        // nicht festgelegt (TODO(HW)): kein Funk, Routen laufen ohne Partner (wait_for -> Timeout)
        while (true)
            pros::delay(100);
    }
    static hal::ProsLinkTransport transport(static_cast<std::uint8_t>(config::link_port), config::link_id,
                                            config::link_is_transmitter);
    LinkShared& shared = link_shared();
    constexpr std::size_t kBatch = 8;
    std::uint8_t in_buf[kBatch][LinkFrame::max_wire_size];
    std::size_t in_len[kBatch];
    std::uint8_t out_buf[4][LinkFrame::max_wire_size];
    std::size_t out_len[4];
    bool started = false;
    std::uint32_t last_selftest_ms = 0;
    std::uint32_t now_ms = pros::millis();
    while (true) {
        if (transport.connected()) {
            if (!started) {
                // Sitzungsnummer aus der Zeit der ersten Verbindung (Funk-Kopplung schwankt, die Bootzeit kaum). Ein Neustart
                // mit gleicher Nummer wird zusaetzlich ueber die Funkstille erkannt (link_protocol.hpp, on_bytes).
                const std::uint32_t us = static_cast<std::uint32_t>(pros::micros());
                if (shared.lock()) {
                    shared.restart(static_cast<std::uint8_t>(us ^ (us >> 8) ^ (us >> 16)), link_config());
                    shared.unlock();
                    started = true;
                }
            }
            // 1) Empfangen ohne Sperre, 2) kurz sperren und rechnen, 3) Senden ohne Sperre.
            std::size_t nin = 0;
            while (nin < kBatch && (in_len[nin] = transport.receive(in_buf[nin], sizeof in_buf[nin])) > 0)
                ++nin;
            std::size_t nout = 0;
            if (started && shared.lock()) {
                const std::uint32_t t = pros::millis();
                for (std::size_t i = 0; i < nin; ++i)
                    shared.endpoint().on_bytes(in_buf[i], in_len[i], t);
                if constexpr (config::link_selftest) {
                    if (config::link_self_is_stacker &&
                        static_cast<std::uint32_t>(t - last_selftest_ms) >= config::link_selftest_period_ms) {
                        last_selftest_ms = t;
                        shared.endpoint().send_signal(config::link_selftest_name, t);
                    }
                }
                while (nout < 4 && shared.endpoint().next_frame(t, out_buf[nout], out_len[nout]))
                    ++nout;
                shared.unlock();
            }
            for (std::size_t i = 0; i < nout; ++i)
                transport.send(out_buf[i], out_len[i]);
        }
        pros::c::task_delay_until(&now_ms, config::link_task_period_ms);
    }
}

}  // namespace frostbyte
