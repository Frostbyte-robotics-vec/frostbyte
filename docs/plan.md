# Frostbyte – Plan Codebasis 2026/27

Oct 31, 2026 · @Máté

## Ziele und Grundsätze

Eine Codebasis für beide Roboter, die in jedem Match deterministisch läuft: fester 10-ms-Regeltakt, kein Blockieren, und jede Fehlfunktion führt in einen sicheren Zustand statt in einen Absturz. „Lückenlos“ heißt hier: Jede Anforderung aus dem Masterplan (SW, AUT, SEN, SYS-04, MID-05) hat ein verantwortliches Modul und einen Test.

**Messbare Ziele**

| Ziel | Zielwert |
| --- | --- |
| Regeltakt Antrieb und Mechanismen | 10 ms, Jitter ≤ 1 ms |
| Reaktionszeit Taste → Motorbefehl | ≤ 20 ms |
| Odometrie-Fehler nach 30 s Autonomie | ≤ 2 cm |
| Abstürze oder Freezes im Match | 0 in 50 Testmatches |
| Build aus sauberem Checkout | 1 Befehl, ≤ 2 min |
| Neue Autonomie-Route anlegen und testen | ≤ 30 min |

**Grundsätze**

1. **Nie blockieren:** Kein `delay()` in Logik, keine Warteschleifen ohne Timeout. Alles läuft als Zustandsautomat im Takt.
2. **Hardware hinter Schnittstellen:** Logik kennt keine Ports. So lässt sie sich auf dem Laptop testen.
3. **Regeln im Code verankert:** Besitzgrenze, Expansionsgrenzen und Endgame-Höhe werden von der Software zusätzlich zur Mechanik erzwungen.
4. **Konfiguration statt Kopie:** Großer und kleiner Roboter teilen 100 % der Logik; Unterschiede stehen nur in je einer Konfigurationsdatei.
5. **Alles wird geloggt:** Ohne Log kein Debugging nach einem verlorenen Match.

**KI-Nutzung:** In VEX U erlaubt. Module können mit KI-Unterstützung entstehen; Grundsatz bleibt: Jede Änderung wird von einem Teammitglied gelesen, verstanden und getestet, bevor sie in `main` kommt. Wer am Turnier debuggt, muss den Code kennen.

## Plattform und Architektur

Empfehlung: **PROS (C++) mit Git**, aufgebaut in fünf Schichten, von denen jede nur mit der direkt darunterliegenden spricht.

| Kriterium | PROS (C++) | VEXcode V5 (C++) |
| --- | --- | --- |
| Echtzeit-Tasks mit Prioritäten | ja (FreeRTOS-basiert) | eingeschränkt |
| Build per Kommandozeile, CI möglich | ja | kaum |
| Bibliotheken für Odometrie/Pfade in der Community | große Auswahl | weniger |
| Einstieg | steiler | einfacher |

&#91;embedded content: Software-Architektur · 5 Schichten + Querschnitt\]

Der Regelwächter sitzt quer über allen Schichten: Er prüft jeden Befehl gegen Besitz-, Expansions- und Endgame-Regeln, bevor er die Hardware erreicht.

**Bibliotheken:** Startet entweder mit einer bewährten Community-Bibliothek für Odometrie und Pfadfolge oder schreibt sie selbst. Entscheidungskriterium: Ihr müsst jede Zeile verstehen und am Turnier debuggen können. Kapselt die Bibliothek hinter eurer eigenen Schnittstelle in Schicht 4, damit ihr sie später tauschen könnt.

**Externer Prozessor (VEX U erlaubt):** Für Kamera-Erkennung ist ein Koprozessor möglich. In v1 nicht einplanen; erst wenn SEN-04 gebraucht wird. Kommunikation dann über eine serielle Schnittstelle als eigenes Sensor-Modul in Schicht 5.

## Repository-Struktur und Konfiguration

Ein Repository, zwei Build-Ziele: `stacker` (24″) und `runner` (15″). Die Logik in `lib/` kompiliert zusätzlich auf dem Laptop für Tests – deshalb darf dort nichts Hardware-Spezifisches stehen.

