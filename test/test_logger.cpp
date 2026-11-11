#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/log_format.hpp"
#include "core/logger.hpp"
#include "doctest/doctest.h"
#include "hal/sim/sim_log_sink.hpp"

using namespace frostbyte;

namespace {

LogRecord record_at(double t_s) {
    LogRecord r;
    r.t_s = t_s;
    r.x_in = t_s * 2.0;
    r.has_cup = 1;
    r.cpu_ms = 0.25;
    return r;
}

std::vector<std::string> lines_of(const std::string& s) {
    std::vector<std::string> out;
    std::size_t from = 0;
    while (from < s.size()) {
        const std::size_t nl = s.find('\n', from);
        out.push_back(s.substr(from, nl - from));
        if (nl == std::string::npos)
            break;
        from = nl + 1;
    }
    return out;
}

}  // namespace

TEST_CASE("Logger: Kopfzeile, dann Zeilen byte-gleich zu logfmt::format_row") {
    hal::MemoryLogSink sink;
    Logger<16> log("stacker", "abc123");
    REQUIRE(log.open(sink));
    LogRecord r = record_at(1.5);
    r.set_text(r.cycle, "AM_GOAL");
    r.set_text(r.event, "PLACED:NEUTRAL_R1:0");
    REQUIRE(log.record(r));
    log.drain(sink);

    char header[128];
    logfmt::format_header(header, sizeof header, "stacker", "robot", logfmt::no_seed, "abc123");
    logfmt::Row row;
    row.t_s = 1.5;
    row.robot = "stacker";
    row.x_in = 3.0;
    row.has_cup = 1;
    row.cpu_ms = 0.25;
    row.cycle = "AM_GOAL";
    row.event = "PLACED:NEUTRAL_R1:0";
    char line[512];
    logfmt::format_row(line, sizeof line, row);

    const std::string expect = std::string(header) + "\n" + logfmt::columns + "\n" + line + "\n";
    CHECK(sink.contents() == expect);
}

TEST_CASE("Logger: Textfelder werden beim Einreihen kopiert (Quelle danach ungültig)") {
    hal::MemoryLogSink sink;
    Logger<4> log("runner", "x");
    REQUIRE(log.open(sink));
    char step[32];
    std::snprintf(step, sizeof step, "%s", "12:cycle NEUTRAL_R1");
    LogRecord r = record_at(0.0);
    r.set_text(r.route_step, step);
    REQUIRE(log.record(r));
    std::memset(step, 'Z', sizeof step - 1);  // Quelle überschreiben
    r.set_text(r.route_step, "");             // auch der Record des Aufrufers ändert nichts mehr
    log.drain(sink);
    CHECK(sink.contents().find(",12:cycle NEUTRAL_R1,") != std::string::npos);
    CHECK(sink.contents().find("ZZZ") == std::string::npos);
}

TEST_CASE("Logger: zu lange Textfelder werden abgeschnitten, nicht überlaufen") {
    LogRecord r;
    const std::string big(500, 'a');
    r.set_text(r.event, big.c_str());
    CHECK(std::strlen(r.event) == sizeof r.event - 1);
}

TEST_CASE("Logger: Verbraucher steht (SD hängt) -> Erzeuger läuft weiter, Verlust steht im Log") {
    hal::MemoryLogSink sink;
    Logger<4> log("stacker", "b");
    REQUIRE(log.open(sink));
    int accepted = 0;
    for (int i = 0; i < 10; ++i)
        accepted += log.record(record_at(i * 0.02)) ? 1 : 0;
    CHECK(accepted == 4);
    CHECK(log.dropped() == 6);
    log.drain(sink);
    CHECK(log.record(record_at(0.2)));  // danach wieder Platz
    log.drain(sink);

    const auto lines = lines_of(sink.contents());
    REQUIRE(lines.size() == 2 + 4 + 1);
    CHECK(lines[2].rfind("0.000,", 0) == 0);  // die ersten vier bleiben, in Reihenfolge
    CHECK(lines[5].rfind("0.060,", 0) == 0);
    CHECK(lines[6].rfind("0.200,", 0) == 0);
    CHECK(lines[6].find("LOGDROP:6") != std::string::npos);  // erster Eintrag nach der Lücke trägt die Zahl
    CHECK(lines[2].find("LOGDROP") == std::string::npos);
    CHECK(lines[5].find("LOGDROP") == std::string::npos);
}

