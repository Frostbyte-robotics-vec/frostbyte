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

void apply_demo(SimConfig& cfg) {
    cfg.build = "demo";
    // Platzhalter-Posen (nicht aus field.json): Stacker fährt nach rechts, biegt nach oben ab und trifft den Runner,
    // der von oben kommt.
    cfg.stacker.start = Pose{-50.0, -30.0, 0.0};
    cfg.runner.start = Pose{-8.0, 55.0, -90.0};
    cfg.stacker.open_loop = play({{2500, 4.0, 4.0},       // geradeaus
                                  {3600, -3.0, 3.0},      // nach links drehen (rund 90 Grad)
                                  {9000, 4.0, 4.0},       // auf den Runner zu, schieben
                                  {12000, -4.0, -4.0},    // zurück
                                  {13100, -3.0, 3.0},     // nach links drehen
                                  {25000, 5.0, 5.0}});    // zum Feldrand
    cfg.runner.open_loop = play({{9000, 4.0, 4.0},        // nach unten, dem Stacker entgegen
                                 {12000, -4.0, -4.0},     // zurück
                                 {13100, -3.0, 3.0},      // nach links drehen
                                 {25000, 5.0, 5.0}});     // zum Feldrand
}

}  // namespace frostbyte::sim
