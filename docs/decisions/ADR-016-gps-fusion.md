# ADR-016: GPS-Fusion mit Güte- und Tempo-Grenze

Status: angenommen (2. Nov 2026), Ticket D-04.

## Entscheidung
- `lib/estimation/gps_fusion.hpp`, Klasse `GpsFusion`, reine Logik ohne PROS und ohne `config/`. Eingabe je Takt: Odometrie-`Pose`, `GpsReading` (gültig, x, y, Heading, gemeldete Güte in Zoll) und das Tempo aus der Odometrie (Zoll/s).
- Die Odometrie bleibt unverändert. Die Fusion hält nur einen Versatz (x, y, Heading) und gibt `Pose` = Odometrie + Versatz aus. Ein `reset()` der Odometrie bekommt ein `reset()` der Fusion dazu.
- Eine Messung zählt nur, wenn: gültig und endlich, gemeldete Güte ≤ `max_error_in`, Tempo ≤ `max_speed_in_s`, Abstand zur aktuellen fusionierten Pose ≤ `gate_in` (außer nach `resync_samples` Ausreißern in Folge, siehe unten). Sonst wird sie verworfen; `last_result()` sagt warum (`no_data`, `bad_quality`, `too_fast`, `outlier`).
- Angenommene Messung: Versatz += Gewinn × Abweichung (`position_gain`, `heading_gain`). Heading aus dem GPS ist aus, solange `heading_gain` = 0; der Wrap bei ±180° ist berücksichtigt.
- Das Tempo kommt aus der Odometrie, nicht aus der Wahrheit (so sieht es auch der Roboter).

## Folgen
- Die Schwellen und Gewinne sind Platzhalter für die Tests; echte Werte stehen später in `config/` (TODO(HW)) und werden an echten GPS-Logs abgeleitet.
- Ausreißer-Rückfall: gültige, gute, langsame Messungen außerhalb des Tors werden gezählt; ab `resync_samples` in Folge gilt das GPS wieder (Odometrie ist dann verrutscht, z. B. Schlupf nach Kollision) und zieht die Pose mit dem normalen Gewinn heran. Ein guter Wert innerhalb des Tors setzt den Zähler zurück. Ohne das würde ein einmaliger Fehler über `gate_in` das GPS für den Rest des Matches abschalten.
- Offen: GPS-Latenz (Messung zeigt die Pose von vor einigen Takten); `max_speed_in_s` fängt das nur grob ab.

## Nachtrag SIM-44 (2026-11-15): Fusion in der Simulation
- `sim/world.cpp` führt je Roboter die reine Odometrie (`odo_pose`) und gibt sie mit der GPS-Messung des Steuertakts an `GpsFusion::update`; Route und Regler sehen die fusionierte Pose (`est`), das Log zeigt sie in `x_in, y_in, h_deg`. Das Tempo für die Tempo-Grenze kommt aus der Odometrie (Weg je Takt), nicht aus der Wahrheit.
- Parameter: `roboter.<name>.sensoren.fusion` in `sim/sim_params.json` (`aktiv`, `max_guete_in`, `max_tempo_in_s`, `tor_in`, `gewinn_position`, `gewinn_heading`, `resync_messungen`), alle `schaetzung`. Fehlt der Block, ist die Fusion aus und das Log byte-gleich wie vorher (GPS wird dann nur im Logschritt gemessen). Mit Fusion wird das GPS in jedem Steuertakt gemessen; das Log zeigt dieselbe Messung.
- Tests (`test/test_sim_fusion.cpp`) benutzen eigene Fixture-Werte, nicht die Platzhalter.
- Messung (Skills-Routen, 20 Läufe, Seed 1–20, Rauschprofil realistic), Fusion aus → an: Odometriefehler am Ende (Median) Runner 7,63 → 0,09 in, Stacker 2,44 → 0,09 in. Punkte (Median der Teamsumme) 46 → 16; Kontakte Feldrand (Median je Lauf) Runner 11 → 13,5, Stacker 11 → 10; Kontakte Roboter je 1 → 0. Der Runner drückt mit ehrlicher Pose länger an der Wand beim Toggle.
- Der Punkteeinbruch ist **kein Fehler der Fusion**, sondern Folge der ehrlichen Pose: Der Runner-Schritt `toggle TOGGLE_RED_1` wird nur innerhalb von `toggle_reichweite_in` (12 in) der *wahren* Pose angenommen. Das Ziel von `goto TOGGLE_RED_1` liegt an der Wand (x = −71,5); der Roboter bleibt an der Wand bei y ≈ −7,7 hängen (Abstand zum Toggle 12,6 in, vorher 10,5 in, weil der Odometriefehler ihn zufällig näher an den Toggle geführt hatte). Der Toggle schlägt fehl, die Neutral-Goals zählen ohne Toggle nicht (Punktestand erst bei 52 s über MIDFIELD). Mit `--noise none` und Fusion ebenfalls 16. Die frühere Zahl 46 beruhte also auf einem Zufallstreffer der Odometrie; die Routen brauchen echte Anfahrposen (eigenes Ticket SIM-45).
- Folge (SIM-45): Anfahrposen `APPROACH_TOGGLE_*` in field.json (61 in von der Mitte, 10,5 in vom Toggle, Schätzung), der Runner fährt sie statt der Toggle-Punkte an der Wand an. Gleiche 20 Läufe mit Fusion: Runner-Toggles 20/20 ausgelöst, Punkte (Median der Teamsumme) 16 → 68,5, P10 61 (`target_p10 40` erfüllt).
