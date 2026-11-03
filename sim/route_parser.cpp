#include "sim/route_parser.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>

#include "sim/json.hpp"

namespace frostbyte::sim {

namespace {

struct Fail {
    std::string msg;
};

using Tokens = std::vector<std::string>;

bool to_number(const std::string& s, double& out) {
    if (s.empty())
        return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size() && std::isfinite(out);
}

double number(const std::string& what, const std::string& s) {
    double v = 0;
    if (!to_number(s, v))
        throw Fail{what + ": '" + s + "' ist keine Zahl"};
    return v;
}

double positive(const std::string& what, const std::string& s) {
    const double v = number(what, s);
    if (!(v > 0.0))
        throw Fail{what + ": muss größer als 0 sein (ist " + s + ")"};
    return v;
}

void set_name(RouteCmd& c, const std::string& n, const char* what) {
    if (n.empty() || n.size() >= static_cast<std::size_t>(kRouteNameLen))
        throw Fail{std::string(what) + " '" + n + "' ist zu lang (höchstens " + std::to_string(kRouteNameLen - 1) +
                   " Zeichen)"};
    std::memcpy(c.name, n.c_str(), n.size() + 1);
}

bool contains(const std::vector<std::string>& v, const std::string& n) {
    return std::find(v.begin(), v.end(), n) != v.end();
}

enum Cat { kGoal = 1, kLoader = 2, kToggle = 4, kPose = 8 };

const char* cat_name(const RouteNames& names, const std::string& n) {
    if (contains(names.goals, n))
        return "Goal";
    if (contains(names.loaders, n))
        return "Loader";
    if (contains(names.toggles, n))
        return "Toggle";
    if (contains(names.poses, n))
        return "Pose";
    return nullptr;
}

int cat_of(const RouteNames& names, const std::string& n) {
    return (contains(names.goals, n) ? kGoal : 0) | (contains(names.loaders, n) ? kLoader : 0) |
           (contains(names.toggles, n) ? kToggle : 0) | (contains(names.poses, n) ? kPose : 0);
}

// Name muss in field.json stehen und in einer der erlaubten Kategorien liegen.
void check_name(const RouteNames& names, const std::string& n, int allowed, const char* expected) {
    const char* found = cat_name(names, n);
    if (!found)
        throw Fail{"unbekannter Name '" + n + "' (nicht in field/field.json)"};
    if (!(cat_of(names, n) & allowed))
        throw Fail{"'" + n + "' ist ein " + found + ", erwartet wird " + expected};
}

// Positionale Teile und Schlüssel=Wert getrennt; jeder Schlüssel nur einmal und nur aus der erlaubten Liste.
struct Args {
    Tokens pos;
    std::map<std::string, std::string> kv;
};

Args split_args(const Tokens& t, std::size_t from, const std::vector<std::string>& allowed, const std::string& op) {
    Args a;
    for (std::size_t i = from; i < t.size(); ++i) {
        const std::size_t eq = t[i].find('=');
        if (eq == 0)
            throw Fail{"'" + t[i] + "': erwartet Schlüssel=Wert"};
        if (eq == std::string::npos) {
            a.pos.push_back(t[i]);
            continue;
        }
        const std::string k = t[i].substr(0, eq), v = t[i].substr(eq + 1);
        if (std::find(allowed.begin(), allowed.end(), k) == allowed.end())
            throw Fail{"unbekannter Schlüssel '" + k + "' für '" + op + "'"};
        if (a.kv.count(k))
            throw Fail{"Schlüssel '" + k + "' doppelt"};
        if (v.empty())
            throw Fail{"Schlüssel '" + k + "' ohne Wert"};
        a.kv[k] = v;
    }
    return a;
}

bool has(const Args& a, const char* k) { return a.kv.count(k) != 0; }
const std::string& get(const Args& a, const char* k) { return a.kv.at(k); }

void need(const Args& a, const char* k, const char* hint = nullptr) {
    if (!has(a, k))
        throw Fail{std::string("Schlüssel '") + k + "' fehlt" + (hint ? std::string(" (") + hint + ")" : "")};
}

void expect_pos(const Args& a, std::size_t n, const std::string& op, const char* usage) {
    if (a.pos.size() != n)
        throw Fail{op + ": erwartet " + usage};
}

void timeouts(RouteCmd& c, const Args& a) {
    if (has(a, "timeout"))
        c.timeout_s = positive("timeout", get(a, "timeout"));
    if (has(a, "on_timeout")) {
        const std::string& v = get(a, "on_timeout");
        if (c.timeout_s < 0)
            throw Fail{"on_timeout ohne timeout"};
        if (v == "skip")
            c.on_timeout = RouteOnTimeout::skip;
        else if (v == "retry")
            c.on_timeout = RouteOnTimeout::retry;
        else if (v == "abort")
            c.on_timeout = RouteOnTimeout::abort;
        else
            throw Fail{"on_timeout: '" + v + "' unbekannt (skip, retry oder abort)"};
    }
}

void pose_args(RouteCmd& c, const Args& a) {
    need(a, "x", "pose braucht x= y= h=");
    need(a, "y", "pose braucht x= y= h=");
    need(a, "h", "pose braucht x= y= h=");
    c.x_in = number("x", get(a, "x"));
    c.y_in = number("y", get(a, "y"));
    c.h_deg = number("h", get(a, "h"));
    c.has_pose = true;
}

const std::vector<std::string> kTimeoutKeys = {"timeout", "on_timeout"};

std::vector<std::string> keys(std::initializer_list<const char*> own) {
    std::vector<std::string> k(own.begin(), own.end());
    k.insert(k.end(), kTimeoutKeys.begin(), kTimeoutKeys.end());
    return k;
}

// Liest eine Einzelanweisung (ohne Block, ohne start/target_p10/end/at). tokens[from] ist der Befehlsname.
RouteCmd parse_simple(const Tokens& t, std::size_t from, const RouteNames& names) {
    RouteCmd c;
    const std::string& op = t[from];
    const std::size_t a0 = from + 1;
    auto args = [&](const std::vector<std::string>& allowed) { return split_args(t, a0, allowed, op); };
    if (op == "goto") {
        if (a0 < t.size() && t[a0] == "pose") {
            c.op = RouteOp::goto_pose;
            const Args a = args(keys({"x", "y", "h"}));
            if (a.pos.size() != 1)
                throw Fail{"goto pose: erwartet x= y= h="};
            pose_args(c, a);
            timeouts(c, a);
        } else {
            c.op = RouteOp::goto_name;
            const Args a = args(keys({"tol"}));
            expect_pos(a, 1, op, "goto <NAME> oder goto pose x= y= h=");
            check_name(names, a.pos[0], kGoal | kLoader | kToggle | kPose, "ein Feldpunkt");
            set_name(c, a.pos[0], "Name");
            if (has(a, "tol"))
                c.tol_in = positive("tol", get(a, "tol"));
            timeouts(c, a);
        }
    } else if (op == "turn") {
        c.op = RouteOp::turn;
        const Args a = args(keys({}));
        expect_pos(a, 1, op, "turn <deg>");
        c.h_deg = number("turn", a.pos[0]);
        timeouts(c, a);
    } else if (op == "intake") {
        c.op = RouteOp::intake;
        const Args a = args(keys({"until"}));
        expect_pos(a, 0, op, "intake until=unit");
        if (!has(a, "until") || get(a, "until") != "unit")
            throw Fail{"intake: erwartet until=unit"};
        timeouts(c, a);
    } else if (op == "cycle") {
        c.op = RouteOp::cycle;
        const Args a = args(keys({"goal", "level", "source"}));
        expect_pos(a, 0, op, "cycle goal=<GOAL> level=<n|auto>");
        need(a, "goal");
        need(a, "level", "0 bis 5 oder auto");
        check_name(names, get(a, "goal"), kGoal, "ein Goal");
        set_name(c, get(a, "goal"), "Goal");
        const std::string& l = get(a, "level");
        if (l == "auto") {
            c.level = kRouteLevelAuto;
        } else {
            double v = 0;
            if (!to_number(l, v) || v != std::floor(v) || v < 0 || v > kRouteMaxLevel)
                throw Fail{"level: '" + l + "' ungültig (ganze Zahl 0 bis " + std::to_string(kRouteMaxLevel) +
                           " oder auto)"};
            c.level = static_cast<int>(v);
        }
        if (has(a, "source")) {
            const std::string& s = get(a, "source");
            if (s == "preload")
                c.source = RouteSource::preload;
            else if (s == "quiver")
                c.source = RouteSource::quiver;
            else
                throw Fail{"source: '" + s + "' unbekannt (preload oder quiver)"};
        }
        timeouts(c, a);
    } else if (op == "toggle") {
        c.op = RouteOp::toggle;
        const Args a = args(keys({}));
        expect_pos(a, 1, op, "toggle <TOGGLE>");
        check_name(names, a.pos[0], kToggle, "ein Toggle");
        set_name(c, a.pos[0], "Name");
        timeouts(c, a);
    } else if (op == "signal" || op == "wait_for") {
        const bool wf = op == "wait_for";
        c.op = wf ? RouteOp::wait_for : RouteOp::signal;
        const Args a = wf ? args(keys({})) : args({});
        expect_pos(a, 1, op, wf ? "wait_for <robot>.<name>" : "signal <name>");
        std::string ev = a.pos[0];
        if (wf) {
            const std::size_t dot = ev.find('.');
            if (dot == std::string::npos)
                throw Fail{"wait_for: erwartet <robot>.<name>"};
            const std::string r = ev.substr(0, dot);
            if (r == "stacker")
                c.robot = RouteRobot::stacker;
            else if (r == "runner")
                c.robot = RouteRobot::runner;
            else
                throw Fail{"wait_for: Roboter '" + r + "' unbekannt (stacker oder runner)"};
            ev = ev.substr(dot + 1);
        }
        const bool ok = !ev.empty() && std::all_of(ev.begin(), ev.end(), [](char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
        });
        if (ev.size() >= static_cast<std::size_t>(kRouteNameLen))
            throw Fail{"Ereignisname '" + ev + "' ist zu lang (höchstens " + std::to_string(kRouteNameLen - 1) +
                       " Zeichen)"};
        if (!ok)
            throw Fail{"Ereignisname '" + ev + "' ungültig (nur a-z, 0-9 und _)"};
        set_name(c, ev, "Ereignisname");
        if (wf)
            timeouts(c, a);
    } else if (op == "wait") {
        c.op = RouteOp::wait;
        const Args a = args({});
        expect_pos(a, 1, op, "wait <s>");
        c.value = positive("wait", a.pos[0]);
    } else {
        throw Fail{"unbekannte Anweisung '" + op + "'"};
    }
    return c;
}

Tokens tokenize(const std::string& line) {
    Tokens t;
    std::string cur;
    for (char ch : line) {
        if (ch == '#')
            break;
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            if (!cur.empty())
                t.push_back(cur), cur.clear();
        } else {
            cur += ch;
        }
    }
    if (!cur.empty())
        t.push_back(cur);
    return t;
}

}  // namespace