```
frostbyte-code/
├── lib/                    # reine Logik, ohne PROS-Abhängigkeit
│   ├── control/            # PID, Bewegungsprofile, Filter
│   ├── estimation/         # Odometrie, Sensorfusion
│   ├── planning/           # Pfade, Pfadfolger
│   ├── tasks/              # Schicht 2: CycleController (Zyklus aus Aufnehmen, Heben, Setzen)
│   ├── subsystems/         # Zustandsautomaten Intake, Lift, Toggle
│   ├── match/              # Phasen-Automat, Befehls-Queue, Regelwächter
│   └── util/               # Zeit, Einheiten, Ringpuffer
├── hal/                    # Hardware-Abstraktion: Interfaces + PROS-Implementierung
│   ├── interfaces/
│   ├── pros/
│   └── sim/                # Fake-Hardware für Tests und Simulation
├── robots/
│   ├── stacker/config.hpp  # Ports, Übersetzungen, Grenzwerte, Reglerparameter
│   └── runner/config.hpp
├── autos/                  # Autonomie-Routinen als Daten (Pfade + Aktionen)
├── src/main.cpp            # Competition Template, verdrahtet alles
├── test/                   # Unit-Tests für lib/ auf dem Laptop
├── tools/                  # Log-Auswertung, Pfadplaner, Tuning-Skripte
└── docs/                   # Architektur, Konventionen, Port-Belegung
```

**Konfiguration pro Roboter** (eine Datei, die einzige Stelle mit Unterschieden):

- Ports und Drehrichtung aller Motoren und Sensoren
- Getriebeübersetzungen, Raddurchmesser, Spurweite, Tracking-Rad-Abstände
- Reglerparameter (PID, Profile: v\_max, a\_max)
- Mechanische Grenzen: Lift-Endlagen, Endgame-Höhe, Expansionsgrenzen
- Welche Subsysteme existieren (z. B. Toggle-Arm nur am Runner)

**Konventionen:** SI-Einheiten intern (m, s, rad), Einheit im Variablennamen wo nötig (`dist_m`); `main` ist immer turnierfähig, Arbeit läuft auf Feature-Branches mit Review durch eine zweite Person.

## Laufzeitmodell

Ein einziger Steuer-Task mit festem 10-ms-Takt führt alles aus, was Motoren bewegt – in Autonomie, Fahrerphase und Endgame gleich. Die Competition-Callbacks setzen nur die Phase; dadurch gibt es beim Übergang Autonomie → Fahrer keine Lücke und keinen Neustart von Reglern.

**Tasks**

| Task | Takt | Priorität | Aufgabe | Zeitbudget pro Zyklus |
| --- | --- | --- | --- | --- |
| Steuer-Task | 10 ms, mit `delay_until` (kein driftender Takt) | hoch | Sensoren lesen, Odometrie, Logik, Regler, Motoren schreiben | ≤ 3 ms gemessen |
| Logger-Task | 50 ms | niedrig | Ringpuffer auf SD-Karte schreiben | darf den Steuer-Task nie blockieren |
| UI-Task | 100 ms | niedrig | Brain-Display, Controller-Anzeige, Auswahlmenü | – |
| Health-Task | 100 ms | mittel | Motortemperaturen, Verbindungen, Akkuspannung prüfen | – |

**Reihenfolge in jedem 10-ms-Zyklus**

1. Zeitstempel nehmen, Überlauf des letzten Zyklus prüfen (Takt verpasst → Zähler + Log-Eintrag)
2. Alle Sensoren und Controller-Eingaben einmal lesen → unveränderlicher Messwert-Snapshot
3. Odometrie aktualisieren
4. Match-Logik: Phase, Befehls-Queue bzw. Fahrer-Makros → Sollwerte für Subsysteme
5. Subsystem-Automaten weiterschalten
6. Regelwächter prüft alle Ausgaben (Besitz, Expansion, Endgame-Höhe, Matchende)
7. Regler rechnen, Motorbefehle schreiben
8. Snapshot + Sollwerte in den Log-Ringpuffer kopieren (nur kopieren, nicht schreiben)

**Regeln für „schnell“**

- Kein dynamischer Speicher im Steuer-Task nach dem Start (`new`, wachsende Vektoren) – alles beim Init anlegen.
- Keine Ausgaben auf Konsole oder Display im Steuer-Task.
- Daten zwischen Tasks nur über Ringpuffer oder kurz gesperrte Kopien, nie lange Mutex-Sperren.
- Zykluszeit wird ständig gemessen und geloggt; Grenze 5 ms löst eine Warnung im Health-Monitor aus.

## Module, Zustandsautomaten und Schnittstellen

Jedes Modul hat einen Verantwortlichen, eine schmale Schnittstelle und deckt konkrete Anforderungs-IDs aus dem Masterplan ab.

