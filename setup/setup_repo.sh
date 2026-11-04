#!/usr/bin/env bash
# Frostbyte – Repo einmalig aufsetzen (nur Admin). Läuft unter macOS, Linux und Windows (Git Bash).
# Aufruf:  bash setup/setup_repo.sh <github-name-2> <github-name-3>
# Optional: REPO=frostbyte OWNER=<user-oder-org> bash setup/setup_repo.sh ...
set -euo pipefail
cd "$(dirname "$0")/.."

say() { printf '\n== %s\n' "$*"; }
need() { command -v "$1" >/dev/null 2>&1 || { echo "FEHLT: $1 – bitte installieren und neu starten."; exit 1; }; }

say "Werkzeuge prüfen"
need git; need gh
PY=$(command -v python3 || command -v python || true); [ -n "$PY" ] || { echo "FEHLT: python3"; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "Nicht bei GitHub angemeldet: zuerst 'gh auth login' ausführen."; exit 1; }

REPO=${REPO:-frostbyte}
OWNER=${OWNER:-$(gh api user -q .login)}
URL="https://github.com/$OWNER/$REPO.git"

say "Inhalt prüfen"
[ -f docs/plan.md ] || { echo "docs/plan.md fehlt: Bauplan in Claude Docs als Markdown exportieren und als docs/plan.md speichern."; exit 1; }
"$PY" tools/board.py validate

say "Git-Repository anlegen"
if [ ! -d .git ]; then git init -q -b main; fi
git add -A
git -c user.name="${GIT_NAME:-$(gh api user -q .login)}" -c user.email="${GIT_EMAIL:-$(gh api user -q .id)+$(gh api user -q .login)@users.noreply.github.com}" \
    commit -q -m "Startpaket Frostbyte" || echo "(nichts Neues zu committen)"

if gh repo view "$OWNER/$REPO" >/dev/null 2>&1; then
  echo "Repo $OWNER/$REPO existiert schon – pushe nur."
  git remote get-url origin >/dev/null 2>&1 || git remote add origin "$URL"
  git push -u origin main
else
  gh repo create "$OWNER/$REPO" --private --source . --remote origin --push
fi

say "GitHub Actions dürfen schreiben (Board auf done setzen)"
gh api -X PUT "repos/$OWNER/$REPO/actions/permissions/workflow" -f default_workflow_permissions=write -F can_approve_pull_request_reviews=false \
  && echo ok || echo "Bitte von Hand: Settings → Actions → General → Read and write permissions"

say "Fortschrittsseite (GitHub Pages) einschalten"
if gh api -X POST "repos/$OWNER/$REPO/pages" -f build_type=workflow >/dev/null 2>&1 \
   || gh api -X PUT "repos/$OWNER/$REPO/pages" -f build_type=workflow >/dev/null 2>&1; then
  echo "ok – Seite: https://$OWNER.github.io/$REPO/board/ (Fortschritt; die Wurzel ist die Teamseite; erscheint nach dem ersten Workflow-Lauf)"
else
  echo "Pages nicht verfügbar. Für private Repos braucht es GitHub Pro (gratis im GitHub Student Developer Pack)."
  echo "Ohne Pages: python3 tools/board.py export site/board  und  site/board/index.html lokal öffnen."
fi

say "Teammitglieder einladen"
for u in "$@"; do
  gh api -X PUT "repos/$OWNER/$REPO/collaborators/$u" -f permission=push >/dev/null && echo "eingeladen: $u" || echo "Einladung fehlgeschlagen: $u"
done
[ $# -gt 0 ] || echo "(keine Namen übergeben – später: bash setup/setup_repo.sh name2 name3)"

say "Einrichtungsdatei für die anderen erzeugen"
OUT="$(cd .. && pwd)/FROSTBYTE_EINRICHTUNG.md"
sed "s#{{REPO_URL}}#$URL#g" setup/ONBOARDING_TEMPLATE.md > "$OUT"
echo "Fertig. Schick diese Datei an die anderen:  $OUT"
echo "Repo: https://github.com/$OWNER/$REPO"
