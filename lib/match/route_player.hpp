#pragma once

#include <cmath>
#include <cstdint>

#include "match/route.hpp"

// Schicht 1: RoutePlayer (A-03, ADR-029). Spielt die flache RouteCmd-Liste (ADR-023) Takt für Takt ab: Reihenfolge, `parallel`,
// `repeat`, `at <t>`, Timeouts und `on_timeout`. Reine Steuerlogik ohne Spielfeld: Feldnamen, `level=auto` und die Geräte kennt
// nur der RouteExecutor (A-10), den der Player nach unten aufruft. Zeit kommt als Parameter. Feste Felder, kein dynamischer
// Speicher, keine PROS-Header.
namespace frostbyte {

enum class StepStatus { running, done, failed };

// Wird von der Schicht darunter implementiert (Adapter auf Antrieb, CycleController, Intake, Toggle, Funk). `lane` ist der Slot:
// je Lane läuft höchstens ein Befehl.
class RouteExecutor {
public:
    virtual ~RouteExecutor() = default;
    virtual void start(int lane, const RouteCmd& cmd, double now_s) = 0;
    virtual StepStatus poll(int lane, double now_s) = 0;
    // Bricht den Befehl der Lane ab und liefert seinen Endstatus: done, wenn das Ziel schon erreicht war (z. B. Objekt gesetzt),
    // sonst failed. on_timeout greift nur bei failed (ADR-029).
    virtual StepStatus cancel(int lane, double now_s) = 0;
};

enum class RouteStatus { idle, running, done, aborted, invalid };

// Standard-Timeouts je Befehlsart, wenn die Route keins nennt (jeder Befehl hat eins, CLAUDE.md). Startwerte; Werte je Roboter
// in config/.
struct RoutePlayerConfig {
    double goto_s = 8.0;  // goto <NAME>, goto pose
    double turn_s = 4.0;
    double intake_s = 4.0;
    double cycle_s = 12.0;
    double toggle_s = 4.0;
    double wait_for_s = 10.0;
    double other_s = 1.0;  // start, signal
};

class RoutePlayer {
public:
    static constexpr int kMaxLanes = 6;
    static constexpr int kMaxRepeatDepth = 4;
    static constexpr int kMaxAts = 8;
    static constexpr int kMaxStepsPerTick = 64;

    // Prüft die Struktur sofort (status() == invalid statt Abbruch im Match): Blockgrenzen, end_hold genau einmal und zuletzt,
    // at nur oben mit genau einer Einzelanweisung, Lanes und repeat-Tiefe in den Grenzen.
    RoutePlayer(const RouteCmd* cmds, int len, RouteExecutor& exec, const RoutePlayerConfig& cfg)
        : cmds_(cmds), len_(len), exec_(exec), cfg_(cfg) {
        status_ = validate() ? RouteStatus::idle : RouteStatus::invalid;
    }

    void begin(double) {
        if (status_ != RouteStatus::idle)
            return;
        for (Lane& l : lanes_)
            l = Lane{};
        init_lane(0, 0, len_, -1);
        status_ = RouteStatus::running;
    }

    // match_time_s: Zeit seit Matchbeginn (NaN = unbekannt: kein `at` löst aus, ein erreichtes `at` wartet).
    void update(double now_s, double match_time_s) {
        if (status_ != RouteStatus::running)
            return;
        steps_ = 0;
        check_at(now_s, match_time_s);
        for (int i = 0; i < kMaxLanes && status_ == RouteStatus::running; ++i)
            run_lane(i, now_s, match_time_s);
    }

    RouteStatus status() const { return status_; }
    // Befehl der Hauptlane (für die Log-Spalte route_step); nullptr, wenn keine Route läuft.
    const RouteCmd* current_cmd() const {
        return status_ == RouteStatus::running && lanes_[0].st != LaneState::free ? &cmds_[lanes_[0].cur] : nullptr;
    }
    int failed_steps() const { return failed_; }  // gescheiterte Versuche (Timeout oder failed), auch bei skip/retry
    int last_failed_line() const { return last_failed_line_; }

private:
    enum class LaneState : std::uint8_t { free, ready, leaf, children, at_wait };

    struct Frame {
        int start = 0, end = 0, remaining = 0;
    };

    struct Lane {
        LaneState st = LaneState::free;
        int pc = 0, end = 0, cur = 0, parent = -1, children_left = 0, depth = 0;
        Frame frames[kMaxRepeatDepth];
        double leaf_start_s = 0.0;
        bool retried = false;
        bool exec_active = false;  // der Executor führt den Befehl der Lane gerade aus
    };

    // ---- Prüfung beim Anlegen ----
    static bool is_block(RouteOp op) { return op == RouteOp::repeat || op == RouteOp::parallel || op == RouteOp::at; }

