#pragma once

#include <array>
#include <cstdint>

#include "core/clock.hpp"

// Unveränderlicher Messwert-Satz eines Zyklus. Sensorwerte kommen mit den jeweiligen Tickets dazu.
namespace frostbyte {

enum class Button { a, b, x, y, up, down, left, right, l1, l2, r1, r2 };
inline constexpr int button_count = 12;

// Controller-Eingabe eines Zyklus: Sticks -1..1 (hoch/rechts positiv), Tasten gedrückt = true.
struct ControllerInput {
    double left_x = 0.0;
    double left_y = 0.0;
    double right_x = 0.0;
    double right_y = 0.0;
    std::array<bool, button_count> buttons{};

    bool down(Button b) const { return buttons[static_cast<int>(b)]; }
    void set(Button b, bool pressed = true) { buttons[static_cast<int>(b)] = pressed; }
};

struct Snapshot {
    std::uint32_t cycle = 0;
    double t_s = 0.0;
    double dt_s = 0.0;
    ControllerInput controller;
};

class SnapshotBuilder {
public:
    explicit SnapshotBuilder(const Clock& clock) : clock_(clock), last_t_s_(clock.now_s()) {}

    Snapshot next() {
        Snapshot s;
        s.t_s = clock_.now_s();
        s.dt_s = s.t_s - last_t_s_;
        s.cycle = ++cycle_;
        last_t_s_ = s.t_s;
        return s;
    }

private:
    const Clock& clock_;
    double last_t_s_;
    std::uint32_t cycle_ = 0;
};

}  // namespace frostbyte
