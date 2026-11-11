#!/usr/bin/env python3
"""Spielelemente aus dem Feld-CAD (VEX 276-9250-000, STEP) als GLB für den Browser-Viewer exportieren (SIM-37).

  python3 tools/export_field_models.py <feld.step> [--out tools/viewer/models] [--field field/field.json]

Schreibt je Variante eine .glb, manifest.json und models.js (alles in einer Skript-Datei, damit der Viewer auch von file://
funktioniert). Nur Optik: Kollision und Physik der Simulation kommen weiter aus sim/sim_params.json.
Die Ausgabe ist von VEX abgeleitet und wird nicht committet (.gitignore, Lizenzfrage siehe CAD-03); die STEP-Datei kommt nie ins Repo.
Braucht cadquery (OCP) nur zum Erzeugen: pip install cadquery. Der Rest dieser Datei läuft mit der Standardbibliothek.

Koordinaten: Der Viewer benutzt dieselbe Achsenzuordnung wie das CAD (x, y nach oben, z = −y_in); es wird nur von mm auf Zoll
umgerechnet. Pins und Cups behalten den CAD-Ursprung (Mitte der Höhe, Achse = y), Goals stehen mit dem Boden auf y = 0,
Loader sind auf den Feldpunkt aus field.json bezogen und schon in Spielrichtung gedreht.
"""
import argparse
import base64
import json
import re
import struct
import sys
from pathlib import Path

MM_PER_IN = 25.4
GLB_VERSION = 1

# Obergrenzen für die Dreiecke je Netz (Browser, 7 Goals und rund 120 Pins/Cups gleichzeitig; Netze werden je Variante geteilt).
# Pins und Cups haben im CAD über 1000 Flächen, unter etwa 11 000 Dreiecken kommt man ohne eigenes Dezimieren nicht.
TRI_LIMIT = {"goal": 40000, "pin": 12000, "cup": 12000, "loader": 5000}

# Höhe nach Game Manual v2.1 (Pin 165 mm, Cup 164,5 mm = 6,5 Zoll), Toleranz in Zoll
MANUAL_HEIGHT_IN = {"pin": 6.5, "cup": 6.5}
MANUAL_HEIGHT_TOL_IN = 0.05

# Variante -> Bauteilname im STEP, Art, Drehpunkt. Namen fest wie im CAD (276-9250-000, 2026-04-26).
VARIANTS = {
    "pin_red_blue":        {"step": "276-9250-80x_Red-Blue",       "kind": "pin",  "pivot": "origin"},
    "pin_red_neutral":     {"step": "276-9250-80x_Red-Neutral",    "kind": "pin",  "pivot": "origin"},
    "pin_blue_neutral":    {"step": "276-9250-80x_Blue-Neutral",   "kind": "pin",  "pivot": "origin"},
    "pin_neutral_neutral": {"step": "276-9250-80x_Neutral-Neutral", "kind": "pin", "pivot": "origin"},
    "cup":                 {"step": "276-9250-81x",                "kind": "cup",  "pivot": "origin"},
    "goal_red":            {"step": "276-9250-840_Red 2",          "kind": "goal", "pivot": "floor"},
    "goal_blue":           {"step": "276-9250-840_Blue 2",         "kind": "goal", "pivot": "floor"},
    "goal_neutral":        {"step": "276-9250-840_Neutral Goal 1", "kind": "goal", "pivot": "floor"},
    "goal_midfield":       {"step": "276-9250-840_Neutral Goal 0", "kind": "goal", "pivot": "floor", "limit": 70000},  # nur ein Exemplar
    "loader_red":          {"step": "276-9250-031_Red Loader",     "kind": "loader", "pivot": "loader"},
    "loader_blue":         {"step": "276-9250-031_Blue Loader",    "kind": "loader", "pivot": "loader"},
}
# Alliance-Goals stehen auf dem Boden: ihre Modellunterkante ist die Bezugshöhe für "floor"
FLOOR_REFERENCE = "276-9250-840_Red 2"
# Welcher Feldpunkt aus field.json gehört zu welchem Loader (Seite/y-Vorzeichen ergeben sich aus der Position im CAD)
LOADER_FIELD_ID = {"loader_red": "LOADER_RED_1", "loader_blue": "LOADER_BLUE_2"}