    // Misst [a, b): gleichzeitig nötige Lanes (die laufende eingerechnet) und repeat-Tiefe. false bei Strukturfehlern.
    bool measure(int a, int b, int nest, int& lanes, int& depth) {
        lanes = 1;
        depth = 0;
        if (nest > 16)
            return false;
        for (int i = a; i < b;) {
            const RouteCmd& c = cmds_[i];
            const int span = 1 + c.body_len;
            if (c.body_len < 0 || i + span > b)
                return false;
            int l = 1, d = 0;
            switch (c.op) {
                case RouteOp::end_hold:
                    if (nest != 0 || i != len_ - 1 || c.body_len != 0)
                        return false;
                    break;
                case RouteOp::repeat:
                    if (c.body_len < 1 || !(c.value >= 1.0 && c.value <= 1000.0))
                        return false;
                    if (!measure(i + 1, i + span, nest + 1, l, d) || d + 1 > kMaxRepeatDepth)
                        return false;
                    ++d;
                    break;
                case RouteOp::parallel: {
                    if (c.body_len < 1)
                        return false;
                    int sum = 0;
                    for (int j = i + 1; j < i + span;) {
                        const int cs = 1 + cmds_[j].body_len;
                        int cl = 0, cd = 0;
                        if (cmds_[j].body_len < 0 || j + cs > i + span || !measure(j, j + cs, nest + 1, cl, cd))
                            return false;
                        sum += cl;
                        d = cd > d ? cd : d;
                        j += cs;
                    }
                    l = 1 + sum;
                    break;
                }
                case RouteOp::at: {
                    if (nest != 0 || c.body_len != 1 || !std::isfinite(c.value) || !(c.value > 0.0) ||
                        n_ats_ >= kMaxAts)
                        return false;
                    const RouteOp body = cmds_[i + 1].op;
                    if (is_block(body) || body == RouteOp::end_hold || body == RouteOp::target_p10)
                        return false;
                    ats_[n_ats_++] = i;
                    break;
                }
                case RouteOp::wait:
                    if (c.body_len != 0 || !std::isfinite(c.value) || c.value < 0.0)
                        return false;
                    break;
                default:
                    if (c.body_len != 0)
                        return false;
                    break;
            }
            lanes = l > lanes ? l : lanes;
            depth = d > depth ? d : depth;
            i += span;
        }
        return true;
    }

    bool validate() {
        n_ats_ = 0;
        if (cmds_ == nullptr || len_ < 1 || cmds_[len_ - 1].op != RouteOp::end_hold)
            return false;
        int lanes = 0, depth = 0;
        if (!measure(0, len_, 0, lanes, depth))
            return false;
        return lanes <= kMaxLanes && depth <= kMaxRepeatDepth;
    }

    // ---- Ablauf ----
    void init_lane(int i, int pc, int end, int parent) {
        Lane& l = lanes_[i];
        l = Lane{};
        l.st = LaneState::ready;
        l.pc = pc;
        l.cur = pc;
        l.end = end;
        l.parent = parent;
    }

    int alloc_lane() const {
        for (int i = 0; i < kMaxLanes; ++i)
            if (lanes_[i].st == LaneState::free)
                return i;
        return -1;
    }

    // Index des Top-Level-Befehls, in dem die Hauptlane gerade steht.
    int top_index() const {
        const Lane& m = lanes_[0];
        return m.depth > 0 ? m.frames[0].start - 1 : m.cur;
    }

    double timeout_for(const RouteCmd& c) const {
        if (c.timeout_s > 0.0)
            return c.timeout_s;
        switch (c.op) {
            case RouteOp::goto_name:
            case RouteOp::goto_pose: return cfg_.goto_s;
            case RouteOp::turn: return cfg_.turn_s;
            case RouteOp::intake: return cfg_.intake_s;
            case RouteOp::cycle: return cfg_.cycle_s;
            case RouteOp::toggle: return cfg_.toggle_s;
            case RouteOp::wait_for: return cfg_.wait_for_s;
            default: return cfg_.other_s;
        }
    }

    void cancel_all(double now_s) {
        for (int i = 0; i < kMaxLanes; ++i) {
            if (lanes_[i].exec_active)
                exec_.cancel(i, now_s);
            lanes_[i].exec_active = false;
        }
    }

    void abort_route(double now_s) {
        cancel_all(now_s);
        for (Lane& l : lanes_)
            l.st = LaneState::free;
        status_ = RouteStatus::aborted;
    }

    void check_at(double now_s, double match_time_s) {
        if (!std::isfinite(match_time_s))
            return;
        int best = -1;
        for (int k = 0; k < n_ats_; ++k) {
            const int a = ats_[k];
            if (cmds_[a].value <= match_time_s && a > top_index())
                best = a;  // aufsteigend: das letzte fällige hat die höchste Position
        }
        if (best < 0)
            return;
        cancel_all(now_s);
        for (int i = 1; i < kMaxLanes; ++i)
            lanes_[i].st = LaneState::free;
        init_lane(0, best + 1, len_, -1);
        lanes_[0].cur = best;
    }

