#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "hal/interface/controller_output.hpp"
#include "match/driver_map.hpp"

namespace frostbyte {

// Rückmeldung an den Fahrer (C-04): Vibration bei 15 s (kurz) und 10 s (lang) Restzeit, Zeile 0 zeigt "<Restzeit>s <Status>".
// update() im niedrig priorisierten Task aufrufen (nicht im Steuer-Task), mit der Zeit seit Beginn des Fahrerlaufs.
// TODO(HW): Der Lauf beginnt mit opcontrol(); ob das mit dem Spielstart zusammenfällt, am Feld prüfen.
class ControllerFeedback {
public:
    static constexpr double kMinPrintGapS = 0.1;  // PROS: mindestens 50 ms zwischen Controllerbefehlen, Reserve
    static constexpr std::size_t kLineChars = 15;

    ControllerFeedback(hal::ControllerOutput& out, double duration_s) : out_(out), duration_s_(duration_s) {}

    // Neuer Lauf: Vorgängerzeit und Anzeige vergessen.
    void reset() {
        have_prev_ = false;
        last_text_.clear();
        last_print_s_ = -1e9;
    }

    void update(double elapsed_s, const char* status) {
        const double remaining_s = std::max(0.0, duration_s_ - elapsed_s);
        if (have_prev_ && endgame_rumble(prev_remaining_s_, remaining_s))
            out_.rumble(remaining_s <= 10.0 ? "-" : ".");  // bei einem Sprung über beide gilt die niedrigere Schwelle
        prev_remaining_s_ = remaining_s;
        have_prev_ = true;

        char buf[64];
        std::snprintf(buf, sizeof buf, "%ds %s", static_cast<int>(std::ceil(remaining_s)), status);
        std::string text(buf);
        if (text.size() > kLineChars)
            text.resize(kLineChars);
        if (text == last_text_ || elapsed_s - last_print_s_ < kMinPrintGapS)
            return;
        if (out_.print(0, text.c_str())) {  // abgelehnt: last_text_ bleibt alt, nächster Zyklus versucht es erneut
            last_text_ = text;
            last_print_s_ = elapsed_s;
        }
    }

private:
    hal::ControllerOutput& out_;
    double duration_s_;
    bool have_prev_ = false;
    double prev_remaining_s_ = 0.0;
    std::string last_text_;
    double last_print_s_ = -1e9;
};

}  // namespace frostbyte
