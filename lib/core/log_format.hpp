#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

// Log-Format v1 (docs/log_format.md). Roboter (Logger-Task) und Simulation formatieren beide hier,
// damit die Dateien identisch sind. Kein new: geschrieben wird in einen Puffer des Aufrufers.
// Spalten nie umbenennen oder umsortieren ohne neue Versionsnummer.
namespace frostbyte::logfmt {

constexpr int version = 1;
constexpr std::int64_t no_seed = -1;
constexpr double none = std::numeric_limits<double>::quiet_NaN();
constexpr int no_flag = -1;

constexpr const char* columns =
    "t_s,robot,x_in,y_in,h_deg,gx_in,gy_in,gh_deg,g_err_in,tx_in,ty_in,th_deg,lift_deg,arm_deg,claw_in,"
    "lift_set_deg,arm_set_deg,cycle,route_step,has_cup,has_pin,pin_yellow_up,score,cpu_ms,batt_v,temp_max_c,event";

// Ein Takt eines Roboters. NaN, no_flag oder nullptr = Wert nicht vorhanden (leere Zelle).
struct Row {
    double t_s = none;
    const char* robot = nullptr;
    double x_in = none, y_in = none, h_deg = none;
    double gx_in = none, gy_in = none, gh_deg = none, g_err_in = none;
    double tx_in = none, ty_in = none, th_deg = none;
    double lift_deg = none, arm_deg = none, claw_in = none;
    double lift_set_deg = none, arm_set_deg = none;
    const char* cycle = nullptr;
    const char* route_step = nullptr;
    int has_cup = no_flag, has_pin = no_flag, pin_yellow_up = no_flag;
    int score = no_flag;
    double cpu_ms = none, batt_v = none, temp_max_c = none;
    const char* event = nullptr;
};

// Schreibt die Kopfzeile ohne Zeilenende. Rückgabe wie snprintf.
inline int format_header(char* buf, std::size_t n, const char* robot, const char* source, std::int64_t seed,
                         const char* build) {
    if (seed == no_seed)
        return std::snprintf(buf, n, "# frostbyte-log v%d robot=%s source=%s seed=- build=%s", version, robot, source,
                             build);
    return std::snprintf(buf, n, "# frostbyte-log v%d robot=%s source=%s seed=%lld build=%s", version, robot, source,
                         static_cast<long long>(seed), build);
}

namespace detail {

class Writer {
public:
    Writer(char* buf, std::size_t n) : buf_(buf), n_(n) {
        if (n_ > 0)
            buf_[0] = '\0';
    }

    void num(double v) {
        sep();
        if (std::isnan(v))
            return;
        // Feste 3 Nachkommastellen; Werte, die auf 0 runden, ohne Vorzeichen (keine "-0.000").
        if (std::fabs(v) < 0.0005)
            v = 0.0;
        put(std::snprintf(cur(), room(), "%.3f", v));
    }

    void flag(int v) {
        sep();
        if (v != no_flag)
            put(std::snprintf(cur(), room(), "%d", v));
    }

    void text(const char* s) {
        sep();
        if (s)
            put(std::snprintf(cur(), room(), "%s", s));
    }

    int length() const { return static_cast<int>(len_); }

private:
    void sep() {
        if (first_) {
            first_ = false;
            return;
        }
        put(std::snprintf(cur(), room(), ","));
    }
    char* cur() { return buf_ + (len_ < n_ ? len_ : (n_ > 0 ? n_ - 1 : 0)); }
    std::size_t room() const { return len_ < n_ ? n_ - len_ : (n_ > 0 ? 1 : 0); }
    void put(int written) {
        if (written > 0)
            len_ += static_cast<std::size_t>(written);
    }

    char* buf_;
    std::size_t n_;
    std::size_t len_ = 0;
    bool first_ = true;
};

}  // namespace detail

// Schreibt eine Datenzeile ohne Zeilenende. Rückgabe: benötigte Länge (wie snprintf; >= n heißt abgeschnitten).
inline int format_row(char* buf, std::size_t n, const Row& r) {
    detail::Writer w(buf, n);
    w.num(r.t_s);
    w.text(r.robot);
    w.num(r.x_in);
    w.num(r.y_in);
    w.num(r.h_deg);
    w.num(r.gx_in);
    w.num(r.gy_in);
    w.num(r.gh_deg);
    w.num(r.g_err_in);
    w.num(r.tx_in);
    w.num(r.ty_in);
    w.num(r.th_deg);
    w.num(r.lift_deg);
    w.num(r.arm_deg);
    w.num(r.claw_in);
    w.num(r.lift_set_deg);
    w.num(r.arm_set_deg);
    w.text(r.cycle);
    w.text(r.route_step);
    w.flag(r.has_cup);
    w.flag(r.has_pin);
    w.flag(r.pin_yellow_up);
    w.flag(r.score);
    w.num(r.cpu_ms);
    w.num(r.batt_v);
    w.num(r.temp_max_c);
    w.text(r.event);
    return w.length();
}

}  // namespace frostbyte::logfmt
