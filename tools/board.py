#!/usr/bin/env python3
"""Ticket-Board von Frostbyte. Quelle der Wahrheit: tasks/*.md (Kopfbereich zwischen ---).

Befehle:
  summary                 Überblick: Zähler, laufende Tickets, was als Nächstes frei ist
  next [--hw] [--owner N] nächstes freies Ticket (Abhängigkeiten erledigt, niemand dran)
  show ID                 Ticket ausgeben
  claim ID OWNER          Ticket übernehmen: status in_progress, owner, branch
  status ID STATUS        Status setzen (todo|in_progress|needs_hw|review|done|blocked)
  merged ID               nach dem Merge des PRs (board-done.yml): done, außer needs_hw bleibt needs_hw
  release ID              Ticket freigeben (status todo, owner leer) – z. B. wenn man aufhört
  validate                Konsistenz prüfen (läuft in CI)
  export DIR [--demo] [--fragment]  Fortschritts-Webseite nach DIR/index.html (+ board.json) schreiben
"""
import datetime, json, os, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TASKS = ROOT / "tasks"
STATUS = ["todo", "in_progress", "needs_hw", "review", "done", "blocked"]
DONE_LIKE = {"done"}
ORDER = ["0", "1", "1b", "2", "3", "4", "5", "6"]


def parse(path):
    text = path.read_text(encoding="utf-8")
    m = re.match(r"---\n(.*?)\n---\n", text, re.S)
    if not m:
        raise SystemExit(f"{path.name}: Kopfbereich fehlt")
    meta = {}
    for line in m.group(1).splitlines():
        if ":" in line:
            k, v = line.split(":", 1)
            meta[k.strip()] = v.strip()
    deps = meta.get("depends", "[]").strip("[]")
    meta["depends"] = [d.strip() for d in deps.split(",") if d.strip()]
    meta["_path"] = path
    meta["_text"] = text
    return meta


def load():
    return {t["id"]: t for t in (parse(p) for p in sorted(TASKS.glob("*.md")))}


def write_field(t, key, value):
    text = t["_path"].read_text(encoding="utf-8")
    head, rest = text.split("\n---\n", 1)
    head = re.sub(rf"^{key}:.*$", f"{key}: {value}", head, count=1, flags=re.M)
    t["_path"].write_text(head + "\n---\n" + rest, encoding="utf-8")


def sort_key(t):
    num = int(re.sub(r"\D", "", t["id"]) or 0)
    return (ORDER.index(t["phase"].strip('"')) if t["phase"].strip('"') in ORDER else 99, t["id"].split("-")[0], num)


def ready(tasks):
    out = []
    for t in tasks.values():
        if t["status"] != "todo" or t.get("owner"):
            continue
        if all(tasks[d]["status"] in DONE_LIKE for d in t["depends"]):
            out.append(t)
    return sorted(out, key=sort_key)


def cmd_summary(tasks):
    counts = {s: 0 for s in STATUS}
    for t in tasks.values():
        counts[t["status"]] = counts.get(t["status"], 0) + 1
    print("Board:", " · ".join(f"{s} {n}" for s, n in counts.items()), f"(gesamt {len(tasks)})")
    active = [t for t in tasks.values() if t["status"] in ("in_progress", "needs_hw", "review", "blocked")]
    if active:
        print("\nLaufend:")
        for t in sorted(active, key=sort_key):
            print(f"  {t['id']:7} {t['status']:11} {t.get('owner') or '-':12} {t['titel'][:70]}")
    rev = [t for t in tasks.values() if t["status"] in ("review", "needs_hw")]
    if rev:
        print("\nMerge bzw. Hardware-Test gibt frei:")
        for t in sorted(rev, key=sort_key):
            unlock = [u["id"] for u in tasks.values() if t["id"] in u["depends"] and u["status"] == "todo"
                      and all(tasks[d]["status"] == "done" or d == t["id"] for d in u["depends"])]
            print(f"  {t['id']:7} ({t['status']}) → {', '.join(unlock) if unlock else '–'}")
    r = ready(tasks)
    print("\nFrei und startbereit:")
    for t in r[:8]:
        print(f"  {t['id']:7} hw={t['hw']:4} {t['titel'][:70]}")
    if not r:
        print("  (keins – warten auf laufende Tickets oder Hardware)")


def cmd_next(tasks, args):
    want_hw = "--hw" in args
    r = [t for t in ready(tasks) if (t["hw"] == "ja") == want_hw]
    if not r:
        print("Kein freies Ticket" + (" mit Hardware." if want_hw else " ohne Hardware."))
        return 1
    print(r[0]["id"])


def status_after_merge(status):
    """Status nach dem Merge des PRs (board-done.yml). needs_hw bleibt: der Hardware-Test steht noch aus."""
    return status if status == "needs_hw" else "done"


def cmd_validate(tasks):
    errs = []
    for t in tasks.values():
        if t["status"] not in STATUS:
            errs.append(f"{t['id']}: unbekannter Status {t['status']}")
        for d in t["depends"]:
            if d not in tasks:
                errs.append(f"{t['id']}: Abhängigkeit {d} existiert nicht")
            elif t["status"] == "done" and tasks[d]["status"] != "done":
                errs.append(f"{t['id']}: done, aber Abhängigkeit {d} ist {tasks[d]['status']}")
        if t["status"] in ("in_progress", "needs_hw", "review") and not t.get("owner"):
            errs.append(f"{t['id']}: Status {t['status']} ohne owner")
        if t["_path"].stem != t["id"]:
            errs.append(f"{t['_path'].name}: id {t['id']} passt nicht zum Dateinamen")
    # Zyklen
    seen, stack = set(), set()
    def visit(i):
        if i in stack:
            errs.append(f"Abhängigkeitszyklus bei {i}")
            return
        if i in seen or i not in tasks:
            return
        stack.add(i)
        for d in tasks[i]["depends"]:
            visit(d)
        stack.discard(i)
        seen.add(i)
    for i in tasks:
        visit(i)
    for e in errs:
        print("FEHLER:", e)
    print("Board ok." if not errs else f"{len(errs)} Fehler.")
    return 1 if errs else 0