    void run_lane(int i, double now_s, double match_time_s) {
        Lane& l = lanes_[i];
        if (l.st == LaneState::at_wait) {
            if (std::isfinite(match_time_s) && match_time_s >= cmds_[l.cur].value) {
                l.st = LaneState::ready;
                l.pc = l.cur + 1;
            }
        }
        if (l.st == LaneState::leaf)
            poll_leaf(i, now_s);
        if (l.st == LaneState::ready)
            advance(i, now_s, match_time_s);
    }

    void finish_lane(int i) {
        Lane& l = lanes_[i];
        l.st = LaneState::free;
        if (l.parent < 0) {
            status_ = RouteStatus::done;  // nur ohne end_hold möglich; die Prüfung verlangt ihn
            return;
        }
        Lane& p = lanes_[l.parent];
        if (--p.children_left == 0)
            p.st = LaneState::ready;
    }

    void begin_attempt(int i, double now_s) {
        Lane& l = lanes_[i];
        const RouteCmd& c = cmds_[l.pc];
        l.st = LaneState::leaf;
        l.leaf_start_s = now_s;
        if (c.op != RouteOp::wait) {
            exec_.start(i, c, now_s);
            l.exec_active = true;
        }
    }

    void poll_leaf(int i, double now_s) {
        Lane& l = lanes_[i];
        const RouteCmd& c = cmds_[l.pc];
        StepStatus s;
        if (c.op == RouteOp::wait) {
            s = now_s - l.leaf_start_s + 1e-9 >= c.value ? StepStatus::done : StepStatus::running;
        } else {
            s = exec_.poll(i, now_s);
            if (s == StepStatus::running && now_s - l.leaf_start_s >= timeout_for(c)) {
                s = exec_.cancel(i, now_s);  // Ablauf: der Endstatus entscheidet, ob es ein Fehlschlag war
                l.exec_active = false;
            }
        }
        if (s == StepStatus::running)
            return;
        l.exec_active = false;
        if (s == StepStatus::done) {
            ++l.pc;
            l.st = LaneState::ready;
            return;
        }
        ++failed_;
        last_failed_line_ = c.line;
        switch (c.on_timeout) {
            case RouteOnTimeout::retry:
                if (!l.retried) {
                    l.retried = true;
                    begin_attempt(i, now_s);
                    return;
                }
                [[fallthrough]];
            case RouteOnTimeout::skip:
                ++l.pc;
                l.st = LaneState::ready;
                return;
            case RouteOnTimeout::abort: abort_route(now_s); return;
        }
    }

    void advance(int i, double now_s, double match_time_s) {
        Lane& l = lanes_[i];
        while (l.st == LaneState::ready && status_ == RouteStatus::running) {
            while (l.depth > 0 && l.pc >= l.frames[l.depth - 1].end) {
                Frame& f = l.frames[l.depth - 1];
                if (f.remaining > 0) {
                    --f.remaining;
                    l.pc = f.start;
                    break;
                }
                --l.depth;
            }
            if (l.pc >= l.end) {
                finish_lane(i);
                return;
            }
            if (++steps_ > kMaxStepsPerTick)
                return;  // Rest im nächsten Takt
            const RouteCmd& c = cmds_[l.pc];
            switch (c.op) {
                case RouteOp::target_p10: ++l.pc; break;
                case RouteOp::end_hold:
                    cancel_all(now_s);
                    for (Lane& o : lanes_)
                        o.st = LaneState::free;
                    status_ = RouteStatus::done;
                    return;
                case RouteOp::repeat:
                    l.frames[l.depth++] = Frame{l.pc + 1, l.pc + 1 + c.body_len, static_cast<int>(c.value) - 1};
                    l.cur = l.pc;
                    ++l.pc;
                    break;
                case RouteOp::parallel: {
                    const int stop = l.pc + 1 + c.body_len;
                    int n = 0;
                    for (int j = l.pc + 1; j < stop; j += 1 + cmds_[j].body_len) {
                        const int k = alloc_lane();
                        if (k < 0) {
                            abort_route(now_s);  // durch die Prüfung ausgeschlossen
                            return;
                        }
                        init_lane(k, j, j + 1 + cmds_[j].body_len, i);
                        ++n;
                    }
                    l.cur = l.pc;
                    l.pc = stop;
                    l.children_left = n;
                    l.st = LaneState::children;
                    return;
                }
                case RouteOp::at:
                    l.cur = l.pc;
                    if (std::isfinite(match_time_s) && match_time_s >= c.value) {
                        ++l.pc;  // der Befehl hinter dem at läuft als nächster
                    } else {
                        l.st = LaneState::at_wait;
                        return;
                    }
                    break;
                default:
                    l.cur = l.pc;
                    l.retried = false;
                    begin_attempt(i, now_s);
                    poll_leaf(i, now_s);
                    break;
            }
        }
    }

    const RouteCmd* cmds_;
    int len_;
    RouteExecutor& exec_;
    RoutePlayerConfig cfg_;
    RouteStatus status_ = RouteStatus::idle;
    Lane lanes_[kMaxLanes];
    int ats_[kMaxAts] = {};
    int n_ats_ = 0;
    int steps_ = 0;
    int failed_ = 0;
    int last_failed_line_ = 0;
};

}  // namespace frostbyte
