#include "sim/demo.hpp"

#include <utility>
#include <vector>

namespace frostbyte::sim {

namespace {

// Bis until_ms gilt die Spannung links/rechts; danach die nächste Zeile, nach der letzten 0 V.
struct Segment {
    int until_ms;
    double left_v;
    double right_v;
};

std::function<std::pair<double, double>(int)> play(std::vector<Segment> segments) {
    return [segments](int t_ms) {
        for (const Segment& s : segments)
            if (t_ms < s.until_ms)
                return std::make_pair(s.left_v, s.right_v);
        return std::make_pair(0.0, 0.0);
    };
}

}  // namespace

void apply_demo(SimConfig& cfg, const std::string& params_path) {
    cfg.build = "demo";
    // Platzhalter-Posen (nicht aus field.json): Stacker fährt nach rechts, biegt nach oben ab und trifft den Runner,
    // der von oben kommt.
    cfg.stacker.start = Pose{-50.0, -30.0, 0.0};
    cfg.runner.start = Pose{-8.0, 55.0, -90.0};
    std::vector<Segment> stacker = {{2500, 4.0, 4.0},     // geradeaus
                                    {3600, -3.0, 3.0},    // nach links drehen (rund 90 Grad)
                                    {9000, 4.0, 4.0},     // auf den Runner zu, schieben
                                    {12000, -4.0, -4.0},  // zurück
                                    {13100, -3.0, 3.0},   // nach links drehen
                                    {20000, 5.0, 5.0}};   // zum Feldrand
    std::vector<Segment> runner = {{9000, 4.0, 4.0},      // nach unten, dem Stacker entgegen
                                   {12000, -4.0, -4.0},   // zurück
                                   {13100, -3.0, 3.0},    // nach links drehen
                                   {20000, 5.0, 5.0}};    // zum Feldrand
    // Danach bis kurz vor Matchende abwechselnd 3 s geradeaus und 1 s drehen, der Feldrand hält sie im Feld.
    for (int t = 20000, k = 0; t + 4000 <= 56000; t += 4000, ++k) {
        stacker.push_back({t + 3000, 4.0, 4.0});
        stacker.push_back({t + 4000, k % 2 ? 3.0 : -3.0, k % 2 ? -3.0 : 3.0});
        runner.push_back({t + 3000, 4.0, 4.0});
        runner.push_back({t + 4000, k % 2 ? -3.0 : 3.0, k % 2 ? 3.0 : -3.0});
    }
    cfg.stacker.open_loop = play(stacker);
    cfg.runner.open_loop = play(runner);

    if (params_path.empty())
        return;
    // Platzhalter-Spielzustand: Stacker baut im roten, Runner im blauen Quadranten, alle 10 s ein Zyklus (die Dauer eines
    // Zyklus liegt weit darunter). Wo die Roboter dabei stehen, spielt keine Rolle: das Szenario gibt Zeit und Goal vor.
    load_game_params(load_json_file(params_path), cfg.game);
    cfg.game.goals.push_back(GoalState{Zone::red_quadrant, ToggleColor::red, {}});
    cfg.game.goals.push_back(GoalState{Zone::blue_quadrant, ToggleColor::blue, {}});
    for (int t = 5000; t + 10000 <= 55000; t += 10000) {
        cfg.game.cycles.push_back({t, 0, 0, 0, Source::loader});
        cfg.game.cycles.push_back({t + 2000, 1, 1, 0, Source::loader});
    }
}

}  // namespace frostbyte::sim
