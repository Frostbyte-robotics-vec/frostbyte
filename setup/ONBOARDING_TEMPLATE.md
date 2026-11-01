# Frostbyte – Einrichtung für Claude Code

> So benutzt du diese Datei: Leeren Ordner anlegen (z. B. `frostbyte`), diese Datei hineinlegen,
> den Ordner in VS Code öffnen, Claude-Code-Chat öffnen und diese Datei in den Chat ziehen. Fertig – den Rest macht Claude.

---

**Anweisung an Claude Code:** Richte für mich die Arbeitsumgebung des VEX-U-Teams Frostbyte ein.
Arbeite die Schritte der Reihe nach ab und führe die Befehle selbst aus. Frag mich nur, wenn ich etwas eingeben,
mich anmelden oder eine Installation bestätigen muss. Erkläre jeden Schritt in einem Satz auf Deutsch.

- Repository: {{REPO_URL}}
- Zielordner: der aktuell geöffnete Ordner (enthält nur diese Datei)

1. **System erkennen:** Betriebssystem und Shell feststellen.
2. **Werkzeuge prüfen**, fehlende nach kurzer Rückfrage installieren (Windows: winget, macOS: brew, Linux: apt):
   git, GitHub CLI (gh), Python ≥ 3.10, CMake ≥ 3.20, ein C++20-Compiler
   (Windows: Visual Studio Build Tools mit C++-Workload; macOS: `xcode-select --install`; Linux: build-essential).
   Nach einer Installation unter Windows ggf. ein neues Terminal öffnen, damit der PATH stimmt.
3. **GitHub-Anmeldung:** `gh auth status`. Nicht angemeldet → mich bitten, im Terminal `gh auth login` auszuführen
   (GitHub.com, HTTPS, Browser) und Bescheid zu geben. Danach `gh auth setup-git`.
4. **Einladung annehmen:** offene Einladungen mit `gh api user/repository_invitations` lesen; die für dieses Repository
   mit `gh api -X PATCH user/repository_invitations/<id>` annehmen. Keine Einladung da → mir sagen, ich soll den Admin fragen.
5. **Repo holen** (in den aktuellen Ordner, diese Datei bleibt dabei liegen):
   `git init -b main`, `git remote add origin {{REPO_URL}}`, `git fetch origin`, `git checkout -B main --track origin/main`.
6. **Git-Identität:** mich nach meinem Namen fragen, genau so, wie er im Ticket-Board stehen soll.
   `git config user.name "<Name>"`; als E-Mail die GitHub-noreply-Adresse (`<id>+<login>@users.noreply.github.com`, Werte aus `gh api user`).
7. **PROS (optional):** fragen, ob ich Programme auf das V5-Brain laden werde. Ja → `code --install-extension sigbots.pros`
   (die PROS-Extension bringt CLI und Toolchain mit). Nein → überspringen, für Simulation und Tests nicht nötig.
8. **Prüfen:** `python3 tools/board.py validate` und `python3 tools/board.py summary` (unter Windows ggf. `python`).
   Zeigt das Board S-02 als done: zusätzlich `cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test`.
9. **Aufräumen:** Diese Einrichtungsdatei löschen. Sie gehört nicht ins Repo und darf nicht committet werden.
10. **Abschluss** – zeig mir den Board-Überblick und sag mir genau diese drei Dinge:
    1. Diesen Chat schließen und in diesem Ordner einen **neuen** Claude-Code-Chat öffnen
       (erst dann werden CLAUDE.md, die Befehle /start /handoff /finish und die Modell-Einstellungen geladen).
    2. Im neuen Chat `/advisor` eingeben; steht dort nicht Opus → `/advisor opus`.
    3. `/auto` eingeben. Ab da arbeitet Claude selbstständig nach dem Board (einzelnes Ticket: `/start`).
