# ADR-048: Health-Monitor

Status: angenommen (11. Nov 2026), Ticket F-06.

## Entscheidung
- **Reine Logik in `lib/util/health_monitor.hpp`:** `HealthMonitor<MaxMotors>` prüft Motoren (`hal::Motor::connected()`, `temperature_c()`), Akkuspannung und die Takt-Warnung des Steuer-Tasks (`CycleStats::warning()`, 3 Überzüge in Folge, jetzt auch in `SharedCycleStats` veröffentlicht). Er meldet nur; Reaktionen (Strom senken, Subsystem auf FEHLER) gehören den Subsystemen.
- **Wichtigste Warnung gewinnt:** Motor getrennt > Motor heiß > Akku niedrig > Takt. Dazu der erste betroffene Motor.
- **Entprellung:** ein Motor gilt nach `lost_polls = 2` Messungen in Folge als getrennt. Der Monitor allein braucht bei 100 ms Takt schon die ganzen 200 ms; bis zum Text am Controller kämen die Phase der opcontrol-Schleife und die 100-ms-Sperre von ControllerFeedback dazu (gemessen im Test mit Health-Task, Schleife und Controller zusammen: 295 ms). Darum Health-Task **50 ms** und opcontrol-Schleife **50 ms** (plan.md nennt 100 ms für den Health-Task); der Ende-zu-Ende-Test belegt ≤ 200 ms für alle Phasen beider Tasks. Wieder angeschlossen: Warnung sofort weg.
- **Hysterese** bei Temperatur (55 °C an, 50 °C aus) und Akku (11,0 V an, 11,5 V aus). Temperatur eines getrennten Motors zählt nicht. Unbekannte Akkuspannung (NaN) wird nicht bewertet.
- **Task:** Health-Task alle 50 ms, `TASK_PRIORITY_DEFAULT`, nur lesen und rechnen. Übergabe an die anderen Tasks über `SharedHealth` (Atomics, wie `SharedCycleStats`). Der Controller zeigt den Text anstelle des Robotername in Zeile 0 (`AUS Lift`, `HEISS Lift`, `AKKU NIEDRIG`, `TAKT`).
- **Motoren eintragen:** `add_motor()` beim Start, sobald es HAL-Instanzen mit Ports gibt (F-09, M-12, M-13). Bis dahin prüft der Roboter nur Akku und Takt.
- **Log-Token** (`HEALTH:MOTOR_LOST:<name>` usw., `event_token()`) sind vorbereitet; der Steuer-Task schreibt sie, sobald der Logger (F-05) gemergt ist. Neue Token ändern das Log-Format nicht.
- Schwellen stehen in `config::health` (config/stacker.hpp, config/runner.hpp), Startwerte (Software-Entscheidung), keine Messung: TODO(HW). Tests setzen eigene Werte.
- Motornamen kurz halten (Controllerzeile: 15 Zeichen, davon 4 für die Restzeit).

## Offen am Roboter
Motor-Drosselschwelle und Akkuverhalten unter Last, Kabel ziehen am echten Roboter (Warnung ≤ 200 ms), Lesbarkeit der Controller-Zeile.
