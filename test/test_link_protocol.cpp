#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "match/route_player.hpp"
#include "util/link_protocol.hpp"
#include "util/link_test.hpp"

using namespace frostbyte;

namespace {

// Simulierter Kanal in eine Richtung: Verlust (Prozent), feste Verzögerung plus Streuung, alles aus einem Seed
// (gleicher Seed = gleiches Ergebnis). Ein Schalter `down` kappt die Verbindung ganz.
struct Wire {
    struct Item {
        std::uint32_t arrive_ms;
        std::vector<std::uint8_t> bytes;
    };
    std::vector<Item> items;
    std::uint64_t rng;
    int loss_percent = 0;
    int dup_percent = 0;
    std::uint32_t delay_ms = 5;
    std::uint32_t jitter_ms = 0;
    bool down = false;

    explicit Wire(std::uint64_t seed) : rng(seed) {}
    std::uint32_t rnd(std::uint32_t n) {
        rng = rng * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>((rng >> 33) % n);
    }
    void put(const std::uint8_t* b, std::size_t n, std::uint32_t now_ms) {
        if (down || static_cast<int>(rnd(100)) < loss_percent)
            return;
        const int copies = static_cast<int>(rnd(100)) < dup_percent ? 2 : 1;
        for (int i = 0; i < copies; ++i)
            items.push_back({now_ms + delay_ms + (jitter_ms ? rnd(jitter_ms + 1) : 0), {b, b + n}});
    }
    // Alle bis now_ms angekommenen Pakete, früheste zuerst.
    std::vector<std::vector<std::uint8_t>> take(std::uint32_t now_ms) {
        std::vector<std::vector<std::uint8_t>> out;
        while (true) {
            std::size_t best = items.size();
            for (std::size_t j = 0; j < items.size(); ++j)
                if (items[j].arrive_ms <= now_ms &&
                    (best == items.size() || items[j].arrive_ms < items[best].arrive_ms))
                    best = j;
            if (best == items.size())
                break;
            out.push_back(items[best].bytes);
            items.erase(items.begin() + static_cast<long>(best));
        }
        return out;
    }
};

LinkConfig fast_cfg() {
    LinkConfig c;
    c.status_period_ms = 100;
    c.goals_period_ms = 200;
    c.retry_ms = 40;
    c.signal_deadline_ms = 1000;
    c.loss_ms = 500;
    return c;
}

// Zwei Endpunkte (Stacker, Runner) über zwei Kanäle; ein Schritt = 1 ms.
struct Pair {
    LinkEndpoint a;  // Stacker
    LinkEndpoint b;  // Runner
    Wire ab;
    Wire ba;
    std::uint32_t now_ms = 1000;

    explicit Pair(std::uint64_t seed, LinkConfig cfg = fast_cfg(), std::uint8_t sa = 11, std::uint8_t sb = 22)
        : a(LinkRobot::stacker, sa, cfg), b(LinkRobot::runner, sb, cfg), ab(seed), ba(seed ^ 0x9E3779B97F4A7C15ull) {}

    void pump(LinkEndpoint& from, Wire& wire) {
        std::uint8_t buf[LinkFrame::max_wire_size];
        std::size_t n = 0;
        for (int i = 0; i < 4 && from.next_frame(now_ms, buf, n); ++i)
            wire.put(buf, n, now_ms);
    }
    void step() {
        ++now_ms;
        for (auto& f : ab.take(now_ms))
            b.on_bytes(f.data(), f.size(), now_ms);
        for (auto& f : ba.take(now_ms))
            a.on_bytes(f.data(), f.size(), now_ms);
        pump(a, ab);
        pump(b, ba);
    }
    void run(std::uint32_t ms) {
        for (std::uint32_t i = 0; i < ms; ++i)
            step();
    }
};

}  // namespace

