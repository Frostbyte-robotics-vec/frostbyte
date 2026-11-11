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
Stand: fertig, PR offen. Lizenz geklärt: Schweizer VEX-Vertreter hat am 2026-11-11 mündlich zugestimmt (arjanssen); Release `vex-step-1.0.0` mit
`vex_step_1_0_0.zip` (116 837 198 Byte, SHA-256 wie in tools/fetch_cad_lib.py) ist angelegt. Echter Lauf auf dem Mac des Menschen:
243 STEP-Dateien + Marker in cad/lib/, zweiter Lauf tut nichts. Der SSL-Fehler (python.org-Python ohne Zertifikate) ist behoben:
bei Zertifikatsfehler weicht das Script auf curl aus (System-Speicher), die Prüfsumme gilt weiter; drei neue Tests (mit Mock).
Am Rechner zu prüfen: auf einem zweiten Laptop `/start` bzw. `python3 tools/fetch_cad_lib.py` laufen lassen (Windows hat ab 10 curl;
sonst „Install Certificates“). Schriftliche VEX-Bestätigung gibt es nicht; wer sie bekommt, legt sie in ADR-045 ab.

## Verlauf
<!-- je Sitzung eine Zeile anhängen: Datum · Person · was gemacht wurde · Commit -->
2026-11-10 · arjanssen (KI) · Ticket angelegt und übernommen · –
2026-11-10 · arjanssen (KI) · fetch_cad_lib.py + Test, /start, cad/BIBLIOTHEK.md · –
2026-11-11 · arjanssen (KI) · ANLEITUNG.md ergänzt, Tests/Format/validate grün, PR geöffnet; Release + Lizenzprüfung offen (Mensch) · –
2026-11-11 · arjanssen (KI) · Branch gepusht, Tests grün; Release/Lizenz offen → blocked · –
2026-11-11 · arjanssen (KI) · VEX-Freigabe (mündlich), Release angelegt, curl-Ausweg bei SSL-Fehler, echter Download getestet (243 Dateien) · siehe Branch