# ---------- reine Teile (Standardbibliothek) ----------

def convert_part(part, shift_mm=(0.0, 0.0, 0.0)):
    """Teil von mm nach Zoll; vorher um shift_mm verschieben. Eingabe bleibt unverändert."""
    sx, sy, sz = shift_mm
    pos = []
    for i in range(0, len(part["pos"]), 3):
        pos.append((part["pos"][i] + sx) / MM_PER_IN)
        pos.append((part["pos"][i + 1] + sy) / MM_PER_IN)
        pos.append((part["pos"][i + 2] + sz) / MM_PER_IN)
    out = {"name": part["name"], "rgba": list(part["rgba"]), "pos": pos, "idx": list(part["idx"])}
    if part.get("nrm"):
        out["nrm"] = list(part["nrm"])
    return out


def parts_bounds(parts):
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    for p in parts:
        for i, v in enumerate(p["pos"]):
            a = i % 3
            lo[a] = min(lo[a], v)
            hi[a] = max(hi[a], v)
    return lo, hi


def tri_count(parts):
    return sum(len(p["idx"]) // 3 for p in parts)


def pivot_shift_mm(mode, origin_above_floor_mm):
    """Verschiebung, die der Modellursprung für den gewählten Drehpunkt braucht (mm)."""
    if mode == "origin":
        return (0.0, 0.0, 0.0)
    if mode == "floor":
        return (0.0, float(origin_above_floor_mm), 0.0)
    raise ValueError(f"unbekannter Drehpunkt: {mode}")


def loader_shift_mm(field_xy_in, floor_mm):
    """Verschiebung für ein schon in Weltlage gedrehtes Loader-Netz: der Feldpunkt aus field.json (z = −y) auf dem Boden
    (floor_mm, y im CAD) wird Ursprung."""
    fx, fy = field_xy_in
    return (-fx * MM_PER_IN, -float(floor_mm), fy * MM_PER_IN)


# Farben (RGBA 0..1). Das STEP trägt nur die SolidWorks-Standardfarbe, deshalb Färbung nach Bauteilname.
RED = (0.753, 0.224, 0.169, 1.0)      # wie die Feldobjekte im Viewer (0xc0392b)
BLUE = (0.118, 0.420, 0.682, 1.0)     # 0x1e6bae
NEUTRAL = (0.604, 0.631, 0.659, 1.0)  # 0x9aa1a8
YELLOW = (0.878, 0.702, 0.255, 1.0)   # 0xe0b341
DARK = (0.220, 0.231, 0.251, 1.0)
CLEAR = (0.85, 0.90, 0.95, 0.35)
WHITE = (0.93, 0.93, 0.93, 1.0)
GREY = (0.60, 0.60, 0.62, 1.0)

HARDWARE_PREFIXES = ("SBT", "THF", "NNY", "94629A", "SPTM")  # Schrauben, Muttern, Gewindeteile: unsichtbar, viele Dreiecke


def is_hardware(name):
    return name.startswith(HARDWARE_PREFIXES)


def color_for(variant, component):
    """Farbe eines Bauteils einer Variante (nach Namen, siehe oben)."""
    alliance = {"red": RED, "blue": BLUE}.get(variant.split("_")[1] if "_" in variant else "", NEUTRAL)
    if variant.startswith("pin_"):
        for key, col in (("Bright Red", RED), ("Bright Blue", BLUE), ("Yellow", YELLOW)):
            if key in component:
                return col
    elif variant == "cup":
        if "Clear" in component:
            return CLEAR
        if "Dark Gray" in component:
            return (0.26, 0.27, 0.29, 1.0)
    elif variant.startswith("goal_"):
        if "003_Master" in component:
            return alliance
        if "AprilTag" in component:
            return WHITE
        if "4847-011" in component or "9250-004" in component or "9250-005" in component:
            return DARK
    elif variant.startswith("loader_"):
        return alliance
    return GREY


def _pad(b, fill):
    return b + fill * (-len(b) % 4)


def write_glb(parts):
    """Binäres glTF 2.0: ein Mesh, je Teil eine Primitive mit eigenem Material. Gleiche Eingabe gibt gleiche Bytes."""
    if not parts:
        raise ValueError("keine Teile")
    blob = bytearray()
    views, accessors, prims, mats = [], [], [], []

    def add(data, target):
        off = len(blob)
        blob.extend(data)
        blob.extend(b"\0" * (-len(blob) % 4))
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(data), "target": target})
        return len(views) - 1

    for p in parts:
        pos = p["pos"]
        n = len(pos) // 3
        lo = [min(pos[a::3]) for a in range(3)]
        hi = [max(pos[a::3]) for a in range(3)]
        v = add(struct.pack(f"<{len(pos)}f", *pos), 34962)
        accessors.append({"bufferView": v, "componentType": 5126, "count": n, "type": "VEC3", "min": lo, "max": hi})
        attrs = {"POSITION": len(accessors) - 1}
        if p.get("nrm"):
            v = add(struct.pack(f"<{len(p['nrm'])}f", *p["nrm"]), 34962)
            accessors.append({"bufferView": v, "componentType": 5126, "count": n, "type": "VEC3"})
            attrs["NORMAL"] = len(accessors) - 1
        v = add(struct.pack(f"<{len(p['idx'])}I", *p["idx"]), 34963)
        accessors.append({"bufferView": v, "componentType": 5125, "count": len(p["idx"]), "type": "SCALAR"})
        r, g, b, a = p["rgba"]
        mats.append({"name": p["name"], "alphaMode": "BLEND" if a < 1.0 else "OPAQUE", "doubleSided": a < 1.0,
                     "pbrMetallicRoughness": {"baseColorFactor": [r, g, b, a], "metallicFactor": 0.0, "roughnessFactor": 0.6}})
        prims.append({"attributes": attrs, "indices": len(accessors) - 1, "material": len(mats) - 1, "mode": 4})

    doc = {"asset": {"version": "2.0", "generator": "frostbyte export_field_models"},
           "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0}], "meshes": [{"primitives": prims}],
           "materials": mats, "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(blob)}]}
    js = _pad(json.dumps(doc, separators=(",", ":")).encode("utf-8"), b" ")
    bn = _pad(bytes(blob), b"\0")
    total = 12 + 8 + len(js) + 8 + len(bn)
    return (struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(js), b"JSON") + js
            + struct.pack("<I4s", len(bn), b"BIN\0") + bn)


