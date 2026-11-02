#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "control/pid.hpp"
#include "core/snapshot.hpp"

// Schicht 4: Tuning-Modus (D-06, ADR-020). Reglerwerte (PID für Fahren und Drehen) per Controller ändern, als Text
// serialisieren und wieder einlesen. Reine Logik: kennt weder Speicher noch PROS (Laden/Speichern: tuning_store.hpp),
// kein dynamischer Speicher, keine Ausgabe. Einheiten wie in pid.hpp (lateral: V je Zoll, angular: V je Grad).
namespace frostbyte {

struct TuningSet {
    PidGains lateral;
    PidGains angular;
};

// Was getunt werden darf. i_max_v und out_max_v bleiben in config/ (Sicherheitsgrenzen, nicht per Controller änderbar).
struct TuningParam {
    const char* key;   // Name in Datei und Log, ohne Komma und Semikolon
    double min_value;  // Grenzen gelten beim Ändern per Controller und beim Einlesen der Datei
    double max_value;
    double step_fine;
    double step_coarse;
};

inline constexpr int tuning_param_count = 8;

inline const TuningParam& tuning_param(int i) {
    static constexpr TuningParam table[tuning_param_count] = {
        {"lat.kp", 0.0, 50.0, 0.05, 0.5},  {"lat.ki", 0.0, 20.0, 0.01, 0.1},  {"lat.kd", 0.0, 20.0, 0.005, 0.05},
        {"lat.iz", 0.0, 100.0, 0.5, 2.0},  {"ang.kp", 0.0, 5.0, 0.005, 0.05}, {"ang.ki", 0.0, 5.0, 0.01, 0.1},
        {"ang.kd", 0.0, 5.0, 0.001, 0.01}, {"ang.iz", 0.0, 90.0, 0.5, 2.0},
    };
    return table[std::clamp(i, 0, tuning_param_count - 1)];
}

inline double tuning_get(const TuningSet& s, int i) {
    switch (i) {
        case 0: return s.lateral.kp;
        case 1: return s.lateral.ki;
        case 2: return s.lateral.kd;
        case 3: return s.lateral.i_zone;
        case 4: return s.angular.kp;
        case 5: return s.angular.ki;
        case 6: return s.angular.kd;
        default: return s.angular.i_zone;
    }
}

inline void tuning_set(TuningSet& s, int i, double v) {
    switch (i) {
        case 0: s.lateral.kp = v; break;
        case 1: s.lateral.ki = v; break;
        case 2: s.lateral.kd = v; break;
        case 3: s.lateral.i_zone = v; break;
        case 4: s.angular.kp = v; break;
        case 5: s.angular.ki = v; break;
        case 6: s.angular.kd = v; break;
        default: s.angular.i_zone = v; break;
    }
}

inline void apply_tuning(const TuningSet& s, Pid& lateral, Pid& angular) {
    lateral.set_gains(s.lateral);
    angular.set_gains(s.angular);
}

inline constexpr const char* tuning_header = "# frostbyte-tuning v1";

// Schreibt Kopfzeile und je Parameter "schluessel=wert", jede Zeile mit \n. Rückgabe: Länge ohne Endnull, -1 wenn der
// Puffer nicht reicht (nie abgeschnitten).
inline int serialize_tuning(const TuningSet& s, char* buf, std::size_t n) {
    std::size_t len = 0;
    auto put = [&](int w) {
        if (w < 0 || len + static_cast<std::size_t>(w) >= n)
            return false;
        len += static_cast<std::size_t>(w);
        return true;
    };
    if (n == 0 || !put(std::snprintf(buf, n, "%s\n", tuning_header)))
        return -1;
    for (int i = 0; i < tuning_param_count; ++i)
        if (!put(std::snprintf(buf + len, n - len, "%s=%.9g\n", tuning_param(i).key, tuning_get(s, i))))
            return -1;
    return static_cast<int>(len);
}

// Liest eine Datei von serialize_tuning. Ganz oder gar nicht: bei falscher Version, fehlendem, doppeltem oder
// unbekanntem Schlüssel, nicht endlichem Wert, Wert außerhalb der Grenzen oder fehlendem Zeilenende bleibt out
// unverändert und die Rückgabe ist false. Nicht getunte Felder von out (Grenzen) bleiben erhalten.
inline bool parse_tuning(const char* text, std::size_t n, TuningSet& out) {
    TuningSet result = out;
    bool seen[tuning_param_count] = {};
    std::size_t pos = 0;
    bool header_ok = false;
    while (pos < n) {
        const char* nl = static_cast<const char*>(std::memchr(text + pos, '\n', n - pos));
        if (!nl)
            return false;  // letzte Zeile ohne Zeilenende: abgeschnitten
        char line[64];
        const std::size_t len = static_cast<std::size_t>(nl - (text + pos));
        if (len >= sizeof(line))
            return false;
        std::memcpy(line, text + pos, len);
        line[len] = '\0';
        pos += len + 1;

        if (!header_ok) {
            if (std::strcmp(line, tuning_header) != 0)
                return false;
            header_ok = true;
            continue;
        }
        char* eq = std::strchr(line, '=');
        if (!eq)
            return false;
        *eq = '\0';
        int idx = -1;
        for (int i = 0; i < tuning_param_count; ++i)
            if (std::strcmp(line, tuning_param(i).key) == 0)
                idx = i;
        if (idx < 0 || seen[idx])
            return false;
        char* end = nullptr;
        const double v = std::strtod(eq + 1, &end);
        if (end == eq + 1 || *end != '\0' || !std::isfinite(v) || v < tuning_param(idx).min_value ||
            v > tuning_param(idx).max_value)
            return false;
        seen[idx] = true;
        tuning_set(result, idx, v);
    }
    if (!header_ok)
        return false;
    for (bool b : seen)
        if (!b)
            return false;
    out = result;
    return true;
}

struct TuningState {
    int index = 0;        // gewählter Parameter, tuning_param(index).key
    double value = 0.0;   // sein aktueller Wert
    bool coarse = false;  // grobe Schritte
    bool dirty = false;   // Werte weichen vom Stand beim Start bzw. letzten Speichern ab
};

// Tasten (nur aktiv, wenn der Modus eingeschaltet ist; dann sind A/B/X in diesem Modus keine Fahrer-Aktionen):
// Hoch/Runter wählt den Parameter, Links/Rechts ändert ihn, X wechselt fein/grob, A fordert Speichern an,
// B verwirft die Änderungen seit Start bzw. letztem Speichern. Jede Taste wirkt nur auf die Flanke.
class TuningMode {
public:
    TuningMode(const TuningSet& start, bool enabled) : values_(start), saved_(start), enabled_(enabled) {}