TEST_CASE("Frame: alle Arten gehen hin und zurück") {
    std::uint8_t buf[LinkFrame::max_wire_size];

    LinkFrame st;
    st.kind = LinkFrame::Kind::status;
    st.sender = LinkRobot::runner;
    st.session = 7;
    st.seq = 513;
    st.status = {42, LinkStatus::has_pin};
    std::size_t n = st.encode(buf);
    CHECK(n > 0);
    LinkFrame out;
    REQUIRE(LinkFrame::decode(buf, n, out));
    CHECK(out.kind == LinkFrame::Kind::status);
    CHECK(out.sender == LinkRobot::runner);
    CHECK(out.session == 7);
    CHECK(out.seq == 513);
    CHECK(out.status.route_step == 42);
    CHECK(out.status.flags == LinkStatus::has_pin);

    LinkFrame g;
    g.kind = LinkFrame::Kind::goals;
    for (int i = 0; i < LinkFrame::kGoals; ++i)
        g.goals[i] = static_cast<std::uint8_t>(i + 1);
    n = g.encode(buf);
    REQUIRE(LinkFrame::decode(buf, n, out));
    CHECK(out.kind == LinkFrame::Kind::goals);
    for (int i = 0; i < LinkFrame::kGoals; ++i)
        CHECK(out.goals[i] == i + 1);

    LinkFrame s;
    s.kind = LinkFrame::Kind::signal;
    std::strcpy(s.name, "stack_done_1");
    n = s.encode(buf);
    REQUIRE(LinkFrame::decode(buf, n, out));
    CHECK(std::string(out.name) == "stack_done_1");

    LinkFrame ak;
    ak.kind = LinkFrame::Kind::ack;
    ak.ack_session = 9;
    ak.ack_seq = 65535;
    n = ak.encode(buf);
    REQUIRE(LinkFrame::decode(buf, n, out));
    CHECK(out.ack_session == 9);
    CHECK(out.ack_seq == 65535);
}

TEST_CASE("Frame: kaputte Frames werden abgelehnt, ohne `out` zu ändern") {
    std::uint8_t buf[LinkFrame::max_wire_size];
    LinkFrame s;
    s.kind = LinkFrame::Kind::signal;
    std::strcpy(s.name, "go");
    const std::size_t n = s.encode(buf);
    LinkFrame out;
    out.seq = 77;

    CHECK_FALSE(LinkFrame::decode(buf, n - 1, out));  // zu kurz
    CHECK_FALSE(LinkFrame::decode(buf, n + 1, out));  // zu lang
    std::uint8_t bad[LinkFrame::max_wire_size];
    for (std::size_t i = 0; i < n; ++i) {
        std::memcpy(bad, buf, n);
        bad[i] ^= 0x10;
        CHECK_FALSE(LinkFrame::decode(bad, n, out));  // jedes geänderte Byte (Prüfsumme/Magic/Art)
    }
    CHECK(out.seq == 77);
    CHECK_FALSE(LinkFrame::decode(buf, 0, out));
}

TEST_CASE("Frame: Namen müssen a-z0-9_ sein, höchstens 31 Zeichen") {
    CHECK(link_name_valid("a"));
    CHECK(link_name_valid("stack_done_2"));
    CHECK(link_name_valid("0123456789012345678901234567890"));         // 31
    CHECK_FALSE(link_name_valid("01234567890123456789012345678901"));  // 32
    CHECK_FALSE(link_name_valid(""));
    CHECK_FALSE(link_name_valid("Go"));
    CHECK_FALSE(link_name_valid("a-b"));
    CHECK_FALSE(link_name_valid("a b"));

    // Ein Frame mit ungültigem Namen (z. B. ohne Endnull) wird beim Dekodieren abgelehnt.
    std::uint8_t buf[LinkFrame::max_wire_size];
    LinkFrame s;
    s.kind = LinkFrame::Kind::signal;
    std::strcpy(s.name, "ok");
    std::size_t n = s.encode(buf);
    std::strcpy(s.name, "BAD");
    n = s.encode(buf);
    LinkFrame out;
    CHECK_FALSE(LinkFrame::decode(buf, n, out));
}

TEST_CASE("Frame: nicht verwechselbar mit den Paketen des Link-Tests (L-01)") {
    std::uint8_t buf[LinkFrame::max_wire_size];
    LinkFrame f;
    f.kind = LinkFrame::Kind::ack;
    const std::size_t n = f.encode(buf);
    LinkPacket p;
    CHECK_FALSE(LinkPacket::decode(buf, n, p));

    std::uint8_t pbuf[LinkPacket::wire_size];
    LinkPacket{LinkPacket::Kind::ping, 3, 4, 0}.encode(pbuf);
    LinkFrame out;
    CHECK_FALSE(LinkFrame::decode(pbuf, sizeof pbuf, out));
}

