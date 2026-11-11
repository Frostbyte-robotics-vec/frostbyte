"""Tests für export_field_models.py (SIM-37). Laufen ohne STEP-Datei und ohne cadquery: eigene Fixture-Netze, keine echten
Feldmaße. Der Teil mit OCP (Box-Solid tesselieren) wird übersprungen, wenn cadquery fehlt.

Ausführen: python3 -m unittest discover -s tools -p "test_export_field_models.py" -v
"""
import json
import struct
import unittest

import export_field_models as efm


def box_part(name="box", rgba=(1.0, 0.0, 0.0, 1.0), lo=(0.0, 0.0, 0.0), hi=(25.4, 50.8, 76.2)):
    """Quader aus 8 Ecken und 12 Dreiecken, Maße in mm."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    pos = [x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1]
    idx = [0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5]
    return {"name": name, "rgba": list(rgba), "pos": pos, "idx": idx}


class TestUnits(unittest.TestCase):
    def test_mm_zu_zoll(self):
        p = efm.convert_part(box_part())
        self.assertAlmostEqual(max(p["pos"][0::3]), 1.0, places=6)
        self.assertAlmostEqual(max(p["pos"][1::3]), 2.0, places=6)
        self.assertAlmostEqual(max(p["pos"][2::3]), 3.0, places=6)

    def test_verschiebung_in_mm_vor_der_umrechnung(self):
        p = efm.convert_part(box_part(), shift_mm=(25.4, -50.8, 0.0))
        self.assertAlmostEqual(min(p["pos"][0::3]), 1.0, places=6)
        self.assertAlmostEqual(min(p["pos"][1::3]), -2.0, places=6)
        self.assertAlmostEqual(min(p["pos"][2::3]), 0.0, places=6)

    def test_eingabe_bleibt_unveraendert(self):
        src = box_part()
        before = list(src["pos"])
        efm.convert_part(src, shift_mm=(1, 2, 3))
        self.assertEqual(src["pos"], before)

    def test_bounds_ueber_mehrere_teile(self):
        parts = [efm.convert_part(box_part()), efm.convert_part(box_part(lo=(-25.4, 0, 0), hi=(0, 25.4, 25.4)))]
        lo, hi = efm.parts_bounds(parts)
        self.assertEqual([round(v, 6) for v in lo], [-1.0, 0.0, 0.0])
        self.assertEqual([round(v, 6) for v in hi], [1.0, 2.0, 3.0])


class TestPivot(unittest.TestCase):
    def test_floor_setzt_boden_auf_null(self):
        # Modellursprung liegt 64 mm über dem Boden: Verschiebung in y um genau diesen Betrag
        self.assertEqual(efm.pivot_shift_mm("floor", origin_above_floor_mm=64.0), (0.0, 64.0, 0.0))

    def test_origin_verschiebt_nicht(self):
        self.assertEqual(efm.pivot_shift_mm("origin", origin_above_floor_mm=64.0), (0.0, 0.0, 0.0))

    def test_unbekannter_modus(self):
        with self.assertRaises(ValueError):
            efm.pivot_shift_mm("mitte", origin_above_floor_mm=0.0)


class TestGlb(unittest.TestCase):
    def setUp(self):
        self.parts = [efm.convert_part(box_part("a", (1, 0, 0, 1))),
                      efm.convert_part(box_part("b", (0.8, 0.9, 1.0, 0.35), lo=(30, 0, 0), hi=(40, 10, 10)))]
        self.data = efm.write_glb(self.parts)
        self.doc = efm.read_glb(self.data)

    def test_kopfzeile(self):
        magic, version, length = struct.unpack_from("<4sII", self.data, 0)
        self.assertEqual(magic, b"glTF")
        self.assertEqual(version, 2)
        self.assertEqual(length, len(self.data))
        self.assertEqual(len(self.data) % 4, 0)

    def test_ein_mesh_je_teil_mit_material(self):
        j = self.doc["json"]
        self.assertEqual(len(j["meshes"][0]["primitives"]), 2)
        self.assertEqual(len(j["materials"]), 2)
        self.assertEqual(j["asset"]["version"], "2.0")

    def test_transparenz_nur_bei_alpha_unter_eins(self):
        m = self.doc["json"]["materials"]
        self.assertEqual(m[0]["alphaMode"], "OPAQUE")
        self.assertEqual(m[1]["alphaMode"], "BLEND")
        self.assertAlmostEqual(m[1]["pbrMetallicRoughness"]["baseColorFactor"][3], 0.35, places=5)

    def test_zugriffe_passen_zum_binaerblock(self):
        j, blob = self.doc["json"], self.doc["bin"]
        self.assertEqual(j["buffers"][0]["byteLength"], len(blob))
        for acc in j["accessors"]:
            bv = j["bufferViews"][acc["bufferView"]]
            size = {"VEC3": 3, "SCALAR": 1}[acc["type"]] * 4
            self.assertLessEqual(bv["byteOffset"] + acc["count"] * size, len(blob))

    def test_positionen_und_indizes_rund(self):
        pos, idx = efm.glb_primitive(self.doc, 0)
        self.assertEqual(len(pos), 8)
        self.assertEqual(len(idx), 36)
        self.assertAlmostEqual(max(p[0] for p in pos), 1.0, places=5)
        self.assertTrue(all(0 <= i < 8 for i in idx))

    def test_accessor_min_max_gesetzt(self):
        acc = self.doc["json"]["accessors"][0]
        self.assertEqual([round(v, 5) for v in acc["max"]], [1.0, 2.0, 3.0])

    def test_gleiche_eingabe_gleiche_bytes(self):
        self.assertEqual(efm.write_glb(self.parts), efm.write_glb(self.parts))

    def test_dreieckszahl(self):
        self.assertEqual(efm.tri_count(self.parts), 24)

    def test_leere_liste_ist_fehler(self):
        with self.assertRaises(ValueError):
            efm.write_glb([])


class TestManifest(unittest.TestCase):
    def setUp(self):
        self.parts = [efm.convert_part(box_part(lo=(-25.4, -82.5, -25.4), hi=(25.4, 82.5, 25.4)))]

    def test_eintrag_enthaelt_hoehe_und_dreiecke(self):
        e = efm.manifest_entry("pin", "pin_red_blue", self.parts)
        self.assertEqual(e["kind"], "pin")
        self.assertEqual(e["tris"], 12)
        self.assertAlmostEqual(e["height_in"], 165.0 / 25.4, places=3)
        self.assertEqual(e["pivot"], "origin")

    def test_eintrag_nennt_teile_mit_farbe_und_hoehenbereich(self):
        # der Viewer braucht, welche Hälfte eines Pins oben liegt (oben_farbe in field.json)
        low = efm.convert_part(box_part("unten", (0, 0, 1, 1), lo=(-10, -82.5, -10), hi=(10, 0, 10)))
        up = efm.convert_part(box_part("oben", (1, 0, 0, 1), lo=(-10, 0, -10), hi=(10, 82.5, 10)))
        e = efm.manifest_entry("pin", "pin_red_blue", [low, up])
        self.assertEqual([t["name"] for t in e["teile"]], ["unten", "oben"])
        self.assertAlmostEqual(e["teile"][1]["y_min_in"], 0.0, places=4)
        self.assertAlmostEqual(e["teile"][1]["y_max_in"], 82.5 / 25.4, places=3)
        self.assertEqual(e["teile"][1]["rgba"][:3], [1, 0, 0])

    def test_manifest_schema(self):
        m = efm.make_manifest({"pin_red_blue": efm.manifest_entry("pin", "pin_red_blue", self.parts)})
        self.assertEqual(m["version"], 1)
        self.assertEqual(m["einheiten"], "Zoll")
        self.assertIn("pin_red_blue", m["modelle"])
        json.dumps(m)  # serialisierbar

    def test_grenze_je_art(self):
        self.assertLess(efm.TRI_LIMIT["pin"], efm.TRI_LIMIT["goal"])
        efm.check_tri_limit("pin", efm.TRI_LIMIT["pin"])
        with self.assertRaises(ValueError):
            efm.check_tri_limit("pin", efm.TRI_LIMIT["pin"] + 1)
        efm.check_tri_limit("goal", 50000, limit=70000)  # eigene Grenze je Variante

    def test_masse_gegen_manual(self):
        # Manual v2.1: Pin und Cup 6,5 Zoll hoch (164,5 / 165 mm), Toleranz 0,05 Zoll
        for kind, mm in (("pin", 165.0), ("cup", 164.5)):
            parts = [efm.convert_part(box_part(lo=(-10, -mm / 2, -10), hi=(10, mm / 2, 10)))]
            e = efm.manifest_entry(kind, kind, parts)
            efm.check_height_against_manual(kind, e["height_in"])
        with self.assertRaises(ValueError):
            efm.check_height_against_manual("cup", 4.5)


class TestGoalHoehe(unittest.TestCase):
    FIELD = {"goals": [{"typ": "alliance_red", "hoehe_in": 3.25}, {"typ": "neutral", "hoehe_in": 5.8},
                       {"typ": "midfield", "hoehe_in": 8.7}, {"typ": "alliance_blue", "hoehe_in": None}]}

    def test_passt(self):
        efm.check_goal_top("goal_red", 3.25, self.FIELD)
        efm.check_goal_top("goal_neutral", 5.77, self.FIELD)
        efm.check_goal_top("goal_blue", 99.0, self.FIELD)  # keine Höhe in field.json: nichts zu vergleichen

    def test_weicht_ab(self):
        with self.assertRaises(ValueError):
            efm.check_goal_top("goal_midfield", 5.8, self.FIELD)


class TestBundle(unittest.TestCase):
    def test_bundle_ist_skript_fuer_file_urls(self):
        parts = [efm.convert_part(box_part())]
        e = efm.manifest_entry("cup", "cup", parts)
        js = efm.bundle_js(efm.make_manifest({"cup": e}), {"cup": efm.write_glb(parts)})
        self.assertTrue(js.startswith("window.FB_MODELS = "))
        payload = json.loads(js[len("window.FB_MODELS = "):].rstrip(";\n"))
        self.assertEqual(payload["manifest"]["modelle"]["cup"]["kind"], "cup")
        import base64
        self.assertEqual(base64.b64decode(payload["glb"]["cup"])[:4], b"glTF")


class TestLoaderBezug(unittest.TestCase):
    def test_feldpunkt_wird_ursprung(self):
        # Welt-Netz des Loaders: der Feldpunkt aus field.json (x −69,161, y −58,763 → z +58,763) liegt auf dem Boden
        # (floor_mm = Boden im CAD) danach im Nullpunkt.
        floor_mm = -294.6
        s = efm.loader_shift_mm(field_xy_in=(-69.161, -58.763), floor_mm=floor_mm)
        point = (-69.161 * 25.4, floor_mm, 58.763 * 25.4)
        moved = [p + d for p, d in zip(point, s)]
        self.assertTrue(all(abs(v) < 1e-9 for v in moved), moved)

    def test_cad_ursprung_liegt_neben_dem_feldpunkt(self):
        # CAD-Ursprung des roten Loaders x −68,276: nach der Verschiebung 0,885 Zoll neben dem Feldpunkt
        s = efm.loader_shift_mm(field_xy_in=(-69.161, -58.763), floor_mm=0.0)
        self.assertAlmostEqual((-68.276 * 25.4 + s[0]) / 25.4, 0.885, places=3)


class TestFarben(unittest.TestCase):
    def test_pin_haelften(self):
        self.assertEqual(efm.color_for("pin_red_blue", "276-9250-001_PMS 199c (Bright Red)")[3], 1.0)
        red = efm.color_for("pin_red_blue", "276-9250-001_PMS 199c (Bright Red)")
        blue = efm.color_for("pin_red_blue", "276-9250-001_PMS 299c (Bright Blue)")
        yellow = efm.color_for("pin_blue_neutral", "276-9250-001_Yellow")
        self.assertGreater(red[0], red[2])
        self.assertGreater(blue[2], blue[0])
        self.assertGreater(yellow[0], yellow[2])

    def test_cup_klar_ist_durchsichtig(self):
        self.assertLess(efm.color_for("cup", "276-9250-002_Clear")[3], 1.0)
        self.assertEqual(efm.color_for("cup", "276-9250-002_PMS 425c - Dark Gray")[3], 1.0)

    def test_goal_koerper_nach_allianz(self):
        red = efm.color_for("goal_red", "276-9250-003_Master")
        blue = efm.color_for("goal_blue", "276-9250-003_Master")
        neutral = efm.color_for("goal_neutral", "276-9250-003_Master")
        self.assertGreater(red[0], red[2])
        self.assertGreater(blue[2], blue[0])
        self.assertAlmostEqual(neutral[0], neutral[2], delta=0.1)

    def test_unbekanntes_teil_bekommt_grau(self):
        c = efm.color_for("goal_red", "276-9250-999")
        self.assertEqual(c[3], 1.0)

    def test_kleinteile_fallen_weg(self):
        for n in ("SBT0832_276-5006-001", "THF0832-0250_275-1014-001", "NNY_Imperial_275-1027-001",
                  "94629A270_Thread-Cutting Screws for Plastic_94629A270", "SPTM3-TC_228-7045-010"):
            self.assertTrue(efm.is_hardware(n), n)
        for n in ("276-9250-003_Master", "276-4847-011", "276-9250-001_Yellow", "276-9250-002_Clear"):
            self.assertFalse(efm.is_hardware(n), n)


@unittest.skipUnless(efm.have_ocp(), "cadquery/OCP nicht installiert (nur für das Erzeugen der Modelle nötig)")
class TestTessellierung(unittest.TestCase):
    def test_quader(self):
        import cadquery as cq
        solid = cq.Workplane("XY").box(25.4, 50.8, 76.2).val().wrapped
        parts = efm.tessellate(solid, name="q", rgba=(0.5, 0.5, 0.5, 1.0), deflection_mm=0.5)
        lo, hi = efm.parts_bounds([efm.convert_part(p) for p in parts])
        self.assertAlmostEqual(hi[0] - lo[0], 1.0, places=4)
        self.assertAlmostEqual(hi[1] - lo[1], 2.0, places=4)
        self.assertAlmostEqual(hi[2] - lo[2], 3.0, places=4)
        self.assertEqual(efm.tri_count(parts), 12)


if __name__ == "__main__":
    unittest.main()