TEST_CASE("Logger: LOGDROP wird an vorhandene Ereignisse mit ';' angehängt") {
    hal::MemoryLogSink sink;
    Logger<1> log("stacker", "b");
    REQUIRE(log.open(sink));
    (void)log.record(record_at(0.0));
    (void)log.record(record_at(0.02));  // verloren
    log.drain(sink);
    LogRecord r = record_at(0.04);
    r.set_text(r.event, "BLOCKED");
    REQUIRE(log.record(r));
    log.drain(sink);
    CHECK(lines_of(sink.contents()).back().find(",BLOCKED;LOGDROP:1") != std::string::npos);
}

TEST_CASE("Logger: Sink schreibt nicht (Karte fehlt) -> open meldet es, drain verliert nichts im Speicher") {
    hal::MemoryLogSink sink;
    sink.set_fail_writes(true);
    Logger<4> log("stacker", "b");
    CHECK_FALSE(log.open(sink));
    CHECK_FALSE(log.is_open());
    CHECK_FALSE(log.record(record_at(0.0)));  // ohne geöffnete Datei wird nichts gesammelt
    CHECK(log.dropped() == 0);                // ... und das zählt nicht als Verlust
    sink.set_fail_writes(false);
    CHECK(log.open(sink));
    CHECK(log.record(record_at(0.0)));
}

TEST_CASE("Logger: Schreibfehler im Betrieb -> Zeile geht verloren und wird gezählt, Logger macht weiter") {
    hal::MemoryLogSink sink;
    Logger<8> log("stacker", "b");
    REQUIRE(log.open(sink));
    REQUIRE(log.record(record_at(0.0)));
    sink.set_fail_writes(true);
    log.drain(sink);
    CHECK(log.write_errors() == 1);
    sink.set_fail_writes(false);
    REQUIRE(log.record(record_at(0.02)));
    log.drain(sink);
    CHECK(sink.contents().find("0.020,") != std::string::npos);
}

TEST_CASE("Logger: 60-s-Lauf, alle 20 ms eine Zeile, Verbraucher alle 50 ms -> 3000 lesbare Zeilen") {
    hal::MemoryLogSink sink;
    Logger<64> log("runner", "deadbee");
    REQUIRE(log.open(sink));
    int steps = 0;
    for (int ms = 0; ms < 60000; ms += 10) {  // Steuer-Task: 10 ms Takt, Logzeile jeden zweiten
        if (LogCadence::due(ms / 10))
            REQUIRE(log.record(record_at(ms / 1000.0)));
        if (ms % 50 == 0)
            log.drain(sink);
        ++steps;
    }
    log.drain(sink);
    CHECK(steps == 6000);
    CHECK(log.dropped() == 0);
    const auto lines = lines_of(sink.contents());
    REQUIRE(lines.size() == 2 + 3000);
    CHECK(lines[0].rfind("# frostbyte-log v1 robot=runner source=robot seed=- build=deadbee", 0) == 0);
    CHECK(lines[1] == logfmt::columns);
    int columns = 1;
    for (const char c : std::string(logfmt::columns))
        columns += (c == ',');
    for (std::size_t i = 2; i < lines.size(); ++i) {
        int commas = 0;
        for (const char c : lines[i])
            commas += (c == ',');
        REQUIRE(commas + 1 == columns);
    }
    // Handprüfung: FB_LOGGER_DUMP=<datei> schreibt den Lauf für tools/log_plot.py und tools/sim_report.py.
    if (const char* dump = std::getenv("FB_LOGGER_DUMP")) {
        if (std::FILE* f = std::fopen(dump, "wb")) {
            std::fwrite(sink.contents().data(), 1, sink.contents().size(), f);
            std::fclose(f);
        }
    }
    CHECK(lines[2].rfind("0.000,runner,", 0) == 0);
    CHECK(lines.back().rfind("59.980,runner,", 0) == 0);
}

TEST_CASE("LogCadence: jeder zweite 10-ms-Takt (20 ms, docs/log_format.md)") {
    CHECK(LogCadence::due(0));
    CHECK_FALSE(LogCadence::due(1));
    CHECK(LogCadence::due(2));
    CHECK(LogCadence::period_ms == 20);
}
