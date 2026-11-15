---
id: SIM-41
phase: "5"
titel: Befunde aus dem KI-Review der PRs #114–#116 (Livesim-Physik, Greifen, Aufrichten) abarbeiten
status: review
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
Stand 2026-11-15 (Branch ticket/sim-41): alle Funde bearbeitet (erledigt, SIM-43 oder Fehlalarm), PR offen. Zeilennummern im Ticket sind veraltet, Code nach Funktionsnamen suchen (physics.js hat 323 Zeilen).

Testen ohne node (fehlt auf diesem Mac): Attrappe `node` in den PATH, die die JS-Datei in `chrome --headless=new --dump-dom` ausführt (siehe docs/notes.md, 2026-11-15); dann `python3 -m unittest test_livesim` in tools/viewer (6 Tests inkl. C++-Abgleich). Die Attrappe liegt nur im Scratchpad der Sitzung und muss neu gebaut werden. Stand: Livesim 80 ok, Physik 114 ok, test_livesim OK, Format ok.

Triage (Rat des Advisors: jeder Fund bekommt ein Etikett)
- [x] Einheiten: Radiant intern ist Ausnahme (ADR-051 Nachtrag), Felder `theta_rad`, `omega_rad_s`, `phi_rad`, `crit_rad`
- [x] Ereignisse STACK/TOPPLED über `lastTouch`: Greifen/Loslassen setzen es, kein Ersatz durch den Stacker mehr (Test mit Runner, vorher rot)
- [x] schon in SIM-42 (#119): Modellzahlen 0,5/±0,3, SG6 vor Auswahl, zweiseitige Greifhöhe, `release()` setzt theta/omega zurück
- [ ] → SIM-43 (nicht hier): `pushStanding` ω-Spitze, `toLying` Überlappung, `settle()` gleich hohe Teile, Loslassen unter Auflage teleportiert
- [x] liegende Teile: Reichweite/Abstand/Nächstes aus dem Fußpunkt (`pose`), `isFree` für liegende Teile immer frei (beide Tests vorher rot)
- [x] Livesim gegen C++ bei aktiver `physik`: die C++-Sim kennt den Block nicht (`grep physik sim/` trifft nur einen Kommentar in objects.hpp), der Abgleichtest nutzt deshalb zu Recht eine Fixture ohne `physik`. Ein Abgleich wäre neue Arbeit, kein Fund; dokumentierte Abweichung (ADR-051: Physik gibt es nur im Browser-Livesim)
- [x] Tests gestärkt: Tautologie `=== -1 || …` → `&&`; tote Variablen (`tMove`, `far`, `gp2`) entfernt; neu: entferntes liegendes Teil mit Aufrichten, SG6 für liegendes Pin. Mutationsprobe SG6-Prüfung abgeschaltet → 3 Tests rot, zurück → grün. Nicht umgesetzt: test_livesim.py „Reichweite nur frontal, ein Wert für beide Roboter“ (Erweiterung, kein Fehler)
- [x] Schalter: ein Tippfehler in `physik.greifen` (z. B. `aufricht_rad`) wird jetzt abgelehnt statt still abzuschalten (Schlüsselliste in physics.js `GRAB_KEYS`; neuer Schlüssel in sim_params.json → dort ergänzen). sim_params.json selbst unverändert
- [x] Aufräumen: `setSupport` gibt es nicht mehr (Fehlalarm). `kick`, `drop`, `impact` haben keinen Aufrufer im Livesim, aber Tests und die API (SIM-39) benutzen sie: bewusst belassen. Greif-Bedingung doppelt in `doGrab`/`hold()`: belassen (beide Orte prüfen Verschiedenes: Auswahl vs. Zustandswechsel)

Geprüft: ctest 2/2 grün, Format ok, board.py validate ok, test_livesim 6 Tests (mit Chrome-Attrappe) ok. Roboter-Builds nicht gelaufen (nur JS/Doku geändert). Am Roboter nichts zu prüfen; im Browser prüfen: Cmd+R/Cmd+V im Livesim, Backspace-Reset mit liegendem Pin (gelbe Seite wie im Feldplan).

KI-Review (PR #124) abgearbeitet: `toLying` im Zweig „langsamer Aufschlag“ meldet jetzt toppled, wenn das Teil noch nicht als gefallen galt (Test 7b2). Zur Kenntnis: `GRAB_KEYS` in physics.js ist eine feste Liste, jeder neue Schlüssel in `physik.greifen` muss dort ergänzt werden. PR #123 (SIM-43) nutzt noch die alten Feldnamen (`theta`, `omega`, `phi`); wer zweiter merged, rebased und stellt auf `_rad` um.

Review-Hinweis (KI-Review PR #124, 2026-11-15, Empfehlung: ändern): `integrate` → `toLying` (Aufschlag mit `wIn < ruhe_omega_rad_s`) meldet kein `toppled`, wenn das Teil nie über dem Kippwinkel einen Schritt beendet hat; Test 7b prüft nur den Rückprall-Zweig. `toppled` auch dort melden (`!fallen`) mit Test bei kleinem omega, oder als Fehlalarm begründen. Merge-Reihenfolge: PR #123 (SIM-43) nutzt die alten Feldnamen in physics.js; wer zweiter merged, rebased.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-12 · arjanssen (KI) · Ticket angelegt aus /code-review high auf #114–#116 (unbestätigt) · –
2026-11-15 · arjanssen (KI) · Einheiten-Ausnahme (ADR-051), lastTouch bei Greifen/Loslassen, Triage in Übergabe · ticket/sim-41
2026-11-15 · arjanssen (KI) · fallen/pristine in physics.js, Tasten und Reset in index.html · ticket/sim-41
2026-11-15 · arjanssen (KI) · Fußpunkt-Reichweite, Tests gestärkt, Tippfehler-Schutz, Triage abgeschlossen · ticket/sim-41
2026-11-15 · arjanssen (KI) · Review-Punkt toppled bei langsamem Aufschlag behoben, Test 7b2 · siehe Branch
