#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>

// GoalBook: Höhe über dem Boden (Zoll), auf der ein Objekt auf einem Goal in Ebene 0..5 abgelegt wird.
// Ebene 0 = Goal-Höhe (hoehe_in aus field.json), jede weitere Ebene addiert level_step_in. Ist die Stufe unbekannt (NaN),
// gibt es nur Ebene 0; höhere Ebenen liefern "unbekannt", nie einen geratenen Wert. Die Höhe gilt im Feldrahmen; den Versatz
// zwischen Greifer und gemessener Lifthöhe rechnet der Aufrufer (config/) dazu. Kein dynamischer Speicher, keine PROS-Header;
// die Namen müssen so lange leben wie das Buch.
namespace frostbyte {

struct GoalEntry {
    const char* id = "";
    double base_in = 0.0;
};

class GoalBook {
public:
    static constexpr int kGoals = 9;
    static constexpr int kLevels = 6;

    // false und ungültig, wenn eine Goal-Höhe nicht endlich und > 0 ist, ein Name leer oder doppelt ist oder die Stufe
    // weder NaN (unbekannt) noch endlich und > 0 ist.
    bool set(const std::array<GoalEntry, kGoals>& goals, double level_step_in) {
        valid_ = false;
        if (!std::isnan(level_step_in) && !(std::isfinite(level_step_in) && level_step_in > 0.0))
            return false;
        for (int i = 0; i < kGoals; ++i) {
            if (goals[i].id == nullptr || std::string_view(goals[i].id).empty())
                return false;
            if (!std::isfinite(goals[i].base_in) || goals[i].base_in <= 0.0)
                return false;
            for (int j = 0; j < i; ++j)
                if (std::string_view(goals[i].id) == goals[j].id)
                    return false;
        }
        goals_ = goals;
        step_in_ = level_step_in;
        valid_ = true;
        return true;
    }

    bool valid() const { return valid_; }
    bool level_step_known() const { return valid_ && !std::isnan(step_in_); }

    // Index des Goals mit diesem Namen, -1 wenn es ihn nicht gibt (oder das Buch ungültig ist).
    int index_of(std::string_view id) const {
        if (!valid_)
            return -1;
        for (int i = 0; i < kGoals; ++i)
            if (id == goals_[i].id)
                return i;
        return -1;
    }

    std::optional<double> place_height_in(int goal, int level) const {
        if (!valid_ || goal < 0 || goal >= kGoals || level < 0 || level >= kLevels)
            return std::nullopt;
        if (level > 0 && std::isnan(step_in_))
            return std::nullopt;
        return goals_[goal].base_in + (level > 0 ? step_in_ * level : 0.0);
    }

    std::optional<double> place_height_in(std::string_view id, int level) const {
        return place_height_in(index_of(id), level);
    }

private:
    std::array<GoalEntry, kGoals> goals_{};
    double step_in_ = std::numeric_limits<double>::quiet_NaN();
    bool valid_ = false;
};

}  // namespace frostbyte
