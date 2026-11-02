# ADR-016: GPS-Fusion mit Güte- und Tempo-Grenze

Status: angenommen (2. Nov 2026), Ticket D-04.

## Entscheidung
- `lib/estimation/gps_fusion.hpp`, Klasse `GpsFusion`, reine Logik ohne PROS und ohne `config/`. Eingabe je Takt: Odometrie-`Pose`, `GpsReading` (gültig, x, y, Heading, gemeldete Güte in Zoll) und das Tempo aus der Odometrie (Zoll/s).
- Die Odometrie bleibt unverändert. Die Fusion hält nur einen Versatz (x, y, Heading) und gibt `Pose` = Odometrie + Versatz aus. Ein `reset()` der Odometrie bekommt ein `reset()` der Fusion dazu.
- Eine Messung zählt nur, wenn: gültig und endlich, gemeldete Güte ≤ `max_error_in`, Tempo ≤ `max_speed_in_s`, Abstand zur aktuellen fusionierten Pose ≤ `gate_in`. Sonst wird sie verworfen; `last_result()` sagt warum (`no_data`, `bad_quality`, `too_fast`, `outlier`).
- Angenommene Messung: Versatz += Gewinn × Abweichung (`position_gain`, `heading_gain`). Heading aus dem GPS ist aus, solange `heading_gain` = 0; der Wrap bei ±180° ist berücksichtigt.
- Das Tempo kommt aus der Odometrie, nicht aus der Wahrheit (so sieht es auch der Roboter).

## Folgen
- Die Schwellen und Gewinne sind Platzhalter für die Tests; echte Werte stehen später in `config/` (TODO(HW)) und werden an echten GPS-Logs abgeleitet.
- Der Ausreißertor-Wert muss größer sein als der Odometrie-Fehler, der sich zwischen zwei angenommenen Messungen aufbaut, sonst wird nie mehr korrigiert (bei Dauer-Ablehnung fehlt ein Rückfall; Beobachtung in den Logs).
- Offen: GPS-Latenz (Messung zeigt die Pose von vor einigen Takten); `max_speed_in_s` fängt das nur grob ab.
