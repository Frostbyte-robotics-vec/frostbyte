#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "core/log_format.hpp"
#include "core/spsc_queue.hpp"
#include "hal/interface/log_sink.hpp"

// Logger (F-05, ADR-047): Der Steuer-Task kopiert je Logzeile einen LogRecord in eine Warteschlange (record(), kein
// Schreiben, kein Blockieren). Der Logger-Task formatiert mit logfmt (dasselbe wie die Simulation, byte-gleich) und
// hängt die Zeilen an das LogSink an (drain()). Textfelder sind feste Puffer: die Quelle darf nach record() ungültig
// werden. Geht eine Zeile verloren (Schlange voll, Schreibfehler), trägt die nächste gespeicherte Zeile das
// Ereignis LOGDROP:<anzahl> (neues Token, keine Formatänderung, docs/log_format.md).
namespace frostbyte {

// Logzeile alle 20 ms (docs/log_format.md) bei einem Steuertakt von 10 ms: jeder zweite Takt.
struct LogCadence {
    static constexpr std::uint32_t period_ms = 20;
    static constexpr std::uint32_t control_period_ms = 10;
    static constexpr bool due(std::uint32_t control_cycle) {
        return control_cycle % (period_ms / control_period_ms) == 0;
    }
};

// Ein Takt, wie logfmt::Row, aber mit eigenen Textpuffern. Der Roboter wird vom Logger eingesetzt, nicht je Zeile.
struct LogRecord {
    static constexpr std::size_t cycle_len = 12;
    static constexpr std::size_t route_step_len = 32;
    static constexpr std::size_t event_len = 64;

    double t_s = logfmt::none;
    double x_in = logfmt::none, y_in = logfmt::none, h_deg = logfmt::none;
    double gx_in = logfmt::none, gy_in = logfmt::none, gh_deg = logfmt::none, g_err_in = logfmt::none;
    double tx_in = logfmt::none, ty_in = logfmt::none, th_deg = logfmt::none;
    double lift_deg = logfmt::none, arm_deg = logfmt::none, claw_in = logfmt::none;
    double lift_set_deg = logfmt::none, arm_set_deg = logfmt::none;
    char cycle[cycle_len] = "";
    char route_step[route_step_len] = "";
    int has_cup = logfmt::no_flag, has_pin = logfmt::no_flag, pin_yellow_up = logfmt::no_flag;
    int score = logfmt::no_flag;
    double cpu_ms = logfmt::none, batt_v = logfmt::none, temp_max_c = logfmt::none;
    char event[event_len] = "";
    bool new_run = false;           // erste Zeile eines Laufs: der Logger beginnt dafür eine neue Datei
    std::uint32_t lost_before = 0;  // vom Logger gesetzt: Zeilen, die vor dieser verloren gingen

    // Kopiert s in einen der Puffer oben, bei Überlänge abgeschnitten; nullptr = leer.
    template <std::size_t N>
    static void set_text(char (&dst)[N], const char* s) {
        std::size_t i = 0;
        if (s)
            for (; i + 1 < N && s[i]; ++i)
                dst[i] = s[i];
        dst[i] = '\0';
    }
};

template <std::size_t N>
class Logger {
public:
    // robot und build müssen so lange gültig sein wie der Logger (Stringliterale, FB_COMMIT; build leer -> "-" setzen, ADR-007).
    Logger(const char* robot, const char* build) : robot_(robot), build_(build) {}

    // Beginnt die erste Datei und schreibt Kopf- und Spaltenzeile. Danach nimmt record() Zeilen an. false, wenn das
    // Ziel nicht schreibt. Vor dem Start der Tasks aufrufen. Jeder weitere Lauf (LogRecord::new_run) bekommt im
    // Logger-Task eine eigene Datei mit eigener Kopfzeile, t_s beginnt dort wieder bei 0 (ein Lauf = eine Datei, ADR-007).
    bool open(hal::LogSink& sink) {
        if (!sink.begin_file() || !write_header(sink))
            return false;
        open_.store(true, std::memory_order_release);
        return true;
    }

    bool is_open() const { return open_.load(std::memory_order_acquire); }

    // Steuer-Task. Kopiert und kehrt sofort zurück. false = nicht gespeichert (nicht geöffnet: zählt nicht als
    // Verlust; Schlange voll: zählt).
    bool record(const LogRecord& r) {
        if (!is_open())
            return false;
        LogRecord copy = r;
        copy.lost_before = pending_lost_;
        if (queue_.try_push(copy)) {
            pending_lost_ = 0;
            return true;
        }
        ++pending_lost_;
        return false;
    }