| Modul | Schicht | Aufgabe | Deckt ab | Verantwortlich |
| --- | --- | --- | --- | --- |
| `hal::Motor`, `hal::Sensor` | 5 | Einheitliche Schnittstelle, Einheiten umrechnen, Ausfall melden | SEN-05, SYS-12 | offen |
| `Odometry` | 4 | Pose aus Tracking-Rädern + Inertialsensor, Korrektur per Wand-Distanz | SEN-01, SEN-02 | offen |
| `MotionProfile`, `Pid` | 4 | Trapez-/S-Profile, Regler mit Begrenzung und Anti-Windup | SW-02, DRV-02 | offen |
| `PathFollower` | 4 | Pfade abfahren, Ziel erreicht / Timeout melden | AUT-01–AUT-03 | offen |
| `Drive` | 3 | Fahrerfahrt, Hold-Modus, Bewegung zu Pose | DRV-01–DRV-06, MID-03 | offen |
| `Intake` | 3 | Greifen, Orientierung erkennen, Besitz zählen | INT-01–INT-06, SYS-04 | offen |
| `Lift` | 3 | Positionen anfahren, Endgame-Modus, Endlagenschutz | STK-01–STK-03, MID-02 | offen |
| `ToggleArm` | 3 | Toggle drehen, loslassen, Farbe prüfen | TGL-01–TGL-03 | offen |
| `MatchPhase` | 2 | Autonomie / Fahrer / Endgame / Ende aus Feldsignal + Uhr | MID-05, AUT-04 | offen |
| `CommandQueue` | 2 | Befehle nacheinander oder parallel, jeder mit Timeout | AUT-03 | offen |
| `RuleGuard` | quer | Befehle vor der Ausgabe gegen Regeln prüfen | SW-04, SYS-02–SYS-04 | offen |
| `HealthMonitor` | quer | Temperatur, Verbindungen, Zykluszeit | DRV-04, SW-05 | offen |
| `Logger` | quer | Ringpuffer → SD-Karte, CSV | SW-05 | offen |

**Zustandsautomat Intake (Beispiel für alle Subsysteme)**

| Zustand | Bedeutung | Wechsel |
| --- | --- | --- |
| `LEER` | nichts im Roboter | Taste/Befehl „Greifen“ → `GREIFT` |
| `GREIFT` | Rollen laufen, Sensor wartet auf Objekt | Objekt erkannt → `HAELT_PIN` / `HAELT_CUP`; Timeout 1,5 s → `LEER` |
| `HAELT_PIN`, `HAELT_CUP`, `HAELT_BEIDE` | Besitz gezählt, Orientierung bekannt | „Setzen“ → `SETZT`; zweites gleichartiges Objekt erkannt → `AUSWERFEN` |
| `SETZT` | Ablauf Ablegen läuft | Sensor leer → `LEER`; Timeout → `FEHLER` |
| `AUSWERFEN` | Überbesitz aktiv abbauen (SYS-04) | Zähler wieder regelkonform → vorheriger Halte-Zustand |
| `FEHLER` | Motor blockiert oder Sensor unplausibel | Fahrer kann manuell übersteuern; Log-Eintrag |

**Schnittstellen-Regeln**

- Jedes Subsystem hat genau drei öffentliche Arten von Funktionen: Befehl setzen (`request(...)`), Zustand abfragen (`state()`, `isDone()`), Takt (`update(snapshot)`).
- Jeder Befehl hat ein Timeout. Nichts wartet unbegrenzt.
- Subsysteme rufen sich nie gegenseitig auf; Koordination nur über Schicht 2.

## Autonomie-Framework und Fahrersteuerung

Autonomie-Routinen sind Daten (Liste von Befehlen mit Zeitbudget), keine handgeschriebenen Ablauf-Funktionen. So lassen sie sich schnell ändern, auf dem Laptop simulieren und für Rot/Blau spiegeln.

**Aufbau einer Routine**

- Startpose + Liste von Schritten: `fahre(Pfad)`, `drehe(Winkel)`, `greife()`, `setze(Goal)`, `toggle()`, `warte_bis(Bedingung, Timeout)`, `parallel(...)`
- Jeder Schritt hat ein Timeout und eine Ausweichregel (überspringen, abbrechen, sichere Endposition)
- **Zeitwächter:** Bei 27 s ohne Fertigmeldung fährt der Roboter in seine sichere Endposition ohne Wandkontakt (AUT-04)
- **Spiegelung:** Routinen werden für die rote Seite geschrieben; die blaue Version entsteht automatisch durch Spiegelung der Koordinaten
- **Zwei Roboter, ein Plan:** Pro Startaufstellung gibt es ein Routinen-Paar mit Zeitmarken; Kollisionsfreiheit wird im Pfadplaner (`tools/`) geprüft, nicht auf dem Feld