def read_glb(data):
    """GLB zerlegen (für Prüfungen): {"json": ..., "bin": bytes}."""
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or length != len(data):
        raise ValueError("kein gültiges GLB 2.0")
    off, out = 12, {}
    while off < length:
        size, kind = struct.unpack_from("<I4s", data, off)
        chunk = data[off + 8:off + 8 + size]
        out["json" if kind == b"JSON" else "bin"] = json.loads(chunk) if kind == b"JSON" else bytes(chunk)
        off += 8 + size
    return out


def glb_primitive(doc, i):
    """Positionen (Liste von (x, y, z)) und Indizes der i-ten Primitive."""
    j, blob = doc["json"], doc["bin"]
    prim = j["meshes"][0]["primitives"][i]

    def read(acc_i, fmt, comps):
        acc = j["accessors"][acc_i]
        bv = j["bufferViews"][acc["bufferView"]]
        n = acc["count"] * comps
        return struct.unpack_from(f"<{n}{fmt}", blob, bv["byteOffset"])

    flat = read(prim["attributes"]["POSITION"], "f", 3)
    return [tuple(flat[k:k + 3]) for k in range(0, len(flat), 3)], list(read(prim["indices"], "I", 1))


def manifest_entry(kind, variant, parts, pivot="origin", file=None):
    lo, hi = parts_bounds(parts)
    teile = []
    for p in parts:
        plo, phi = parts_bounds([p])
        teile.append({"name": p["name"], "rgba": [round(v, 3) for v in p["rgba"]],
                      "y_min_in": round(plo[1], 4), "y_max_in": round(phi[1], 4)})
    return {"kind": kind, "teile": teile, "variant": variant, "file": file or f"{variant}.glb", "pivot": pivot,
            "tris": tri_count(parts), "height_in": round(hi[1] - lo[1], 4),
            "top_in": round(hi[1], 4), "min_in": [round(v, 4) for v in lo], "max_in": [round(v, 4) for v in hi]}