    // Logger-Task. Formatiert alle wartenden Zeilen, schreibt sie gebündelt und schiebt sie dann auf die Karte.
    void drain(hal::LogSink& sink) {
        std::size_t used = 0;
        std::uint32_t rows = 0;
        std::uint32_t reported = 0;  // LOGDROP-Zahlen, die im Puffer stehen
        LogRecord r;
        while (queue_.try_pop(r)) {
            if (r.new_run && file_has_rows_) {
                flush_chunk(sink, used, rows, reported);
                used = 0;
                rows = 0;
                reported = 0;
                if (!sink.begin_file() || !write_header(sink)) {
                    write_errors_.store(write_errors_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
                    carry_lost_ += 1 + r.lost_before;
                    continue;  // der nächste Lauf-Beginn versucht es wieder
                }
                file_has_rows_ = false;
            }
            char event[LogRecord::event_len + 24];
            int len = format(r, make_event(r, event), buf_ + used, sizeof buf_ - used);
            if (len < 0 && used > 0) {  // Rest des Puffers zu klein: erst schreiben, dann neu formatieren
                flush_chunk(sink, used, rows, reported);
                used = 0;
                rows = 0;
                reported = 0;
                len = format(r, make_event(r, event), buf_, sizeof buf_);
            }
            if (len < 0) {  // eine einzelne Zeile passt nie (nicht erreichbar bei festen Feldgrößen)
                carry_lost_ += 1 + r.lost_before;
                continue;
            }
            reported += r.lost_before + carry_lost_;
            carry_lost_ = 0;
            used += static_cast<std::size_t>(len);
            buf_[used++] = '\n';
            file_has_rows_ = true;
            ++rows;
        }
        flush_chunk(sink, used, rows, reported);
    }

    std::size_t dropped() const { return queue_.dropped(); }
    std::size_t write_errors() const { return write_errors_.load(std::memory_order_relaxed); }

private:
    bool write_header(hal::LogSink& sink) {
        char line[256];
        int n = logfmt::format_header(line, sizeof line, robot_, "robot", logfmt::no_seed, build_);
        if (n < 0 || static_cast<std::size_t>(n) + 1 >= sizeof line)
            return false;
        line[n++] = '\n';
        if (!sink.append(line, static_cast<std::size_t>(n)))
            return false;
        n = std::snprintf(line, sizeof line, "%s\n", logfmt::columns);
        return sink.append(line, static_cast<std::size_t>(n)) && sink.flush();
    }

    // Ereignisfeld der Zeile: eigene Ereignisse plus LOGDROP für Zeilen, die davor verloren gingen.
    const char* make_event(const LogRecord& r, char (&out)[LogRecord::event_len + 24]) const {
        const std::uint32_t lost = r.lost_before + carry_lost_;
        if (lost > 0)
            std::snprintf(out, sizeof out, "%s%sLOGDROP:%lu", r.event, r.event[0] ? ";" : "",
                          static_cast<unsigned long>(lost));
        else
            std::snprintf(out, sizeof out, "%s", r.event);
        return out;
    }

    // Formatiert r nach dst (Platz n, mit Platz für das Zeilenende). Rückgabe: Länge, -1 wenn es nicht passt.
    int format(const LogRecord& r, const char* event, char* dst, std::size_t n) const {
        logfmt::Row row;
        row.t_s = r.t_s;
        row.robot = robot_;
        row.x_in = r.x_in;
        row.y_in = r.y_in;
        row.h_deg = r.h_deg;
        row.gx_in = r.gx_in;
        row.gy_in = r.gy_in;
        row.gh_deg = r.gh_deg;
        row.g_err_in = r.g_err_in;
        row.tx_in = r.tx_in;
        row.ty_in = r.ty_in;
        row.th_deg = r.th_deg;
        row.lift_deg = r.lift_deg;
        row.arm_deg = r.arm_deg;
        row.claw_in = r.claw_in;
        row.lift_set_deg = r.lift_set_deg;
        row.arm_set_deg = r.arm_set_deg;
        row.cycle = r.cycle;
        row.route_step = r.route_step;
        row.has_cup = r.has_cup;
        row.has_pin = r.has_pin;
        row.pin_yellow_up = r.pin_yellow_up;
        row.score = r.score;
        row.cpu_ms = r.cpu_ms;
        row.batt_v = r.batt_v;
        row.temp_max_c = r.temp_max_c;
        row.event = event;
        const int len = logfmt::format_row(dst, n, row);
        return (len >= 0 && static_cast<std::size_t>(len) + 1 < n) ? len : -1;
    }

    // Schreibt den Puffer. Misslingt das, gelten seine Zeilen als verloren und die nächste meldet sie.
    void flush_chunk(hal::LogSink& sink, std::size_t used, std::uint32_t rows, std::uint32_t reported) {
        if (used == 0)
            return;
        if (sink.append(buf_, used) && sink.flush())
            return;
        write_errors_.store(write_errors_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        carry_lost_ += rows + reported;
    }

    const char* robot_;
    const char* build_;
    SpscQueue<LogRecord, N> queue_;
    std::atomic<bool> open_{false};
    std::uint32_t pending_lost_ = 0;  // nur Steuer-Task
    std::uint32_t carry_lost_ = 0;    // nur Logger-Task
    bool file_has_rows_ = false;      // nur Logger-Task: in der aktuellen Datei steht schon eine Datenzeile
    std::atomic<std::size_t> write_errors_{0};
    char buf_[2048];
};

}  // namespace frostbyte
