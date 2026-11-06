#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "doctest/doctest.h"
#include "sim/drive.hpp"
#include "sim/json.hpp"

using namespace frostbyte::sim;

namespace {

const std::string fixtures = FB_TEST_FIXTURES;

// Testwerte, bewusst verschieden von den Schätzwerten in sim/sim_params.json.
DriveParams test_params() {
    DriveParams p;
    p.kS_V = 1.0;
    p.kV_V_per_in_s = 0.2;
    p.kA_V_per_in_s2 = 0.04;
    p.spurbreite_in = 12.0;
    p.mu = 1.0;
    p.motor_max_V = 12.0;
    p.akku_V = 12.6;
    p.g_in_s2 = 386.1;
    return p;
}

constexpr double dt_s = 0.001;

void run(DriveModel& m, double seconds, double links_V, double rechts_V) {
    const int steps = static_cast<int>(std::lround(seconds / dt_s));
    for (int i = 0; i < steps; ++i)
        m.step(dt_s, links_V, rechts_V);
}

double wrap_deg(double d) {
    d = std::fmod(d, 360.0);
    if (d > 180.0)
        d -= 360.0;
    if (d <= -180.0)
        d += 360.0;
    return d;
}

constexpr double pi = 3.14159265358979323846;

// Spannung fuer ein Seitentempo: Vorsteuerung aus der Kennlinie plus P-Anteil auf den Tempofehler.
double side_voltage(const DriveParams& p, double v_set, double v_ist) {
    const double sign = v_set > 0.0 ? 1.0 : (v_set < 0.0 ? -1.0 : 0.0);
    return p.kS_V * sign + p.kV_V_per_in_s * v_set + 1.0 * (v_set - v_ist);
}

// Testregler mit relativen Zielen aus den Seitenwegen (wie Odometrie aus Encodern), nicht aus der wahren Pose:
// so schliesst das Quadrat nur, wenn Seitendynamik und Kinematik zusammenpassen.
// fwd_in / arc_in: Soll-Weg der Mitte bzw. Soll-Bogen jeder Seite (rechts vor, links zurueck).
void move(DriveModel& m, const DriveParams& p, double fwd_in, double arc_in) {
    const double l0 = m.links_in(), r0 = m.rechts_in();
    for (int i = 0; i < 3000; ++i) {  // Steuertakt 10 ms, hoechstens 30 s
        const double dl = m.links_in() - l0, dr = m.rechts_in() - r0;
        const double err_fwd = fwd_in - (dl + dr) / 2.0;
        const double err_arc = arc_in - (dr - dl) / 2.0;
        if (std::abs(err_fwd) < 0.01 && std::abs(err_arc) < 0.005 && std::abs(m.links_in_s()) < 0.05 &&
            std::abs(m.rechts_in_s()) < 0.05)
            return;
        const double v_fwd = std::clamp(4.0 * err_fwd, -40.0, 40.0);  // sanft: kein Schlupf
        const double v_arc = std::clamp(4.0 * err_arc, -20.0, 20.0);
        for (int k = 0; k < 10; ++k)
            m.step(dt_s, side_voltage(p, v_fwd - v_arc, m.links_in_s()),
                   side_voltage(p, v_fwd + v_arc, m.rechts_in_s()));
    }
    FAIL("Testregler erreicht das Ziel nicht");
}

}  // namespace

TEST_CASE("Fahrmodell: 12 V geradeaus erreicht das Endtempo (12 - kS) / kV") {
    const DriveParams p = test_params();
    DriveModel m(p, Pose{0.0, 0.0, 0.0});
    run(m, 10.0 * p.kA_V_per_in_s2 / p.kV_V_per_in_s + 1.0, 12.0, 12.0);
    const double v_formel = (12.0 - p.kS_V) / p.kV_V_per_in_s;
    CHECK(m.links_in_s() == doctest::Approx(v_formel).epsilon(0.03));
    CHECK(m.rechts_in_s() == doctest::Approx(v_formel).epsilon(0.03));
    CHECK(m.pose().h_deg == doctest::Approx(0.0));
    CHECK(m.pose().y_in == doctest::Approx(0.0));
    CHECK(m.pose().x_in > 0.0);
}