def make_manifest(entries):
    return {"version": GLB_VERSION, "einheiten": "Zoll",
            "achsen": "wie Viewer: x = x_in, y oben, z = −y_in; Pin/Cup Ursprung Mitte der Höhe, Goal Boden y = 0",
            "quelle": "VEX Feld-CAD 276-9250-000 (2026-04-26), nur Optik", "modelle": entries}


def check_tri_limit(kind, tris, limit=None):
    limit = TRI_LIMIT[kind] if limit is None else limit
    if tris > limit:
        raise ValueError(f"{kind}: {tris} Dreiecke, erlaubt {limit}")


def check_height_against_manual(kind, height_in):
    want = MANUAL_HEIGHT_IN.get(kind)
    if want is not None and abs(height_in - want) > MANUAL_HEIGHT_TOL_IN:
        raise ValueError(f"{kind}: Höhe {height_in:.3f} Zoll, Manual {want} Zoll")


GOAL_FIELD_TYPE = {"goal_red": "alliance_red", "goal_blue": "alliance_blue", "goal_neutral": "neutral", "goal_midfield": "midfield"}
GOAL_TOP_TOL_IN = 0.1


def check_goal_top(variant, top_in, field):
    """Oberkante des Goal-Modells gegen hoehe_in in field.json (gleiche CAD-Quelle, nur Plausibilitätsprüfung)."""
    typ = GOAL_FIELD_TYPE[variant]
    heights = {g["hoehe_in"] for g in field["goals"] if g["typ"] == typ and g.get("hoehe_in") is not None}
    if heights and min(abs(top_in - h) for h in heights) > GOAL_TOP_TOL_IN:
        raise ValueError(f"{variant}: Oberkante {top_in:.2f} Zoll, field.json {sorted(heights)}")


def bundle_js(manifest, glbs):
    """Eine Skript-Datei mit Manifest und allen GLB (base64); lässt sich per <script> von file:// laden."""
    payload = {"manifest": manifest, "glb": {k: base64.b64encode(v).decode("ascii") for k, v in sorted(glbs.items())}}
    return "window.FB_MODELS = " + json.dumps(payload, separators=(",", ":")) + ";\n"


# ---------- OCP-Teil: nur zum Erzeugen der Modelle ----------

def have_ocp():
    try:
        import OCP  # noqa: F401
        return True
    except ImportError:
        return False


def tessellate(shape, name, rgba, deflection_mm=0.5, angle_rad=0.5, faces=None):
    """OCC-Shape in ein Teil (mm, y oben) umwandeln. faces: nur diese Flächen (sonst alle)."""
    from OCP.BRep import BRep_Tool
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED
    from OCP.TopExp import TopExp_Explorer
    from OCP.TopLoc import TopLoc_Location
    from OCP.TopoDS import TopoDS

    BRepMesh_IncrementalMesh(shape, deflection_mm, False, angle_rad, True)
    if faces is None:
        faces = []
        ex = TopExp_Explorer(shape, TopAbs_FACE)
        while ex.More():
            faces.append(TopoDS.Face_s(ex.Current()))
            ex.Next()
    pos, nrm, idx = [], [], []
    for f in faces:
        loc = TopLoc_Location()
        t = BRep_Tool.Triangulation_s(f, loc)
        if t is None:
            continue
        tr = loc.Transformation()
        base = len(pos) // 3
        has_n = t.HasNormals()
        for i in range(1, t.NbNodes() + 1):
            p = t.Node(i).Transformed(tr)
            pos += [p.X(), p.Y(), p.Z()]
            if has_n:
                nn = t.Normal(i)
                nn.Transform(tr)
                if f.Orientation() == TopAbs_REVERSED:
                    nn.Reverse()
                nrm += [nn.X(), nn.Y(), nn.Z()]
        rev = f.Orientation() == TopAbs_REVERSED
        for i in range(1, t.NbTriangles() + 1):
            a, b, c = t.Triangle(i).Get()
            a, b, c = a - 1 + base, b - 1 + base, c - 1 + base
            idx += [a, c, b] if rev else [a, b, c]
    part = {"name": name, "rgba": list(rgba), "pos": pos, "idx": idx}
    if has_n_all(pos, nrm):
        part["nrm"] = nrm
    return [part] if idx else []


