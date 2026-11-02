#pragma once

#include <limits>

// Zustandsverwaltung für Subsystem-Automaten. Jeder Zustand kann ein Timeout mit Ausweichzustand haben
// (CLAUDE.md: nichts wartet unbegrenzt). Die Übergangslogik selbst steht im jeweiligen Subsystem.
namespace frostbyte {

template <typename State>
class StateMachine {
public:
    StateMachine(State initial, double now_s) : state_(initial), entered_s_(now_s) {}

    State state() const { return state_; }
    bool timed_out() const { return timed_out_; }
    double elapsed_s(double now_s) const { return now_s - entered_s_; }

    void enter(State next, double now_s) { enter(next, now_s, no_timeout, next); }

    void enter(State next, double now_s, double timeout_s, State on_timeout) {
        state_ = next;
        entered_s_ = now_s;
        timeout_s_ = timeout_s;
        on_timeout_ = on_timeout;
        timed_out_ = false;
    }

    // Gibt true zurück, wenn in diesem Aufruf das Timeout ausgelöst und in den Ausweichzustand gewechselt wurde.
    bool update(double now_s) {
        if (elapsed_s(now_s) < timeout_s_)
            return false;
        enter(on_timeout_, now_s);
        timed_out_ = true;
        return true;
    }

private:
    static constexpr double no_timeout = std::numeric_limits<double>::infinity();

    State state_;
    State on_timeout_ = state_;
    double entered_s_;
    double timeout_s_ = no_timeout;
    bool timed_out_ = false;
};

}  // namespace frostbyte