TEST_CASE("Signal: ohne Verlust ankommen, bestätigt, nicht doppelt zugestellt") {
    Pair p(1);
    CHECK_FALSE(p.b.signal_seen("stack_done"));
    REQUIRE(p.a.send_signal("stack_done", p.now_ms));
    p.run(200);
    CHECK(p.b.signal_seen("stack_done"));
    CHECK_FALSE(p.b.signal_seen("other"));
    CHECK(p.a.pending_signals() == 0);
    CHECK(p.a.stats().signals_acked == 1);
    CHECK(p.a.stats().signals_expired == 0);
    CHECK(p.a.stats().signals_sent == 1);
}

TEST_CASE("Signal: Sender und Empfänger sehen nur Signale des anderen") {
    Pair p(2);
    REQUIRE(p.a.send_signal("x", p.now_ms));
    p.run(100);
    CHECK(p.b.signal_seen("x"));
    CHECK_FALSE(p.a.signal_seen("x"));  // eigenes Signal kommt nicht zurück
}

TEST_CASE("Signal: bei 40 % Verlust und Duplikaten kommt es trotzdem an") {
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        Pair p(seed);
        p.ab.loss_percent = p.ba.loss_percent = 40;
        p.ab.dup_percent = p.ba.dup_percent = 20;
        p.ab.jitter_ms = p.ba.jitter_ms = 30;  // Pakete überholen sich
        REQUIRE(p.a.send_signal("go", p.now_ms));
        REQUIRE(p.b.send_signal("back", p.now_ms));
        p.run(900);
        INFO("seed " << seed);
        CHECK(p.b.signal_seen("go"));
        CHECK(p.a.signal_seen("back"));
        CHECK(p.a.pending_signals() == 0);
        CHECK(p.b.pending_signals() == 0);
    }
}

TEST_CASE("Signal: gleicher Seed gibt dasselbe Ergebnis") {
    auto run = [](std::uint64_t seed) {
        Pair p(seed);
        p.ab.loss_percent = p.ba.loss_percent = 50;
        p.a.send_signal("go", p.now_ms);
        p.run(1500);
        return std::array<std::uint32_t, 3>{p.a.stats().signals_sent, p.a.stats().signals_acked, p.b.stats().frames_in};
    };
    CHECK(run(5) == run(5));
}

TEST_CASE("Signal: totaler Ausfall — nach der Frist aufgegeben und gezählt, Verbindung gilt als weg") {
    Pair p(3);
    p.run(300);  // Status fließt
    CHECK(p.a.connected(p.now_ms));
    p.ab.down = p.ba.down = true;
    REQUIRE(p.a.send_signal("go", p.now_ms));
    p.run(700);
    CHECK(p.a.pending_signals() == 1);  // versucht es noch (Frist 1000 ms)
    CHECK_FALSE(p.a.connected(p.now_ms));
    p.run(400);  // Frist vorbei
    CHECK(p.a.pending_signals() == 0);
    CHECK(p.a.stats().signals_expired == 1);
    CHECK(p.a.stats().signals_acked == 0);
    CHECK_FALSE(p.b.signal_seen("go"));
    CHECK_FALSE(p.b.connected(p.now_ms));
}

TEST_CASE("Signal: nach Wiederverbindung geht der Betrieb weiter") {
    Pair p(4);
    p.ab.down = p.ba.down = true;
    p.run(2000);
    CHECK_FALSE(p.a.connected(p.now_ms));
    p.ab.down = p.ba.down = false;
    REQUIRE(p.a.send_signal("again", p.now_ms));
    p.run(300);
    CHECK(p.a.connected(p.now_ms));
    CHECK(p.b.connected(p.now_ms));
    CHECK(p.b.signal_seen("again"));
}

TEST_CASE("Signal: ungültiger Name und volle Warteschlange werden abgelehnt und gezählt") {
    Pair p(6);
    p.ab.down = true;  // nichts wird bestätigt
    CHECK_FALSE(p.a.send_signal("Gross", p.now_ms));
    CHECK_FALSE(p.a.send_signal("", p.now_ms));
    CHECK(p.a.stats().signals_rejected == 2);
    for (std::size_t i = 0; i < LinkEndpoint::max_pending; ++i)
        CHECK(p.a.send_signal("s", p.now_ms));
    CHECK_FALSE(p.a.send_signal("s", p.now_ms));
    CHECK(p.a.stats().signals_rejected == 3);
    CHECK(p.a.pending_signals() == LinkEndpoint::max_pending);
}

