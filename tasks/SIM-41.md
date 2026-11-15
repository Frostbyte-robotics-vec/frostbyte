---
id: SIM-41
phase: "5"
titel: Befunde aus dem KI-Review der PRs #114–#116 (Livesim-Physik, Greifen, Aufrichten) abarbeiten
status: in_progress
owner: arjanssen
branch: ticket/sim-41
depends: [SIM-40]
hw: nein
---

## Ziel
`/code-review high` lief am 2026-11-12 auf #114 (SIM-38), #115 (SIM-39), #116 (SIM-40) und lieferte 28 Funde. **Sie sind nicht einzeln nachgeprüft** (kein Reproduktionstest), jeder Punkt ist zuerst mit einem roten Test zu bestätigen oder als Fehlalarm zu schließen. Das ist kein offizielles `ki-geprueft`-Review nach CLAUDE.md (das PR-Autor-Konto ist dasselbe). Die Gewichtung unten ist meine Einschätzung beim Lesen, nicht geprüft.

## Fertig, wenn …
Je Fund: roter Test, dann Fix, oder Begründung „Fehlalarm“ in der Übergabe. Zuerst die Regelverstöße gegen CLAUDE.md:
- **Modellzahlen im Code (CLAUDE.md: nur in sim/sim_params.json):** livesim.js:187/194 `+ 0.5` (Greif-Höhentoleranz) und `±0.3` (Abstand zweier gehaltener Teile) nach `physik.greifen` in sim_params.json (`quelle: schaetzung`)
- **Einheiten (intern Zoll, Grad, Sekunden):** physics.js:1 rechnet intern in Radiant (`theta`, `omega`, `phi`, `HALF_PI`); ADR-051 begründet es, markiert es aber nicht als Ausnahme. Entweder auf Grad umstellen oder im ADR ausdrücklich als Ausnahme festhalten (Advisor fragen), Namen mit Einheit (`theta_rad`)
- **Spielregel SG6 (RuleGuard, nie umgehen):** livesim.js:201 `doGrab` wählt zuerst das nächste Teil und prüft dann SG6; ein nahes, abgelehntes Pin sperrt einen weiter entfernten, erlaubten Cup. SG6 muss die Kandidaten vor der Auswahl filtern; Test mit Pin in der Hand, Pin näher als Cup
- Greifen mit angehobener Hand: livesim.js:194 prüft nur `b.z > hand_z + 0.5`, tiefere Teile (Boden, Goal) werden mit hohem Lift trotzdem gegriffen; zweiseitige Prüfung `|b.z − hand_z| ≤ Toleranz`

Danach (Korrektheit, nach Wichtigkeit):
- physics.js:291 `release()` setzt `theta`/`omega` nicht zurück (wie `drop()`): ein beim Aufrichten losgelassenes Pin landet „stehend“ mit `theta > 0`; Test: Loslassen mitten im Aufrichten
- livesim.js:395 liegende Teile: Reichweite/Abstand/Nächstes aus dem Schwerpunkt statt aus dem Fußpunkt (`x − c·cos φ`); physics.js:262 `isFree` nutzt für liegende Teile die Stehend-Logik (`o.z > b.z`)
- livesim.js:481/219 Ereignisse (STACK, TOPPLED) werden über `lastTouch` zugeordnet; Greifen/Loslassen setzt es nicht, Fallback `ROBOTS[0]` → der Runner-Griff landet im Stacker-Log. `doGrab`/`doRelease` setzen `lastTouch`; Kontakt Objekt-gegen-Objekt nie dem Stacker zuschreiben
- physics.js:353 `fallen` wird nie zurückgesetzt (nach `drop()`/Aufstehen); physics.js:343 Umkippen direkt zu „liegend“ meldet kein `toppled`
- physics.js:429 `pushStanding`: Winkelsprung → riesiges `omega` und heftiges Zurückprallen (`dt`-abhängig)
- physics.js:332 `toLying` verschiebt den Kreis ohne Überlappungsauflösung (Teil kann in Roboter/Nachbar/Feldrand liegen)
- physics.js:403 `settle()`: `self.z = Infinity` zählt gleich hohe Teile als Unterlage (Stapel schwebt)
- physics.js:364 loslassen unterhalb der Auflage teleportiert das Teil auf `sup.z` (ADR „kein Teleport“)
- index.html:119 `liegeRichtung` wird beim Reset nicht zurückgesetzt, `pristine` (physics.js:320) wird nie auf false gesetzt
- index.html:804 Tasten E/Q/F/V ignorieren Strg/Cmd/Alt und rufen kein `preventDefault` (Cmd+V = Pin loslassen); bei `ctrlKey`/`metaKey`/`altKey` zurückkehren
- livesim.js:179 Livesim und C++-Simulation weichen bei aktiver `physik` ab (andere TOPPLED/COLLISION-Folge); der Abgleichtest nutzt eine Fixture ohne `physik`. Entscheiden: Abgleich mit der echten Konfiguration oder dokumentierte Abweichung
- Tests: physics_test.js:579 (`indexOf === -1 || hand.length === 2` ist fast immer wahr), :361 (`tMove = … * 0`, tote Variablen `gp2`, `far`, `pos`), :319 (Test „fern/liegend nicht gegriffen“ läuft nur ohne `aufricht_rad_s`: kein Test für entfernte/blockierte liegende Teile, SG6 für liegende Teile, Loslassen mitten im Aufrichten); test_livesim.py:621 (Reichweite nur frontal, ein Wert für beide Roboter)
- Aufräumen: `kick` doppelt `pushLying`; `impact`-Ereignis, `drop`, `setSupport` ohne Aufrufer im Livesim; Greif-Bedingung doppelt in `doGrab` und `hold()` (`phys.canGrab(i)`); `aufricht_rad_s` nur über „Schlüssel fehlt“ = aus (Tippfehler schaltet still ab): ausdrücklicher Schalter
- Laptop-Tests grün (ctest, physics_test.js, livesim_test.js, test_livesim.py), `python3 tools/board.py validate` grün; die Änderungen gehen auf die PR-Branches bzw. nach deren Merge auf einen neuen Branch ticket/sim-41

