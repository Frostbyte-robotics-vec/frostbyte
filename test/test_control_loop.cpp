#include <cstdint>
#include <limits>

#include "core/control_loop.hpp"
#include "doctest/doctest.h"

using namespace frostbyte;

TEST_CASE("PhaseFlag: Start ist DISABLED, Callbacks setzen nur die Phase") {
    PhaseFlag p;
    CHECK(p.get() == Phase::Disabled);
    p.set(Phase::Autonomous);
    CHECK(p.get() == Phase::Autonomous);
    p.set(Phase::Driver);
    CHECK(p.get() == Phase::Driver);
}

TEST_CASE("next_wake_ms: festes Raster ab Ursprung, haengt nicht von der Rechenzeit ab") {
    CHECK(next_wake_ms(5000, 5004, 10) == 5010);
    CHECK(next_wake_ms(5000, 5014, 10) == 5020);
    CHECK(next_wake_ms(5000, 5000, 10) == 5010);
}

TEST_CASE("next_wake_ms: ueberzogene Takte werden uebersprungen, nicht nachgeholt") {
    CHECK(next_wake_ms(0, 25, 10) == 30);
}

TEST_CASE("next_wake_ms: Ende exakt auf der Rastergrenze ergibt den folgenden Takt, keinen bereits faelligen") {
    CHECK(next_wake_ms(0, 10, 10) == 20);
    CHECK(next_wake_ms(0, 9, 10) == 10);
}

TEST_CASE("next_wake_ms: Ueberlauf von millis() bricht das Raster nicht") {
    const std::uint32_t max = std::numeric_limits<std::uint32_t>::max();
    const std::uint32_t origin = max - 14;  // Raster ... max-4, max+6 (= 5) ...
    CHECK(next_wake_ms(origin, max - 10, 10) == max - 4);
    CHECK(next_wake_ms(origin, max - 2, 10) == 5u);
    CHECK(next_wake_ms(origin, 3, 10) == 5u);
}

TEST_CASE("delay_until_base_ms: task_delay_until(base, Periode) weckt genau zur Rasterzeit") {
    CHECK(delay_until_base_ms(5010, 10) == 5000);
    CHECK(delay_until_base_ms(10, 10) == 0u);
    // Unter der Periode laeuft die Basis ueber; delay_until addiert die Periode wieder dazu.
    const std::uint32_t base = delay_until_base_ms(5, 10);
    CHECK(base == std::numeric_limits<std::uint32_t>::max() - 4);
    CHECK(static_cast<std::uint32_t>(base + 10u) == 5u);
}

TEST_CASE("CycleStats: Ueberzug zaehlt, Folge-Zaehler setzt bei gutem Zyklus zurueck") {
    CycleStats s(10000);
    s.record_work_us(12000);
    CHECK(s.overruns() == 1);
    CHECK(s.consecutive_overruns() == 1);
    s.record_work_us(13000);
    CHECK(s.consecutive_overruns() == 2);
    s.record_work_us(1000);
    CHECK(s.consecutive_overruns() == 0);
    CHECK(s.overruns() == 2);
    CHECK(s.work_max_us() == 13000u);
    CHECK_FALSE(s.warning());
}

TEST_CASE("CycleStats: Warnung ab 3 Ueberzuegen in Folge") {
    CycleStats s(10000);
    for (int i = 0; i < 3; ++i) s.record_work_us(15000);
    CHECK(s.warning());
}

TEST_CASE("CycleStats: reset() loescht Maxima und Ueberzuege, die laufende Folge bleibt") {
    CycleStats s(10000);
    s.record_jitter_us(4000);
    s.record_work_us(12000);
    s.record_work_us(12000);
    s.reset();
    CHECK(s.jitter_max_us() == 0u);
    CHECK(s.work_max_us() == 0u);
    CHECK(s.overruns() == 0);
    // Die Folge beschreibt den aktuellen Zustand des Tasks, keine Statistik: eine dritte Ueberziehung warnt weiter.
    CHECK(s.consecutive_overruns() == 2);
    s.record_work_us(12000);
    CHECK(s.warning());
}

TEST_CASE("ControlLoop: erster Takt zur Startzeit, danach festes Raster ohne Drift") {
    ControlLoop loop(10, 5000);
    CHECK(loop.wake_ms() == 5000u);
    loop.begin(1'000'000);
    loop.end(5004, 1'004'000);  // 4 ms Rechenzeit verschieben das Raster nicht
    CHECK(loop.wake_ms() == 5010u);
    loop.begin(1'010'300);
    loop.end(5014, 1'014'300);
    CHECK(loop.wake_ms() == 5020u);
    CHECK(loop.stats().work_max_us() == 4000u);
    CHECK(loop.stats().jitter_max_us() == 300u);
}