TEST_CASE("Signal: Neustart des Senders — Bestätigung der alten Sitzung zählt nicht") {
    LinkConfig cfg = fast_cfg();
    LinkEndpoint old_sender(LinkRobot::stacker, 5, cfg);
    LinkEndpoint peer(LinkRobot::runner, 9, cfg);
    std::uint8_t buf[LinkFrame::max_wire_size];
    std::size_t n = 0;
    REQUIRE(old_sender.send_signal("go", 1000));
    REQUIRE(old_sender.next_frame(1000, buf, n));
    peer.on_bytes(buf, n, 1001);
    std::uint8_t ack_for_old[LinkFrame::max_wire_size];
    std::size_t ack_n = 0;
    REQUIRE(peer.next_frame(1001, ack_for_old, ack_n));  // Bestätigung (Sitzung 5, Nummer 0)

    LinkEndpoint rebooted(LinkRobot::stacker, 6, cfg);  // neue Sitzung, Nummer fängt wieder bei 0 an
    REQUIRE(rebooted.send_signal("go", 2000));
    rebooted.on_bytes(ack_for_old, ack_n, 2001);
    CHECK(rebooted.pending_signals() == 1);
    CHECK(rebooted.stats().signals_acked == 0);
}

TEST_CASE("Status und Goal-Stand: der Partner sieht den neuesten Wert, ältere Frames überschreiben nicht") {
    Pair p(7);
    CHECK_FALSE(p.b.peer_status().has_value());
    CHECK_FALSE(p.b.peer_goals().has_value());
    p.a.set_status({3, LinkStatus::has_cup});
    std::array<std::uint8_t, LinkFrame::kGoals> g{};
    g[2] = 4;
    p.a.set_goals(g);
    p.run(500);
    REQUIRE(p.b.peer_status().has_value());
    CHECK(p.b.peer_status()->route_step == 3);
    CHECK(p.b.peer_status()->flags == LinkStatus::has_cup);
    REQUIRE(p.b.peer_goals().has_value());
    CHECK((*p.b.peer_goals())[2] == 4);

    p.a.set_status({9, 0});
    p.run(300);
    CHECK(p.b.peer_status()->route_step == 9);

    // Ein älteres Status-Frame (kleinere Nummer, gleiche Sitzung) kommt verspätet an: ignorieren.
    LinkFrame old;
    old.kind = LinkFrame::Kind::status;
    old.sender = LinkRobot::stacker;
    old.session = 11;
    old.seq = 0;
    old.status = {1, 0};
    std::uint8_t buf[LinkFrame::max_wire_size];
    const std::size_t n = old.encode(buf);
    p.b.on_bytes(buf, n, p.now_ms);
    CHECK(p.b.peer_status()->route_step == 9);
}

TEST_CASE("Status: überlebt Verlust und Streuung; Verbindung bleibt stehen") {
    Pair p(8);
    p.ab.loss_percent = p.ba.loss_percent = 30;
    p.ab.jitter_ms = p.ba.jitter_ms = 20;
    p.a.set_status({5, 0});
    p.run(3000);
    CHECK(p.b.connected(p.now_ms));
    CHECK(p.a.connected(p.now_ms));
    REQUIRE(p.b.peer_status().has_value());
    CHECK(p.b.peer_status()->route_step == 5);
}

TEST_CASE("Fremde und eigene Frames werden gezählt, nicht verarbeitet") {
    Pair p(9);
    std::uint8_t buf[LinkFrame::max_wire_size];
    LinkFrame f;
    f.kind = LinkFrame::Kind::signal;
    f.sender = LinkRobot::stacker;  // an a geschickt, der selbst Stacker ist: Echo
    std::strcpy(f.name, "echo");
    const std::size_t n = f.encode(buf);
    p.a.on_bytes(buf, n, p.now_ms);
    CHECK_FALSE(p.a.signal_seen("echo"));
    CHECK_FALSE(p.a.connected(p.now_ms));

    const std::uint8_t junk[] = {1, 2, 3};
    p.a.on_bytes(junk, sizeof junk, p.now_ms);
    CHECK(p.a.stats().frames_bad == 1);
}

