# ADR-014: Fahrer-Belegung als Absichten (Intents)

Status: angenommen (1. Nov 2026), Ticket C-03. Der Advisor (Opus) hat die Platzierung und die Aufteilung vorgegeben.

## Entscheidung
- `lib/match/driver_map.hpp` (Schicht 2) bildet die Controller-Eingabe auf `DriverIntent` ab: ein `DriveRequest` plus Aktionen (Greifen, Setzen, Cup drehen, Endgame, Toggle setzen, Assistenz an/aus, Langsam, Hold). Sie ruft keine Subsysteme auf. Die Makros setzen die Tickets der Subsysteme um (M-01, M-03, M-04, …).
- Die Controller-Eingabe (`ControllerInput`) ist Teil des `Snapshot` (`lib/core/snapshot.hpp`), kein zweiter Eingabetyp.
- Die Belegung (Aktion → Taste) und die Fahrerprofile liegen pro Roboter in `config/stacker.hpp` und `config/runner.hpp`, jetzt in `config::stacker` bzw. `config::runner`, damit Tests beide einbinden können. `config/robot.hpp` übernimmt den gewählten Roboter nach `config::`.
- Standard-Layout (V5): linker Stick vor/zurück, rechter Stick drehen; R1 Greifen, R2 Setzen, L1 Cup drehen, L2 Langsam (gehalten), B Hold (gehalten), A Toggle setzen, X Assistenz an/aus, Y Endgame (nur Stacker). Das ist Entwurf, keine Hardware-Tatsache; der Fahrer bestätigt ihn am Roboter.
- Manuelle Stick-Eingabe außerhalb der Totzone setzt `manual_drive`; Fahr-Makros müssen dann abbrechen. Hold gilt nur ohne Stick-Eingabe.
- Assistenz startet immer aus und wird nur per Taste umgeschaltet.
- Endgame-Vibration: `endgame_rumble(vorher_s, jetzt_s)` ist true im Zyklus, in dem die Restzeit 15 s oder 10 s erreicht oder überspringt.
- Export: `frostbyte_controls_export docs/controls.md` erzeugt die Tabelle aus denselben Daten. Ein Test vergleicht die eingecheckte Datei mit der Ausgabe, damit sie nicht veraltet.

## Offen
- Hat der Runner Lift/Endgame? Bis zur Klärung hat er keine Endgame-Taste.
- Anbindung an Controller und Motoren (src/) folgt; bis dahin nutzt nichts in `src/` den `DriverMap`. Die Header kompilieren für ARM, weil `config/robot.hpp` sie einbindet; ihr Verhalten ist nur auf dem Laptop getestet.
