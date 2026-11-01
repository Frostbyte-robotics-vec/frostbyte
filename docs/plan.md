# Codebasis – Bauplan für Claude Code

Nov 1, 2026 · @Máté

## Das Konzept in einfachen Worten

Ein einziges C++-Programm (PROS) läuft auf beiden Robotern. Es unterscheidet sich nur in einer Konfigurationsdatei pro Roboter und erledigt Driving Skills und Autonomous Skills mit genau denselben Bausteinen. Dieser Plan ersetzt für die Umsetzung den älteren [Plan Codebasis 2026/27](https://claude.ai/artifact/UFsd7XbvyNmU7RPbJmE8Ev): Dessen Grundsätze gelten weiter, hier kommen die konkreten Roboter (Rev. E), die Skills-Strategie und die Arbeitspakete für Claude Code dazu.

**Fünf Ideen tragen alles:**

1. **Ein Herzschlag.** Alle 10 ms liest das Programm einmal alle Sensoren, entscheidet, und schreibt einmal alle Motoren. Nichts wartet, nichts blockiert. Darum reagiert der Roboter immer, auch wenn ein Teil hängt.
2. **Jeder Mechanismus ist ein Zustandsautomat.** Der Greifer weiß zu jedem Zeitpunkt, ob er offen, beim Schließen oder geschlossen ist. Man gibt ihm Wünsche („schließen“), nicht Motorbefehle. Wie er das macht und wann er aufgibt, regelt er selbst.
3. **Ein Zyklus ist ein Auftrag.** Der Kern beider Roboter ist der Zyklus „Einheit aufnehmen → zum Goal → setzen“. Er ist einmal programmiert. Der Fahrer löst ihn per Taste aus, die Autonomie ruft ihn aus einer Liste auf.
4. **Autonomie ist Daten, nicht Code.** Eine Skills-Route ist eine Tabelle aus Schritten (fahre zu, nimm auf, setze, stelle Toggle). Sie lässt sich ändern, ohne C++ anzufassen, und vorher am Laptop simulieren.
5. **Hardware hinter Schnittstellen.** Die Logik kennt keine Ports. Am Laptop steckt statt des Roboters ein Simulator dahinter. So kann Claude Code fast alles testen, bevor der Roboter überhaupt fertig ist.

**Was das für euch bedeutet:** Ihr arbeitet fast nur an zwei Stellen, an der Konfiguration (Ports, Übersetzungen, Reglerwerte) und an den Routen. Der Rest ist Fundament, das einmal sauber gebaut wird.

## Hardware-Grundlage (Stand Rev. E)

Der Stacker hat 9 Motoren, der Runner 8. Die Port-Nummern unten sind ein Vorschlag für den ersten Aufbau und werden beim ersten Bench-Test am echten Roboter bestätigt (Ticket F-03). Alle Werte stehen nur in `config/stacker.hpp` und `config/runner.hpp`.

**Stacker (24″)**

| Port | Gerät | Aufgabe | Übersetzung / Hinweis |
| --- | --- | --- | --- |
| 1, 2 | Motor 11 W, grün | Antrieb links vorne, hinten | 84T → 60T, 4″-Räder, ≈ 280 U/min |
| 3, 4 | Motor 11 W, grün | Antrieb rechts vorne, hinten | wie links, umgekehrte Drehrichtung |
| 5, 6 | Motor 11 W | DR4B-Lift links, rechts | 12T→84T, 36T→84T = 16,3 : 1; −40° … +45° |
| 7 | Motor 11 W | Schwenkarm | 12T → 84T = 7 : 1; Greifpunkt Ruhe ≈ 97° |
| 8 | Motor 11 W | Greifer (Zahnstange) | Backenabstand Cup 1,29″, Pin 0,19″ |
| 9 | Motor 11 W | Aufnahme (Band + Kipprolle) | Kette, eine Drehrichtung = einziehen |
| 10 | Inertialsensor | Richtung | Zukauf |
| 11, 12 | Rotationssensor | Tracking-Räder längs, quer | 2,75″-Räder, Zukauf |
| 13 | Rotationssensor | Liftwinkel (absolut) | empfohlen, Zukauf |
| 14 | Optischer Sensor | Farbe im Köcher (Pin-Ausrichtung) | Zukauf |
| 15 | GPS-Sensor | Absolute Position am Feldrand | empfohlen, Zukauf (Strip hängt in Coding Skills, RSC4a) |
| 16 | Funkmodul | Controller; für den Roboter-Link (Stufe 2) vermutlich ein zweites Modul an Port 17 | VUR10b erlaubt Funk zwischen den Robotern |
| ADI A | Bumper | Einheit im Köcher |  |

**Runner (15″)**

| Port | Gerät | Aufgabe | Übersetzung / Hinweis |
| --- | --- | --- | --- |
| 1, 2 | Motor 11 W, grün | Antrieb links | 84T → 60T, 4″-Räder |
| 3, 4 | Motor 11 W, grün | Antrieb rechts | umgekehrte Drehrichtung |
| 5, 6 | Motor 11 W | Viergelenk-Lift links, rechts | 12T → 60T = 5 : 1; −20° … +60° |
| 7 | Motor 11 W | Greifer | wie Stacker |
| 8 | Motor 11 W | Aufnahme hinten | Zukauf (17. Motor) |
| 10 | Inertialsensor | Richtung | Zukauf |
| 13 | Rotationssensor | Liftwinkel | empfohlen, Zukauf |
| 15 | GPS-Sensor | Absolute Position | empfohlen, Zukauf |
| 16 | Funkmodul | Controller; Link-Modul an Port 17 wie beim Stacker |  |
| ADI A | Bumper | Einheit im Köcher |  |

Der Runner hat im Modell keine Tracking-Räder. Seine Position kommt aus Motorencodern, IMU und GPS. Reicht das in der Messung nicht (Ziel: ≤ 3 cm nach 30 s), bekommt er zwei Tracking-Räder wie der Stacker.

## Architektur

Fünf Schichten, jede ruft nur die darunter auf. Der Regelwächter prüft jeden Motorbefehl, bevor er die Hardware erreicht.

&#91;embedded content: Architektur · 5 Schichten + Querschnitt\]

**Der 10-ms-Takt, in dieser Reihenfolge, in einem einzigen Steuer-Task** (`delay_until`, kein driftender Takt):

1. Zeitstempel nehmen, verpasste Takte zählen.
2. Alle Sensoren und den Controller **einmal** lesen → unveränderlicher `Snapshot`.
3. Pose aktualisieren (Odometrie, alle 100 ms GPS-Korrektur, wenn die GPS-Güte gut ist).
4. Match-Ebene: Phase bestimmen, Fahrer-Eingaben oder nächsten Routinen-Schritt auswerten.
5. Aufträge weiterschalten (Zyklus, Queue).
6. Subsystem-Automaten weiterschalten → Sollwerte.
7. RuleGuard prüft und begrenzt die Sollwerte.
8. Regler rechnen, Motoren schreiben.
9. Snapshot + Sollwerte in den Log-Ringpuffer **kopieren**, nicht schreiben.

**Harte Regeln für den Steuer-Task:** kein `delay()` außer `delay_until` am Ende, kein `new` nach dem Start, keine Ausgabe auf Konsole oder Display, Budget ≤ 3 ms (Warnung ab 5 ms). Logger (50 ms), UI (100 ms) und Health (100 ms) laufen als eigene, niedrig priorisierte Tasks und tauschen Daten nur über Ringpuffer.

## Repository-Struktur und Build

Ein Git-Repo (`frostbyte`), zwei Roboter-Builds und ein Laptop-Build für Tests. Alles unter `lib/` und `hal/interface/` ist frei von PROS-Headern und kompiliert auch am Laptop.

```
frostbyte/
├─ CLAUDE.md                 Arbeitsregeln für Claude Code (Abschnitt „Übergabe“)
├─ project.pros, Makefile    PROS 4, C++20; ROBOT=stacker | runner
├─ src/
│  ├─ main.cpp               PROS-Einstieg: Callbacks setzen nur die Phase
│  └─ robot/stacker.cpp, runner.cpp   verdrahtet Config → Objekte
├─ lib/                      hardwarefrei, am Laptop testbar
│  ├─ core/                  Snapshot, Einheiten, Uhr, Ringpuffer, StateMachine<>
│  ├─ control/               Pid, TrapezProfil, MotionController (LemLib-Adapter)
│  ├─ localization/          Odometrie, GPS-Fusion, Wand-Reset
│  ├─ subsystems/            drive, lift, swing_arm, claw, intake, toggle
│  ├─ tasks/                 cycle_controller, command_queue, coordinator
│  ├─ match/                 phase, route_player, driver_map
│  ├─ safety/                rule_guard, health
│  └─ log/                   logger (Ringpuffer → CSV)
├─ hal/
│  ├─ interface/             IMotor, IRotation, IImu, IGps, IOptical, IDigitalIn, ILink, IController
│  ├─ pros/                  echte Implementierung auf dem Brain
│  └─ sim/                   Geräte-Attrappen, lesen/schreiben in die Simulationswelt
├─ sim/                      Simulationsplattform: main.cpp (frostbyte_sim), world/, sim_params.json
├─ config/                   common.hpp, stacker.hpp, runner.hpp (einzige Stelle mit Unterschieden)
├─ routes/                   skills_stacker.route, skills_runner.route, test_*.route
├─ field/                    field.json: Goals, Toggles, Loader, Midfield in Zoll
├─ test/                     CMake + doctest: Unit- und Simulationstests
├─ tools/                    route2cpp.py, sim_report.py, sysid.py, log_plot.py, viewer/index.html
├─ docs/                     plan.md, log_format.md, controls.md, ports.md, tuning.md, decisions/ (ADRs)
└─ .github/workflows/ci.yml  baut beide Roboter + führt Tests aus
```

**Befehle, die immer funktionieren müssen:**

| Zweck | Befehl |
| --- | --- |
| Stacker bauen und auf Slot 1 laden | `make ROBOT=stacker && pros upload --slot 1` |
| Runner bauen und auf Slot 1 laden | `make ROBOT=runner && pros upload --slot 1` |
| Alle Tests am Laptop | `cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test` |
| Skills-Lauf simulieren und als Bild ausgeben | `frostbyte_sim --stacker routes/skills_stacker.route --runner routes/skills_runner.route --runs 100 --out out/sim/ && python tools/sim_report.py out/sim/` |
| Log vom Roboter auswerten | `python tools/log_plot.py logs/<datei>.csv` |

**Bibliotheken:** PROS 4 (Kernel), LemLib für Odometrie und Pfadfolge, doctest für Tests. LemLib wird nur in `lib/control/motion_controller` benutzt und dort hinter einer eigenen Schnittstelle versteckt. So lässt es sich am Laptop durch eine Simulation ersetzen und später austauschen. Versionen werden beim Setup festgeschrieben (`project.pros`, `test/CMakeLists.txt`).

**Routen als Textdateien:** Eine Route ist eine Liste von Schritten (siehe „Autonomie“). `tools/route2cpp.py` übersetzt sie beim Build in eine C++-Tabelle. Im Trainingsmodus kann das Brain dieselbe Datei auch von der SD-Karte laden, damit eine Route ohne Neu-Kompilieren geändert werden kann.

## Module im Detail

Jedes Subsystem hat dieselben drei öffentlichen Funktionen. Mehr dürfen andere Module nicht aufrufen:

```cpp
class Claw : public Subsystem {
public:
  void request(ClawGoal goal);          // Wunsch: OPEN, CLOSE_CUP, CLOSE_PIN
  ClawState state() const;              // wo bin ich gerade, Fehler?
  void update(const Snapshot& s, Outputs& out);  // einmal pro 10-ms-Takt
};
```

| Modul | Zustände | Endet wenn | Timeout | Roboter |
| --- | --- | --- | --- | --- |
| `Drive` | MANUAL, FOLLOW\_PATH, TURN, HOLD | Pose im Toleranzfenster (2 cm, 2°) | pro Schritt aus der Route | beide |
| `Lift` | HOMING, MOVE, HOLD, FAULT | Winkel ± 1° und Geschwindigkeit ≈ 0 | 1,5 s | beide |
| `SwingArm` | PICK, SWING, FRONT, FAULT | Winkel ± 2° | 0,8 s | Stacker |
| `Claw` | OPEN, CLOSING, CLOSED, FAULT | Strom-Plateau (Objekt gefasst) oder Endlage | 0,5 s | beide |
| `Intake` | IDLE, RUN, JAM\_CLEAR, FULL | Bumper meldet Einheit im Köcher | 3 s, dann kurz rückwärts | beide |
| `Toggle` | (über Lift) Mitnehmer anfahren, Bogen fahren, wegziehen | Lift-Winkelfolge fertig | 1,5 s | Runner |

**Der Kern: `CycleController`** in `lib/tasks`. Er verknüpft die Subsysteme zum Zyklus „Einheit aufnehmen und setzen“. Fahrer und Autonomie benutzen ihn identisch. Der Fahrer löst Schritte per Taste aus, die Route über `cycle(goal, level)`.

&#91;embedded content: CycleController · 7 Zustände, Stacker (Runner ohne Schwenkarm)\]

**Setzhöhe:** `placeHeight(goal, level)` = Goal-Höhe (Alliance 3,25″, neutral 5,8″, Midfield 8,7″) + 6,5″ pro Ebene + Versatz aus der Konfiguration. Die Umrechnung Höhe → Liftwinkel steckt in `Lift::angleForHeight()` (Kinematik aus der Config, am Roboter mit 5 Messpunkten kalibriert).

**Pin-Ausrichtung:** Der optische Sensor im Köcher meldet, ob Gelb oben ist. Der Zyklus setzt trotzdem. Der Wert geht aber in den Punktezähler der Route und ins Log, damit ihr seht, wie oft Pins falsch herum ankommen.

## Autonomie

Im Autonomous-Skills-Lauf spielt jeder Roboter seine eigene Route ab. Eine Route ist eine Liste benannter Schritte auf benannten Feldpunkten, mit Timeout pro Schritt und einem festen Zeitpunkt, ab dem es ins Midfield geht.

**Koordinaten:** Ursprung in der Feldmitte, Zoll und Grad, 0° zeigt von der roten zur blauen Station. Alle Feldpunkte (Goals, Loader, Toggles, Midfield, Startfelder) stehen einmal in `field/field.json` und werden aus dem offiziellen STEP-Modell des Feldes vermessen (Ticket A-01). Routen benutzen nur Namen, nie Zahlen.

**Wo bin ich? (`lib/localization`)**

| Quelle | Stacker | Runner | Wofür |
| --- | --- | --- | --- |
| Tracking-Räder (Rotationssensoren) | 2 | – | Weg, sehr genau auf kurze Sicht |
| Antriebs-Encoder | Plausibilität | Weg | Ersatz, wenn ein Tracking-Rad ausfällt |
| Inertialsensor | ja | ja | Richtung, driftet langsam |
| GPS-Sensor am Feldrand-Strip | ja | ja | absolute Korrektur, nur bei guter Güte und langsamer Fahrt |

Die Fusion bleibt bewusst einfach: Odometrie läuft im 10-ms-Takt. Alle 100 ms zieht ein Komplementärfilter die Pose um einen kleinen Anteil (Startwert 10 %) zur GPS-Pose, aber nur wenn der GPS-Fehler unter 1″ liegt und der Roboter langsamer als 20 ″/s fährt. In Driving Skills hängt der GPS-Strip nicht zwingend. Dann läuft die Odometrie allein.

**Wie komme ich hin? (`lib/control/motion_controller`)** Drei Bewegungsarten reichen: `turnTo(heading)`, `moveTo(pose)` für kurze Wege mit Endrichtung und `follow(path)` für lange Wege. Intern ruft der Adapter LemLib auf. Jede Bewegung meldet `DONE`, `TIMEOUT` oder `BLOCKED` (Strom hoch, keine Bewegung über 300 ms).

**Eine Route als Textdatei (`routes/skills_stacker.route`):**

```
# Stacker, Autonomous Skills – Zoll, Grad, Namen aus field.json
start    START_STACKER
cycle    source=preload  goal=RED_ALLIANCE_1  level=auto
repeat 6 {
  goto   LOADER_RED_1     tol=1.5  timeout=3.0
  intake until=unit       timeout=2.5
  cycle  goal=NEUTRAL_R1  level=auto  timeout=6.0
}
at 52s   goto MIDFIELD_S  timeout=6.0
end      hold
```

Der `RoutePlayer` arbeitet die Schritte nacheinander ab. `parallel { … }` erlaubt Fahren und Heben gleichzeitig. Jeder Schritt kann `on_timeout=skip|retry|abort` haben (Standard: `skip`). `at 52s` ist ein Zeitwächter: Egal wo die Route steht, ab dieser Zeit fährt der Roboter ins Midfield.

**GoalBook:** Ein kleines gemeinsames Verzeichnis, wie viele Einheiten in welchem Goal stecken. `level=auto` liest daraus die nächste Ebene. Beide Roboter halten es über den Funk-Link synchron (Abschnitt „Zusammenspiel“), damit sie nicht in dasselbe Goal auf dieselbe Höhe setzen.

**Loader im Autonomous-Lauf:** Ihr dürft auch dann nachladen (VUG4). Der Ladeplan steht als Tabelle in `docs/loader_plan.md`: wann welcher Loader welche Einheit bekommt. Der Roboter wartet am Loader auf den Bumper, höchstens bis zum Timeout.

## Fahrerbedienung

Im Driving-Skills-Lauf steuert ein Fahrer einen Roboter. Gefahren wird von Hand, der Zyklus läuft per Taste. Beide Controller haben dieselbe Grundbelegung, damit die Fahrer tauschen können.

| Taste | Stacker | Runner |
| --- | --- | --- |
| Linker Stick | Vor / zurück (Arcade) | gleich |
| Rechter Stick X | Drehen | gleich |
| R1 | Nächster Zyklus-Schritt (aufnehmen → greifen → heben → setzen) | gleich |
| R2 | Abbrechen, sichere Lage | gleich |
| L1 / L2 | Zielebene + / − (Anzeige am Controller) | gleich |
| A | Aufnahme an / aus | gleich |
| B | Aufnahme rückwärts (Stau lösen) | gleich |
| X | – | Toggle stellen (Mitnehmer-Ablauf) |
| Y | Midfield-Modus: Lift ab, Hold | gleich |
| Pfeiltasten | Lift / Arm manuell fein (übersteuert den Automaten) | Lift fein |

**Assistenzen, alle abschaltbar:**

- **Fahrkurve** pro Fahrer in der Config: Totzone 5 %, Exponent 2, Langsam-Modus 50 % solange der Lift über 30″ steht (Kippschutz).
- **Am-Goal-Ausrichten:** Halten von R1 im Schritt AM GOAL dreht den Roboter auf die gespeicherte Anfahrrichtung des nächsten Goals.
- **Anzeige am Controller (3 Zeilen):** Restzeit, Zielebene, Zustand des Zyklus. Vibration bei 15 s und 10 s Restzeit.
- **Manuell schlägt Automat:** Jede Eingabe auf ein Subsystem übernimmt sofort; der Zyklus geht in BEREIT.

Die Belegung steht als Tabelle in `lib/match/driver_map` und wird von dort automatisch nach `docs/controls.md` exportiert, damit Code und Doku nie auseinanderlaufen.

## Zusammenspiel der zwei Roboter

Die Roboter teilen sich das Feld über feste Zuständigkeiten. Der Funk-Link zwischen ihnen ist eine Verbesserung, keine Voraussetzung: Fällt er aus, läuft jede Route trotzdem fehlerfrei weiter.

**Stufe 1 – feste Aufteilung (Pflicht, ab Phase 4):**

- Jedes Goal gehört genau einem Roboter. Vorschlag: Der Stacker baut die hohen Türme an den Goals nahe Loader 1, der Runner bedient Loader 2 und die übrigen Goals. Die Zuteilung steht in `field/field.json` (`owner`).
- Jeder Roboter hat Fahrzonen pro Zeitfenster. `frostbyte_sim` spielt beide Routen gleichzeitig ab und meldet jede Überschneidung der Roboter-Umrisse als Fehler.
- Der Runner stellt die Toggles. Im Autonomous-Lauf stehen sie in seiner Route vorne, im Driving-Lauf macht es sein Fahrer zuerst.

**Stufe 2 – Funk-Link (ab Phase 6):** VUR10b erlaubt Funk zwischen den Robotern über V5-Brain und V5-Funkmodul. PROS bietet dafür `pros::Link`. Dafür braucht jeder Roboter vermutlich ein zweites Funkmodul neben dem für den Controller. Das prüft Ticket L-01 am echten Gerät.

| Nachricht | Inhalt | Takt |
| --- | --- | --- |
| `Status` | Pose, Zyklus-Zustand, Restzeit | 10 Hz |
| `GoalBook` | Goal-ID, neue Ebene, Pin-Ausrichtung | bei jeder Änderung, mit Bestätigung |
| `Signal` | benannte Ereignisse, z. B. `toggles_done` | bei Ereignis |

In Routen: `signal toggles_done` und `wait_for runner.toggles_done timeout=4`. Kommt das Signal nicht, läuft die Route nach dem Timeout normal weiter. Der Link hat eine eigene Verbindungsanzeige am Brain-Display.

## Tests und Abnahme

Ein Ticket ist erst fertig, wenn sein Test am Laptop grün ist. Am Roboter wird nur noch geprüft, was der Laptop nicht kann: Ports, Drehrichtungen, Reglerwerte.

| Stufe | Wo | Was | Wann |
| --- | --- | --- | --- |
| 1 Unit-Test | Laptop, doctest | Regler, Profile, Odometrie-Mathematik, jeder Zustandsautomat inkl. Timeouts, RuleGuard, Route-Parser | bei jedem Commit, CI |
| 2 Simulation | Laptop, `hal/sim` | ganze Routen beider Roboter: Laufzeit ≤ 60 s, keine Kollision, keine Regelverletzung, Punkte geschätzt | bei jeder Routenänderung, CI |
| 3 Bench-Test | Roboter aufgebockt | `port_check`-Programm: jeder Motor dreht kurz, jeder Sensor zeigt Werte, Ausfall durch Kabelziehen | nach jedem Umbau |
| 4 Feldtest | Feld | 10 Läufe je Route mit Log; Odometrie-Fehler, Zykluszeit, Punkte | wöchentlich |

**Der Simulator (`hal/sim`)** muss nicht realistisch sein, nur ehrlich: Differenzialantrieb mit Massenträgheit und Motorkennlinie, Mechanismen als Gelenke mit Geschwindigkeitsgrenze, Objekte als „da / nicht da“. Er läuft schneller als Echtzeit, damit 100 Läufe pro Route in Sekunden durchgehen. Optional mit Zufallsfehlern (Schlupf, verpasste Einheit), um die Timeouts zu prüfen.

**Logging:** Jeder Lauf schreibt eine CSV auf die SD-Karte: Zeit, Pose (Odometrie und GPS), Soll/Ist aller Mechanismen, Zyklus-Zustände, Strom, Temperatur, Zykluszeit. `tools/log_plot.py` zeichnet daraus Fahrweg und Zeitachse.

**Messbare Ziele, an denen die Software abgenommen wird:**

| Kennzahl | Ziel |
| --- | --- |
| Steuer-Takt | 10 ms, Rechenzeit ≤ 3 ms |
| Taste → Motor | ≤ 20 ms |
| Positionsfehler nach 30 s Autonomie | ≤ 2 cm Stacker, ≤ 3 cm Runner |
| Zykluszeit (Loader → Goal → Loader) | ≤ 6 s, Ziel 4 s |
| Erfolgsquote Zyklus | ≥ 95 % über 50 Versuche |
| Abstürze oder Hänger | 0 in 50 Läufen |

## Simulationsplattform

Die Simulation führt denselben C++-Code wie das Brain aus und schreibt dasselbe Log wie der echte Roboter. Damit wird jede Route am Laptop 100-mal gefahren, bevor sie aufs Feld kommt, und jeder echte Lauf lässt sich neben die Simulation legen. Sie prüft Software und Strategie. Mechanik (hält der Greifer? zieht das Band?) bleibt Sache des echten Roboters.

&#91;embedded content: Simulationsplattform · Datenfluss\]

**Bausteine**

| Baustein | Ort | Aufgabe |
| --- | --- | --- |
| `frostbyte_sim` | `sim/main.cpp` | Kommandozeilenprogramm: lädt Config, Routen, Feld, führt N Läufe aus |
| Welt | `sim/world/` | Feld, beide Roboter, Objekte, Zeit; feste Schrittweite 1 ms, Steuerung alle 10 ms |
| Geräte-Attrappen | `hal/sim/` | IMotor, IRotation, IImu, IGps … lesen und schreiben in die Welt |
| Modellwerte | `sim/sim_params.json` | alle Zahlen der Modelle, pro Roboter; nichts davon im Code |
| Bericht | `tools/sim_report.py` | JSON → HTML: Punkteverteilung, Fehler pro Zustand, Zeitachse |
| Viewer | `tools/viewer/index.html` | spielt CSV-Logs im Browser ab, 3D-Modell aus dem Liftmodell |
| Systemidentifikation | `tools/sysid.py` | passt das Fahrmodell an Messfahrten an, schreibt `sim_params.json` |

**Aufruf:**

```
frostbyte_sim --stacker routes/skills_stacker.route --runner routes/skills_runner.route \
              --runs 100 --seed 1 --noise realistic --out out/sim/
```

Ergebnis: `out/sim/run_001.csv` … `run_100.csv`, `summary.json`, Exit-Code 0 nur wenn alle Abnahmeregeln (unten) erfüllt sind. CI ruft genau diesen Befehl mit `--runs 20` auf.

**Fahrmodell (pro Seite).** Jede Antriebsseite ist ein Motor mit Feed-Forward-Kennlinie. Die drei Parameter kommen erst aus dem Datenblatt, später aus der Systemidentifikation:

```latex
U = k_S\,\operatorname{sign}(v) + k_V\,v + k_A\,\dot v
```

- U ist die Motorspannung in Volt, v die Bahngeschwindigkeit der Seite in Zoll pro Sekunde.
- kS ist die Spannung gegen Haftreibung, kV die für konstantes Tempo, kA die für Beschleunigung. Aufgelöst nach der Beschleunigung ergibt sich die Bewegungsgleichung, die die Welt integriert.
- Aus den Seiten-Geschwindigkeiten vL und vR folgen Vorwärtstempo (vL + vR)/2 und Drehrate (vR − vL)/b, mit b als wirksamer Spurbreite (aus der Systemidentifikation, nicht aus der CAD-Spurbreite).
- Schlupf: Beschleunigung begrenzt auf μ · g mit μ aus `sim_params.json` (Startwert 0,9).
- Spannung begrenzt auf 12 V. Unterhalb des Akkustands sinkt die Grenze linear mit der simulierten Akkuspannung.

**Sensormodelle**, alle per Seed reproduzierbar:

| Sensor | Modell | Startwerte |
| --- | --- | --- |
| Tracking-Rad / Encoder | wahrer Weg × (1 + Skalierungsfehler) + Rauschen | 0,5 %, σ = 0,01″ pro Takt |
| Inertialsensor | wahre Richtung + Drift + Rauschen | 1° pro Minute, σ = 0,05° |
| GPS | wahre Pose + Rauschen, Güte schlechter bei Tempo, zufällige Aussetzer | σ = 0,5″, 2 % Aussetzer |
| Bumper, optischer Sensor | Ereignis aus dem Objektmodell | – |

**Mechanismen und Objekte.** Lift, Schwenkarm und Greifer sind Gelenke mit Höchstgeschwindigkeit und Beschleunigungsgrenze aus den Übersetzungen der Config. Cups und Pins werden **nicht** physikalisch simuliert, sondern als Ereignisse mit Wahrscheinlichkeiten:

| Ereignis | Modell | Startwert |
| --- | --- | --- |
| Einheit aus Loader im Köcher | Dauer normalverteilt; Fehlschlag | 1,2 s ± 0,3 s; 5 % |
| Einheit vom Boden im Köcher | wie oben, länger | 1,8 s ± 0,5 s; 10 % |
| Pin mit Gelb oben | Wahrscheinlichkeit | Loader 95 %, Boden 50 % |
| Greifer fasst | Wahrscheinlichkeit | 98 % |
| Setzen gelingt | sinkt mit Ebene | 98 % − 1 % pro Ebene |

Die Startwerte sind Schätzungen. Ticket SIM-08 ersetzt sie durch Messwerte vom Roboter (Zählungen aus den Logs).

**Welt und Regeln.** Roboter-Umrisse als gedrehte Rechtecke inklusive ausgefahrener Teile. Geprüft wird jede Millisekunde: Roboter gegen Roboter, Roboter gegen Feldelemente (Goals, Loader, Rand), Besitz ≤ 1 Pin + 1 Cup, Umriss ≤ 24″ × 24″, Höhe ≤ 50″, Bewegung nach 0:00. Die Punkte zählt ein eigener `Scorer` nach RSC3: sichtbare Hälften, Toggle-Farben, Midfield.

**Log-Format (Roboter und Simulation identisch).** Eine Zeile alle 20 ms pro Roboter, Kopfzeile mit Version:

| Spalte | Einheit | Bedeutung |
| --- | --- | --- |
| `t_s` | s | Matchzeit |
| `robot` | – | `stacker` oder `runner` |
| `x_in`, `y_in`, `h_deg` | Zoll, Grad | Pose aus der Odometrie |
| `gx_in`, `gy_in`, `gh_deg`, `g_err_in` | Zoll, Grad | GPS-Pose und Güte (leer, wenn keine) |
| `tx_in`, `ty_in`, `th_deg` | Zoll, Grad | wahre Pose (nur Simulation) |
| `lift_deg`, `arm_deg`, `claw_in` | Grad, Zoll | Ist-Werte der Mechanismen |
| `lift_set_deg`, `arm_set_deg` | Grad | Sollwerte |
| `cycle`, `route_step` | Text | Zustand des Zyklus, aktueller Routenschritt |
| `has_cup`, `has_pin`, `pin_yellow_up` | 0/1 | Besitz und Ausrichtung |
| `score` | Punkte | laufender Punktestand (Simulation: Scorer; Roboter: Schätzung) |
| `cpu_ms`, `batt_v`, `temp_max_c` | ms, V, °C | Gesundheit |
| `event` | Text | Ereignisse: TIMEOUT, BLOCKED, COLLISION, RULE … |

**Browser-Viewer.** Eine einzelne HTML-Datei ohne Server, three.js r128 wie das Liftmodell. CSV per Drag-and-drop laden, Zeitregler mit Abspielen und ×0,25 bis ×4, Feld aus `field.json`, beide Roboter als 3D-Modell mit Lift- und Armwinkel aus dem Log. Ein zweites Log lässt sich halbtransparent darüberlegen (echt gegen Simulation). Daneben ein Diagramm Pose-Abweichung über Zeit und die Ereignisliste zum Anklicken.

**Systemidentifikation.** Ein Messprogramm auf dem Roboter (`src/tools/sysid_run.cpp`) fährt eine langsame Spannungsrampe (0,25 V/s) und Spannungssprünge auf 6 V, vorwärts, rückwärts und drehend, und loggt mit 10 ms. `tools/sysid.py` schätzt daraus kS, kV, kA pro Seite und die wirksame Spurbreite per linearer Regression und schreibt sie in `sim_params.json`. Gleichzeitig liefert das gute Startwerte für die Regler am echten Roboter.

**Abnahmeregeln für eine Route (Exit-Code 0 nur wenn alle erfüllt):**

| Regel | Grenze |
| --- | --- |
| Kollisionen Roboter–Roboter oder mit Feldelementen | 0 in allen Läufen |
| Regelverletzungen (Besitz, 24″, 50″, Bewegung nach 0:00) | 0 |
| Beide Roboter bei Matchende (60 s) im Midfield | ≥ 95 % der Läufe |
| Route ohne Abbruch zu Ende | ≥ 95 % der Läufe |
| Punkte P10 (die schlechtesten 10 %) | ≥ Zielwert in der Route (`target_p10`) |

## Umsetzungsplan für Claude Code

Acht Phasen, 47 Tickets. Phasen 0 bis 2 laufen komplett am Laptop und brauchen keinen fertigen Roboter. Claude Code arbeitet die Tickets in dieser Reihenfolge ab, ein Ticket pro Branch und Pull Request. Die Termine sind ein Vorschlag und hängen am Baufortschritt.

| Phase | Ziel | Vorschlag bis | Hardware nötig |
| --- | --- | --- | --- |
| 0 Setup | Repo baut, Tests laufen, CI grün | 8. Nov | nein |
| 1 Fundament | 10-ms-Takt, HAL, Simulator, Logger | 22. Nov | nein |
| 1b Simulation | frostbyte\_sim, Monte-Carlo-Bericht, Browser-Viewer (SIM-01 bis SIM-07) | 6. Dez, parallel zu Phase 2 | nein (SIM-08: Testchassis) |
| 2 Antrieb + Ort | Fahren und Position, am Simulator getestet | 6. Dez | Testchassis für D-07 |
| 3 Mechanismen | Lift, Arm, Greifer, Aufnahme, Toggle | 20. Dez | ja |
| 4 Zyklus + Fahrer | Driving Skills spielbar | 10. Jan | ja |
| 5 Autonomie | Autonomous Skills spielbar | 31. Jan | ja |
| 6 Link + Feinschliff | Funk, Tuning, Turnier-Release | Februar | ja |

| ID | Aufgabe | Fertig, wenn … | HW |
| --- | --- | --- | --- |
| S-01 | PROS-4-Projekt, `Makefile` mit `ROBOT=stacker\|runner`, LemLib eingebunden, Versionen fixiert | beide Ziele bauen fehlerfrei | – |
| S-02 | Laptop-Build `test/CMakeLists.txt` mit doctest, kompiliert `lib/` + `hal/sim` | `ctest` läuft mit einem Beispieltest | – |
| S-03 | CI in `.github/workflows/ci.yml`: beide Roboter bauen + Tests | roter Build blockiert den Merge | – |
| S-04 | `CLAUDE.md`, `.clang-format`, `docs/decisions/` mit ADR-001 (Architektur) | Inhalt wie im Abschnitt „Übergabe“ | – |
| F-01 | `lib/core`: Einheiten, `Snapshot`, `Clock`, `RingBuffer`, `StateMachine<>` | Unit-Tests für Puffer und Automat | – |
| F-02 | `hal/interface` + `hal/sim` + `hal/pros` für alle Geräte aus der Port-Tabelle | Sim-Motor folgt Spannung mit Kennlinie | – |
| F-03 | `port_check`-Programm: jeder Port kurz testen, Ergebnis aufs Display | am Roboter: alle Ports bestätigt, Config korrigiert | ja |
| F-04 | Steuer-Task mit `delay_until`, Phasen aus Competition-Callbacks, Zykluszeitmessung | Jitter ≤ 1 ms im Log | – |
| F-05 | Logger-Task: Ringpuffer → CSV auf SD; `tools/log_plot.py` | 60-s-Lauf erzeugt lesbare CSV | – |
| F-06 | Health-Task: Temperatur, Port-Ausfall, Akku; Warnung am Controller | Kabel ziehen → Warnung in ≤ 200 ms | – |
| D-01 | `Drive`: Arcade, Fahrkurve, Langsam-Modus, HOLD | Unit-Test der Kurve, Sim fährt Quadrat | – |
| D-02 | Odometrie Stacker (2 Tracking-Räder + IMU) | Sim-Quadrat 3 m: Fehler ≤ 1 cm ohne Rauschen | – |
| D-03 | Odometrie Runner (Motorencoder + IMU) + Umschaltung bei Ausfall | Ausfall im Sim → Fallback ohne Sprung | – |
| D-04 | GPS-Fusion mit Güte- und Tempo-Grenze | Sim mit 2″ Drift wird auf ≤ 1″ zurückgezogen | – |
| D-05 | `MotionController`: `turnTo`, `moveTo`, `follow`; LemLib-Adapter + Sim-Adapter | DONE / TIMEOUT / BLOCKED im Test | – |
| D-06 | Tuning-Modus: PID-Werte per Controller ändern, ins Log schreiben | Werte überleben Neustart (SD) | – |
| D-07 | Antrieb am Testchassis tunen | 2 m geradeaus ≤ 2 cm, 10 Drehungen ≤ 2° | ja |
| M-01 | `Lift`: Homing, MOVE/HOLD, `angleForHeight()`, Endlagen | Kalibrierung mit 5 Punkten, Fehler ≤ 0,3″ | ja |
| M-02 | `SwingArm` (Stacker): PICK ↔ FRONT, gekoppelt an Lifthöhe | schwenkt nie unter Mindesthöhe | ja |
| M-03 | `Claw`: Strom-Plateau-Erkennung, Cup/Pin-Modus | 50 Griffe, ≥ 48 erkannt | ja |
| M-04 | `Intake`: Lauf bis Bumper, Stau-Erkennung, Rückwärtsimpuls | Stau im Sim und am Roboter gelöst | ja |
| M-05 | Optischer Sensor: Gelb oben ja/nein | 20 Pins, ≥ 19 richtig | ja |
| M-06 | `Toggle` (Runner): Mitnehmer-Ablauf über Lift-Winkelfolge | Toggle 10 × gesetzt, nie berührt danach | ja |
| M-07 | `RuleGuard`: Besitz, 24″/50″-Grenzen, Halt bei 0:00 | jede Regel hat einen Test, der sie verletzt | – |
| C-01 | `CycleController` mit 7 Zuständen und Timeouts | jeder Übergang und jedes Timeout getestet | – |
| C-02 | `GoalBook` + `placeHeight(goal, level)` | Tabelle aller 9 Goals, Ebenen 0–5 | – |
| C-03 | `driver_map` + Export nach `docs/controls.md` | Belegung wie im Abschnitt „Fahrerbedienung“ | – |
| C-04 | Controller-Anzeige, Vibration bei 15 s / 10 s | sichtbar am Controller | ja |
| C-05 | Erster Driving-Skills-Lauf mit beiden Robotern | 3 Läufe mit Log, Punkte notiert | ja |
| A-01 | `field/field.json` aus dem STEP-Feldmodell vermessen | alle Goals, Loader, Toggles, Midfield, Startfelder | – |
| A-02 | Route-Format, Parser, `tools/route2cpp.py` | Fehlerhafte Route → klare Meldung mit Zeilennummer | – |
| A-03 | `RoutePlayer`: Schritte, `parallel`, `repeat`, `at <t>`, `on_timeout` | Unit-Tests je Befehl | – |
| A-04 | `frostbyte_sim` mit beiden Skills-Routen gleichzeitig (baut auf SIM-01 bis SIM-06) | Kollision im Testfall erkannt | – |
| A-05 | Routen v1 `skills_stacker` und `skills_runner` | Sim: ≤ 60 s, Midfield erreicht, geschätzte Punkte im Bericht | – |
| A-06 | Auswahlmenü am Brain, Route von SD im Trainingsmodus | Auswahl bleibt nach Neustart | ja |
| A-07 | Feldtest Autonomous Skills | 10 Läufe, ≥ 8 ohne Abbruch | ja |
| L-01 | Funk-Link am echten Gerät testen (Radios, Reichweite, Latenz) | Ergebnis als ADR | ja |
| L-02 | Nachrichten `Status`, `GoalBook`, `Signal` mit Bestätigung | Link-Abbruch im Test → Routen laufen weiter | ja |
| L-03 | Release-Ablauf: Version + Commit aufs Display, Fallback in Slot 2 | Checkliste in `docs/release.md` | ja |

**Tickets der Simulationsplattform (Phase 1b).** Start nach F-01 und F-02; ab SIM-02 testen die Antriebs-Tickets D-01 bis D-05 gegen dieses Modell.

| ID | Aufgabe | Fertig, wenn … | HW |
| --- | --- | --- | --- |
| SIM-01 | Welt-Grundgerüst: 1-ms-Schritt, Steuerung alle 10 ms, Seed, `field.json` laden, CLI `frostbyte_sim`, CSV nach `docs/log_format.md` | gleicher Seed → byte-gleiche CSV; leere Route läuft 60 s durch | – |
| SIM-02 | Fahrmodell (kS, kV, kA, Spurbreite, Schlupf, Akkuspannung) aus `sim_params.json` | 12 V geradeaus: Endtempo ± 3 % zur Formel; Quadrat 2 m schließt auf ≤ 0,5″ ohne Rauschen | – |
| SIM-03 | Sensormodelle: Tracking-Rad, Encoder, IMU-Drift, GPS mit Güte und Aussetzern | gemessene Statistik in 1000 Proben ± 10 % der Vorgabe | – |
| SIM-04 | Mechanismen als Gelenke + Objektmodell als Ereignisse (Loader, Boden, Ausrichtung, Greifen, Setzen) | ein Zyklus läuft im Sim durch; Fehlerquote ± 2 % über 1000 Versuche | – |
| SIM-05 | Kollisionen (Roboter, Feldelemente, Rand), Regelprüfer, `Scorer` nach RSC3 | je ein Testfall pro Regel; Scorer stimmt mit 5 Handrechnungen überein | – |
| SIM-06 | Monte-Carlo-Lauf, `summary.json`, `tools/sim_report.py` (HTML), Abnahmeregeln als Exit-Code, CI-Schritt | 100 Läufe beider Routen in ≤ 30 s; Bericht zeigt P10/Median/P90 und Fehler pro Zustand | – |
| SIM-07 | Browser-Viewer `tools/viewer/index.html`: CSV laden, Feld, beide Roboter in 3D, Zeitregler, Overlay zweier Logs, Ereignisliste | Sim-CSV und eine Roboter-CSV laufen flüssig nebeneinander | – |
| SIM-08 | Systemidentifikation: `src/tools/sysid_run.cpp` + `tools/sysid.py`; Ereignis-Wahrscheinlichkeiten aus echten Logs | gleiche Spannungsfolge: Sim-Weg weicht ≤ 5 % vom echten ab | ja |

## Übergabe an Claude Code

Claude Code liest beim Start die Datei `CLAUDE.md` im Repo. Sie ist der Vertrag: was das Projekt ist, wie gebaut wird, was nie passieren darf. Den Inhalt unten legt Ticket S-04 an. Danach gebt ihr Claude Code jeweils ein Ticket, zum Beispiel: „Bearbeite Ticket F-01 aus `docs/plan.md`.“

**Vorbereitung durch euch (einmalig):**

1. GitHub-Repo `frostbyte` anlegen, Claude Code Zugriff geben.
2. Diesen Plan als `docs/plan.md` ins Repo legen (Export aus diesem Dokument als Markdown).
3. PROS-CLI und Python 3 auf dem Rechner, an dem ihr Programme aufs Brain ladet.
4. Ein Teammitglied als Software-Lead benennen: liest jeden Pull Request, bevor er in `main` kommt.

**Inhalt für `CLAUDE.md`:**

```markdown
# Frostbyte – VEX U Override 2026/27 (nur Skills)

Zwei Roboter, eine Codebasis: Stacker (24″) und Runner (15″).
Plan und Tickets: docs/plan.md. Immer genau ein Ticket pro Branch und PR.

## Bauen und testen
- Roboter:  make ROBOT=stacker | make ROBOT=runner
- Laptop:   cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test
- Simulation: ./build-test/frostbyte_sim --stacker routes/skills_stacker.route --runner routes/skills_runner.route --runs 20 --out out/sim/
- Bericht:    python tools/sim_report.py out/sim/   ·   Viewer: tools/viewer/index.html im Browser öffnen
- Vor jedem Commit: Laptop-Tests grün, beide Roboter bauen.

## Architektur (nicht verletzen)
- 5 Schichten: match → tasks → subsystems → control/localization → hal. Nur nach unten aufrufen.
- lib/ und hal/interface/ enthalten keine PROS-Header. PROS nur in hal/pros/ und src/.
- Ein Steuer-Task, 10 ms, delay_until. Kein delay(), kein new, keine Ausgabe im Steuer-Task.
- Subsysteme: nur request(), state(), update(). Jeder Befehl hat ein Timeout.
- Unterschiede zwischen den Robotern nur in config/stacker.hpp und config/runner.hpp.
- Feldpunkte nur in field/field.json, Routen benutzen Namen.
- Intern Zoll, Grad, Sekunden (wie Feldplan und CAD); Einheit im Namen: dist_in, heading_deg.
- Simulation: alle Modellzahlen nur in sim/sim_params.json. Gleicher Seed = byte-gleiches Ergebnis.
- Log-Format steht in docs/log_format.md; Roboter und Simulation schreiben es identisch. Änderung nur mit neuer Versionsnummer in der Kopfzeile.

## Regeln aus dem Spiel (RuleGuard, nie umgehen)
- Besitz max. 1 Pin + 1 Cup (SG6). Footprint 24″ × 24″ (SG2), Höhe 50″ (SG3).
- Bei Matchende 0:00 alle Motoren auf Halt.

## Arbeitsweise
- Erst Test schreiben, dann Code. Ticket ist fertig, wenn „Fertig, wenn …“ aus docs/plan.md erfüllt ist.
- Hardware-Annahmen (Ports, Richtungen, Übersetzungen) nie raten: in config/ als TODO(HW) markieren.
- Kein neues Paket ohne Eintrag in docs/decisions/.
- Commit-Nachrichten auf Deutsch, mit Ticket-ID: „F-01: RingBuffer mit Tests“.
- Am Ende jedes Tickets: kurze Zusammenfassung, was am echten Roboter noch zu prüfen ist.
```

**Was Claude Code nicht allein kann:** alles mit „HW: ja“ in der Tabelle. Dort schreibt es den Code und eine Prüfanleitung. Ihr führt sie am Roboter aus und gebt die Messwerte zurück (Logs als CSV ins Repo unter `logs/`).

## Offene Entscheidungen und Annahmen

Diese Punkte entscheidet ihr. Bis dahin arbeitet Claude Code mit der jeweils genannten Annahme.

| Frage | Annahme im Plan | Entscheiden bis |
| --- | --- | --- |
| PROS oder VEXcode? | PROS 4 + LemLib | vor S-01 |
| Wer ist Software-Lead und liest die Pull Requests? | offen | vor S-01 |
| Zukauf GPS-Sensor (2 Stück) und Rotationssensoren für den Liftwinkel? | eingeplant, Software läuft auch ohne | vor D-04 / M-01 |
| Zweites Funkmodul pro Roboter für den Link? | erst nach Test L-01 | Februar |
| Goal-Zuteilung Stacker / Runner | Stacker: Goals nahe Loader 1; Runner: Rest + Toggles | vor A-05 |
| Liegen im Skills-Match dieselben Objekte auf dem Feld wie im Head-to-Head (Figur VURS1)? | ja | vor A-01 |
| Wie hoch ist die untere Loader-Öffnung? | Einheit steht auf der Matte | vor M-04 (am Feld-CAD messen) |
| Testchassis für Phase 2 vor dem echten Antrieb? | ja, einfacher 4-Motor-Rahmen aus dem Kit | Mitte November |