## Kontext
tasks/SIM-38.md, SIM-39.md, SIM-40.md, ADR-051, tools/viewer/physics.js, tools/viewer/livesim.js, tools/viewer/physics_test.js. Reihenfolge: PRs #114 → #115 → #116 sind gestapelt; Funde zu SIM-38 gehören auf ticket/sim-38, bevor es gemergt wird (sonst als Folge-PR).

## Hardware-Prüfung
—

## Übergabe
Stand 2026-11-15 (Branch ticket/sim-41): Einheiten, lastTouch, fallen/pristine, Tasten/Reset erledigt; offen siehe unten. Zeilennummern im Ticket sind veraltet, Code nach Funktionsnamen suchen (physics.js hat 323 Zeilen).

Testen ohne node (fehlt auf diesem Mac): Attrappe `node` in den PATH, die die JS-Datei in `chrome --headless=new --dump-dom` ausführt (siehe docs/notes.md, 2026-11-15); dann `python3 -m unittest test_livesim` in tools/viewer (6 Tests inkl. C++-Abgleich). Die Attrappe liegt nur im Scratchpad der Sitzung und muss neu gebaut werden. Stand: Livesim 80 ok, Physik 109 ok, test_livesim OK, Format ok.

Triage (Rat des Advisors: jeder Fund bekommt ein Etikett)
- [x] Einheiten: Radiant intern ist Ausnahme (ADR-051 Nachtrag), Felder `theta_rad`, `omega_rad_s`, `phi_rad`, `crit_rad`
- [x] Ereignisse STACK/TOPPLED über `lastTouch`: Greifen/Loslassen setzen es, kein Ersatz durch den Stacker mehr (Test mit Runner, vorher rot)
- [x] schon in SIM-42 (#119): Modellzahlen 0,5/±0,3, SG6 vor Auswahl, zweiseitige Greifhöhe, `release()` setzt theta/omega zurück
- [ ] → SIM-43 (nicht hier): `pushStanding` ω-Spitze, `toLying` Überlappung, `settle()` gleich hohe Teile, Loslassen unter Auflage teleportiert
- [ ] offen: liegende Teile: Reichweite/Abstand/Nächstes aus dem Fußpunkt statt Schwerpunkt (livesim.js `doGrab`); `isFree` für liegende Teile (physics.js)
- [x] `fallen` wird beim Aufrichten, Greifen, Loslassen, `drop()` zurückgesetzt (Mutationsprobe: ohne Fix 4 Tests rot). „Umkippen direkt zu liegend ohne toppled“: Fehlalarm, der Schlag ≥ 90° bleibt `tipping` mit Rückprall, `toppled` kommt (Regressionstest 7b)
- [x] `pristine` fällt beim ersten Schieben/Greifen, `setLieDir` nur bei ruhendem Teil (Test vorher rot); `liegeRichtung` wird beim Reset (Backspace) zurückgesetzt (nur Browser, nicht automatisch getestet: im Browser prüfen)
- [x] Livesim-Tasten ignorieren Strg/Cmd/Alt (nur Browser, nicht automatisch getestet)
- [ ] offen: Livesim gegen C++ bei aktiver `physik`: zuerst `grep physik sim/*.cpp` (nur sim/objects.hpp trifft); implementiert die C++-Sim den Block nicht, ist ein Abgleich neue Arbeit → dokumentierte Abweichung im ADR, ggf. neues Ticket
- [ ] offen: schwache Tests stärken (physics_test.js `indexOf === -1 || hand.length === 2`, tote Variablen, Test „fern/liegend“ nur ohne `aufricht_rad_s`); je Test Mutationsprobe (Code kaputt machen → rot → zurück) in der Übergabe festhalten
- [ ] offen: Aufräumen: `kick` doppelt `pushLying`; `impact`-Ereignis, `drop` ohne Aufrufer; Greif-Bedingung doppelt in `doGrab`/`hold()`; ausdrücklicher Schalter statt „aufricht_rad_s fehlt = aus“ (dann ctest, die C++-Sim liest sim_params.json auch). `setSupport` gibt es nicht mehr: Fehlalarm.

Neue Tests nur mit Fixture-Werten, nie mit `schaetzung`-Werten aus sim_params.json. Vor /finish: ctest (cmake war auf diesem Mac vorhanden, noch nicht gelaufen), beide Roboter bauen, Format, board.py validate.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt aus /code-review high auf #114–#116 (unbestätigt) · –
2026-11-15 · arjanssen (KI) · Einheiten-Ausnahme (ADR-051), lastTouch bei Greifen/Loslassen, Triage in Übergabe · ticket/sim-41
2026-11-15 · arjanssen (KI) · fallen/pristine in physics.js, Tasten und Reset in index.html · ticket/sim-41
