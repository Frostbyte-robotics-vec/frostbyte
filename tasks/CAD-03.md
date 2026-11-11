---
id: CAD-03
phase: "1"
titel: VEX-STEP-Bibliothek automatisch auf jeden Laptop holen (Release-ZIP, Download-Script, /start)
status: in_progress
owner: arjanssen
branch: ticket/cad-03
depends: [CAD-02]
hw: nein
---

## Ziel
Alle im Team haben die VEX-Teilebibliothek als STEP (243 Dateien, aus CAD-02) in `cad/lib/`, ohne etwas zu tun: `/start` ruft das Download-Script auf, das beim ersten Mal das ZIP aus einem GitHub-Release lädt und entpackt. `cad/lib/` ist nicht im Git.

## Fertig, wenn …
- `tools/fetch_cad_lib.py` (nur Standardbibliothek): lädt ZIP aus dem Release, prüft SHA-256, entpackt nach `cad/lib/`, tut nichts wenn schon vorhanden (Marker-Datei), ZIP-Pfade ausserhalb des Ziels werden abgelehnt
- Test `tools/test_fetch_cad_lib.py` (lokales ZIP über file://): Erstlauf, zweiter Lauf ohne Download, falsche Prüfsumme, unsicherer ZIP-Pfad
- `cad/lib/` in `.gitignore`; `/start` (`.claude/commands/start.md`) ruft das Script auf; `cad/BIBLIOTHEK.md` beschreibt es (und wie man eine neue Fassung veröffentlicht)
- Release `vex-step-1.0.0` mit `vex_step_1_0_0.zip` existiert (Mensch gibt frei, siehe Übergabe)
- `python3 tools/board.py validate` grün

## Kontext
Repo ist öffentlich. Weitergabe der VEX-CAD-Bibliothek vorher auf Lizenz prüfen (Übergabe).

## Hardware-Prüfung
—

## Übergabe
Stand: alles Code-seitige fertig und gepusht (Script, Test, .gitignore, /start, cad/BIBLIOTHEK.md, cad/tools/ANLEITUNG.md). Alle Python-Tests (115),
ctest, Format und board.py validate grün. Advisor-Review: erst /finish, wenn das Release existiert.
Blockiert, nur ein Mensch kann das klären: (1) VEX-Lizenz für Weitergabe prüfen (Repo ist öffentlich); (2) Release `vex-step-1.0.0` mit
`vex_step_1_0_0.zip` (SHA256 in tools/fetch_cad_lib.py) anlegen. Mensch hat am 2026-11-11 „nicht jetzt“ gesagt.
Danach: `gh release view vex-step-1.0.0 --json assets`, dann `python3 tools/fetch_cad_lib.py` echt laufen lassen (243 Dateien + Marker in cad/lib/),
dann /finish (PR, review).
Stolperfalle, vor dem Merge klären: Auf dem Mac des Menschen schlägt der Download mit SSL CERTIFICATE_VERIFY_FAILED fehl
(python.org-Python ohne Zertifikate: „Install Certificates.command“ ausführen oder certifi nutzen). Der Fehlerpfad ist sauber
(Meldung, Exit 1, kein cad/lib/ und kein Marker), aber der echte Download ist dort bisher ungetestet.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-10 · arjanssen (KI) · Ticket angelegt und übernommen · –
2026-11-10 · arjanssen (KI) · fetch_cad_lib.py + Test, /start, cad/BIBLIOTHEK.md · –
2026-11-11 · arjanssen (KI) · ANLEITUNG.md ergänzt, Tests/Format/validate grün, PR geöffnet; Release + Lizenzprüfung offen (Mensch) · –
2026-11-11 · arjanssen (KI) · Branch gepusht, Tests grün; Release/Lizenz offen → blocked · –
