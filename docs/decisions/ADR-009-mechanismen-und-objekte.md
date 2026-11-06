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

## Nachtrag SIM-29 (5. Nov 2026): Lift im Match
- **Bestand:** Das Log hat `lift_deg` und `lift_set_deg` seit v1, der Viewer (`tools/viewer/index.html`) zeichnet den Lift schon aus `lift_deg` (Bereich aus `robots.js`, gleich den Grenzen in `sim_params.json`). Es fehlte nur der Schreiber in `run_match`. **Kein neues Spaltenformat, keine neue Versionsnummer.**
- `Game` hält je Roboter ein `Joint` für den Lift (Grenzen aus `roboter.<name>.mechanismen.lift`, Start auf der unteren Endlage). `ObjectModel::place_unit` meldet in `CycleResult::lift_start_s`, wann der Hub beginnt (nach Aufnahme und Schließen, `< 0` = kein Hub, z. B. bei Greiffehler). Es gibt keine neuen Zufallsziehungen: Dauer, Ergebnis und Punktestand bleiben gleich, die Posen auch.
- `Game::launch` legt zwei Sollwerte ab: Hub zur Ebenenhöhe bei Beginn des Hubs, Rückfahrt nach unten bei Zyklusende (auch bei Setzfehler). `Game::step` setzt sie zeitgenau und schaltet das Gelenk um den echten Zeitschritt weiter (gleiches Trapezprofil wie `time_to_reach`). `cancel` löscht die Sollwerte und setzt den Sollwert sofort nach unten.
- `run_match` schreibt `lift_deg` und `lift_set_deg` nur mit Spielzustand (wie `score`, `has_cup`); ohne bleiben sie leer (nichts erfunden). Arm und Greifer bleiben leer (noch nicht gefahren).
- **Näherung:** Die Dauer des Hubs wird beim Start des Zyklus vom unteren Anschlag aus berechnet (`time_to_reach`). Beginnt ein Zyklus, solange der Lift noch zurückfährt, weicht der Ist-Winkel kurz vom Plan ab (Gelenk fährt vom echten Winkel); die Zyklusdauer ändert das nicht.
- Ebene 0 = untere Endlage: der erste Stein je Goal bewegt den Lift nicht.
- SG3 (Höhe) wird in der Simulation weiter nicht geprüft: die Höhe ist nicht modelliert (Geometrie aus M-09/M-11, Winkel-Höhen-Tabelle gesperrt bis gemessen). Der Lift bleibt nur durch das Gelenk in seinen Endlagen. Die Prüfung gehört zu einem eigenen Ticket nach der Messung.

## Nachtrag SIM-32 (6. Nov 2026): SG3 im Match
- `roboter.<name>.mechanismen.lift.hoehe` (optional): 5 Stützpunkte `winkel_deg` → `hoehe_in` (Oberkante des Roboters), streng steigend, monotone Interpolation wie am Roboter (`lib/util/monotone_table.hpp`). Fehlt der Block, ist die Höhe nicht modelliert und SG3 wird für diesen Roboter nicht geprüft (nichts erfunden). Unplausible Werte: `std::runtime_error` beim Laden.
- `run_match` prüft in jedem Simulationsschritt die Höhe (`Game::lift_height_in`) gegen `check_rules`; `RULE:SG3` steht einmal je Überschreitung im Log (steigende Flanke), `rule_violations` zählt sie. Genau 50″ ist erlaubt.
- **Winkelbezug:** `winkel_deg` ist der absolute Gelenkwinkel der Sim (gleicher Bezug wie `min_deg`/`max_deg`), NICHT der Winkel relativ zu Home wie `cal_angle_deg` in config/ und `Lift::height_for_angle`. Ist Home die untere Endlage, gilt absolut = relativ + `min_deg`; ob Home dort liegt, ist offen (TODO(HW), M-01/M-11). Gemessene Kalibrierpunkte also erst umrechnen.
- **Platzhalter:** Die Tabellen in `sim_params.json` sind Schätzungen (`quelle: schaetzung`, grob linear, bleiben unter 50″), bis M-11 misst. Die Tests nehmen eigene Tabellen und hängen nicht von ihnen ab. Wenn die Messwerte da sind, nur die Zahlen in `sim_params.json` ersetzen.


## Am Roboter zu prüfen (SIM-29)
- Lift-Grenzen und -Geschwindigkeiten sind Schätzungen (M-01/M-11), ebenso die Ebenenhöhen als lineare Teilung von min..max.
