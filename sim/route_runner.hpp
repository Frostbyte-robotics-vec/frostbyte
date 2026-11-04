#pragma once

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "control/motion_controller.hpp"
#include "control/pid.hpp"
#include "match/route.hpp"
#include "match/route_player.hpp"
#include "sim/drive.hpp"
#include "sim/game.hpp"
#include "sim/json.hpp"

// Routen in der Simulation (A-04, ADR-031): der RoutePlayer (A-03) spielt eine geparste Route ab, ein Sim-Executor setzt goto
// und turn in MotionController-Befehle um. Das ist kein Vorgriff auf A-10 (Roboter-Adapter): intake, cycle und toggle sind
// hier Platzhalter ohne Modell und enden sofort mit done, solange kein Spielzustand läuft. Mit Spielzustand (SIM-22,
// ADR-032) lösen intake und cycle Aufnahme und Zyklus im Game aus. Zoll, Grad, Sekunden.
namespace frostbyte::sim {

// Koordinate eines Feldpunkts aus field.json; leer = null (noch nicht vermessen: nie nach (0, 0) fahren).
struct FieldPoint {
    std::string id;
    std::optional<std::pair<double, double>> xy_in;
    bool loader = false;  // steht in field.json unter loader (Quelle der Aufnahme: loader, sonst Boden)
};

// goals, loader, toggles und posen aus field.json mit ihren Koordinaten (fehlende Blöcke zählen als leer).
std::vector<FieldPoint> load_field_points(const Json& field);

// Reglerwerte und Standard-Timeouts je Roboter. Die Vorgaben sind neutral; frostbyte_sim setzt sie aus config/stacker.hpp
// und config/runner.hpp, Tests setzen eigene (unabhängig von config/).
struct RouteControl {
    MotionConfig motion;
    PidGains lateral;
    PidGains angular;
    RoutePlayerConfig player;
};

// Ereignisse, die ein Roboter sendet und der andere abwartet ("stacker.<name>").
using SignalBus = std::set<std::string>;

// Routenbefehle für die Simulation. Eine Bewegung zur Zeit: startet eine zweite Lane goto/turn, scheitert die erste.
class SimRouteExecutor : public RouteExecutor {
public:
    // game: Spielzustand der Simulation oder nullptr (dann sind intake und cycle Platzhalter); self wählt den Roboter darin.
    SimRouteExecutor(MotionController& motion, const std::vector<FieldPoint>& points, SignalBus& bus, RouteRobot self,
                     const RoutePlayerConfig& cfg, Game* game = nullptr)
        : motion_(motion), points_(points), bus_(bus), self_(self), cfg_(cfg), game_(game) {}

    void start(int lane, const RouteCmd& cmd, double now_s) override;
    StepStatus poll(int lane, double now_s) override;
    StepStatus cancel(int lane, double now_s) override;

    // Pose des Roboters (Schätzung), vor jedem Routenschritt gesetzt; toggle prüft damit die Reichweite (SIM-25).
    void set_pose(const Pose& p) { pose_ = p; }

    // true, solange eine Lane auf den Befehl des MotionController wartet.
    bool motion_active() const { return motion_lane_ >= 0; }

private:
    struct LaneRec {
        StepStatus status = StepStatus::done;
        RouteRobot wait_robot = RouteRobot::none;
        std::string wait_name;
        bool waiting = false;
        bool game_op = false;  // intake/cycle im Game läuft bis done_ms
        bool game_ok = false;
        int done_ms = 0;
    };

    void begin_motion(int lane, const MotionCommand& cmd, double now_s);
    void begin_game_op(int lane, const CycleTicket& t);
    int robot_index() const { return self_ == RouteRobot::runner ? 1 : 0; }

    MotionController& motion_;
    const std::vector<FieldPoint>& points_;
    SignalBus& bus_;
    RouteRobot self_;
    RoutePlayerConfig cfg_;
    Game* game_;
    Pose pose_;
    bool last_goto_loader_ =
        false;  // letztes goto-Ziel war ein Ladepunkt: intake nimmt dann vom Loader, sonst vom Boden
    LaneRec lanes_[RoutePlayer::kMaxLanes];
    int motion_lane_ = -1;
};

// Alles, was ein Roboter für seine Route braucht, mit stabilen Adressen (Player und Executor halten Referenzen).
class RouteRunner {
public:
    RouteRunner(std::vector<RouteCmd> cmds, const std::vector<FieldPoint>& points, SignalBus& bus, RouteRobot self,
                const RouteControl& ctl, Game* game = nullptr);
    RouteRunner(const RouteRunner&) = delete;
    RouteRunner& operator=(const RouteRunner&) = delete;

    // Einmal je Steuertakt: Player, dann Bewegung mit der geschätzten Pose. Rückgabe: Spannung links/rechts.
    std::pair<double, double> step(int t_ms, const Pose& est);

    RouteStatus status() const { return player_.status(); }
    int failed_steps() const { return player_.failed_steps(); }
    // "Zeile:Befehl Name" für die Log-Spalte route_step; leer, wenn keine Route läuft.
    std::string step_text() const;

private:
    std::vector<RouteCmd> cmds_;
    Pid lat_;
    Pid ang_;
    MotionController motion_;
    SimRouteExecutor exec_;
    RoutePlayer player_;
    bool begun_ = false;
};

}  // namespace frostbyte::sim
