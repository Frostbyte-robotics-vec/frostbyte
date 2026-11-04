# Startpaket Frostbyte-Codebasis

## Schnellstart (automatisch)

**Admin (einmal):** Ordner `frostbyte-start` in VS Code öffnen, Claude-Code-Chat öffnen, `setup/SETUP_FUER_ADMIN.md` in den Chat ziehen.
Claude legt das GitHub-Repo an, lädt die anderen ein und erzeugt die Datei `FROSTBYTE_EINRICHTUNG.md`.

**Alle anderen:** leeren Ordner anlegen, `FROSTBYTE_EINRICHTUNG.md` hineinlegen, Ordner in VS Code öffnen,
Datei in den Claude-Code-Chat ziehen. Claude installiert, was fehlt, holt das Repo und richtet alles ein.

Danach bei allen: neuer Claude-Code-Chat im Repo-Ordner → `/advisor` prüfen → `/start`.

Die Schritte unten beschreiben dasselbe von Hand.

Dieses Paket ist der Anfang des Repos `frostbyte`. Claude Code baut den Rest Ticket für Ticket.

## Einmalig von euch

1. GitHub-Repo `frostbyte` anlegen und den Inhalt dieses Ordners hineinlegen.
2. Den Bauplan aus Claude Docs als Markdown exportieren und als `docs/plan.md` speichern
   (Dokument „Codebasis – Bauplan für Claude Code“, Export → Markdown).
3. PROS-CLI, CMake ≥ 3.20, einen C++20-Compiler und Python 3 installieren.
4. Claude Code im Repo starten.

## Was schon drin ist

| Datei | Inhalt |
| --- | --- |
| CLAUDE.md | Arbeitsregeln, die Claude Code bei jedem Start liest |
| docs/log_format.md | Log-Format v1, gilt für Roboter und Simulation |
| docs/decisions/ADR-001-architektur.md | erste Architekturentscheidung |
| sim/sim_params.json | Startwerte aller Simulationsmodelle (Schätzungen, markiert) |
| field/field.json | alle Feldpunkte mit festen Namen, Koordinaten noch leer (A-01) |
| routes/*.route | Entwürfe der Skills-Routen und eine Kalibrierroute |
| setup/ | SETUP_FUER_ADMIN.md, setup_repo.sh, ONBOARDING_TEMPLATE.md (Vorlage für die Einrichtungsdatei) |
| web/, tools/web_build.py | Öffentliche Teamseite (inhalt.json: Sponsoren, Team, Kontakt; Logos nach web/img/sponsoren/) |
| tools/dashboard/, .github/workflows/dashboard.yml | Fortschrittsseite (board.py export) |
| tasks/*.md, tools/board.py | Ticket-Board mit allen 47 Tickets und Abhängigkeiten |
| .claude/commands/, .claude/settings.json | /start, /handoff, /finish; Board-Überblick beim Start |
| .github/ | PR-Vorlage, Workflow setzt Tickets beim Merge auf done |
| docs/WORKFLOW.md, docs/notes.md, docs/route_format.md | Ablauf für Menschen, Stolperfallen, Routen-Format |
| tools/viewer/reference/frostbyte-lift.html | 3D-Modell beider Roboter als Vorlage für den Viewer (SIM-07) |

## Autonomer Ablauf (wichtig)

Das Repo enthält ein Ticket-Board (tasks/, 47 Dateien) und feste Befehle für Claude Code.
Jede Sitzung – egal von wem von euch – liest das Board, nimmt sich das richtige Ticket und hinterlässt eine Übergabe.
Details für euch: docs/WORKFLOW.md. Regeln für die KI: CLAUDE.md, Abschnitt „Autonomer Ablauf“.

| Befehl in Claude Code | Wirkung |
| --- | --- |
| /auto | Dauerbetrieb: Ticket nach Ticket, dazwischen Reviews, bis nichts mehr frei ist |
| /review | offenen Pull Request eines anderen prüfen (mit Advisor), Empfehlung als Kommentar |
| /start | Board lesen, passendes Ticket übernehmen, Plan zeigen, loslegen |
| /start SIM-02 | dieses Ticket nehmen |
| /handoff | Stand sichern, Übergabe schreiben, pushen (vor Limit oder Feierabend) |
| /handoff frei | dito, und für die anderen zur Übernahme freigeben |
| /finish | Fertig-Kriterien prüfen, Pull Request öffnen |
| python3 tools/board.py summary | Überblick im Terminal, auch ohne Claude |

Jeder von euch stellt einmal seinen Namen ein: `git config user.name "<Name>"` – das ist der owner im Board.
GitHub CLI (`gh`) installieren und anmelden, damit /finish Pull Requests öffnen kann.
Im Repo unter Settings → Actions → General „Read and write permissions“ erlauben (für board-done.yml).
Modelle: Das Repo stellt Sonnet als Hauptmodell und Opus als Advisor ein (.claude/settings.json), für alle drei automatisch.
Zeigt `/advisor` trotzdem „off“ (z. B. wegen einer persönlichen Einstellung oder einer älteren Claude-Code-Version):
einmal `/advisor opus` eingeben, das wird als Standard gespeichert. Advisor-Antworten zählen mit zum Nutzungslimit.
Falls der Start-Hook in .claude/settings.json Probleme macht: Datei löschen, /start funktioniert trotzdem.

## Erste Aufträge an Claude Code (in dieser Reihenfolge, je einer pro Sitzung)

Einfach `/start` – das Board kennt die Reihenfolge über die Abhängigkeiten. Zu Beginn sind vier Tickets frei,
gut für drei Leute parallel: S-01 (Build, zuerst!), SIM-07 (Viewer), A-01 (Feld vermessen), S-04 (Formatierung).
Nach jedem Ticket: Pull Request von jemand anderem lesen lassen und mergen.

## Fortschrittsseite
Eine Webseite zeigt jederzeit den Stand: erledigte Tickets, Fortschritt pro Phase, wer woran arbeitet (mit Übergabe),
welcher Merge was freischaltet, die letzten Einträge und alle Tickets zum Filtern.
- Online: GitHub Pages unter https://<besitzer>.github.io/frostbyte/board/ (die Wurzel ist die öffentliche Teamseite, ADR-035) – wird bei jedem Push auf main neu gebaut (.github/workflows/dashboard.yml).
  Achtung: Eine Pages-Seite ist öffentlich erreichbar, auch wenn das Repo privat ist (nur mit GitHub Enterprise nicht).
  Sie zeigt Ticket-Titel und eure Board-Namen, keinen Code. Wollt ihr das nicht: Workflow löschen und lokal arbeiten.
- Lokal: python3 tools/board.py export site/board, dann site/board/index.html im Browser öffnen.
