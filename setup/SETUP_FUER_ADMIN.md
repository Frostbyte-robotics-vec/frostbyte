# Frostbyte – Repo aufsetzen (Admin)

> So benutzt du diese Datei: Startpaket entpacken, den Ordner `frostbyte-start` in VS Code öffnen,
> Claude-Code-Chat öffnen und diese Datei in den Chat ziehen.

---

**Anweisung an Claude Code:** Setze das GitHub-Repository des Teams Frostbyte aus diesem Ordner auf.
Führe Befehle selbst aus; frag mich nur, wenn ich etwas eingeben, mich anmelden oder bestätigen muss. Erkläre jeden Schritt in einem Satz auf Deutsch.

1. **Werkzeuge:** git, GitHub CLI (gh) und Python 3 prüfen; fehlende nach Rückfrage installieren (winget / brew / apt).
2. **Anmeldung:** `gh auth status`; nicht angemeldet → mich bitten, `gh auth login` im Terminal auszuführen.
3. **Bauplan:** Prüfen, ob `docs/plan.md` existiert. Wenn nicht: mich bitten, das Dokument „Codebasis – Bauplan für Claude Code“
   in Claude Docs als Markdown zu exportieren und als `docs/plan.md` in diesen Ordner zu legen. Warten, bis die Datei da ist.
4. **Angaben erfragen:** Repo-Name (Vorschlag `frostbyte`), Besitzer (mein GitHub-Konto oder eine Organisation)
   und die GitHub-Benutzernamen der beiden anderen Teammitglieder.
5. **Mein Name im Board:** nach meinem Namen fragen und `git config --global user.name` setzen, falls noch leer.
6. **Ausführen:** `REPO=<name> OWNER=<besitzer> bash setup/setup_repo.sh <name2> <name3>`
   (Windows: in Git Bash). Bei Fehlern die Ursache erklären und beheben, dann erneut ausführen – das Skript ist wiederholbar.
7. **Abschluss:** Mir sagen,
   - wo die erzeugte Datei `FROSTBYTE_EINRICHTUNG.md` liegt (ein Ordner über diesem) – die schicke ich den anderen,
   - dass ich selbst jetzt einen neuen Claude-Code-Chat in diesem Ordner öffnen, `/advisor` prüfen und `/auto` eingeben soll.