**Routinen-Katalog (Minimum v1)**

| Name | Zweck | Muss |
| --- | --- | --- |
| `safe_red` / `safe_blue` | sichere Punkte nur auf eigener Seite | ja |
| `awp_red` / `awp_blue` | Win-Point-Kriterien (VUG6) | ja |
| `skills` | 60-s-Skills mit beiden Robotern | ja |
| `nothing` | stehen bleiben (Notfall) | ja |
| `test_*` | Kalibrierung: geradeaus 2 m, 10 Drehungen, Quadrat | ja |

**Auswahl:** Menü auf dem Brain-Display, Auswahl bleibt nach Neustart gespeichert; der Controller zeigt die gewählte Routine an. Vor jedem Match prüft das Drive-Team: Routine, Seite, Akku, Pneumatik-Druck.

**Fahrersteuerung**

- Belegung pro Roboter als Tabelle in `docs/`, identische Grundbelegung (Fahren, Greifen, Setzen) auf beiden Controllern
- Ein-Tasten-Makros: Greifen, Setzen, Cup drehen, Endgame-Modus (Lift einfahren + Hold), Toggle setzen
- Makros sind abbrechbar: Jede manuelle Eingabe am gleichen Subsystem übernimmt sofort
- Fahrkurve (Totzone, Exponent) und Höchstgeschwindigkeit pro Fahrer einstellbar
- Endgame-Hinweis: Controller vibriert bei 15 s und 10 s Restzeit
- Fahrassistenz (z. B. automatisches Ausrichten am Goal) nur zuschaltbar, nie dauerhaft aktiv

## Fehlertoleranz und Sicherheit

Jeder Ausfall eines Motors oder Sensors wird erkannt und führt zu einem definierten Rückfall – der Roboter fährt weiter, statt stehenzubleiben oder Regeln zu brechen.

| Ausfall | Erkennung | Reaktion |
| --- | --- | --- |
| Motor getrennt oder Port defekt | Motor meldet keine Daten / Fehlerflag | Subsystem auf `FEHLER`, Controller vibriert, Antrieb fährt mit restlichen Motoren weiter |
| Motor überhitzt | Temperatur über Schwelle (Health-Task) | Strombegrenzung senken, Warnung am Controller; Fahrer entscheidet |
| Mechanismus blockiert | hoher Strom + keine Bewegung > 300 ms | Motor stoppen, kurz zurück, Log-Eintrag |
| Tracking-Rad oder Inertialsensor fällt aus | Sprung oder Stillstand der Werte, Plausibilität gegen Antriebsencoder | Odometrie wechselt auf Antriebsencoder; Autonomie läuft im „Safe“-Modus weiter |
| Lift-Positionssensor unplausibel | Abweichung zu Motorencoder | Lift nur noch in sichere Endlage, Endgame-Modus bleibt verfügbar |
| Steuer-Task überzieht den Takt | Zyklus > 10 ms | Zähler + Log; ab 3 in Folge Warnung im Display |
| Absturz einer Routine | Timeout eines Schritts, Ausnahme | Zeitwächter übernimmt → sichere Endposition |
| Funkabbruch | Feldsteuerung meldet getrennt | Firmware stoppt Motoren; Code setzt nach Wiederverbindung alle Automaten in sichere Zustände |

**Regelwächter (RuleGuard) – harte Sperren**

- Besitz: Greifen wird blockiert, wenn bereits ein Pin bzw. Cup gehalten wird
- Expansion: Lift und Arme haben Software-Endlagen innerhalb der mechanischen Anschläge (50″, 24″-Footprint)
- Endgame: Ab 10 s Restzeit ist Lifthöhe auf Endgame-Höhe begrenzt, sobald der Roboter im Midfield ist (Position aus Odometrie)
- Autonomie: Pfade, die die Autonomielinie kreuzen würden, werden schon im Planer abgelehnt
- Matchende: Bei 0:00 Antrieb und Mechanismen in Hold, keine Bewegung danach (MID-05)
- Toggle: Nach dem Drehen fährt der Arm automatisch weg, damit der Toggle am Ende nicht berührt wird

