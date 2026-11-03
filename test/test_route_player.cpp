#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "match/route_player.hpp"

using namespace frostbyte;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---- Routen von Hand bauen (nicht routes/skills_*.route: das ist ein Entwurf, A-05 schreibt ihn um) ----
RouteCmd cmd(RouteOp op, const char* name = "", double timeout_s = -1.0,
             RouteOnTimeout on_timeout = RouteOnTimeout::skip) {
    RouteCmd c;
    c.op = op;
    std::strncpy(c.name, name, sizeof c.name - 1);
    c.timeout_s = timeout_s;
    c.on_timeout = on_timeout;
    return c;
}
RouteCmd block(RouteOp op, int body_len, double value = 0.0) {
    RouteCmd c;
    c.op = op;
    c.body_len = body_len;
    c.value = value;
    return c;
}
RouteCmd wait_cmd(double s) {
    RouteCmd c;
    c.op = RouteOp::wait;
    c.value = s;
    return c;
}
RouteCmd end_hold() { return cmd(RouteOp::end_hold); }
std::vector<RouteCmd> numbered(std::vector<RouteCmd> v) {
    for (size_t i = 0; i < v.size(); ++i)
        v[i].line = static_cast<int>(i) + 1;
    return v;
}

// ---- Fake-Executor: Dauer, Fehlschlag und "beim Abbrechen schon erreicht" je Befehl einstellbar ----
struct Behavior {
    double duration_s = 0.5;
    bool fail = false;            // endet nach duration_s mit failed
    bool done_on_cancel = false;  // cancel() meldet done (Ziel war schon erreicht)
};

struct Fake : RouteExecutor {
    struct Slot {
        bool active = false;
        double start_s = 0.0;
        Behavior b;
        int line = 0;
    };
    std::function<Behavior(const RouteCmd&, int start_count)> script = [](const RouteCmd&, int) { return Behavior{}; };
    Slot slots[16];
    std::map<int, int> starts;  // je Zeile
    std::vector<std::string> log;
    int cancels = 0;

    int starts_of(int line) const {
        auto it = starts.find(line);
        return it == starts.end() ? 0 : it->second;
    }
    int active_count() const {
        int n = 0;
        for (const Slot& s : slots)
            n += s.active ? 1 : 0;
        return n;
    }

    void start(int lane, const RouteCmd& c, double now_s) override {
        REQUIRE(lane >= 0);
        REQUIRE(lane < 16);
        REQUIRE_FALSE(slots[lane].active);  // eine Lane startet nie zwei Befehle gleichzeitig
        const int n = ++starts[c.line];
        slots[lane] = Slot{true, now_s, script(c, n), c.line};
        log.push_back("start:" + std::to_string(c.line) + ":" + c.name);
    }
    StepStatus poll(int lane, double now_s) override {
        Slot& s = slots[lane];
        if (!s.active)
            return StepStatus::done;
        if (now_s - s.start_s + 1e-9 < s.b.duration_s)
            return StepStatus::running;
        s.active = false;
        return s.b.fail ? StepStatus::failed : StepStatus::done;
    }
    StepStatus cancel(int lane, double) override {
        Slot& s = slots[lane];
        ++cancels;
        log.push_back("cancel:" + std::to_string(s.line));
        const bool was_active = s.active;
        s.active = false;
        return was_active && s.b.done_on_cancel ? StepStatus::done : StepStatus::failed;
    }
};

struct H {
    Fake ex;
    std::vector<RouteCmd> route;
    RoutePlayerConfig cfg;
    RoutePlayer* p = nullptr;
    int ticks = 0;
    double match_offset = 0.0;
    bool match_nan = false;

    explicit H(std::vector<RouteCmd> r, RoutePlayerConfig c = RoutePlayerConfig{})
        : route(numbered(std::move(r))), cfg(c) {}
    ~H() { delete p; }
    void begin() {
        p = new RoutePlayer(route.data(), static_cast<int>(route.size()), ex, cfg);
        p->begin(0.0);
    }
    double t() const { return ticks * 0.01; }
    void tick() {
        ++ticks;
        p->update(t(), match_nan ? kNaN : t() + match_offset);
    }
    void run(double s) {
        const int n = static_cast<int>(std::lround(s / 0.01));
        for (int i = 0; i < n; ++i)
            tick();
    }
};

}  // namespace

