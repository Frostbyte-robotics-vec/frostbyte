# Frostbyte – VEX U Override 2026/27 (nur Skills)

Zwei Roboter, eine Codebasis: Stacker (24″) und Runner (15″).
Plan und Tickets: docs/plan.md. Immer genau ein Ticket pro Branch und PR.

## Bauen und testen
- Roboter:  make ROBOT=stacker | make ROBOT=runner
- Laptop:   cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test
- Simulation: ./build-test/frostbyte_sim --stacker routes/skills_stacker.route --runner routes/skills_runner.route --runs 20 --out out/sim/
- Bericht:    python3 tools/sim_report.py (entsteht mit SIM-Ticket) out/sim/   ·   Viewer: tools/viewer/index.html im Browser öffnen
- Format:     python3 tools/format.py (formatiert) · python3 tools/format.py --check (wie die CI)
- Vor jedem Commit: Format ok, Laptop-Tests grün, beide Roboter bauen.

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
- Werte mit "quelle": "schaetzung" in sim/sim_params.json und null-Koordinaten in field/field.json sind Platzhalter: nie als Wahrheit behandeln, Tests dürfen nicht von ihnen abhängen.
- Kein neues Paket ohne Eintrag in docs/decisions/.
- Commit-Nachrichten auf Deutsch, mit Ticket-ID: „F-01: RingBuffer mit Tests“.
- Am Ende jedes Tickets: kurze Zusammenfassung, was am echten Roboter noch zu prüfen ist.

## Modelle und Advisor (gilt für jede Sitzung)
- Hauptmodell Sonnet, Advisor Opus. Beides steht in .claude/settings.json (model, advisorModel) und gilt für alle im Team.
- Erster Schritt jeder Sitzung, noch vor dem Board: prüfen, ob der Advisor aktiv ist. Ist er es nicht (z. B. weil eine
  persönliche Einstellung ihn überschreibt), den Menschen bitten, einmal `/advisor opus` einzugeben, und erst danach weitermachen.
- Den Advisor gezielt fragen bei:
  - Architektur- und Schnittstellenentscheidungen (neue Klasse, neues Dateiformat, alles was ein ADR wert ist)
  - Fehlern, die nach zwei eigenen Lösungsversuchen weiter bestehen
  - Regelfragen zum Spiel (Manual-Auslegung) und allem, was den RuleGuard betrifft
  - vor /finish: kurzer Blick auf Diff und Fertig-Kriterien
- Nicht fragen bei Routine: Umbenennen, Formatieren, Tests ausführen, Board-Befehle, Übergabe schreiben.
- Den Rat des Advisors kurz in der Übergabe oder im ADR festhalten, wenn er eine Entscheidung geprägt hat.

## Autonomer Ablauf (gilt für jede Sitzung, egal wer sie startet)
Quelle der Wahrheit für „was ist zu tun / was ist erledigt“: die Dateien in tasks/ (Board), nicht dein Gedächtnis.
Werkzeug: python3 tools/board.py (summary, next, show, claim, status, release, validate).

1. Start (/start): git checkout main && git pull --rebase; python3 tools/board.py summary.
2. Ticket wählen, in dieser Reihenfolge:
   a) ein Ticket in_progress mit owner = ich (git config user.name) → weitermachen;
   b) eine Ticket-ID, die der Mensch nennt;
   c) ein Ticket in_progress, dessen Übergabe „pausiert – frei zur Übernahme“ sagt → übernehmen (owner ändern);
   d) python3 tools/board.py next (nur Tickets ohne Hardware, außer der Mensch sagt etwas anderes).
3. Übernehmen: board.py claim <ID> <name>; NUR tasks/<ID>.md committen („board: <ID> übernommen“) und direkt auf main pushen.
   Push abgelehnt → git pull --rebase; ist das Ticket inzwischen vergeben → zurück zu 2.
4. Arbeiten auf Branch ticket/<id> (vorhanden → auschecken und pullen). Vorher tasks/<ID>.md lesen, besonders „Übergabe“,
   dazu den Ticket-Abschnitt in docs/plan.md. Test zuerst, dann Code.
5. Nach jedem grünen Zwischenschritt: „Übergabe“ in tasks/<ID>.md aktualisieren, committen, Branch pushen.
   So geht bei einem abrupten Ende (Limit, Absturz) höchstens ein kleiner Schritt verloren.
6. Ende ohne fertiges Ticket (/handoff): Übergabe vollständig schreiben (Stand, nächster konkreter Schritt, offene Fragen,
   wie man testet), Verlauf-Zeile anhängen, committen, pushen. Will der Mensch nicht selbst weitermachen:
   Übergabe beginnt mit „pausiert – frei zur Übernahme“.
