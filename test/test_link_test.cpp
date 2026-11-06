#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "util/link_test.hpp"

using namespace frostbyte;

namespace {

// Simulierter Kanal: jedes Paket bekommt Verzoegerung (ms) oder geht verloren. Reihenfolge ergibt sich aus der Ankunftszeit.
struct Wire {
    struct Item {
        std::uint32_t arrive_ms;
        LinkPacket p;
    };
    std::vector<Item> items;
    void put(const LinkPacket& p, std::uint32_t now_ms, std::uint32_t delay_ms) {
        items.push_back({now_ms + delay_ms, p});
    }
    // Liefert alle bis now_ms angekommenen Pakete in Ankunftsreihenfolge.
    std::vector<LinkPacket> take(std::uint32_t now_ms) {
        std::vector<LinkPacket> out;
        for (std::size_t i = 0; i < items.size();) {
            // fruehestes angekommenes Paket zuerst
            std::size_t best = items.size();
            for (std::size_t j = 0; j < items.size(); ++j)
                if (items[j].arrive_ms <= now_ms &&
                    (best == items.size() || items[j].arrive_ms < items[best].arrive_ms))
                    best = j;
            if (best == items.size())
                break;
            out.push_back(items[best].p);
            items.erase(items.begin() + static_cast<long>(best));
            i = 0;
        }
        return out;
    }
};

std::vector<LinkRecord> drain(LinkPinger& p) {
    std::vector<LinkRecord> v;
    LinkRecord r;
    while (p.take_record(r))
        v.push_back(r);
    return v;
}

}  // namespace

TEST_CASE("LinkPacket: Kodierung hin und zurueck, kaputte Pakete werden abgelehnt") {
    LinkPacket p{LinkPacket::Kind::pong, 0x01020304u, 0xAABBCCDDu, 77u};
    std::uint8_t buf[LinkPacket::wire_size];
    p.encode(buf);
    LinkPacket q;
    REQUIRE(LinkPacket::decode(buf, sizeof buf, q));
    CHECK(q.kind == LinkPacket::Kind::pong);
    CHECK(q.seq == 0x01020304u);
    CHECK(q.t_ping_ms == 0xAABBCCDDu);
    CHECK(q.t_pong_ms == 77u);

    CHECK_FALSE(LinkPacket::decode(buf, sizeof buf - 1, q));  // zu kurz
    buf[0] = 9;                                               // unbekannte Art
    CHECK_FALSE(LinkPacket::decode(buf, sizeof buf, q));
    p.encode(buf);
    buf[LinkPacket::wire_size - 1] ^= 0x5A;  // Pruefsumme passt nicht
    CHECK_FALSE(LinkPacket::decode(buf, sizeof buf, q));
}

TEST_CASE("Reflektor: macht aus Ping einen Pong mit gleicher Nummer und eigener Zeit; Pong und Muell ignoriert er") {
    LinkReflector r;
    LinkPacket ping{LinkPacket::Kind::ping, 5, 1000, 0};
    LinkPacket pong;
    REQUIRE(r.reflect(ping, 1234, pong));
    CHECK(pong.kind == LinkPacket::Kind::pong);
    CHECK(pong.seq == 5);
    CHECK(pong.t_ping_ms == 1000);
    CHECK(pong.t_pong_ms == 1234);
    CHECK(r.reflected() == 1);
    CHECK_FALSE(r.reflect(pong, 1300, pong));
    CHECK(r.reflected() == 1);
}

TEST_CASE("Ideale Leitung: jeder Ping kommt zurueck, RTT = 2 x Verzoegerung") {
    LinkPinger pinger(50, 500);
    LinkReflector refl;
    Wire to, from;
    for (std::uint32_t t = 0; t <= 1000; ++t) {
        LinkPacket out;
        if (pinger.next_ping(t, out))
            to.put(out, t, 4);
        for (const LinkPacket& p : to.take(t)) {
            LinkPacket pong;
            if (refl.reflect(p, t, pong))
                from.put(pong, t, 4);
        }
        for (const LinkPacket& p : from.take(t))
            pinger.on_packet(p, t);
        pinger.update(t);
    }
    const auto recs = drain(pinger);
    REQUIRE(recs.size() >= 19);
    for (std::size_t i = 0; i + 1 < recs.size(); ++i) {
        CHECK(recs[i].seq == i);
        CHECK(recs[i].rtt_ms == 8);
    }
    CHECK(pinger.stats().lost == 0);
    CHECK(pinger.stats().late == 0);
    CHECK(pinger.stats().duplicate == 0);
    CHECK(pinger.stats().reordered == 0);
}