RouteResult parse_route(std::string_view text, const RouteNames& names) {
    RouteResult res;
    std::vector<std::pair<std::size_t, int>> open;  // Index des Blocks in cmds, Zeile der öffnenden Klammer
    bool seen_start = false, seen_target = false, seen_end = false;
    int line_no = 0, last_line = 1;
    std::istringstream in{std::string(text)};
    std::string line;
    {
        while (std::getline(in, line)) {
            ++line_no;
            const Tokens t = tokenize(line);
            if (t.empty())
                continue;
            last_line = line_no;
            try {
                if (seen_end)
                    throw Fail{"nach 'end hold' darf nichts mehr stehen"};
                const std::string& op = t[0];
                if (op == "}") {
                    if (t.size() != 1)
                        throw Fail{"nach '}' darf nichts stehen"};
                    if (open.empty())
                        throw Fail{"'}' ohne öffnende Klammer"};
                    res.cmds[open.back().first].body_len = static_cast<int>(res.cmds.size() - open.back().first - 1);
                    open.pop_back();
                    continue;
                }
                const bool top = open.empty();
                auto top_only = [&](const char* what) {
                    if (!top)
                        throw Fail{std::string(what) + " nur auf der obersten Ebene erlaubt (Block aus Zeile " +
                                   std::to_string(open.back().second) + " ist noch offen)"};
                };
                RouteCmd c;
                if (op == "target_p10") {
                    top_only("target_p10");
                    if (seen_target || seen_start)
                        throw Fail{seen_target ? "target_p10 doppelt" : "target_p10 muss vor start stehen"};
                    if (t.size() != 2)
                        throw Fail{"target_p10: erwartet target_p10 <punkte>"};
                    c.op = RouteOp::target_p10;
                    c.value = positive("target_p10", t[1]);
                    seen_target = true;
                } else if (op == "start") {
                    top_only("start");
                    if (seen_start)
                        throw Fail{"start doppelt"};
                    c.op = RouteOp::start;
                    if (t.size() >= 2 && t[1] == "pose") {
                        const Args a = split_args(t, 2, {"x", "y", "h"}, "start");
                        if (!a.pos.empty())
                            throw Fail{"start pose: erwartet x= y= h="};
                        pose_args(c, a);
                    } else {
                        if (t.size() != 2)
                            throw Fail{"start: erwartet start <POSE> oder start pose x= y= h="};
                        check_name(names, t[1], kPose, "eine Pose");
                        set_name(c, t[1], "Name");
                    }
                    seen_start = true;
                } else if (!seen_start) {
                    throw Fail{"die Route muss mit start beginnen (vor '" + op + "')"};
                } else if (op == "end") {
                    top_only("end hold");
                    if (t.size() != 2 || t[1] != "hold")
                        throw Fail{"erwartet 'end hold'"};
                    c.op = RouteOp::end_hold;
                    seen_end = true;
                } else if (op == "at") {
                    top_only("at");
                    if (t.size() < 3)
                        throw Fail{"at: erwartet at <t>s <anweisung>"};
                    const std::string& ts = t[1];
                    if (ts.size() < 2 || ts.back() != 's')
                        throw Fail{"at: Zeit mit 's' angeben, z. B. 52s (ist '" + ts + "')"};
                    c.op = RouteOp::at;
                    c.value = positive("at", ts.substr(0, ts.size() - 1));
                    c.body_len = 1;
                    const std::string& inner = t[2];
                    if (t.back() == "{" || inner == "parallel" || inner == "repeat")
                        throw Fail{"at: ein Block ist als Anweisung nicht erlaubt"};
                    if (inner == "at" || inner == "start" || inner == "target_p10" || inner == "end")
                        throw Fail{"at: '" + inner + "' ist als Anweisung nicht erlaubt"};
                    c.line = line_no;
                    RouteCmd body = parse_simple(t, 2, names);
                    body.line = line_no;
                    res.cmds.push_back(c);
                    res.cmds.push_back(body);
                    continue;
                } else if (op == "parallel" || op == "repeat") {
                    const bool rep = op == "repeat";
                    if (t.back() != "{" || t.size() != (rep ? 3u : 2u))
                        throw Fail{rep ? "repeat: erwartet repeat <n> {" : "parallel: erwartet 'parallel {'"};
                    c.op = rep ? RouteOp::repeat : RouteOp::parallel;
                    if (rep) {
                        double n = 0;
                        if (!to_number(t[1], n) || n != std::floor(n) || n < 1 || n > 100)
                            throw Fail{"repeat: '" + t[1] + "' ungültig (ganze Zahl 1 bis 100)"};
                        c.value = n;
                    }
                    c.line = line_no;
                    open.emplace_back(res.cmds.size(), line_no);
                    res.cmds.push_back(c);
                    continue;
                } else {
                    c = parse_simple(t, 0, names);
                }
                c.line = line_no;
                res.cmds.push_back(c);
            } catch (const Fail& f) {
                res.cmds.clear();
                res.error_line = line_no;
                res.error = "Zeile " + std::to_string(line_no) + ": " + f.msg;
                return res;
            }
        }
    }
    auto fail = [&](int ln, const std::string& msg) {
        res.cmds.clear();
        res.error_line = ln;
        res.error = "Zeile " + std::to_string(ln) + ": " + msg;
        return res;
    };
    if (!open.empty())
        return fail(open.back().second, "Block nicht geschlossen ('}' fehlt)");
    if (!seen_start)
        return fail(1, "die Route enthält kein start");
    if (!seen_end)
        return fail(last_line, "die Route muss mit 'end hold' enden");
    return res;
}