TEST_CASE("Fahrmodell: Anfahren folgt der Kennlinie (Zeitkonstante kA / kV)") {
    const DriveParams p = test_params();
    DriveModel m(p, Pose{});
    const double tau_s = p.kA_V_per_in_s2 / p.kV_V_per_in_s;
    run(m, tau_s, 12.0, 12.0);
    const double v_formel = (12.0 - p.kS_V) / p.kV_V_per_in_s;
    CHECK(m.links_in_s() == doctest::Approx(v_formel * (1.0 - std::exp(-1.0))).epsilon(0.01));
}

TEST_CASE("Fahrmodell: Haftreibung haelt bei |V| <= kS, Reibung bremst ohne Spannung bis zum Stillstand") {
    const DriveParams p = test_params();
    DriveModel m(p, Pose{5.0, -3.0, 30.0});
    run(m, 1.0, p.kS_V, -p.kS_V);
    run(m, 1.0, 0.5 * p.kS_V, -0.5 * p.kS_V);  // unter kS: auch kein Kriechen rueckwaerts
    CHECK(m.pose().x_in == 5.0);
    CHECK(m.pose().y_in == -3.0);
    CHECK(m.pose().h_deg == doctest::Approx(30.0));
    run(m, 1.0, 12.0, 12.0);
    run(m, 2.0, 0.0, 0.0);
    CHECK(m.links_in_s() == 0.0);
    CHECK(m.rechts_in_s() == 0.0);
}

TEST_CASE("Fahrmodell: Fahrt entlang des Headings, 0 Grad = +x, gegen den Uhrzeigersinn positiv") {
    const DriveParams p = test_params();
    DriveModel m(p, Pose{0.0, 0.0, 90.0});
    run(m, 1.0, 6.0, 6.0);
    CHECK(m.pose().x_in == doctest::Approx(0.0).epsilon(1e-9));
    CHECK(m.pose().y_in > 10.0);
    DriveModel d(p, Pose{});
    run(d, 0.5, -6.0, 6.0);  // rechts vorwaerts, links rueckwaerts: Linksdrehung
    CHECK(d.pose().h_deg > 10.0);
}

TEST_CASE("Fahrmodell: Heading immer in (-180, 180], schon vor dem ersten Schritt") {
    const DriveParams p = test_params();
    CHECK(DriveModel(p, Pose{0.0, 0.0, 270.0}).pose().h_deg == doctest::Approx(-90.0));
    CHECK(DriveModel(p, Pose{0.0, 0.0, -180.0}).pose().h_deg == doctest::Approx(180.0));
    CHECK(DriveModel(p, Pose{0.0, 0.0, 720.0 + 45.0}).pose().h_deg == doctest::Approx(45.0));
    DriveModel m(p, Pose{0.0, 0.0, 170.0});
    run(m, 1.0, -8.0, 8.0);  // dreht ueber 180 hinaus
    CHECK(m.pose().h_deg > -180.0);
    CHECK(m.pose().h_deg <= 180.0);
}

TEST_CASE("Fahrmodell: Punktdrehung aendert nur das Heading, Drehrate = (vR - vL) / Spurbreite") {
    const DriveParams p = test_params();
    DriveModel m(p, Pose{10.0, 20.0, 0.0});
    run(m, 1.0, -8.0, 8.0);
    CHECK(m.pose().x_in == doctest::Approx(10.0));
    CHECK(m.pose().y_in == doctest::Approx(20.0));
    // Nach 1 s (= 5 tau) im Endtempo: gemessene Drehung der Pose gegen die Formel, nicht nur die Rate-Funktion.
    const double v = (8.0 - p.kS_V) / p.kV_V_per_in_s;
    const double rate_formel_deg_s = 2.0 * v / p.spurbreite_in * 180.0 / pi;
    const double h0 = m.pose().h_deg;
    run(m, 0.1, -8.0, 8.0);
    CHECK(wrap_deg(m.pose().h_deg - h0) == doctest::Approx(0.1 * rate_formel_deg_s).epsilon(0.01));
    CHECK(m.heading_rate_deg_s() == doctest::Approx(rate_formel_deg_s).epsilon(0.01));
}