TEST_CASE("Verlust: Pong bleibt aus -> nach dem Timeout steht der Ping als verloren im Protokoll") {
    LinkPinger pinger(100, 300);
    LinkPacket p0, p1;
    REQUIRE(pinger.next_ping(0, p0));
    CHECK_FALSE(pinger.next_ping(50, p1));  // noch nicht an der Reihe
    REQUIRE(pinger.next_ping(100, p1));
    pinger.on_packet(LinkPacket{LinkPacket::Kind::pong, p1.seq, p1.t_ping_ms, 103}, 106);  // nur Ping 1 kommt zurueck
    pinger.update(299);
    CHECK(pinger.stats().lost == 0);  // Ping 0 hat noch Zeit
    pinger.update(300);
    CHECK(pinger.stats().lost == 1);
    const auto recs = drain(pinger);
    REQUIRE(recs.size() == 2);
    CHECK(recs[0].seq == p1.seq);  // Ping 1 wurde zuerst abgeschlossen
    CHECK(recs[0].rtt_ms == 6);
    CHECK(recs[1].seq == p0.seq);
    CHECK(recs[1].rtt_ms == LinkRecord::lost_rtt);
    pinger.update(1000);  // nicht doppelt zaehlen
    CHECK(pinger.stats().lost == 1);
}

TEST_CASE("Verspaeteter Pong nach dem Timeout: zaehlt als late, nicht als Antwort; zweiter Pong ist duplicate") {
    LinkPinger pinger(100, 200);
    LinkPacket p0;
    REQUIRE(pinger.next_ping(0, p0));
    pinger.update(250);
    CHECK(pinger.stats().lost == 1);
    pinger.on_packet(LinkPacket{LinkPacket::Kind::pong, p0.seq, 0, 240}, 260);
    CHECK(pinger.stats().late == 1);
    CHECK(pinger.stats().received == 0);

    LinkPacket p1;
    REQUIRE(pinger.next_ping(300, p1));
    const LinkPacket pong{LinkPacket::Kind::pong, p1.seq, p1.t_ping_ms, 310};
    pinger.on_packet(pong, 320);
    pinger.on_packet(pong, 330);  // Kanal liefert doppelt
    CHECK(pinger.stats().received == 1);
    CHECK(pinger.stats().duplicate == 1);
    CHECK(drain(pinger).size() == 2);  // je Ping genau eine Zeile
}

TEST_CASE("Reihenfolge: ein Pong, der nach einem neueren ankommt, zaehlt als reordered und behaelt seine RTT") {
    LinkPinger pinger(10, 500);
    LinkPacket a, b;
    REQUIRE(pinger.next_ping(0, a));
    REQUIRE(pinger.next_ping(10, b));
    pinger.on_packet(LinkPacket{LinkPacket::Kind::pong, b.seq, 10, 20}, 30);
    pinger.on_packet(LinkPacket{LinkPacket::Kind::pong, a.seq, 0, 25}, 40);
    CHECK(pinger.stats().received == 2);
    CHECK(pinger.stats().reordered == 1);
    const auto recs = drain(pinger);
    REQUIRE(recs.size() == 2);
    CHECK(recs[1].seq == a.seq);
    CHECK(recs[1].rtt_ms == 40);
}

