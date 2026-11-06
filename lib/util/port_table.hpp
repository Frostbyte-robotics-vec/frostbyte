#pragma once

#include <cstdlib>

#include "util/port_check.hpp"

// Port-Tabelle (F-07): Pruefungen fuer die Eintraege in config/ (PortExpect, gleiche Struktur wie bei port_check). Die Werte selbst
// stehen nur in config/stacker.hpp und config/runner.hpp, die Begruendung in docs/ports.md. Ein Port, der noch nicht
// feststeht (weder CAD noch Bench-Test), bleibt unassigned_port und ist dort als TODO(HW) markiert; geraten wird nichts.
namespace frostbyte {

inline constexpr int unassigned_port = 0;
inline constexpr int max_port = PortCheck::port_count;

constexpr int port_abs(int port) { return port < 0 ? -port : port; }

// Gueltig = ein echter V5-Port 1..21 (Vorzeichen = Motor umgekehrt, zaehlt nicht mit).
constexpr bool port_valid(int port) { return port_abs(port) >= 1 && port_abs(port) <= max_port; }

enum class PortIssue : unsigned char { ok, out_of_range, duplicate };

// Offene Ports (unassigned_port) sind erlaubt und koennen mehrfach vorkommen; alle gesetzten Ports muessen in 1..21 liegen und
// duerfen je Roboter nur einmal vorkommen (auch mit umgekehrtem Vorzeichen).
constexpr PortIssue port_table_check(const PortExpect* table, int count) {
    for (int i = 0; i < count; ++i) {
        const int p = port_abs(table[i].port);
        if (p == unassigned_port)
            continue;
        if (!port_valid(p))
            return PortIssue::out_of_range;
        for (int j = i + 1; j < count; ++j)
            if (port_abs(table[j].port) == p)
                return PortIssue::duplicate;
    }
    return PortIssue::ok;
}

constexpr int unassigned_count(const PortExpect* table, int count) {
    int n = 0;
    for (int i = 0; i < count; ++i)
        n += table[i].port == unassigned_port ? 1 : 0;
    return n;
}

// Fuer Code, der einen Port wirklich benutzt: gibt ihn unveraendert zurueck, solange er gueltig ist. Ein offener oder ungueltiger Port
// bricht zur Compile-Zeit ab (kein Konstantenausdruck), sonst beim Start mit abort(); so faehrt kein Motor auf einem erfundenen Port.
// Nur beim Aufbau der Geraete aufrufen, nicht im Steuer-Task.
[[noreturn]] inline void port_unassigned_abort() { std::abort(); }
constexpr int checked_port(int port) {
    if (!port_valid(port))
        port_unassigned_abort();
    return port;
}

}  // namespace frostbyte
