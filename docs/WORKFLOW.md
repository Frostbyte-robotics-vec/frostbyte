# So arbeiten wir zu dritt mit Claude Code

Jede Claude-Code-Sitzung – egal von wem, egal auf welchem Account – findet selbst heraus, was zu tun ist.
Das Gedächtnis des Projekts liegt komplett im Repo:

| Ort | Was steht dort | Wer schreibt |
| --- | --- | --- |
| tasks/<ID>.md | Status, Besitzer, Branch, Fertig-Kriterien, Übergabe, Verlauf | KI über tools/board.py und /handoff |
| docs/plan.md | der ganze Bauplan (Architektur, Spezifikationen) | ihr, selten |
| CLAUDE.md | Regeln + Ablauf für jede Sitzung | ihr, selten |
| docs/decisions/ | Entscheidungen (ADR) | KI oder ihr |
| docs/notes.md | Stolperfallen | KI |
| git log, Pull Requests | was genau geändert wurde | KI, Review durch euch |

## Euer Ablauf als Mensch

1. Claude Code im Repo starten → es zeigt automatisch den Board-Überblick.
2. `/auto` eingeben (Dauerbetrieb) oder `/start` für genau ein Ticket (`/start SIM-02` für ein bestimmtes).
3. Arbeiten lassen, Zwischenergebnisse anschauen.
4. Limit wird knapp (`/status`) oder du hörst auf → `/handoff` (oder `/handoff frei`, wenn ein anderer übernehmen soll).
5. Ticket fertig → `/finish` → Pull Request erscheint auf GitHub.
6. Eine KI-Sitzung prüft den PR mit `/review` und kommentiert „Empfehlung: mergen/ändern“.
7. Ein Mensch liest kurz und merged → das Ticket springt automatisch auf done, abhängige Tickets werden frei.
   Tipp: `python3 tools/board.py summary` zeigt unter „Merge gibt frei“, welcher Merge am meisten freischaltet.

## Modelle
Sonnet macht die Arbeit, Opus berät bei schweren Entscheidungen (Advisor). Eingestellt in .claude/settings.json.
Wann die KI den Advisor fragt, steht in CLAUDE.md. Prüfen: `/advisor` zeigt das aktive Berater-Modell.

## Status-Bedeutung

| Status | Bedeutung |
| --- | --- |
| todo | frei |
| in_progress | jemand arbeitet daran (owner, branch) |
| needs_hw | Code fertig, Messung am Roboter durch einen Menschen nötig (Anleitung im Ticket) |
| review | Pull Request offen |
| done | gemerged |
| blocked | wartet auf etwas außerhalb (Grund in der Übergabe) |

## Wenn zwei gleichzeitig dasselbe wollen
Das Übernehmen ist ein winziger Commit direkt auf main. Wer zuerst pusht, hat das Ticket; der andere
bekommt beim Push eine Ablehnung, zieht neu und nimmt das nächste freie Ticket.

## Fortschrittsseite
Eine Webseite zeigt jederzeit den Stand: erledigte Tickets, Fortschritt pro Phase, wer woran arbeitet (mit Übergabe),
welcher Merge was freischaltet, die letzten Einträge und alle Tickets zum Filtern.
- Online: GitHub Pages unter https://<besitzer>.github.io/frostbyte/ – wird bei jedem Push auf main neu gebaut (.github/workflows/dashboard.yml).
  Achtung: Eine Pages-Seite ist öffentlich erreichbar, auch wenn das Repo privat ist (nur mit GitHub Enterprise nicht).
  Sie zeigt Ticket-Titel und eure Board-Namen, keinen Code. Wollt ihr das nicht: Workflow löschen und lokal arbeiten.
- Lokal: python3 tools/board.py export site, dann site/index.html im Browser öffnen.
