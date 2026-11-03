#include "sim/route_runner.hpp"

#include <cmath>
#include <stdexcept>

namespace frostbyte::sim {

std::vector<FieldPoint> load_field_points(const Json& field) {
    std::vector<FieldPoint> out;
    for (const char* key : {"goals", "loader", "toggles", "posen"}) {
        const Json* list = field.find(key);
        if (list == nullptr)
            continue;
        for (std::size_t i = 0; i < list->size(); ++i) {
            const Json& p = (*list)[i];
            FieldPoint f;
            f.id = p.at("id").as_string();
            if (!p.at("x_in").is_null() && !p.at("y_in").is_null())
                f.xy_in = std::make_pair(p.at("x_in").as_number(), p.at("y_in").as_number());
            out.push_back(std::move(f));
        }
    }
    return out;
}

namespace {

const char* op_text(RouteOp op) {
    switch (op) {
        case RouteOp::target_p10: return "target_p10";
        case RouteOp::start: return "start";
        case RouteOp::goto_name: return "goto";
        case RouteOp::goto_pose: return "goto_pose";
        case RouteOp::turn: return "turn";
        case RouteOp::intake: return "intake";
        case RouteOp::cycle: return "cycle";
        case RouteOp::toggle: return "toggle";
        case RouteOp::signal: return "signal";
        case RouteOp::wait_for: return "wait_for";
        case RouteOp::wait: return "wait";
        case RouteOp::parallel: return "parallel";
        case RouteOp::repeat: return "repeat";
        case RouteOp::at: return "at";
        case RouteOp::end_hold: return "end_hold";
    }
    return "?";
}

const char* robot_text(RouteRobot r) { return r == RouteRobot::stacker ? "stacker" : "runner"; }

}  // namespace

void SimRouteExecutor::begin_motion(int lane, const MotionCommand& cmd, double now_s) {
    if (motion_lane_ >= 0 && motion_lane_ != lane)
        lanes_[motion_lane_].status = StepStatus::failed;  // eine Bewegung zur Zeit
    if (motion_.request(cmd, now_s)) {
        motion_lane_ = lane;
        lanes_[lane].status = StepStatus::running;
    } else {
        motion_lane_ = -1;
        lanes_[lane].status = StepStatus::failed;
    }
}

void SimRouteExecutor::start(int lane, const RouteCmd& cmd, double now_s) {
    LaneRec& l = lanes_[lane];
    l = LaneRec{};
    switch (cmd.op) {
        case RouteOp::goto_name: {
            const double to = cmd.timeout_s > 0.0 ? cmd.timeout_s : cfg_.goto_s;
            l.status = StepStatus::failed;
            for (const FieldPoint& p : points_)
                if (p.id == cmd.name) {
                    if (p.xy_in)  // null: nicht vermessen, kein Ziel erfinden
                        begin_motion(lane, MotionCommand::move_to(p.xy_in->first, p.xy_in->second, to), now_s);
                    break;
                }
            break;
        }
        case RouteOp::goto_pose:
            begin_motion(lane,
                         MotionCommand::move_to(cmd.x_in, cmd.y_in, cmd.timeout_s > 0.0 ? cmd.timeout_s : cfg_.goto_s),
                         now_s);
            break;
        case RouteOp::turn:
            begin_motion(lane, MotionCommand::turn_to(cmd.h_deg, cmd.timeout_s > 0.0 ? cmd.timeout_s : cfg_.turn_s),
                         now_s);
            break;
        case RouteOp::signal:
            bus_.insert(std::string(robot_text(self_)) + "." + cmd.name);
            l.status = StepStatus::done;
            break;
        case RouteOp::wait_for:
            l.waiting = true;
            l.wait_robot = cmd.robot;
            l.wait_name = cmd.name;
            l.status = StepStatus::running;
            break;
        default:  // start, intake, cycle, toggle: Platzhalter ohne Modell (ADR-031)
            l.status = StepStatus::done;
            break;
    }
}

StepStatus SimRouteExecutor::poll(int lane, double) {
    LaneRec& l = lanes_[lane];
    if (l.waiting && bus_.count(std::string(robot_text(l.wait_robot)) + "." + l.wait_name) > 0) {
        l.waiting = false;
        l.status = StepStatus::done;
    }
    if (lane == motion_lane_) {
        switch (motion_.state()) {
            case MotionState::done: l.status = StepStatus::done; break;
            case MotionState::timeout:
            case MotionState::blocked: l.status = StepStatus::failed; break;
            case MotionState::running:
            case MotionState::idle: break;
        }
        if (l.status != StepStatus::running)
            motion_lane_ = -1;
    }
    return l.status;
}

StepStatus SimRouteExecutor::cancel(int lane, double now_s) {
    const StepStatus s = poll(lane, now_s);
    LaneRec& l = lanes_[lane];
    l.waiting = false;
    if (lane == motion_lane_)
        motion_lane_ = -1;  // Antrieb steht: step() gibt ohne aktive Bewegung 0 V aus
    l.status = s == StepStatus::done ? StepStatus::done : StepStatus::failed;
    return l.status;
}

RouteRunner::RouteRunner(std::vector<RouteCmd> cmds, const std::vector<FieldPoint>& points, SignalBus& bus,
                         RouteRobot self, const RouteControl& ctl)
    : cmds_(std::move(cmds)),
      lat_(ctl.lateral),
      ang_(ctl.angular),
      motion_(ctl.motion, lat_, ang_),
      exec_(motion_, points, bus, self, ctl.player),
      player_(cmds_.data(), static_cast<int>(cmds_.size()), exec_, ctl.player) {
    if (player_.status() == RouteStatus::invalid)
        throw std::runtime_error("Route für den RoutePlayer ungültig (Struktur, Lanes oder Tiefe)");
}

std::pair<double, double> RouteRunner::step(int t_ms, const Pose& est) {
    const double t_s = t_ms / 1000.0;
    if (!begun_) {
        player_.begin(t_s);
        begun_ = true;
    }
    player_.update(t_s, t_s);  // Matchzeit = Simulationszeit
    if (!exec_.motion_active())
        return {0.0, 0.0};
    const MotionOutput o = motion_.update(t_s, ::frostbyte::Pose{est.x_in, est.y_in, est.h_deg});
    return {o.left_v, o.right_v};
}

std::string RouteRunner::step_text() const {
    const RouteCmd* c = player_.current_cmd();
    if (c == nullptr)
        return {};
    std::string s = std::to_string(c->line) + ":" + op_text(c->op);
    if (c->name[0] != '\0')
        s += std::string(" ") + c->name;
    return s;
}

}  // namespace frostbyte::sim