RouteNames route_names_from_field(const std::string& field_json_path) {
    const Json f = load_json_file(field_json_path);
    RouteNames n;
    auto fill = [&](const char* key, std::vector<std::string>& out) {
        const Json& list = f.at(key);
        for (std::size_t i = 0; i < list.size(); ++i)
            out.push_back(list[i].at("id").as_string());
    };
    fill("goals", n.goals);
    fill("loader", n.loaders);
    fill("toggles", n.toggles);
    fill("posen", n.poses);
    return n;
}

namespace {

const char* op_name(RouteOp op) {
    switch (op) {
        case RouteOp::target_p10: return "target_p10";
        case RouteOp::start: return "start";
        case RouteOp::goto_name: return "goto_name";
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
    return "end_hold";
}

std::string num(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

}  // namespace

std::string route_to_cpp(const std::vector<RouteCmd>& cmds, const std::string& symbol) {
    std::ostringstream o;
    o << "// Erzeugt von tools/route2cpp.py (frostbyte_route_export) – nicht von Hand ändern.\n"
      << "#pragma once\n\n#include \"match/route.hpp\"\n\nnamespace frostbyte::routes {\n\n"
      << "inline constexpr RouteCmd " << symbol << "[" << cmds.size() << "] = {\n";
    for (const RouteCmd& c : cmds) {
        o << "    {RouteOp::" << op_name(c.op) << ", " << c.line << ", \"" << c.name << "\", "
          << (c.has_pose ? "true" : "false") << ", " << num(c.x_in) << ", " << num(c.y_in) << ", " << num(c.h_deg)
          << ", " << num(c.value) << ", " << num(c.tol_in) << ", " << num(c.timeout_s) << ", " << c.level << ", "
          << c.body_len << ", RouteSource::" << (c.source == RouteSource::quiver ? "quiver" : "preload")
          << ", RouteOnTimeout::"
          << (c.on_timeout == RouteOnTimeout::retry   ? "retry"
              : c.on_timeout == RouteOnTimeout::abort ? "abort"
                                                      : "skip")
          << ", RouteRobot::"
          << (c.robot == RouteRobot::stacker  ? "stacker"
              : c.robot == RouteRobot::runner ? "runner"
                                              : "none")
          << "},\n";
    }
    o << "};\n\ninline constexpr int " << symbol << "Len = " << cmds.size()
      << ";\n\n}  // namespace frostbyte::routes\n";
    return o.str();
}

}  // namespace frostbyte::sim