def has_n_all(pos, nrm):
    return bool(nrm) and len(nrm) == len(pos)


# (Abweichung in mm, Winkel in rad), fein zu grob, bis die Dreiecksgrenze der Art eingehalten ist
MESH_STEPS = ((0.3, 0.5), (1.2, 0.5), (1.2, 1.0), (2.4, 1.0), (2.4, 1.5))


def read_step(path):
    """STEP mit Baugruppenstruktur lesen. Gibt (ShapeTool, Namensfunktion, Dokument) zurück; das Dokument muss leben bleiben."""
    from OCP.STEPCAFControl import STEPCAFControl_Reader
    from OCP.TCollection import TCollection_ExtendedString
    from OCP.TDataStd import TDataStd_Name
    from OCP.TDocStd import TDocStd_Document
    from OCP.XCAFApp import XCAFApp_Application
    from OCP.XCAFDoc import XCAFDoc_DocumentTool

    app = XCAFApp_Application.GetApplication_s()
    doc = TDocStd_Document(TCollection_ExtendedString("x"))
    app.NewDocument(TCollection_ExtendedString("MDTV-XCAF"), doc)
    reader = STEPCAFControl_Reader()
    reader.SetNameMode(True)
    reader.ReadFile(str(path))
    reader.Transfer(doc)
    st = XCAFDoc_DocumentTool.ShapeTool_s(doc.Main())

    def name(label):
        n = TDataStd_Name()
        return n.Get().ToExtString() if label.FindAttribute(TDataStd_Name.GetID_s(), n) else "?"

    return st, name, doc


def find_instances(st, name, wanted, max_depth=3):
    """Alle Vorkommen der Bauteile mit diesen Namen: {name: [(Referenz-Label, Weltlage)]}."""
    from OCP.TDF import TDF_Label, TDF_LabelSequence
    from OCP.TopLoc import TopLoc_Location

    found = {w: [] for w in wanted}

    def walk(label, acc, depth):
        ref = label
        if st.IsReference_s(label):
            ref = TDF_Label()
            st.GetReferredShape_s(label, ref)
        nm = name(ref)
        if nm in found:
            found[nm].append((ref, acc))
        if depth < max_depth:
            comps = TDF_LabelSequence()
            st.GetComponents_s(ref, comps)
            for i in range(1, comps.Length() + 1):
                c = comps.Value(i)
                walk(c, acc.Multiplied(st.GetLocation_s(c)), depth + 1)

    top = TDF_LabelSequence()
    st.GetFreeShapes(top)
    for i in range(1, top.Length() + 1):
        walk(top.Value(i), TopLoc_Location(), 0)
    return found


def component_shapes(st, name, ref):
    """[(Bauteilname, Shape)] der sichtbaren Komponenten; ohne Komponenten das Bauteil selbst."""
    from OCP.TDF import TDF_Label, TDF_LabelSequence

    comps = TDF_LabelSequence()
    st.GetComponents_s(ref, comps)
    if comps.Length() == 0:
        return [(name(ref), st.GetShape_s(ref))]
    out = []
    for i in range(1, comps.Length() + 1):
        c = comps.Value(i)
        r = TDF_Label()
        st.GetReferredShape_s(c, r)
        nm = name(r)
        if not is_hardware(nm):
            out.append((nm, st.GetShape_s(c)))
    return out