TEST_CASE("ControlLoop: puenktlicher Takt ergibt Jitter 0, auch bei beliebigem Versatz zwischen millis() und micros()") {
    // micros() laeuft hier 123,456 ms vor millis(): der Versatz darf nicht im Jitter auftauchen.
    ControlLoop loop(10, 0);
    for (std::uint32_t i = 0; i < 100; ++i) {
        const std::uint32_t t_us = 123'456 + i * 10'000;
        loop.begin(t_us);
        loop.end(i * 10 + 2, t_us + 2000);
    }
    CHECK(loop.stats().jitter_max_us() == 0u);
    CHECK(loop.stats().work_max_us() == 2000u);
    CHECK(loop.stats().overruns() == 0);
}

TEST_CASE("ControlLoop: der erste Takt hat keinen Vorgaenger und geht nicht in den Jitter ein") {
    ControlLoop loop(10, 0);
    loop.begin(987'654);
    CHECK(loop.stats().jitter_max_us() == 0u);
}

TEST_CASE("ControlLoop: verspaetetes Wecken zaehlt voll als Jitter (wird nicht weggerundet)") {
    ControlLoop loop(10, 0);
    loop.begin(0);
    loop.end(1, 1000);
    loop.begin(10'800);  // 0,8 ms zu spaet
    loop.end(11, 11'800);
    CHECK(loop.stats().jitter_max_us() == 800u);
    loop.begin(26'000);  // fuer 20 ms angesetzt, erst bei 26 ms geweckt: Abstand 15,2 ms statt 10 ms
    CHECK(loop.stats().jitter_max_us() == 5200u);
}

TEST_CASE("ControlLoop: ein uebersprungener Takt erscheint nicht als Jitter") {
    ControlLoop loop(10, 0);
    loop.begin(0);
    loop.end(25, 25'000);  // Ueberzug: Takte 10 und 20 entfallen
    CHECK(loop.wake_ms() == 30u);
    CHECK(loop.stats().overruns() == 1);
    loop.begin(30'100);
    CHECK(loop.stats().jitter_max_us() == 100u);
}

TEST_CASE("ControlLoop: reset_stats() beim Phasenwechsel, danach zaehlen die Takte wieder normal") {
    ControlLoop loop(10, 0);
    loop.begin(0);
    loop.end(1, 1000);
    loop.begin(16'000);  // Ausreisser beim Start: 6 ms
    loop.end(17, 17'000);
    CHECK(loop.stats().jitter_max_us() == 6000u);
    loop.reset_stats();
    CHECK(loop.stats().jitter_max_us() == 0u);
    CHECK(loop.stats().work_max_us() == 0u);
    // Der Abstand zum Ausreisser (4,2 ms statt 10 ms) gehoert noch zur alten Phase: erster Takt danach zaehlt nicht.
    loop.begin(20'200);
    loop.end(21, 21'200);
    CHECK(loop.stats().jitter_max_us() == 0u);
    loop.begin(30'500);
    CHECK(loop.stats().jitter_max_us() == 300u);
}

TEST_CASE("ControlLoop: reset_stats() in der Reihenfolge des Steuer-Tasks (zwischen begin und end)") {
    ControlLoop loop(10, 0);
    loop.begin(0);
    loop.end(1, 1000);
    loop.begin(16'000);  // Phasenwechsel erkannt in diesem Takt, nach dem Ausreisser
    loop.reset_stats();
    loop.end(17, 17'000);
    CHECK(loop.stats().jitter_max_us() == 0u);
    CHECK(loop.stats().work_max_us() == 1000u);  // Rechenzeit des laufenden Takts zaehlt schon zur neuen Phase
    loop.begin(20'200);  // kein Vorgaenger: zaehlt nicht
    loop.end(21, 21'200);
    CHECK(loop.stats().jitter_max_us() == 0u);
    loop.begin(30'500);  // erst der uebernaechste Abstand zaehlt
    CHECK(loop.stats().jitter_max_us() == 300u);
}

TEST_CASE("SharedCycleStats: Anzeige-Task liest, was der Steuer-Task veroeffentlicht") {
    SharedCycleStats shared;
    CHECK(shared.load().jitter_max_us == 0u);
    CycleStats s(10000);
    s.record_jitter_us(750);
    s.record_work_us(11000);
    s.record_work_us(2500);
    shared.publish(s);
    const CycleStatsView v = shared.load();
    CHECK(v.jitter_max_us == 750u);
    CHECK(v.work_max_us == 11000u);
    CHECK(v.overruns == 1u);
}