TEST_CASE("Fahrmodell: Kreisbogen bleibt auf dem Kreis (keine Drift durch die Integration)") {
    const DriveParams p = test_params();
    DriveModel m(p, Pose{});
    run(m, 0.5, 4.0, 8.0);  // erst einschwingen, dann Radius aus den Seitentempi
    const double vl = m.links_in_s(), vr = m.rechts_in_s();
    const double radius_in = p.spurbreite_in / 2.0 * (vr + vl) / (vr - vl);
    const double h0 = m.pose().h_deg * pi / 180.0;
    const double cx = m.pose().x_in - radius_in * std::sin(h0), cy = m.pose().y_in + radius_in * std::cos(h0);
    run(m, 20.0, 4.0, 8.0);  // viele Runden
    CHECK(std::hypot(m.pose().x_in - cx, m.pose().y_in - cy) == doctest::Approx(radius_in).epsilon(1e-4));
}

TEST_CASE("Fahrmodell: Quadrat 2 m (Seite 78,74 Zoll) schliesst auf <= 0,5 Zoll ohne Rauschen") {
    const DriveParams p = test_params();
    const Pose start{-24.0, 12.0, 90.0};
    DriveModel m(p, start);
    for (int seite = 0; seite < 4; ++seite) {
        move(m, p, 2000.0 / 25.4, 0.0);
        move(m, p, 0.0, pi / 2.0 * p.spurbreite_in / 2.0);  // 90 Grad links
        // Jede Ecke einzeln: zwei 180-Grad-Fehler wuerden das Quadrat sonst auch „schliessen“.
        CHECK(std::abs(wrap_deg(m.pose().h_deg - start.h_deg - 90.0 * (seite + 1))) <= 0.5);
    }
    const double seite_in = 2000.0 / 25.4;
    CHECK(seite_in == doctest::Approx(78.74).epsilon(1e-4));
    CHECK(std::hypot(m.pose().x_in - start.x_in, m.pose().y_in - start.y_in) <= 0.5);
    CHECK(std::abs(wrap_deg(m.pose().h_deg - start.h_deg)) <= 0.5);
}

TEST_CASE("Fahrmodell: Schlupf begrenzt die Beschleunigung auf mu * g, das Endtempo bleibt") {
    DriveParams p = test_params();
    p.mu = 0.1;  // mu * g = 38,6 in/s^2, die Kennlinie allein gaebe (12 - 1) / 0,04 = 275 in/s^2
    DriveModel m(p, Pose{});
    double v_prev = 0.0, a_max = 0.0;
    for (int i = 0; i < 6000; ++i) {
        m.step(dt_s, 12.0, 12.0);
        a_max = std::max(a_max, (m.links_in_s() - v_prev) / dt_s);
        v_prev = m.links_in_s();
    }
    CHECK(a_max <= p.mu * p.g_in_s2 * (1.0 + 1e-9));
    CHECK(a_max == doctest::Approx(p.mu * p.g_in_s2));
    CHECK(m.links_in_s() == doctest::Approx((12.0 - p.kS_V) / p.kV_V_per_in_s).epsilon(0.03));
}

TEST_CASE("Fahrmodell: Akkuspannung unter 12 V begrenzt Befehl und Endtempo") {
    DriveParams p = test_params();
    p.akku_V = 10.0;
    DriveModel m(p, Pose{});
    run(m, 2.0, 12.0, 12.0);
    CHECK(m.links_in_s() == doctest::Approx((10.0 - p.kS_V) / p.kV_V_per_in_s).epsilon(0.01));
    DriveParams voll = test_params();
    voll.motor_max_V = 11.0;  // Motor begrenzt, auch wenn der Akku mehr hergibt
    DriveModel mv(voll, Pose{});
    run(mv, 2.0, 15.0, 15.0);
    CHECK(mv.links_in_s() == doctest::Approx((voll.motor_max_V - voll.kS_V) / voll.kV_V_per_in_s).epsilon(0.01));
}

