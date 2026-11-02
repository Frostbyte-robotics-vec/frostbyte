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