TEST_CASE("Gesehene Signale: Tabelle voll → weitere werden gezählt, nicht überschrieben") {
    LinkEndpoint e(LinkRobot::runner, 1, fast_cfg());
    std::uint8_t buf[LinkFrame::max_wire_size];
    for (std::size_t i = 0; i < LinkEndpoint::max_seen + 3; ++i) {
        LinkFrame f;
        f.kind = LinkFrame::Kind::signal;
        f.sender = LinkRobot::stacker;
        f.session = 3;
        f.seq = static_cast<std::uint16_t>(i);
        std::snprintf(f.name, sizeof f.name, "n%zu", i);
        e.on_bytes(buf, f.encode(buf), 1000);
    }
    CHECK(e.signal_seen("n0"));
    CHECK(e.stats().signals_overflow == 3);
    CHECK_FALSE(e.signal_seen("n2000"));
}

// ---- wait_for im RoutePlayer: der Funk-Executor bleibt bei Ausfall nie hängen ----
namespace {

struct LinkExecutor : RouteExecutor {
    LinkEndpoint& ep;
    double wait_started_s[RoutePlayer::kMaxLanes] = {};
    std::uint32_t& now_ms;
    LinkExecutor(LinkEndpoint& e, std::uint32_t& t) : ep(e), now_ms(t) {}

    void start(int lane, const RouteCmd& c, double now_s) override {
        wait_started_s[lane] = now_s;
        if (c.op == RouteOp::signal)
            ep.send_signal(c.name, now_ms);
    }
    StepStatus poll(int, double) override { return StepStatus::running; }
    StepStatus cancel(int, double) override { return StepStatus::failed; }
};

// Signal schnell (Antwort kommt sofort), wait_for wartet, bis das Signal da ist.
struct LinkExecutor2 : LinkExecutor {
    using LinkExecutor::LinkExecutor;
    const RouteCmd* cur[RoutePlayer::kMaxLanes] = {};
    void start(int lane, const RouteCmd& c, double now_s) override {
        LinkExecutor::start(lane, c, now_s);
        cur[lane] = &c;
    }
    StepStatus poll(int lane, double) override {
        if (cur[lane]->op == RouteOp::signal)
            return StepStatus::done;
        return ep.signal_seen(cur[lane]->name) ? StepStatus::done : StepStatus::running;
    }
};

std::vector<RouteCmd> wait_route(const char* name) {
    RouteCmd w;
    w.op = RouteOp::wait_for;
    std::strncpy(w.name, name, sizeof w.name - 1);
    w.robot = RouteRobot::stacker;
    w.line = 1;
    RouteCmd e;
    e.op = RouteOp::end_hold;
    e.line = 2;
    return {w, e};
}

}  // namespace

TEST_CASE("wait_for über den Funk: Signal da → Route läuft weiter") {
    Pair p(10);
    LinkExecutor2 ex(p.b, p.now_ms);
    auto route = wait_route("go");
    RoutePlayer player(route.data(), 2, ex, RoutePlayerConfig{});
    player.begin(0.0);
    for (int i = 0; i < 100; ++i) {
        p.run(10);
        player.update(i * 0.01, 0.0);
    }
    CHECK(player.status() == RouteStatus::running);  // noch kein Signal
    p.a.send_signal("go", p.now_ms);
    for (int i = 0; i < 100; ++i) {
        p.run(10);
        player.update(1.0 + i * 0.01, 1.0);
    }
    CHECK(player.status() == RouteStatus::done);
    CHECK(player.failed_steps() == 0);
}

TEST_CASE("wait_for bei totalem Funkausfall: die Route läuft nach dem Timeout weiter") {
    Pair p(11);
    p.ab.down = p.ba.down = true;
    LinkExecutor2 ex(p.b, p.now_ms);
    auto route = wait_route("go");
    RoutePlayerConfig cfg;
    cfg.wait_for_s = 2.0;
    RoutePlayer player(route.data(), 2, ex, cfg);
    player.begin(0.0);
    p.a.send_signal("go", p.now_ms);
    for (int i = 0; i < 500 && player.status() == RouteStatus::running; ++i) {
        p.run(10);
        player.update(i * 0.01, i * 0.01);
    }
    CHECK(player.status() == RouteStatus::done);  // skip: nicht abgebrochen, nicht hängend
    CHECK(player.failed_steps() == 1);
}