TEST_CASE("Vor begin() passiert nichts") {
    H h({cmd(RouteOp::goto_name, "A"), end_hold()});
    h.p = new RoutePlayer(h.route.data(), static_cast<int>(h.route.size()), h.ex, h.cfg);
    CHECK(h.p->status() == RouteStatus::idle);
    h.tick();
    CHECK(h.ex.log.empty());
    CHECK(h.p->status() == RouteStatus::idle);
}

TEST_CASE("Executor-Befehle: je Befehlsart genau ein Start, dann weiter") {
    const RouteOp ops[] = {RouteOp::start, RouteOp::goto_name, RouteOp::goto_pose, RouteOp::turn,    RouteOp::intake,
                           RouteOp::cycle, RouteOp::toggle,    RouteOp::signal,    RouteOp::wait_for};
    for (RouteOp op : ops) {
        CAPTURE(static_cast<int>(op));
        H h({cmd(op, "NAME"), end_hold()});
        h.begin();
        h.run(0.3);
        CHECK(h.ex.starts_of(1) == 1);
        CHECK(h.ex.log[0] == "start:1:NAME");
        CHECK(h.p->status() == RouteStatus::running);
        h.run(0.4);
        CHECK(h.p->status() == RouteStatus::done);
        CHECK(h.ex.starts_of(1) == 1);
        CHECK(h.p->failed_steps() == 0);
    }
}

TEST_CASE("target_p10, wait und end_hold erledigt der Player selbst") {
    H h({cmd(RouteOp::target_p10), wait_cmd(0.5), end_hold()});
    h.begin();
    h.run(0.45);
    CHECK(h.p->status() == RouteStatus::running);
    h.run(0.1);
    CHECK(h.p->status() == RouteStatus::done);
    CHECK(h.ex.log.empty());

    H h2({end_hold()});
    h2.begin();
    h2.tick();
    CHECK(h2.p->status() == RouteStatus::done);
    CHECK(h2.ex.log.empty());
}

TEST_CASE("Befehle laufen nacheinander in Reihenfolge") {
    H h({cmd(RouteOp::goto_name, "A"), cmd(RouteOp::turn, "B"), cmd(RouteOp::goto_name, "C"), end_hold()});
    h.begin();
    for (int i = 0; i < 300 && h.p->status() == RouteStatus::running; ++i) {
        h.tick();
        CHECK(h.ex.active_count() <= 1);
    }
    CHECK(h.p->status() == RouteStatus::done);
    REQUIRE(h.ex.log.size() == 3);
    CHECK(h.ex.log[0] == "start:1:A");
    CHECK(h.ex.log[1] == "start:2:B");
    CHECK(h.ex.log[2] == "start:3:C");
}

TEST_CASE("Jeder Befehl hat ein Timeout: ohne timeout= gilt der Standardwert") {
    RoutePlayerConfig cfg;
    cfg.wait_for_s = 2.0;
    H h({cmd(RouteOp::wait_for, "runner.x"), cmd(RouteOp::goto_name, "B"), end_hold()}, cfg);
    h.ex.script = [](const RouteCmd&, int) { return Behavior{100.0}; };
    h.begin();
    h.run(1.9);
    CHECK(h.ex.starts_of(2) == 0);
    h.run(0.2);
    CHECK(h.ex.cancels == 1);
    CHECK(h.ex.starts_of(2) == 1);  // skip: der nächste Befehl läuft
    CHECK(h.p->failed_steps() == 1);
    CHECK(h.p->last_failed_line() == 1);
}