    // Einmal pro Zyklus mit der Controller-Eingabe. Ist der Modus aus, tut es nichts.
    void update(const ControllerInput& in, double /*now_s*/) {
        if (!enabled_) {
            return;
        }
        auto edge = [&](Button b) { return in.down(b) && !prev_.down(b); };
        if (edge(Button::up))
            index_ = (index_ + tuning_param_count - 1) % tuning_param_count;
        if (edge(Button::down))
            index_ = (index_ + 1) % tuning_param_count;
        if (edge(Button::x))
            coarse_ = !coarse_;
        const int dir = (edge(Button::right) ? 1 : 0) - (edge(Button::left) ? 1 : 0);
        if (dir != 0) {
            const TuningParam& p = tuning_param(index_);
            const double step = coarse_ ? p.step_coarse : p.step_fine;
            tuning_set(values_, index_, std::clamp(tuning_get(values_, index_) + dir * step, p.min_value, p.max_value));
            set_event_value();
        }
        if (edge(Button::a)) {
            saved_ = values_;
            save_pending_ = true;
            set_event("TUNE:save");
        }
        if (edge(Button::b)) {
            values_ = saved_;
            set_event("TUNE:revert");
        }
        prev_ = in;
    }

    const TuningSet& values() const { return values_; }

    TuningState state() const {
        TuningState s;
        s.index = index_;
        s.value = tuning_get(values_, index_);
        s.coarse = coarse_;
        s.dirty = differs(values_, saved_);
        return s;
    }

    // true genau einmal pro Anforderung. Das Schreiben macht der Aufrufer außerhalb des Steuer-Tasks.
    bool take_save_request() {
        const bool r = save_pending_;
        save_pending_ = false;
        return r;
    }

    // Schreibt das letzte Ereignis (z. B. "TUNE:lat.kp=1.05") für die Spalte event des Logs in buf, einmal je Ereignis.
    // Kein Komma, kein Semikolon. Rückgabe false, wenn nichts ansteht oder buf zu klein ist (dann bleibt es anstehen).
    bool take_event(char* buf, std::size_t n) {
        if (!event_pending_ || std::strlen(event_) >= n)
            return false;
        std::strcpy(buf, event_);
        event_pending_ = false;
        return true;
    }

private:
    static bool differs(const TuningSet& a, const TuningSet& b) {
        for (int i = 0; i < tuning_param_count; ++i)
            if (tuning_get(a, i) != tuning_get(b, i))
                return true;
        return false;
    }
    void set_event(const char* text) {
        std::snprintf(event_, sizeof(event_), "%s", text);
        event_pending_ = true;
    }
    void set_event_value() {
        std::snprintf(event_, sizeof(event_), "TUNE:%s=%.5g", tuning_param(index_).key, tuning_get(values_, index_));
        event_pending_ = true;
    }

    TuningSet values_;
    TuningSet saved_;
    ControllerInput prev_;
    bool enabled_;
    int index_ = 0;
    bool coarse_ = false;
    bool save_pending_ = false;
    bool event_pending_ = false;
    char event_[48] = {};
};

}  // namespace frostbyte
