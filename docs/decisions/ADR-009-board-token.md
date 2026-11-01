# ADR-009: board-done.yml pusht mit BOARD_TOKEN

Status: angenommen (1. Nov 2026), Ticket S-06.

## Entscheidung
- Der Workflow checkt `main` mit dem Secret `BOARD_TOKEN` aus (fine-grained PAT des Repo-Admins, nur Repo `frostbyte`, Contents: read/write). Der Push nutzt damit die Admin-Ausnahme des Rulesets.
- Verworfen: „GitHub Actions“ in die Bypass-Liste. Bei Repos unter persönlichem Konto oft nicht wählbar, und Pushes mit dem Standard-`GITHUB_TOKEN` lösen keine weiteren Workflows aus (Fortschritts-Website bliebe stehen).

## Folgen
- Der Token läuft ab (Custom, ca. 1 Jahr). Danach bleibt das Board stehen: neuen Token erzeugen und nur den Wert des Secrets `BOARD_TOKEN` ersetzen, der Workflow bleibt unverändert.
- Der Admin muss den Token und das Secret selbst anlegen (Anleitung im Ticket S-06); ohne Secret schlägt der Checkout fehl.
