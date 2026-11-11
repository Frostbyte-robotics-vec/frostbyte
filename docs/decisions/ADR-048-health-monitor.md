# ADR-048: Health-Monitor

Status: angenommen (11. Nov 2026), Ticket F-06.

## Entscheidung
- **Reine Logik in `lib/util/health_monitor.hpp`:** `HealthMonitor<MaxMotors>` prüft Motoren (`hal::Motor::connected()`, `temperature_c()`), Akkuspannung und die Takt-Warnung des Steuer-Tasks (`CycleStats::warning()`, 3 Überzüge in Folge, jetzt auch in `SharedCycleStats` veröffentlicht). Er meldet nur; Reaktionen (Strom senken, Subsystem auf FEHLER) gehören den Subsystemen.
- **Wichtigste Warnung gewinnt:** Motor getrennt > Motor heiß > Akku niedrig > Takt. Dazu der erste betroffene Motor.
- **Entprellung:** ein Motor gilt nach `lost_polls = 2` Messungen in Folge als getrennt. Bei 100 ms Takt ist die Warnung nach höchstens 200 ms da (Test: jeder Zeitpunkt zwischen zwei Messungen). Wieder angeschlossen: Warnung sofort weg.
- **Hysterese** bei Temperatur (55 °C an, 50 °C aus) und Akku (11,0 V an, 11,5 V aus). Temperatur eines getrennten Motors zählt nicht. Unbekannte Akkuspannung (NaN) wird nicht bewertet.
- **Task:** Health-Task alle 100 ms, `TASK_PRIORITY_DEFAULT`, nur lesen und rechnen. Übergabe an die anderen Tasks über `SharedHealth` (Atomics, wie `SharedCycleStats`). Der Controller zeigt den Text anstelle des Robotername in Zeile 0 (`AUS Lift`, `HEISS Lift`, `AKKU NIEDRIG`, `TAKT`).
- **Motoren eintragen:** `add_motor()` beim Start, sobald es HAL-Instanzen mit Ports gibt (F-09, M-12, M-13). Bis dahin prüft der Roboter nur Akku und Takt.
- **Log-Token** (`HEALTH:MOTOR_LOST:<name>` usw., `event_token()`) sind vorbereitet; der Steuer-Task schreibt sie, sobald der Logger (F-05) gemergt ist. Neue Token ändern das Log-Format nicht.
- Schwellen sind Startwerte (Software-Entscheidung), keine Messung: TODO(HW).

## Offen am Roboter
Motor-Drosselschwelle und Akkuverhalten unter Last, Kabel ziehen am echten Roboter (Warnung ≤ 200 ms), Lesbarkeit der Controller-Zeile.