TEST_CASE("Standard-Timeout je Befehlsart aus der Konfiguration") {
    struct Row {
        RouteOp op;
        double RoutePlayerConfig::*field;
    };
    const Row rows[] = {
        {RouteOp::goto_name, &RoutePlayerConfig::goto_s},    {RouteOp::goto_pose, &RoutePlayerConfig::goto_s},
        {RouteOp::turn, &RoutePlayerConfig::turn_s},         {RouteOp::intake, &RoutePlayerConfig::intake_s},
        {RouteOp::cycle, &RoutePlayerConfig::cycle_s},       {RouteOp::toggle, &RoutePlayerConfig::toggle_s},
        {RouteOp::wait_for, &RoutePlayerConfig::wait_for_s}, {RouteOp::start, &RoutePlayerConfig::other_s},
        {RouteOp::signal, &RoutePlayerConfig::other_s}};
    for (const Row& row : rows) {
        CAPTURE(static_cast<int>(row.op));
        RoutePlayerConfig cfg;
        cfg.*row.field = 1.5;
        H h({cmd(row.op, "X"), end_hold()}, cfg);
        h.ex.script = [](const RouteCmd&, int) { return Behavior{100.0}; };
        h.begin();
        h.run(1.4);
        CHECK(h.ex.cancels == 0);
        h.run(0.2);
        CHECK(h.ex.cancels == 1);
        CHECK(h.p->status() == RouteStatus::done);
    }
}

