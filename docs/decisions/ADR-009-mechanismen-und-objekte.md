# ADR-009: Mechanismen als Gelenke, Objekte als Ereignisse

Status: angenommen (1. Nov 2026), Ticket SIM-04. docs/plan.md beschreibt SIM-04 nicht näher; die Schnittstelle ist hier festgelegt.

## Entscheidung
- **Gelenk** (`sim/mechanism.hpp/.cpp`, Klasse `Joint`): folgt einem Sollwert mit Geschwindigkeits- und Beschleunigungsgrenze (Trapezprofil, Bremsweg `sqrt(2·a·Rest)`), Sollwert wird auf die Endlagen begrenzt, am Ziel wird exakt eingerastet (kein Überschwingen). Die Bremskurve ist auf den Zeitschritt abgestimmt (v²/(2a) + v·dt/2 = Rest); beim Einrasten springt die Geschwindigkeit höchstens um 2·a·dt auf 0 (im Test geprüft, Schritte unterwegs höchstens a·dt). `max_acc = 0` heißt: Geschwindigkeit springt (so ist der Greifer modelliert).
- **Greifer** (`Claw`): Gelenk in Zoll von 0 bis `offen_in`; mit gegriffenem Pin schließt er nur bis `pin_in`.
- **Objekte** (`sim/objects.hpp/.cpp`, Klasse `ObjectModel`): keine Physik der Teile, sondern Ereignisse aus `ereignisse` in `sim_params.json`: Aufnahme von Loader/Boden (Dauer gleichverteilt mit Mittel `dauer_s` und Standardabweichung `sigma_s`, mindestens 10 % der Dauer; Fehlschlag), Ausrichtung (`pin_gelb_oben`, Wahrscheinlichkeit je Quelle), Greifen (`greifer_fasst`), Setzen (`erfolg_ebene0 - abzug_pro_ebene·Ebene`, begrenzt auf [0, 1]).
- **Zyklus** (`run_cycle`, bisher nur aus Tests aufgerufen, nicht aus `frostbyte_sim`): Aufnahme → Greifer schließt → Lift fährt zur Zielhöhe → Setzen. Zeiten addieren sich, ein Fehlschlag beendet den Zyklus an der Stelle.
- **Zufall:** `ObjectModel` hat einen eigenen `Rng` (Seed vom Aufrufer), je Aufruf eine feste Zahl Ziehungen. Die Dauer ist gleichverteilt statt normalverteilt, damit dieses Ticket nicht von der Normalverteilung aus SIM-03 abhängt; Mittel und Streuung stimmen trotzdem.
- **Fehlerquoten** werden in den Tests mit eigenen Werten (je Stufe 1–4 %, Zyklus rund 5 %, damit ±2 Prozentpunkte bei 1000 Versuchen mindestens 3 Sigma sind) über 1000 Versuche auf ±2 Prozentpunkte geprüft (Fertig-Kriterium), nicht gegen die Schätzwerte. Toleranzen sind explizite Vergleiche; doctest::Approx(x).epsilon(e) wäre bei kleinen Werten viel lockerer (siehe docs/notes.md).

## Folgen
- Nicht in `run_match` eingehängt: Lift/Arm-Sollwerte und Quellen kommen erst mit der Route (A-03) und dem Fahrmodell (SIM-02). Die CSV bleibt unverändert.
- Regeln des Spiels (Besitz max. 1 Pin + 1 Cup, SG6) prüft der RuleGuard (M-07), nicht dieses Modell; `run_cycle` modelliert nur einen Pin.
- Alle Werte sind Schätzungen; SIM-08 ersetzt die Ereigniswerte aus echten Logs.

## Am Roboter zu prüfen
- Dauer und Fehlschlagquote von Loader- und Bodenaufnahme, Greifererfolg, Setzerfolg je Ebene (Logs).
- Ob Lift- und Armgrenzen (Geschwindigkeit/Beschleunigung) zur gewählten Kassette passen (TODO(HW)).