## Qualitätssicherung, Tests und Releases

Kein Code kommt auf einen Turnierroboter, der nicht gebaut, getestet und von einer zweiten Person gelesen wurde. Vier Teststufen fangen Fehler dort, wo sie am billigsten sind.

| Stufe | Wo | Was | Wann |
| --- | --- | --- | --- |
| 1 Unit-Tests | Laptop, gegen `hal/sim` | Regler, Profile, Odometrie-Mathematik, alle Zustandsautomaten inkl. Timeouts, Regelwächter | bei jedem Push automatisch (CI) |
| 2 Simulation | Laptop | Ganze Autonomie-Routinen mit vereinfachtem Fahrmodell; Laufzeit < 30 s, keine Linienverletzung, keine Kollision der beiden Roboter | bei jeder Routinen-Änderung |
| 3 Bench-Test | Roboter aufgebockt | Ports, Drehrichtungen, Sensoren, Endlagen, Ausfallreaktionen (Kabel ziehen) | nach jedem Hardware-Umbau |
| 4 Feldtest | Feld | 20 Läufe je Routine mit Log, Drills, Vollmatches | wöchentlich |

**Werkzeuge**

- **CI:** Bei jedem Push baut der Server beide Roboter-Ziele und führt alle Unit-Tests aus; ein roter Build darf nicht in `main`
- **Log-Auswertung:** Skript in `tools/` erzeugt aus den SD-Logs Diagramme (Pose, Soll/Ist, Temperaturen, Zykluszeit)
- **Tuning:** Reglerparameter liegen in der Konfiguration; ein Tuning-Modus erlaubt Änderungen per Controller, die danach ins Repo übernommen werden
- **Code-Review:** Jede Änderung wird von einer zweiten Person gelesen – auch und gerade bei KI-generiertem Code

**Release-Ablauf vor einem Turnier**

1. Code-Freeze 3 Tage vorher; nur noch Bugfixes
2. Version taggen (z. B. `v1.2-quali-feb`), Build-Datum und Commit auf dem Brain-Display anzeigen
3. Checkliste: alle Routinen 10× fehlerfrei, Bench-Test bestanden, Log-Speicher leer
4. Vorheriger Release liegt als Fallback in einem zweiten Programmslot auf dem Brain
5. Nach dem Turnier: Logs sichern, Probleme als Issues erfassen

## Roadmap und offene Fragen

Die Software startet vor der fertigen Hardware: Bis Ende November laufen Architektur, Tests und Odometrie auf einem einfachen Testchassis, damit im Januar nur noch Subsysteme und Routinen dazukommen.

| Phase | Bis | Ergebnis |
| --- | --- | --- |
| Setup | 15. Nov 2026 | Repo, Build für beide Ziele, CI mit erstem Unit-Test, Konventionen in `docs/` |
| Fundament | 30. Nov 2026 | HAL, Steuer-Task mit 10-ms-Takt, Logger, Antrieb + Odometrie auf Testchassis (Fehler ≤ 2 cm über 3 m) |
| Subsysteme | 31. Dez 2026 | Intake-, Lift-, Toggle-Automaten mit Unit-Tests; Regelwächter; Fahrer-Makros |
| Autonomie | 31. Jan 2027 | Routinen-Framework, Simulation, `safe_*` und `awp_*` mit ≥ 80 % Erfolg → Gate v1 |
| Turniere | März 2027 | Skills-Routine, Fahrassistenz, Koprozessor nur falls nötig; Release je Turnier |

**Offene Fragen**

- [ ] PROS oder VEXcode final entscheiden (Empfehlung PROS) und eine Person als Software-Lead benennen
- [ ] Eigene Odometrie/Pfadfolge oder Community-Bibliothek – Entscheidung bis 15. Nov
- [ ] Ist das Feld spiegel- oder punktsymmetrisch? Davon hängt ab, wie Routinen für Blau umgerechnet werden
- [ ] Dürfen die zwei Roboter untereinander kommunizieren? Regeltext VUR10/VUR12 und Q&A prüfen; bis dahin nur zeitbasierte Koordination
- [ ] Geklärt (31. Okt): KI-Nutzung für Wettkampf-Code ist in VEX U erlaubt
- [ ] Welche Sensoren sind fest eingeplant (Tracking-Räder, Distanz, Farbe)? Abstimmung mit Hardware bis CAD-Review 30. Nov
- [ ] CI-Dienst und Speicherort für Logs festlegen (z. B. GitHub)