def sections(text):
    body = text.split("\n---\n", 1)[1] if "\n---\n" in text else text
    out = {}
    for part in re.split(r"^## ", body, flags=re.M)[1:]:
        head, _, rest = part.partition("\n")
        out[head.strip()] = rest.strip()
    return out


def git(*args):
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, timeout=5).stdout.strip()
    except Exception:
        return ""


def repo_url():
    if os.environ.get("GITHUB_REPOSITORY"):
        return os.environ.get("GITHUB_SERVER_URL", "https://github.com") + "/" + os.environ["GITHUB_REPOSITORY"]
    u = git("remote", "get-url", "origin")
    m = re.match(r"(?:https://|git@)([^/:]+)[/:](.+?)(?:\.git)?$", u)
    return f"https://{m.group(1)}/{m.group(2)}" if m else ""


def cmd_export(tasks, args):
    out = pathlib.Path(args[0] if args and not args[0].startswith("--") else "site")
    out.mkdir(parents=True, exist_ok=True)
    tickets = []
    for t in sorted(tasks.values(), key=sort_key):
        sec = sections(t["_text"])
        ueb = sec.get("Übergabe", "")
        if ueb.startswith("(aktueller Stand"):
            ueb = ""
        verlauf = [l.strip() for l in sec.get("Verlauf", "").splitlines() if l.strip() and not l.strip().startswith("<!--")]
        fertig = "\n".join(l.strip() for l in sec.get("Fertig, wenn …", "").splitlines() if l.strip())
        tickets.append({"id": t["id"], "phase": t["phase"].strip('"'), "titel": t["titel"], "status": t["status"],
                        "owner": t.get("owner", ""), "branch": t.get("branch", ""), "depends": t["depends"], "hw": t["hw"],
                        "fertig": fertig, "uebergabe": ueb, "verlauf": verlauf})
    data = {"generated": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
            "commit": git("rev-parse", "--short", "HEAD"), "repo_url": repo_url(), "demo": "--demo" in args, "tickets": tickets}
    (out / "board.json").write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    tpl = (ROOT / "tools" / "dashboard" / "template.html").read_text(encoding="utf-8")
    page = tpl.replace("__BOARD_JSON__", json.dumps(data, ensure_ascii=False).replace("</", "<\\/"))
    if "--fragment" not in args:
        page = ('<!doctype html>\n<html lang="de">\n<head>\n<meta charset="utf-8">\n'
                '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
                '<style>:root{padding-top:env(safe-area-inset-top,0px);padding-bottom:env(safe-area-inset-bottom,0px)}body{margin:0}[hidden]{display:none!important}</style>\n'
                '</head>\n<body>\n' + page + '\n</body>\n</html>\n')
    (out / "index.html").write_text(page, encoding="utf-8")
    print(f"Webseite geschrieben: {out / 'index.html'} ({len(tickets)} Tickets)")
    return 0


def main(argv):
    tasks = load()
    if not argv or argv[0] == "summary":
        return cmd_summary(tasks)
    c, a = argv[0], argv[1:]
    if c == "next":
        return cmd_next(tasks, a)
    if c == "show":
        print(tasks[a[0]]["_text"]); return 0
    if c == "claim":
        t = tasks[a[0]]
        if t["status"] != "todo" or t.get("owner"):
            print(f"{t['id']} ist schon {t['status']} ({t.get('owner') or '-'})"); return 1
        open_deps = [d for d in t["depends"] if tasks[d]["status"] != "done"]
        if open_deps:
            print(f"Warnung: offene Abhängigkeiten {open_deps}")
        write_field(t, "status", "in_progress")
        write_field(t, "owner", a[1])
        write_field(t, "branch", f"ticket/{t['id'].lower()}")
        print(f"{t['id']} übernommen von {a[1]}, Branch ticket/{t['id'].lower()}"); return 0
    if c == "status":
        if a[1] not in STATUS:
            print("Status muss einer von", STATUS, "sein"); return 1
        write_field(tasks[a[0]], "status", a[1]); print(f"{a[0]} → {a[1]}"); return 0
    if c == "merged":
        if not a or a[0] not in tasks:
            print(f"Ticket {a[0] if a else '?'} gibt es nicht"); return 1
        t = tasks[a[0]]
        new = status_after_merge(t["status"])
        if new == t["status"]:
            print(f"{t['id']} bleibt {new}" + (" (Hardware-Test steht aus)" if new == "needs_hw" else "")); return 0
        write_field(t, "status", new); print(f"{t['id']} → {new}"); return 0
    if c == "release":
        write_field(tasks[a[0]], "status", "todo"); write_field(tasks[a[0]], "owner", ""); write_field(tasks[a[0]], "branch", ""); print(f"{a[0]} freigegeben"); return 0
    if c == "validate":
        return cmd_validate(tasks)
    if c == "export":
        return cmd_export(tasks, a)
    print(__doc__); return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]) or 0)