TEST_CASE("Falsche Pakete: unbekannte Nummer, falsche Sendezeit und Pings am Pinger veraendern nichts") {
    LinkPinger pinger(100, 500);
    LinkPacket a;
    REQUIRE(pinger.next_ping(0, a));
    pinger.on_packet(LinkPacket{LinkPacket::Kind::pong, 99, 0, 5}, 10);         // nie gesendet
    pinger.on_packet(LinkPacket{LinkPacket::Kind::pong, a.seq, 12345, 5}, 10);  // Sendezeit passt nicht
    pinger.on_packet(LinkPacket{LinkPacket::Kind::ping, a.seq, 0, 0}, 10);      // falsche Art
    CHECK(pinger.stats().received == 0);
    CHECK(pinger.stats().unmatched == 2);
    CHECK(drain(pinger).empty());
}

TEST_CASE("Zeitzaehler laeuft ueber: RTT und Timeout rechnen mit Differenzen") {
    LinkPinger pinger(100, 200);
    const std::uint32_t t0 = 0xFFFFFFF0u;
    LinkPacket a;
    REQUIRE(pinger.next_ping(t0, a));
    pinger.on_packet(LinkPacket{LinkPacket::Kind::pong, a.seq, t0, t0 + 3}, t0 + 20u);  // Ankunft nach dem Ueberlauf
    const auto recs = drain(pinger);
    REQUIRE(recs.size() == 1);
    CHECK(recs[0].rtt_ms == 20);
    LinkPacket b;
    REQUIRE(pinger.next_ping(t0 + 100u, b));
    pinger.update(t0 + 300u);
    CHECK(pinger.stats().lost == 1);
}

TEST_CASE("Mehr offene Pings als Platz: der aelteste wird als verloren abgeschlossen, nichts laeuft ueber") {
    LinkPinger pinger(1, 100000);  // Timeout so lang, dass nur der Platzmangel zaehlt
    LinkPacket p;
    const int n = static_cast<int>(LinkPinger::max_open) + 5;
    for (int i = 0; i < n; ++i)
        REQUIRE(pinger.next_ping(static_cast<std::uint32_t>(i), p));
    CHECK(pinger.stats().sent == static_cast<std::uint32_t>(n));
    CHECK(pinger.stats().lost == 5);
}

TEST_CASE("Zeilen fuer das Protokoll: Kopf und Zeile mit Verlust") {
    char line[96];
    CHECK(link_log_header(line, sizeof line, "stacker", "ping", "abc123") > 0);
    CHECK(std::strcmp(line, "# frostbyte-linklog v1 robot=stacker role=ping build=abc123\n") == 0);
    CHECK(link_log_columns(line, sizeof line) > 0);
    CHECK(std::strcmp(line, "seq,t_send_s,rtt_ms\n") == 0);
    CHECK(link_log_row(line, sizeof line, LinkRecord{3, 1500, 12}) > 0);
    CHECK(std::strcmp(line, "3,1.500,12\n") == 0);
    CHECK(link_log_row(line, sizeof line, LinkRecord{4, 2000, LinkRecord::lost_rtt}) > 0);
    CHECK(std::strcmp(line, "4,2.000,\n") == 0);                // leere Zelle = verloren
    CHECK(link_log_row(line, 4, LinkRecord{4, 2000, 1}) == 0);  // Puffer zu klein -> nichts schreiben
}

TEST_CASE("Beispiellog: der Schreiber erzeugt Byte fuer Byte die Datei, die link_report.py liest") {
    std::string path = std::string(FB_TEST_FIXTURES) + "/linklog_example.csv";
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    std::stringstream expected;
    expected << f.rdbuf();

    char line[96];
    std::string got;
    REQUIRE(link_log_header(line, sizeof line, "stacker", "ping", "abc123") > 0);
    got += line;
    REQUIRE(link_log_columns(line, sizeof line) > 0);
    got += line;
    const LinkRecord recs[] = {
        {0, 12000, 8}, {1, 12050, 9}, {2, 12100, LinkRecord::lost_rtt}, {3, 12150, 11}, {4, 12200, 40}};
    for (const LinkRecord& r : recs) {
        REQUIRE(link_log_row(line, sizeof line, r) > 0);
        got += line;
    }
    CHECK(got == expected.str());
}