TEST_CASE("timeout= aus der Route schlägt den Standardwert; Ablauf bricht ab und läuft weiter (skip)") {
    H h({cmd(RouteOp::goto_name, "A", 1.0), cmd(RouteOp::goto_name, "B"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 1 ? Behavior{10.0} : Behavior{}; };
    h.begin();
    h.run(0.95);
    CHECK(h.ex.starts_of(2) == 0);
    h.run(0.1);
    CHECK(h.ex.log[1] == "cancel:1");
    CHECK(h.ex.starts_of(2) == 1);
    h.run(1.0);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("Meldet der Executor failed, gilt dieselbe Regel wie beim Timeout") {
    H h({cmd(RouteOp::goto_name, "A"), cmd(RouteOp::goto_name, "B"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 1 ? Behavior{0.2, true} : Behavior{}; };
    h.begin();
    h.run(0.3);
    CHECK(h.ex.starts_of(2) == 1);
    CHECK(h.p->failed_steps() == 1);
    CHECK(h.ex.cancels == 0);  // er hat selbst aufgehört, kein cancel nötig
}

TEST_CASE("retry: genau ein weiterer Versuch") {
    {
        H h({cmd(RouteOp::goto_name, "A", 1.0, RouteOnTimeout::retry), end_hold()});
        h.ex.script = [](const RouteCmd&, int n) { return n == 1 ? Behavior{0.2, true} : Behavior{}; };
        h.begin();
        h.run(1.0);
        CHECK(h.ex.starts_of(1) == 2);
        CHECK(h.p->status() == RouteStatus::done);
        CHECK(h.p->failed_steps() == 1);
    }
    {
        // Scheitert auch der zweite Versuch, geht es wie bei skip weiter.
        H h({cmd(RouteOp::goto_name, "A", 1.0, RouteOnTimeout::retry), cmd(RouteOp::turn, "B"), end_hold()});
        h.ex.script = [](const RouteCmd& c, int) { return c.line == 1 ? Behavior{0.2, true} : Behavior{}; };
        h.begin();
        h.run(2.0);
        CHECK(h.ex.starts_of(1) == 2);
        CHECK(h.ex.starts_of(2) == 1);
        CHECK(h.p->status() == RouteStatus::done);
        CHECK(h.p->failed_steps() == 2);
    }
}

TEST_CASE("retry-Zähler beginnt in jedem repeat-Durchgang neu") {
    H h({block(RouteOp::repeat, 1, 2), cmd(RouteOp::goto_name, "A", 1.0, RouteOnTimeout::retry), end_hold()});
    h.ex.script = [](const RouteCmd&, int n) { return n % 2 == 1 ? Behavior{0.2, true} : Behavior{}; };
    h.begin();
    h.run(3.0);
    CHECK(h.p->status() == RouteStatus::done);
    CHECK(h.ex.starts_of(2) == 4);  // je Durchgang: Fehlschlag, dann Erfolg
}

TEST_CASE("abort beendet die Route und schaltet offene at-Wächter ab") {
    H h({cmd(RouteOp::goto_name, "A", 1.0, RouteOnTimeout::abort), cmd(RouteOp::goto_name, "B"),
         block(RouteOp::at, 1, 3.0), cmd(RouteOp::goto_name, "MIDFIELD"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 1 ? Behavior{10.0} : Behavior{}; };
    h.begin();
    h.run(1.2);
    CHECK(h.p->status() == RouteStatus::aborted);
    CHECK(h.ex.active_count() == 0);
    h.run(10.0);
    CHECK(h.ex.starts_of(2) == 0);
    CHECK(h.ex.starts_of(4) == 0);  // auch das at 3s startet nicht mehr
    CHECK(h.p->status() == RouteStatus::aborted);
}

TEST_CASE("abort bricht auch parallele Lanes ab") {
    H h({block(RouteOp::parallel, 2), cmd(RouteOp::goto_name, "A", 1.0, RouteOnTimeout::abort), cmd(RouteOp::turn, "B"),
         end_hold()});
    h.ex.script = [](const RouteCmd&, int) { return Behavior{10.0}; };
    h.begin();
    h.run(1.2);
    CHECK(h.p->status() == RouteStatus::aborted);
    CHECK(h.ex.active_count() == 0);
    CHECK(h.ex.cancels == 2);
}

TEST_CASE("Zyklus im RUECKZUG ins Timeout: cancel meldet done, retry startet nicht noch einmal") {
    H h({cmd(RouteOp::cycle, "RED", 1.0, RouteOnTimeout::retry), cmd(RouteOp::goto_name, "B"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) {
        return c.line == 1 ? Behavior{10.0, false, /*done_on_cancel=*/true} : Behavior{};
    };
    h.begin();
    h.run(1.2);
    CHECK(h.ex.starts_of(1) == 1);
    CHECK(h.ex.starts_of(2) == 1);  // die Route läuft weiter
    CHECK(h.p->failed_steps() == 0);
    h.run(1.0);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("repeat und verschachteltes repeat") {
    H h({block(RouteOp::repeat, 1, 3), cmd(RouteOp::goto_name, "A"), end_hold()});
    h.begin();
    h.run(3.0);
    CHECK(h.ex.starts_of(2) == 3);
    CHECK(h.p->status() == RouteStatus::done);

    H h2({block(RouteOp::repeat, 4, 2), cmd(RouteOp::goto_name, "A"), block(RouteOp::repeat, 1, 3),
          cmd(RouteOp::turn, "B"), cmd(RouteOp::goto_name, "C"), end_hold()});
    h2.begin();
    h2.run(20.0);
    CHECK(h2.ex.starts_of(2) == 2);
    CHECK(h2.ex.starts_of(4) == 6);
    CHECK(h2.ex.starts_of(5) == 2);
    CHECK(h2.p->status() == RouteStatus::done);
    // Reihenfolge im ersten Durchgang: A, B, B, B, C
    CHECK(h2.ex.log[0] == "start:2:A");
    CHECK(h2.ex.log[3] == "start:4:B");
    CHECK(h2.ex.log[4] == "start:5:C");
}

TEST_CASE("parallel: alle Kinder gleichzeitig, fertig erst wenn alle fertig sind") {
    H h({block(RouteOp::parallel, 2), cmd(RouteOp::goto_name, "A"), cmd(RouteOp::turn, "B"),
         cmd(RouteOp::goto_name, "C"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 3 ? Behavior{1.0} : Behavior{0.5}; };
    h.begin();
    h.tick();
    CHECK(h.ex.starts_of(2) == 1);
    CHECK(h.ex.starts_of(3) == 1);
    CHECK(h.ex.active_count() == 2);
    h.run(0.6);  // A ist fertig, B läuft noch
    CHECK(h.ex.starts_of(4) == 0);
    h.run(0.5);
    CHECK(h.ex.starts_of(4) == 1);
    h.run(1.0);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("parallel mit repeat als Kind") {
    H h({block(RouteOp::parallel, 3), block(RouteOp::repeat, 1, 2), cmd(RouteOp::goto_name, "A"),
         cmd(RouteOp::turn, "B"), cmd(RouteOp::goto_name, "C"), end_hold()});
    h.begin();
    h.run(0.3);
    CHECK(h.ex.starts_of(3) == 1);
    CHECK(h.ex.starts_of(4) == 1);
    h.run(0.4);  // erster A-Durchgang fertig, zweiter läuft; B ist fertig, C wartet auf beide
    CHECK(h.ex.starts_of(3) == 2);
    CHECK(h.ex.starts_of(5) == 0);
    h.run(1.0);
    CHECK(h.ex.starts_of(5) == 1);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("at (spät): bricht den laufenden Befehl ab, führt seinen Befehl aus, danach geht es hinter dem at weiter") {
    H h({cmd(RouteOp::goto_name, "A"), block(RouteOp::at, 1, 5.0), cmd(RouteOp::goto_name, "B"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 1 ? Behavior{100.0} : Behavior{}; };
    h.begin();
    h.run(4.9);
    CHECK(h.ex.starts_of(3) == 0);
    h.run(0.2);
    CHECK(h.ex.cancels == 1);
    CHECK(h.ex.starts_of(1) == 1);
    CHECK(h.ex.starts_of(3) == 1);
    h.run(0.6);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("at (früh): der Ablauf wartet bis zur Matchzeit, der Befehl läuft nie vorher") {
    H h({cmd(RouteOp::goto_name, "A"), block(RouteOp::at, 1, 5.0), cmd(RouteOp::goto_name, "B"), end_hold()});
    h.begin();
    h.run(4.9);
    CHECK(h.ex.starts_of(1) == 1);
    CHECK(h.ex.starts_of(3) == 0);
    CHECK(h.p->status() == RouteStatus::running);
    h.run(0.2);
    CHECK(h.ex.starts_of(3) == 1);
    CHECK(h.ex.cancels == 0);
    h.run(0.6);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("at benutzt die gelieferte Matchzeit, nicht die Zeit seit begin()") {
    H h({block(RouteOp::at, 1, 5.0), cmd(RouteOp::goto_name, "B"), end_hold()});
    h.match_offset = 4.5;  // Match läuft schon 4,5 s, als die Route beginnt
    h.begin();
    h.run(0.4);
    CHECK(h.ex.starts_of(2) == 0);
    h.run(0.2);
    CHECK(h.ex.starts_of(2) == 1);
}

TEST_CASE("Mehrere at fällig: das mit der höchsten Position gewinnt") {
    H h({cmd(RouteOp::goto_name, "A"), block(RouteOp::at, 1, 3.0), cmd(RouteOp::goto_name, "B"),
         block(RouteOp::at, 1, 4.0), cmd(RouteOp::goto_name, "C"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 1 ? Behavior{100.0} : Behavior{}; };
    h.match_offset = 10.0;  // beide sind schon fällig
    h.begin();
    h.run(0.1);
    CHECK(h.ex.starts_of(3) == 0);
    CHECK(h.ex.starts_of(5) == 1);
    h.run(1.0);
    CHECK(h.ex.starts_of(3) == 0);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("at bricht alle parallelen Lanes ab") {
    H h({block(RouteOp::parallel, 2), cmd(RouteOp::goto_name, "A"), cmd(RouteOp::turn, "B"), block(RouteOp::at, 1, 2.0),
         cmd(RouteOp::goto_name, "C"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 5 ? Behavior{} : Behavior{100.0}; };
    h.begin();
    h.run(2.1);
    CHECK(h.ex.cancels == 2);
    CHECK(h.ex.starts_of(5) == 1);
    h.run(1.0);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("at feuert nicht erneut, wenn es schon ausgeführt wurde") {
    H h({block(RouteOp::at, 1, 1.0), cmd(RouteOp::goto_name, "B"), cmd(RouteOp::goto_name, "C"), end_hold()});
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 3 ? Behavior{5.0} : Behavior{0.2}; };
    h.begin();
    h.run(3.0);
    CHECK(h.ex.starts_of(2) == 1);
    CHECK(h.ex.starts_of(3) == 1);
    CHECK(h.ex.cancels == 0);
}

TEST_CASE("at: Befehl hat sein eigenes Timeout und on_timeout") {
    H h({cmd(RouteOp::goto_name, "A"), block(RouteOp::at, 1, 2.0),
         cmd(RouteOp::goto_name, "B", 1.0, RouteOnTimeout::abort), cmd(RouteOp::goto_name, "C"), end_hold()});
    h.ex.script = [](const RouteCmd&, int) { return Behavior{100.0}; };
    h.begin();
    h.run(3.5);
    CHECK(h.p->status() == RouteStatus::aborted);
    CHECK(h.ex.starts_of(4) == 0);
}

TEST_CASE("Matchzeit unbekannt (NaN): kein at löst aus, ein erreichtes at wartet") {
    RoutePlayerConfig cfg;
    cfg.goto_s = 2.0;
    H h({cmd(RouteOp::goto_name, "A"), block(RouteOp::at, 1, 1.0), cmd(RouteOp::goto_name, "B"), end_hold()}, cfg);
    h.ex.script = [](const RouteCmd& c, int) { return c.line == 1 ? Behavior{100.0} : Behavior{}; };
    h.match_nan = true;
    h.begin();
    h.run(5.0);
    CHECK(h.ex.starts_of(3) == 0);
    CHECK(h.ex.cancels == 1);                      // A läuft nur ins Standard-Timeout (2 s); das at feuert nicht
    CHECK(h.p->status() == RouteStatus::running);  // das erreichte at wartet

    H h2({cmd(RouteOp::goto_name, "A"), block(RouteOp::at, 1, 1.0), cmd(RouteOp::goto_name, "B"), end_hold()});
    h2.match_nan = true;
    h2.begin();
    h2.run(5.0);
    CHECK(h2.ex.starts_of(3) == 0);
    CHECK(h2.p->status() == RouteStatus::running);
}

TEST_CASE("Schrittgrenze pro Takt verschiebt den Rest, ohne Fehler") {
    std::vector<RouteCmd> v;
    for (int i = 0; i < 200; ++i)
        v.push_back(cmd(RouteOp::signal, "s"));
    v.push_back(end_hold());
    H h(v);
    h.ex.script = [](const RouteCmd&, int) { return Behavior{0.0}; };
    h.begin();
    h.tick();
    CHECK(h.p->status() == RouteStatus::running);
    CHECK(h.ex.log.size() <= static_cast<size_t>(RoutePlayer::kMaxStepsPerTick));
    h.run(0.1);
    CHECK(h.p->status() == RouteStatus::done);
    CHECK(h.ex.log.size() == 200);
    CHECK(h.p->failed_steps() == 0);
}

TEST_CASE("current_cmd zeigt den laufenden Befehl der Hauptlane") {
    H h({cmd(RouteOp::goto_name, "A"), cmd(RouteOp::cycle, "RED"), end_hold()});
    h.begin();
    h.tick();
    REQUIRE(h.p->current_cmd() != nullptr);
    CHECK(h.p->current_cmd()->line == 1);
    h.run(0.6);
    REQUIRE(h.p->current_cmd() != nullptr);
    CHECK(h.p->current_cmd()->line == 2);
    CHECK(std::strcmp(h.p->current_cmd()->name, "RED") == 0);
    h.run(1.0);
    CHECK(h.p->status() == RouteStatus::done);
    CHECK(h.p->current_cmd() == nullptr);
}

TEST_CASE("Nach done und aborted tut update() nichts mehr") {
    H h({cmd(RouteOp::goto_name, "A"), end_hold()});
    h.begin();
    h.run(1.0);
    REQUIRE(h.p->status() == RouteStatus::done);
    const size_t n = h.ex.log.size();
    h.run(1.0);
    CHECK(h.ex.log.size() == n);
    CHECK(h.p->status() == RouteStatus::done);
}

TEST_CASE("Ungültige Strukturen: invalid beim Anlegen, der Executor sieht nichts") {
    std::vector<std::vector<RouteCmd>> bad;
    bad.push_back({});                                                                        // leer
    bad.push_back({cmd(RouteOp::goto_name, "A")});                                            // kein end_hold
    bad.push_back({end_hold(), cmd(RouteOp::goto_name, "A")});                                // end_hold nicht zuletzt
    bad.push_back({cmd(RouteOp::goto_name, "A"), end_hold(), end_hold()});                    // end_hold doppelt
    bad.push_back({block(RouteOp::repeat, 9, 2), cmd(RouteOp::goto_name, "A"), end_hold()});  // body_len zu lang
    bad.push_back({block(RouteOp::repeat, 1, 0), cmd(RouteOp::goto_name, "A"), end_hold()});  // repeat 0
    bad.push_back({block(RouteOp::parallel, -1), cmd(RouteOp::goto_name, "A"), end_hold()});  // body_len negativ
    bad.push_back(
        {block(RouteOp::repeat, 2, 2), cmd(RouteOp::goto_name, "A"), end_hold()});  // Block ragt über end_hold hinaus
    bad.push_back({block(RouteOp::repeat, 3, 2), block(RouteOp::at, 1, 2.0), cmd(RouteOp::goto_name, "A"),
                   cmd(RouteOp::turn, "B"), end_hold()});  // at nicht oben
    bad.push_back({block(RouteOp::at, 2, 2.0), cmd(RouteOp::goto_name, "A"), cmd(RouteOp::turn, "B"),
                   end_hold()});                                                             // at mit 2 Befehlen
    bad.push_back({block(RouteOp::at, 1, 2.0), block(RouteOp::repeat, 0, 2), end_hold()});   // at mit Block
    bad.push_back({block(RouteOp::at, 1, kNaN), cmd(RouteOp::goto_name, "A"), end_hold()});  // at ohne Zeit
    // Lane-Überlauf: 1 + (3 + 1 + 1 + ...) > kMaxLanes
    bad.push_back({block(RouteOp::parallel, 8), block(RouteOp::parallel, 4), block(RouteOp::parallel, 2),
                   cmd(RouteOp::goto_name, "A"), cmd(RouteOp::goto_name, "B"), cmd(RouteOp::goto_name, "C"),
                   cmd(RouteOp::goto_name, "D"), cmd(RouteOp::goto_name, "E"), cmd(RouteOp::goto_name, "F"),
                   end_hold()});
    // repeat zu tief verschachtelt (5 > kMaxRepeatDepth)
    bad.push_back({block(RouteOp::repeat, 5, 2), block(RouteOp::repeat, 4, 2), block(RouteOp::repeat, 3, 2),
                   block(RouteOp::repeat, 2, 2), block(RouteOp::repeat, 1, 2), cmd(RouteOp::goto_name, "A"),
                   end_hold()});
    // zu viele at
    {
        std::vector<RouteCmd> v;
        for (int i = 0; i <= RoutePlayer::kMaxAts; ++i) {
            v.push_back(block(RouteOp::at, 1, 1.0 + i));
            v.push_back(cmd(RouteOp::goto_name, "A"));
        }
        v.push_back(end_hold());
        bad.push_back(v);
    }
    for (size_t i = 0; i < bad.size(); ++i) {
        CAPTURE(i);
        H h(bad[i]);
        h.begin();
        CHECK(h.p->status() == RouteStatus::invalid);
        h.run(1.0);
        CHECK(h.ex.log.empty());
        CHECK(h.p->status() == RouteStatus::invalid);
    }
}

TEST_CASE("Gültige Grenzfälle: 6 Lanes, 4 verschachtelte repeat") {
    {
        H h({block(RouteOp::parallel, 5), cmd(RouteOp::goto_name, "A"), cmd(RouteOp::turn, "B"),
             cmd(RouteOp::goto_name, "C"), cmd(RouteOp::intake, "D"), cmd(RouteOp::toggle, "E"), end_hold()});
        h.begin();
        CHECK(h.p->status() == RouteStatus::running);
        h.tick();
        CHECK(h.ex.active_count() == 5);
        h.run(1.0);
        CHECK(h.p->status() == RouteStatus::done);
    }
    {
        H h({block(RouteOp::repeat, 4, 2), block(RouteOp::repeat, 3, 2), block(RouteOp::repeat, 2, 2),
             block(RouteOp::repeat, 1, 2), cmd(RouteOp::goto_name, "A"), end_hold()});
        h.begin();
        CHECK(h.p->status() == RouteStatus::running);
        h.run(20.0);
        CHECK(h.ex.starts_of(5) == 16);
        CHECK(h.p->status() == RouteStatus::done);
    }
}