def export_variant(variant, spec, comps, shift_mm):
    """Fein genug, aber unter der Dreiecksgrenze der Art: Teile (Zoll) der Variante."""
    from OCP.BRepTools import BRepTools

    for d, ang in MESH_STEPS:
        parts = []
        for nm, shape in comps:
            BRepTools.Clean_s(shape)
            parts += tessellate(shape, nm, color_for(variant, nm), deflection_mm=d, angle_rad=ang)
        conv = [convert_part(p, shift_mm) for p in parts]
        if tri_count(conv) <= spec.get("limit", TRI_LIMIT[spec["kind"]]):
            return conv, (d, ang)
    raise ValueError(f"{variant}: auch bei {MESH_STEPS[-1][0]} mm / {MESH_STEPS[-1][1]} rad über {spec.get('limit', TRI_LIMIT[spec['kind']])} Dreiecken")


def export_all(step_path, out_dir, field_path):
    from OCP.TopLoc import TopLoc_Location

    st, name, _doc = read_step(step_path)
    field = json.loads(Path(field_path).read_text(encoding="utf-8"))
    loaders = {l["id"]: l for l in field["loader"]}
    wanted = sorted({v["step"] for v in VARIANTS.values()} | {FLOOR_REFERENCE})
    found = find_instances(st, name, wanted)
    missing = [w for w in wanted if not found[w]]
    if missing:
        raise SystemExit("nicht im STEP gefunden: " + ", ".join(missing))
    floor_mm = found[FLOOR_REFERENCE][0][1].Transformation().TranslationPart().Y()

    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    entries, glbs = {}, {}
    for variant, spec in VARIANTS.items():
        inst = found[spec["step"]]
        ref, acc = inst[0]
        comps = component_shapes(st, name, ref)
        if spec["pivot"] == "loader":
            fx, fy = (loaders[LOADER_FIELD_ID[variant]][k] for k in ("x_in", "y_in"))
            # die Instanz, die am Feldpunkt steht (z = −y)
            ref, acc = min(inst, key=lambda ia: abs(ia[1].Transformation().TranslationPart().X() / MM_PER_IN - fx)
                           + abs(ia[1].Transformation().TranslationPart().Z() / MM_PER_IN + fy))
            comps = [(nm, sh.Moved(acc)) for nm, sh in component_shapes(st, name, ref)]
            shift = loader_shift_mm((fx, fy), floor_mm)
        else:
            above = acc.Transformation().TranslationPart().Y() - floor_mm
            shift = pivot_shift_mm(spec["pivot"], above)
        parts, defl = export_variant(variant, spec, comps, shift)
        e = manifest_entry(spec["kind"], variant, parts, pivot=spec["pivot"])
        e["deflection_mm"], e["angle_rad"] = defl
        check_tri_limit(spec["kind"], e["tris"], spec.get("limit"))
        check_height_against_manual(spec["kind"], e["height_in"])
        if spec["kind"] == "goal":
            check_goal_top(variant, e["top_in"], field)
        glb = write_glb(parts)
        (out / e["file"]).write_bytes(glb)
        entries[variant], glbs[variant] = e, glb
        print(f"{variant:22s} {e['tris']:6d} Dreiecke  Höhe {e['height_in']:.2f} in  Abweichung {defl[0]} mm / {defl[1]} rad  {len(glb) // 1024} KB")
    manifest = make_manifest(entries)
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    (out / "models.js").write_text(bundle_js(manifest, glbs), encoding="utf-8")
    print(f"geschrieben nach {out} (nicht committen, siehe .gitignore)")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("step")
    ap.add_argument("--out", default=str(Path(__file__).resolve().parent / "viewer" / "models"))
    ap.add_argument("--field", default=str(Path(__file__).resolve().parent.parent / "field" / "field.json"))
    a = ap.parse_args(argv)
    if not have_ocp():
        sys.exit("cadquery/OCP fehlt: pip install cadquery (nur zum Erzeugen der Modelle nötig)")
    export_all(a.step, a.out, a.field)
    return 0


if __name__ == "__main__":
    sys.exit(main())