7. Ticket fertig (/finish): Fertig-Kriterien prüfen, alle Tests + board.py validate grün, Verlauf-Zeile, Branch pushen,
   Pull Request öffnen (gh pr create, Vorlage .github/pull_request_template.md). Danach auf main:
   board.py status <ID> review (Hardware-Tickets: needs_hw) und pushen. Auf done setzt es der GitHub-Workflow beim Merge.
8. Nie: ein Ticket bearbeiten, das jemand anderes in_progress hat (außer Fall 2c); Code direkt auf main committen
   (auf main nur Änderungen in tasks/); Abhängigkeiten überspringen, ohne es in der Übergabe zu begründen.
9. Neue Erkenntnis, die künftige Sitzungen brauchen (Entscheidung, Stolperfalle, Messwert)?
   → Entscheidungen als ADR in docs/decisions/, Stolperfallen in docs/notes.md, Messwerte in config/ bzw. sim/sim_params.json.
   Neue Arbeit, die kein Ticket hat → neues Ticket in tasks/ anlegen (gleiche Vorlage, nächste freie Nummer), nicht nebenbei erledigen.

## Dauerbetrieb (/auto)
Ziel: so viele Tickets wie möglich ohne menschliches Eingreifen, ohne Qualität zu opfern.
Schleife:
1. /start-Schritte 1–4 (Ticket wählen, übernehmen, Branch).
2. Ticket bearbeiten; nach jedem grünen Zwischenschritt Übergabe aktualisieren, committen, pushen (Schritt 5).
3. Fertig → /finish-Schritte (Kriterien prüfen, Advisor-Review, PR, Status review/needs_hw).
4. Zurück auf main, git pull --rebase, nächstes Ticket wie in Schritt 2 des Autonomen Ablaufs.
5. Kein freies Ticket ohne Hardware? → einen offenen PR eines anderen prüfen (/review), dann erneut Board prüfen.
6. Auch kein PR zu prüfen? → Board-Überblick zeigen, sagen welche Merges oder Hardware-Tests Tickets freigeben würden, aufhören.
Abbruchbedingungen (dann sauber /handoff und stoppen):
- der Mensch sagt stopp, oder die im Argument genannte Anzahl Tickets ist erreicht;
- ein Ticket bleibt nach zwei Advisor-Rückfragen blockiert → Status blocked mit Grund, nächstes Ticket nehmen (nicht stoppen);
- Unsicherheit über Spielregeln oder Hardware, die nur ein Mensch klären kann → Frage in die Übergabe, Status blocked, weiter mit anderem Ticket;
- der Gesprächsverlauf wird sehr lang (nach etwa 3 Tickets): /handoff und den Menschen bitten, einen neuen Chat mit /auto zu starten
  (frischer Kontext spart Limit und hält die Qualität hoch).
Nie im Dauerbetrieb: Pull Requests mergen, Hardware-Tickets als erledigt melden, Regeln aus CLAUDE.md lockern, Tests abschwächen
oder überspringen, damit ein Kriterium „grün“ wird.

## Review durch die KI (/review)
1. Offene PRs listen: gh pr list --state open --json number,title,author,headRefName,labels.
   Wähle den ältesten PR, der nicht von mir ist (gh api user) und noch kein Label „ki-geprueft“ hat (oder die genannte Nummer).
2. gh pr checkout <nr>; Ticket-Datei tasks/<ID>.md und den Plan-Abschnitt lesen.
3. Selbst bauen und alle Tests laufen lassen; python3 tools/board.py validate.
4. Advisor fragen: erfüllt der Diff jedes Fertig-Kriterium? Verletzt er CLAUDE.md (Schichten, kein PROS in lib/, Timeouts, kein new im Steuer-Task, Einheiten)?
   Sind die Tests echt (prüfen sie das Verhalten) oder nur Attrappen?
5. Ergebnis als Kommentar: gh pr review <nr> --comment --body "<Bericht>" mit
   „Empfehlung: mergen“ oder „Empfehlung: ändern“ + konkrete Punkte (Datei:Zeile). Danach gh pr edit <nr> --add-label ki-geprueft.
   Bei „ändern“ zusätzlich in tasks/<ID>.md unter Übergabe die Punkte notieren (auf main committen), damit die Autor-Sitzung sie findet.
6. Nie selbst mergen. Zurück zu main.

## Wenn ein Pull Request Änderungswünsche hat
Beim Wählen eines Tickets (Autonomer Ablauf, Schritt 2) zuerst prüfen: Hat einer meiner PRs (owner = ich) Kommentare mit
„Empfehlung: ändern“ oder Änderungswünsche eines Menschen? → diese zuerst abarbeiten, auf denselben Branch pushen, im PR antworten.