TEST_CASE("Fahrmodell: Parameter aus sim_params, fehlende Werte werden gemeldet") {
    const Json params = load_json_file(fixtures + "/sim_params_min.json");
    const DriveParams s = load_drive_params(params, "stacker");
    CHECK(s.kS_V == 1.0);
    CHECK(s.kV_V_per_in_s == 0.2);
    CHECK(s.kA_V_per_in_s2 == 0.04);
    CHECK(s.spurbreite_in == 12.0);
    CHECK(s.mu == 1.0);
    CHECK(s.motor_max_V == 11.5);
    CHECK(s.akku_V == 12.6);
    CHECK(s.g_in_s2 == 386.1);
    CHECK(load_drive_params(params, "runner").spurbreite_in == 10.0);
    CHECK_THROWS_AS(load_drive_params(params, "gibtsnicht"), std::runtime_error);
    CHECK_THROWS_AS(load_drive_params(load_json_file(fixtures + "/sim_params_drive_kv0.json"), "stacker"),
                    std::runtime_error);
}

// SIM-26: Wird der Roboter von einem Hindernis festgehalten (Kollision schiebt die Pose zurück), stehen die Räder
// still. Der Encoder darf den weggeschobenen Weg nicht als gefahren zählen.
TEST_CASE("Kollision schiebt zurück: Radweg folgt der wahren Strecke, Tempo gegen das Hindernis ist null") {
    DriveModel m(test_params(), Pose{0.0, 0.0, 0.0});
    run(m, 0.5, 8.0, 8.0);
    const double v_before = m.links_in_s();
    REQUIRE(v_before > 10.0);
    // Wand voraus: jeder Schritt wird auf x = 20 zurückgesetzt.
    for (int i = 0; i < 2000; ++i) {
        m.step(dt_s, 8.0, 8.0);
        if (m.pose().x_in > 20.0)
            m.set_position(20.0, m.pose().y_in);
    }
    CHECK(m.pose().x_in == doctest::Approx(20.0));
    CHECK(m.links_in() == doctest::Approx(20.0).epsilon(0.01));
    CHECK(m.rechts_in() == doctest::Approx(20.0).epsilon(0.01));
    CHECK(m.links_in_s() == doctest::Approx(0.0).epsilon(0.01));
}

TEST_CASE("Seitliches Verschieben ändert Radweg und Tempo nicht") {
    DriveModel m(test_params(), Pose{0.0, 0.0, 0.0});
    run(m, 0.5, 8.0, 8.0);
    const double d = m.links_in();
    const double v = m.links_in_s();
    m.set_position(m.pose().x_in, m.pose().y_in + 3.0);
    CHECK(m.links_in() == doctest::Approx(d));
    CHECK(m.links_in_s() == doctest::Approx(v));
}

// SIM-30: Schiebt der Roboter ein Feldobjekt, weicht er zurück, bremst aber nicht auf null: Radweg folgt der wahren
// Strecke, das Tempo bleibt.
TEST_CASE("displace: Pose verschoben, Radweg folgt der Strecke in Fahrtrichtung, Tempo bleibt") {
    DriveModel m(test_params(), Pose{0.0, 0.0, 0.0});
    run(m, 0.5, 8.0, 8.0);
    const double d = m.links_in();
    const double v = m.links_in_s();
    const double x = m.pose().x_in;
    m.displace(-0.5, 0.0);
    CHECK(m.pose().x_in == doctest::Approx(x - 0.5));
    CHECK(m.links_in() == doctest::Approx(d - 0.5));
    CHECK(m.rechts_in() == doctest::Approx(d - 0.5));
    CHECK(m.links_in_s() == doctest::Approx(v));  // set_position würde das Tempo auf null setzen
    m.displace(0.0, 2.0);                         // quer zur Fahrt: Radweg unverändert
    CHECK(m.links_in() == doctest::Approx(d - 0.5));
    CHECK(m.pose().y_in == doctest::Approx(2.0));
}
