# Frostbyte Runner – Fusion-Nachbau mit echten VEX-Teilen (VEX-CAD Fusion 360 Library 2.0.0)
# Einheiten im Script: Zoll. Fusion: X rechts, Y vorne (Fahrtrichtung), Z oben, Boden Z = 0.
# Bibliothek: Pfad unten in LIB anpassen. Alles andere (Wellen, Platten, Druckteile) erzeugt das Script selbst.
import adsk.core, adsk.fusion, traceback, math, os

LIB = r'C:\Users\David\Documents\Dokumenti\Frostybte-step\VEX-CAD-Fusion-360-Library-2.0.0\VEX-CAD-Fusion-360-Library-2.0.0'
DOC_NAME = 'Frostbyte Runner'

IN = 2.54
T = 0.063          # Wandstaerke Alu-Profil/Platte (aus Bibliothek gemessen)
R_W = 2.075        # 4" AS Omni, Aussenradius (gemessen)
AX = R_W           # Achshoehe
Z_TOP = AX + 0.25  # Oberkante Rahmenprofile
XI = 3.45          # Innenprofil: Rueckseite (innen, Motoren liegen an)
XO = 7.05          # Aussenprofil: Rueckseite (aussen, Lager liegen an)
M_FACE = 1.74      # V5-Motor: Auflageflaeche der Kassette (nativ Z)

# Lift (Viergelenk): Tuerme auf den Innenprofilen, Lochreihen Y = -6,0 / -5,5
PIV_Y = -6.0       # Drehpunkte am Turm
ZP_L, ZP_U = 9.575, 11.575   # unteres / oberes Armgelenk (2" Abstand)
Z_PIN = 13.075     # Liftmotor-Ritzel 12T, 1,5" ueber dem oberen Gelenk (60T)
L_ARM = 10.0       # Armlaenge (Loch 0 bis Loch 20)
G0 = 4.138         # Ebene Ritzel/60T (Innenseite)
ARM_X = 4.545      # Rueckseite der Arme
BAR_X = 4.05       # Rueckseite der Schlittenholme
DECK_UP = 0.625                    # Brain-Platte auf 0,5" + 0,125" Distanzhuelsen ueber den Innenprofilen (Platz fuer die Tracking-Raeder)
DECK = Z_TOP + DECK_UP             # Unterkante Brain-Platte
QB_TOP = DECK + T + 2.0 + 0.125    # Oberkante Koecherboden (2"-Abstandsbolzen + 0,125" Druckteil)
GRIP_DY, GRIP_DZ = 2.1, 1.6        # Greifpunkt relativ zum unteren Schlittengelenk
TH0 = math.degrees(math.asin((QB_TOP + 4.0 - GRIP_DZ - ZP_L) / L_ARM))   # Startwinkel: Greifer genau auf Greifhoehe

P_ = {
    'ch': r'Structure\Aluminum\2x C-Channel\{n} - 2x C-Chan.f3d',
    'omni': r'Motion\Wheels\Omni Wheels\4 Inch\Antistatic\4 AS Omni, LS Shaft Insert.f3d',
    'g60': r'Motion\Gears\High Strength (v2)\60T\60T HS Gear, LS Shaft Insert (v2).f3d',
    'g84': r'Motion\Gears\High Strength (v2)\84T\84T HS Gear, LS Shaft Insert (v2).f3d',
    'bear': r'Motion\Bearings\Bearing Flat.f3d',
    'collar': r'Motion\Collars\LS Shaft Collar.f3d',
    'motor': r'Electronics\V5 Motor (11W).f3d',
    'brain': r'Electronics\V5 Brain.f3d',
    'batt': r'Electronics\V5 Battery.f3d',
    'clip': r'Electronics\Brain & Battery Accessories\V5 Battery Clip.f3d',
    'imu': r'Electronics\Sensors\V5 Inertial Sensor.f3d',
    'screw': r'Hardware\Screws\8-32\Steel\Torx (Star)\{L} Torx (Star) Screw (8-32, Steel).f3d',
    'nut': r'Hardware\Nuts\8-32 Nuts\Nylock Nuts\Nylock Nut (8-32, Steel).f3d',
    'sp313': r'Motion\Spacers\0.313 OD\0.313 OD Spacer, {L} (Nylon).f3d',
    'sp500': r'Motion\Spacers\0.5 OD\0.5 OD Spacer, {L} (Nylon).f3d',
    'wash': r'Motion\Spacers\Washers\Teflon\Washer, 0.03125 (Teflon).f3d',
    'wash62': r'Motion\Spacers\Washers\Teflon\Washer, 0.0625 (Teflon).f3d',
    'g12': r'Motion\Gears\Low Strength\LS 12T Gear.f3d',
    'g60d': r'Motion\Gears\Low Strength\Drilled Gears\LS 60T Gear (Drilled).f3d',
    'rack': r'Motion\Gears\LS Rack Gear.f3d',
    'standoff': r'Hardware\Standoffs\{L} Standoff (8-32, Aluminum).f3d',
    'bumper': r'Electronics\Sensors\3-Wire\Bumper Switch.f3d',
    'spr16': r'Motion\Sprockets\6P\16T\16T Sprocket - 6P, LS Shaft Insert.f3d',
    'angle': r'Structure\Aluminum\1x1 Angle (Regular)\{n} - 1x1 Angle (Regular).f3d',
    'omni275': r'Motion\Wheels\Omni Wheels\2.75 Inch\Antistatic\2.75 AS Omni, LS Shaft Insert.f3d',
    'rot': r'Electronics\Sensors\V5 Rotation Sensor.f3d',
}

# Aufnahme (Band): untere Rolle, Steigung wie im Viewer-Modell, 24-Loch-Profile -> Rollenabstand 11,5"
BAND_Y0, BAND_Z0 = -5.9, 1.1
BAND_A = math.atan2(10.1, 6.2)
BAND_L = 11.5
TIP_Y, TIP_Z = PIV_Y, 6.575       # Kipprolle (Welle in beiden Tuermen)
CHAIN_X = 2.7                     # Kettenebene rechts

# Odometrie: zwei Tracking-Raeder 2,75" AS Omni (gemessen: Radius 1,376", Breite 0,97") mit V5 Rotation Sensor
# (Dicke 0,815", Welle 0,877" aus der Mitte), unter der angehobenen Brain-Platte. Laengsrad (misst vorwaerts) links,
# Querrad (misst seitwaerts) rechts. Jede Einheit ist eine Schwinge (Drehachse parallel zur Radachse) mit Gummizug als Vorspannung.
ODOM = True
OD_R = 1.376                      # Radius 2,75" AS Omni
OD_W = 0.486                      # halbe Radbreite (Rollen)
F_C = (-2.1, 4.15)                # Laengsrad: Mitte x, y
Q_C = (1.1, 3.7)                  # Querrad:   Mitte x, y
F_PIV = (5.4, 2.5)                # Laengs-Schwinge: Drehpunkt (y, z), Achse entlang X
Q_PIV = (2.6, 2.1)                # Quer-Schwinge:   Drehpunkt (x, z), Achse entlang Y
F_TILT = 20.0                     # Laengs-Sensor 20 Grad nach vorn unten gekippt (langes Ende unter dem Antriebsmotor)
OD_HUB = 0.045                    # Schwingenweg +-2,6 Grad (ca. +-0,08" am Rad)
OD_SLOTS = []                     # mit angehobener Platte keine Ausschnitte noetig

app = adsk.core.Application.get()
REPORT = []


# ---------------- Vektorhilfen (Zoll, Tupel) ----------------
def v(x): return x * IN
def P(p): return adsk.core.Point3D.create(v(p[0]), v(p[1]), v(p[2]))
def V(d): return adsk.core.Vector3D.create(d[0], d[1], d[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def neg(a): return (-a[0], -a[1], -a[2])
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def perp(d):
    ref = (1, 0, 0) if abs(d[0]) < .9 else (0, 1, 0)
    x = cross(ref, d)
    n = math.sqrt(dot(x, x))
    return mul(x, 1 / n)


def mat(origin, nx, ny, nz):
    m = adsk.core.Matrix3D.create()
    m.setWithCoordinateSystem(P(origin), V(nx), V(ny), V(nz))
    return m


def native_to_world(world_pt, native_pt, nx, ny, nz):
    """Ursprung so waehlen, dass native_pt auf world_pt liegt."""
    off = add(add(mul(nx, native_pt[0]), mul(ny, native_pt[1])), mul(nz, native_pt[2]))
    return sub(world_pt, off)


def coll(items):
    c = adsk.core.ObjectCollection.create()
    for i in items:
        c.add(i)
    return c


def fmt(L):
    return '%.3f' % L


# ---------------- Teilebibliothek ----------------
class Lib:
    def __init__(self, root):
        hold = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        hold.component.name = '00 Teilebibliothek (ausgeblendet)'
        self.hold_occ = hold
        self.hold = hold.component
        self.cache = {}

    def vex(self, key, **kw):
        rel = P_[key].format(**kw)
        if rel in self.cache:
            return self.cache[rel]
        path = os.path.join(LIB, rel)
        if not os.path.exists(path):
            raise RuntimeError('Bibliotheksteil fehlt: ' + rel)
        im = app.importManager
        im.importToTarget(im.createFusionArchiveImportOptions(path), self.hold)
        occ = self.hold.occurrences.item(self.hold.occurrences.count - 1)
        self.cache[rel] = occ.component
        return occ.component

    def new(self, key, name):
        if key in self.cache:
            return self.cache[key], False
        occ = self.hold.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        occ.component.name = name
        self.cache[key] = occ.component
        return occ.component, True

    # --- selbst erzeugte Teile ---
    @staticmethod
    def _sk_pt(sk, p):
        return sk.modelToSketchSpace(P(p))

    def _rect_extrude(self, comp, plane, p0, p1, dist):
        sk = comp.sketches.add(plane)
        sk.sketchCurves.sketchLines.addTwoPointRectangle(self._sk_pt(sk, p0), self._sk_pt(sk, p1))
        ex = comp.features.extrudeFeatures
        inp = ex.createInput(sk.profiles.item(0), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        inp.setDistanceExtent(False, adsk.core.ValueInput.createByReal(v(dist)))
        return ex.add(inp)

    def shaft(self, L):
        """1/8"-Vierkantwelle, nativ X 0..L."""
        L = round(L, 3)
        c, fresh = self.new(('shaft', L), 'Welle 1/8" x %.3f (aus 12" gekuerzt)' % L)
        if fresh:
            self._rect_extrude(c, c.yZConstructionPlane, (0, -0.0625, -0.0625), (0, 0.0625, 0.0625), L)
        return c

    def plate(self, nx, ny, slots=()):
        """Alu-Platte (wie 5x Plate), nativ X 0..nx/2, Y 0..ny/2, Z 0..T, Loecher 0,182" quadratisch.
        slots: rechteckige Ausschnitte (x0, y0, x1, y1) in Plattenkoordinaten."""
        name = 'Alu-Platte %dx%d' % (nx, ny) + (' mit Ausschnitten' if slots else '')
        c, fresh = self.new(('plate', nx, ny, tuple(slots)), name)
        if fresh:
            self._rect_extrude(c, c.xYConstructionPlane, (0, 0, 0), (nx * .5, ny * .5, 0), T)
            sk = c.sketches.add(c.xYConstructionPlane)
            sk.isComputeDeferred = True
            h = 0.091
            for i in range(nx):
                for j in range(ny):
                    hx, hy = .25 + .5 * i, .25 + .5 * j
                    if any(x0 - h < hx < x1 + h and y0 - h < hy < y1 + h for x0, y0, x1, y1 in slots):
                        continue
                    a = self._sk_pt(sk, (hx, hy, 0))
                    sk.sketchCurves.sketchLines.addCenterPointRectangle(a, adsk.core.Point3D.create(a.x + v(h), a.y + v(h), 0))
            for x0, y0, x1, y1 in slots:
                sk.sketchCurves.sketchLines.addTwoPointRectangle(self._sk_pt(sk, (x0, y0, 0)), self._sk_pt(sk, (x1, y1, 0)))
            sk.isComputeDeferred = False
            profs = [sk.profiles.item(i) for i in range(sk.profiles.count)]
            big = nx * ny * v(0.5) ** 2 * 0.5
            profs = [p for p in profs if p.areaProperties().area < big]
            ex = c.features.extrudeFeatures
            inp = ex.createInput(coll(profs), adsk.fusion.FeatureOperations.CutFeatureOperation)
            inp.setSymmetricExtent(adsk.core.ValueInput.createByReal(v(1.0)), True)
            inp.participantBodies = [c.bRepBodies.item(0)]
            ex.add(inp)
        return c


    # --- Druckteile (PETG/TPU): Koerper in lokalen Koordinaten ---
    def printed(self, key, name, build):
        c, fresh = self.new(('print', key), name)
        if fresh:
            build(self, c)
            if c.bRepBodies.count > 1:   # ein Druckteil = ein Koerper
                tools = coll([c.bRepBodies.item(i) for i in range(1, c.bRepBodies.count)])
                ci = c.features.combineFeatures.createInput(c.bRepBodies.item(0), tools)
                ci.operation = adsk.fusion.FeatureOperations.JoinFeatureOperation
                c.features.combineFeatures.add(ci)
        return c

    def body_box(self, c, p0, p1):
        sk = c.sketches.add(c.xYConstructionPlane)
        sk.sketchCurves.sketchLines.addTwoPointRectangle(self._sk_pt(sk, (p0[0], p0[1], 0)), self._sk_pt(sk, (p1[0], p1[1], 0)))
        return self._extrude_z(c, sk.profiles.item(0), p0[2], p1[2] - p0[2])

    def body_poly(self, c, pts, z0, h):
        sk = c.sketches.add(c.xYConstructionPlane)
        lines = sk.sketchCurves.sketchLines
        sp = [self._sk_pt(sk, (x, y, 0)) for x, y in pts]
        for i in range(len(sp)):
            lines.addByTwoPoints(sp[i], sp[(i + 1) % len(sp)])
        prof = max([sk.profiles.item(i) for i in range(sk.profiles.count)], key=lambda q: q.areaProperties().area)
        return self._extrude_z(c, prof, z0, h)

    def body_cyl_x(self, c, r, x0, x1, y=0.0, z=0.0):
        sk = c.sketches.add(c.yZConstructionPlane)
        sk.sketchCurves.sketchCircles.addByCenterRadius(self._sk_pt(sk, (0, y, z)), v(r))
        prof = sk.profiles.item(0)
        ex = c.features.extrudeFeatures
        inp = ex.createInput(prof, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        inp.startExtent = adsk.fusion.OffsetStartDefinition.create(adsk.core.ValueInput.createByReal(v(x0)))
        inp.setDistanceExtent(False, adsk.core.ValueInput.createByReal(v(x1 - x0)))
        return ex.add(inp)

    def body_ring_sector(self, c, cx, cy, r_in, r_out, a0, a1, z0, h):
        """Kreisring-Sektor in XY (Winkel in Grad, 0 = +X), extrudiert in Z."""
        pts = []
        n = 16
        for k in range(n + 1):
            a = math.radians(a0 + (a1 - a0) * k / n)
            pts.append((cx + r_out * math.cos(a), cy + r_out * math.sin(a)))
        for k in range(n, -1, -1):
            a = math.radians(a0 + (a1 - a0) * k / n)
            pts.append((cx + r_in * math.cos(a), cy + r_in * math.sin(a)))
        return self.body_poly(c, pts, z0, h)

    def _extrude_z(self, c, prof, z0, h):
        ex = c.features.extrudeFeatures
        inp = ex.createInput(prof, adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
        if abs(z0) > 1e-9:
            inp.startExtent = adsk.fusion.OffsetStartDefinition.create(adsk.core.ValueInput.createByReal(v(z0)))
        inp.setDistanceExtent(False, adsk.core.ValueInput.createByReal(v(h)))
        return ex.add(inp)

    def solid(self, key, name, parts, cuts=()):
        """Druckteil aus Grundkoerpern in Weltkoordinaten (Zoll), als ein Koerper.
        parts/cuts: ('box', (x0, y0, z0), (x1, y1, z1)) oder ('cyl', p0, p1, r)."""
        c, fresh = self.new(('solid', key), name)
        if fresh:
            tb = adsk.fusion.TemporaryBRepManager.get()

            def mk(sp):
                if sp[0] == 'obox':   # ('obox', Mitte, Laengsrichtung, Laenge, Breite (entlang X), Hoehe)
                    ld = V(sp[2]); wd = V((1, 0, 0))
                    ob = adsk.core.OrientedBoundingBox3D.create(P(sp[1]), ld, wd, v(sp[3]), v(sp[4]), v(sp[5]))
                    return tb.createBox(ob)
                if sp[0] == 'box':
                    (x0, y0, z0), (x1, y1, z1) = sp[1], sp[2]
                    ob = adsk.core.OrientedBoundingBox3D.create(P(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)), V((1, 0, 0)), V((0, 1, 0)),
                                                                v(x1 - x0), v(y1 - y0), v(z1 - z0))
                    return tb.createBox(ob)
                return tb.createCylinderOrCone(P(sp[1]), v(sp[3]), P(sp[2]), v(sp[3]))
            body = mk(parts[0])
            for sp in parts[1:]:
                tb.booleanOperation(body, mk(sp), adsk.fusion.BooleanTypes.UnionBooleanType)
            for sp in cuts:
                tb.booleanOperation(body, mk(sp), adsk.fusion.BooleanTypes.DifferenceBooleanType)
            bf = c.features.baseFeatures.add()
            bf.startEdit()
            c.bRepBodies.add(body, bf)
            bf.finishEdit()
        return c

    def drill_z(self, c, pts, dia=0.19):
        """Senkrechte Bohrungen durch alle Koerper der Komponente."""
        sk = c.sketches.add(c.xYConstructionPlane)
        for x, y in pts:
            sk.sketchCurves.sketchCircles.addByCenterRadius(self._sk_pt(sk, (x, y, 0)), v(dia / 2))
        profs = [sk.profiles.item(i) for i in range(sk.profiles.count) if sk.profiles.item(i).areaProperties().area < v(dia) ** 2]
        ex = c.features.extrudeFeatures
        inp = ex.createInput(coll(profs), adsk.fusion.FeatureOperations.CutFeatureOperation)
        inp.setSymmetricExtent(adsk.core.ValueInput.createByReal(v(12.0)), True)
        inp.participantBodies = [c.bRepBodies.item(i) for i in range(c.bRepBodies.count)]
        ex.add(inp)


# ---------------- Roboter ----------------
class Robot:
    def __init__(self, root, lib):
        self.root = root
        self.lib = lib
        self.groups = {}
        self.screws = []     # dict(group, head, dir, length, stage)
        self.units = {}      # Name -> Occurrence (bewegliche Baugruppen)
        self.joints = {}

    def group(self, name):
        if name not in self.groups:
            occ = self.root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
            occ.component.name = name
            self.groups[name] = occ
        return self.groups[name]

    def put(self, group, comp, m, stage=None):
        g = self.group(group)
        occ = g.component.occurrences.addExistingComponent(comp, m)
        try:
            occ.isGroundToParent = True
        except Exception:
            pass
        if stage is not None:
            occ.attributes.add('frostbyte', 'stufe', str(stage))
        return occ

    # --- Normteile platzieren ---
    def channel(self, group, n, row_a, row_b, along, flange):
        """C-Profil: row_a/row_b = erstes Loch der beiden Steg-Lochreihen (Weltpunkte in der Ebene 0,3" ueber der Rueckseite),
        along = Laengsrichtung, flange = Richtung Rueckseite -> Schenkel."""
        across = sub(row_b, row_a)
        if dot(cross(across, along), flange) < 0:
            row_a, across = row_b, neg(across)
        nx = mul(across, 2.0)  # 0.5" -> Einheitsvektor
        nz = cross(nx, along)
        return self.put(group, self.lib.vex('ch', n=n), mat(row_a, nx, along, nz))

    def screw(self, group, head, d, L, nut_gap=None, stage=0):
        """Schraube: head = Kopfauflage, d = Richtung ins Material; Mutter im Abstand nut_gap (Auflage)."""
        nx = perp(d)
        nz = cross(nx, d)
        self.put(group, self.lib.vex('screw', L=fmt(L)), mat(head, nx, d, nz), stage=stage)
        if nut_gap is not None:
            nd = neg(d)
            self.put(group, self.lib.vex('nut'), mat(add(head, mul(d, nut_gap)), nx, nd, cross(nx, nd)), stage=stage)
        self.screws.append(dict(group=group, head=head, dir=d, L=L, stage=stage, nut=nut_gap))

    def axial(self, group, comp, axis_char, nmin, start, s, y, z):
        """Teil mit Achse axis_char (nativ) auf einer Welle in X-Richtung; nmin = native Untergrenze entlang der Achse."""
        a = (s, 0, 0)
        if axis_char == 'X':
            nx, ny = a, (0, 1, 0); nz = cross(nx, ny)
        elif axis_char == 'Y':
            ny, nz = a, (0, 0, 1); nx = cross(ny, nz)
        else:
            nz, nx = a, (0, 1, 0); ny = cross(nz, nx)
        org = (s * (start - nmin), y, z)
        return self.put(group, comp, mat(org, nx, ny, nz))

    # Achsstapel: Liste von (art, laenge). Rueckgabe: Endposition
    def stack(self, group, items, start, s, y, z):
        x = start
        L = self.lib
        for kind, ln in items:
            if kind == 'sp313':
                self.axial(group, L.vex('sp313', L=fmt(ln)), 'X', 0.0, x, s, y, z)
            elif kind == 'sp500':
                self.axial(group, L.vex('sp500', L=fmt(ln)), 'X', 0.0, x, s, y, z)
            elif kind == 'wash':
                self.axial(group, L.vex('wash'), 'X', 0.0, x, s, y, z)
            elif kind == 'collar':
                self.axial(group, L.vex('collar'), 'X', -0.13, x, s, y, z)
            elif kind == 'g84':   # HS-Zahnrad: Achse nativ X, Zahnkranz +-0,25, Vierkant-Einsatz +-0,30
                self.axial(group, L.vex('g84'), 'X', -0.30, x, s, y, z)
            elif kind == 'g60':
                self.axial(group, L.vex('g60'), 'X', -0.30, x, s, y, z)
            elif kind == 'wash62':
                self.axial(group, L.vex('wash62'), 'X', 0.0, x, s, y, z)
            elif kind == 'g12':
                self.axial(group, L.vex('g12'), 'X', -0.188, x, s, y, z)
            elif kind == 'omni':
                self.axial(group, L.vex('omni'), 'Y', -0.524, x, s, y, z)
            elif kind == 'gap':
                pass
            x += ln
        return x

    def bearing(self, group, face_x, s_out, y, z, stage=0):
        """Lager flach an einer Profilwand (Flaeche bei face_x), Lagerkoerper zeigt in Richtung s_out (+1/-1 in X)."""
        nx = (s_out, 0, 0); nz = (0, 1, 0); ny = cross(nz, nx)
        self.put(group, self.lib.vex('bear'), mat((face_x, y, z), nx, ny, nz))
        # Schrauben: Kopf auf Lageroberseite (nativ X = 0,25), durch Lager + Wand, Mutter auf der Gegenseite
        for dy in (-0.5, 0.5):
            head = (face_x + s_out * 0.25, y + dy, z)
            self.screw(group, head, (-s_out, 0, 0), 0.625, nut_gap=0.25 + T, stage=stage)

    def motor(self, group, shaft_face_pt, out_dir, holes_dir, stage=0, wall=T, screw_len=0.375, screws=True):
        """V5-Motor: Kassette an einer Wand; out_dir = Richtung aus der Kassette (durch die Wand), holes_dir = Richtung
        von der Welle zu den Befestigungsloechern. Schrauben kommen von der Gegenseite der Wand."""
        nz = out_dir
        nx = neg(holes_dir)
        ny = cross(nz, nx)
        org = native_to_world(shaft_face_pt, (0.5, 0, M_FACE), nx, ny, nz)
        occ = self.put(group, self.lib.vex('motor'), mat(org, nx, ny, nz))
        if screws:
            for k in (1.0, 0.5):
                hp = add(shaft_face_pt, mul(holes_dir, k))
                self.screw(group, add(hp, mul(out_dir, wall)), neg(out_dir), screw_len, stage=stage)
        return occ

    # ---------------- Rahmen ----------------
    def build_frame(self):
        G = '10 Rahmen'
        for s in (-1, 1):
            # Aussenprofil: Rueckseite aussen bei |x| = 7,05, Schenkel innen, Lochreihen Z = AX-0,5 / AX
            self.channel(G, 29, (s * (XO - .3), -7.0, AX - .5), (s * (XO - .3), -7.0, AX), (0, 1, 0), (-s, 0, 0))
            # Innenprofil: Rueckseite innen bei |x| = 3,45, Schenkel aussen
            self.channel(G, 29, (s * (XI + .3), -7.0, AX - .5), (s * (XI + .3), -7.0, AX), (0, 1, 0), (s, 0, 0))
        # vorderer Quertraeger 28 Loecher, Rueckseite auf den Profiloberkanten
        zc = Z_TOP + .3
        self.channel(G, 28, (-6.75, 6.5, zc), (-6.75, 7.0, zc), (1, 0, 0), (0, 0, 1))
        for s in (-1, 1):  # hintere Querstuecke (Rueckseite offen fuer die Aufnahme)
            x0 = 3.75 if s > 0 else -6.75
            self.channel(G, 7, (x0, -7.0, zc), (x0, -6.5, zc), (1, 0, 0), (0, 0, 1))
        for s in (-1, 1):
            for x in (XO - .3, XI + .3):
                for y in (6.5, 7.0, -6.5, -7.0):
                    self.screw(G, (s * x, y, Z_TOP + T), (0, 0, -1), 0.375, nut_gap=2 * T, stage=1)

    # ---------------- Antrieb ----------------
    def build_drive(self):
        G = '10 Rahmen'
        x_in = XI + T          # Aussenseite Steg Innenprofil
        x_out = XO - T         # Innenseite Steg Aussenprofil
        for s in (-1, 1):
            side = 'R' if s > 0 else 'L'
            for m in (1, -1):
                pos = 'V' if m > 0 else 'H'
                # Motor an der Innenseite des Innenprofils, Welle durch Lochreihe Z = AX
                self.motor(G, (s * XI, m * 1.0, AX), (s, 0, 0), (0, m, 0), stage=2)
                # Motorwelle: Zahnrad 84T in Ebene A (vorn) bzw. B (hinten)
                U = 'Motorwelle %s%s' % (pos, side)
                if m > 0:
                    items = [('sp313', .5), ('wash', .03125), ('g84', .6), ('sp313', .5), ('sp313', .875),
                             ('collar', .26), ('sp313', .5), ('sp313', .125)]
                else:
                    items = [('sp313', .5), ('sp313', .5), ('sp313', .125), ('g84', .6), ('sp313', .5),
                             ('sp313', .5), ('collar', .26), ('sp313', .375), ('wash', .03125)]
                end = self.stack(U, items, x_in, s, m * 1.0, AX)
                REPORT.append('%s: Stapel bis %.3f (Wand innen %.3f, Spiel %.3f)' % (U, end, x_out, x_out - end))
                sh = XO + 0.25 - (XI - 0.4)
                self.put(U, self.lib.shaft(sh), mat((s * (XI - 0.4), m * 1.0, AX), (s, 0, 0), (0, 1, 0), (0, 0, s)))
                self.units[U] = self.groups[U]
                self.bearing(G, s * XO, s, m * 1.0, AX, stage=3)
                # Radachse
                W = 'Rad %s%s' % (pos, side)
                yw = m * 4.0
                # Stellring im Stapel (Stapel ist zwischen den Waenden gefangen -> Welle axial fest)
                if m > 0:
                    items = [('sp313', .5), ('wash', .03125), ('g60', .6), ('collar', .26), ('sp500', .25),
                             ('wash62', .0625), ('wash', .03125), ('omni', 1.069), ('sp313', .5), ('sp313', .125)]
                else:
                    items = [('sp313', .5), ('collar', .26), ('sp313', .25), ('sp313', .125), ('g60', .6),
                             ('omni', 1.069), ('sp313', .5), ('sp313', .125)]
                end = self.stack(W, items, x_in, s, yw, AX)
                REPORT.append('%s: Stapel bis %.3f (Spiel %.3f)' % (W, end, x_out - end))
                self.stack(W, [('collar', .26)], XI - 0.25 - 0.26, s, yw, AX)   # Stellring innen vor dem Lager
                x0 = XI - 0.25 - 0.26
                self.put(W, self.lib.shaft(XO + 0.25 - x0), mat((s * x0, yw, AX), (s, 0, 0), (0, 1, 0), (0, 0, s)))
                self.units[W] = self.groups[W]
                self.bearing(G, s * XO, s, yw, AX, stage=3)
                self.bearing(G, s * XI, -s, yw, AX, stage=3)

    # ---------------- Elektronik ----------------
    def build_electronics(self):
        G = '10 Rahmen'
        E = '30 Elektronik'
        L = self.lib
        # Brain-Platte 16 x 5 auf den Innenprofilen, Lochreihen Y = 3,0 ... 5,0
        slots = [(x0 + 4.0, y0 - 2.75, x1 + 4.0, y1 - 2.75) for x0, y0, x1, y1 in OD_SLOTS] if ODOM else []
        self.put(G, L.plate(16, 5, slots), mat((-4.0, 2.75, DECK), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
        for s in (-1, 1):
            for y in (3.0, 5.0):
                # Distanzhuelsen 0,5" + 0,125" zwischen Profil und Platte, Schraube von oben, Mutter im Profil
                self.put(G, L.vex('sp500', L='0.500'), mat((s * 3.75, y, Z_TOP), (0, 0, 1), (1, 0, 0), (0, 1, 0)))
                self.put(G, L.vex('sp500', L='0.125'), mat((s * 3.75, y, Z_TOP + 0.5), (0, 0, 1), (1, 0, 0), (0, 1, 0)))
                self.screw(G, (s * 3.75, y, DECK + T), (0, 0, -1), 1.0, nut_gap=2 * T + DECK_UP, stage=4)
        # Brain flach auf 0,5"-Distanzen, Bildschirm oben; Loecher 3,0 x 1,5 -> x = -1,25 / 1,75, y = 3,0 / 4,5
        zb = DECK + T + 0.5
        org = native_to_world((1.75, 3.0, zb), (-1.201, 1.222, -0.054), (1, 0, 0), (0, 1, 0), (0, 0, 1))
        self.put(E, L.vex('brain'), mat(org, (1, 0, 0), (0, 1, 0), (0, 0, 1)))
        for x in (-1.25, 1.75):
            for y in (3.0, 4.5):
                self.put(E, L.vex('sp500', L='0.500'), mat((x, y, DECK + T), (0, 0, 1), (1, 0, 0), (0, 1, 0)))
                self.screw(E, (x, y, DECK), (0, 0, 1), 0.75, stage=6)
        # Akku haengend unter dem vorderen Quertraeger, 2 Clips (Loecher x = +-1,25 / +-2,25 auf Reihe y = 6,5)
        nx, ny, nz = (1, 0, 0), (0, 0, -1), (0, 1, 0)
        for xc in (-1.75, 1.75):
            org = native_to_world((xc, 6.5, Z_TOP), (0, -0.354, 0), nx, ny, nz)
            self.put(E, L.vex('clip'), mat(org, nx, ny, nz))
            for dx in (-0.5, 0.5):
                self.screw(E, (xc + dx, 6.5, Z_TOP - 0.078), (0, 0, 1), 0.375, nut_gap=0.078 + T, stage=5)
        org = native_to_world((0, 6.5, Z_TOP), (0, -0.354, 0), nx, ny, nz)
        self.put(E, L.vex('batt'), mat(org, nx, ny, nz), stage=6)

    # ---------------- Lift (Viergelenk) ----------------
    def build_lift(self):
        G = '10 Rahmen'
        L = self.lib
        th = math.radians(TH0)
        c, sn = math.cos(th), math.sin(th)
        along = (0, c, sn)
        down = (0, sn, -c)
        self.lift = dict(c=c, sn=sn, Yp=PIV_Y + L_ARM * c, Zl=ZP_L + L_ARM * sn)
        for s in (-1, 1):
            side = 'R' if s > 0 else 'L'
            # Turm: 24 Loecher, steht auf dem Innenprofil, Rueckseite innen bei |x| = 3,45, Schenkel aussen
            self.channel(G, 24, (s * (XI + .3), PIV_Y, 2.575), (s * (XI + .3), PIV_Y + .5, 2.575), (0, 0, 1), (s, 0, 0))
            # Knotenblech 2 x 5 innen ueber Profil und Turm
            ox = XI - T if s > 0 else -XI
            self.put(G, L.plate(2, 5), mat((ox, PIV_Y - .25, AX - .75), (0, 1, 0), (0, 0, 1), (1, 0, 0)))
            for y in (PIV_Y, PIV_Y + .5):
                for z in (AX - .5, 3.075):
                    self.screw(G, (s * (XI - T), y, z), (s, 0, 0), 0.375, nut_gap=2 * T, stage=1)
            # Liftmotor innen am Turm, Welle bei Z_PIN, Befestigungsloecher darueber
            self.motor(G, (s * XI, PIV_Y, Z_PIN), (s, 0, 0), (0, 0, 1), stage=2)
            U = 'Liftritzel ' + side
            end = self.stack(U, [('sp313', .5), ('sp313', .125), ('g12', .376), ('collar', .26)], XI + T, s, PIV_Y, Z_PIN)
            self.put(U, L.shaft(end - (XI - .4)), mat((s * (XI - .4), PIV_Y, Z_PIN), (s, 0, 0), (0, 1, 0), (0, 0, s)))
            self.units[U] = self.groups[U]
            # untere Arme (21 Loecher), Schraubgelenk am Turm
            A = 'Arme unten'
            ra = (s * (ARM_X + .3), PIV_Y, ZP_L)
            self.channel(A, 21, ra, add(ra, mul(down, .5)), along, (s, 0, 0))
            self.screw(A, (s * XI, PIV_Y, ZP_L), (s, 0, 0), 1.5, nut_gap=ARM_X + T - XI, stage=7)
            self.stack(A, [('sp313', .875), ('sp313', .125), ('wash', .03125)], XI + T, s, PIV_Y, ZP_L)
            # obere Arme mit 60T (gebohrt, an den Arm geschraubt), Schraubgelenk am Turm
            B = 'Arme oben'
            ra = (s * (ARM_X + .3), PIV_Y, ZP_U)
            self.channel(B, 21, ra, add(ra, mul(down, .5)), along, (s, 0, 0))
            self.screw(B, (s * XI, PIV_Y, ZP_U), (s, 0, 0), 1.5, nut_gap=ARM_X + T - XI, stage=7)
            self.stack(B, [('sp313', .5), ('sp313', .125)], XI + T, s, PIV_Y, ZP_U)
            gx = (s, 0, 0)
            self.put(B, L.vex('g60d'), mat((s * (G0 + .188), PIV_Y, ZP_U), gx, along, cross(gx, along)))
            self.stack(B, [('wash', .03125)], G0 + .376, s, PIV_Y, ZP_U)
            for k in (-.5, .5):
                self.screw(B, (s * G0, PIV_Y + k * c, ZP_U + k * sn), (s, 0, 0), 0.75, nut_gap=ARM_X + T - G0, stage=0)   # Vormontage 60T an Arm
        for n in ('Arme unten', 'Arme oben'):
            self.units[n] = self.groups[n]

    # ---------------- Schlitten + Greifer ----------------
    def build_carriage(self):
        L = self.lib
        Yp, Zl = self.lift['Yp'], self.lift['Zl']
        S = 'Schlitten'
        for s in (-1, 1):
            self.channel(S, 8, (s * (BAR_X - .3), Yp, Zl - .5), (s * (BAR_X - .3), Yp + .5, Zl - .5), (0, 0, 1), (-s, 0, 0))
            for z in (Zl, Zl + 2.0):   # Schraubgelenke zu den Armen
                self.screw(S, (s * (BAR_X - T), Yp, z), (s, 0, 0), 0.875, nut_gap=ARM_X + T - (BAR_X - T), stage=8)
                self.stack(S, [('sp313', .375), ('wash62', .0625), ('wash', .03125)], BAR_X, s, Yp, z)
        # Quertraeger hinter den Holmen (16 Loecher), Schenkel nach hinten
        self.channel(S, 16, (-3.75, Yp - .55, Zl + 1.0), (-3.75, Yp - .55, Zl + 1.5), (1, 0, 0), (0, -1, 0))
        for s in (-1, 1):
            for z in (Zl + 1.0, Zl + 1.5):
                self.screw(S, (s * 3.75, Yp - .25 - T, z), (0, 1, 0), 0.375, nut_gap=2 * T, stage=5)
        # Greiferkopf (PETG) auf zwei 0,875"-Abstandsbolzen ueber dem Quertraeger
        zh = Zl + 1.75 + 0.875
        for x in (-2.25, 2.25):
            self.put(S, L.vex('standoff', L='0.875'), mat((x, Yp - .55, Zl + 1.75), (1, 0, 0), (0, 0, 1), (0, -1, 0)))
            self.screw(S, (x, Yp - .55, Zl + 1.75 - T), (0, 0, 1), 0.25, stage=5)
            self.screw(S, (x, Yp - .55, zh + 1.1), (0, 0, -1), 1.25, stage=6)
        pin_y = Yp - 0.6

        def head(lib, comp):
            lib.body_box(comp, (-2.5, -1.8, 0), (2.5, 0.4, 1.1))
            lib.drill_z(comp, [(-2.25, -.55), (2.25, -.55), (0, -1.1), (0, -1.6)], 0.19)
            lib.drill_z(comp, [(0, -0.6)], 0.2)
        self.put(S, L.printed('kopf', 'Greiferkopf (PETG)', head), mat((0, Yp, zh), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
        # Greifermotor steht auf dem Kopf, Welle nach unten zum Ritzel
        self.motor(S, (0, pin_y, zh + 1.1), (0, 0, -1), (0, -1, 0), screws=False)
        for k in (1.0, 0.5):
            self.screw(S, (0, pin_y - k, zh), (0, 0, 1), 1.5, stage=6)
        # Ritzel 12T (senkrecht) unter dem Kopf
        zr = Zl + 2.3
        R = 'Greiferritzel'
        self.put(R, L.vex('g12'), mat((0, pin_y, zr), (0, 0, 1), (1, 0, 0), (0, 1, 0)))
        self.put(R, L.shaft(zh + 1.1 + .4 - (zr - .2)), mat((0, pin_y, zh + 1.5), (0, 0, -1), (1, 0, 0), (0, -1, 0)))
        self.units[R] = self.groups[R]
        self.claw = dict(pin_y=pin_y, zr=zr, Yp=Yp, Zl=Zl)
        # Backen (PETG, V-Kerbe 90 Grad) mit Zahnstangen; j = Abstand Backenflaeche zur Mitte (1,29" = Cup)
        j, d, zc = 1.29, 1.0, GRIP_DY

        def rack24(lib, comp):
            """Zahnstange 24 DP: Teilung pi/24, Kopf 1/24, Fuss 1,25/24, Eingriffswinkel 20 Grad; Zaehne zeigen nach -y."""
            p, ha, hf, tn = math.pi / 24, 1 / 24, 1.25 / 24, math.tan(math.radians(20))
            pts = [(-1.25, 0.30)]
            x = -1.25
            pts.append((x, hf))
            k = 0
            while x + p <= 1.25 + 1e-9:
                c = x + p / 2
                pts += [(c - p / 4 - hf * tn, hf), (c - p / 4 + ha * tn, -ha), (c + p / 4 - ha * tn, -ha), (c + p / 4 + hf * tn, hf)]
                x += p
            pts += [(1.25, hf), (1.25, 0.30)]
            lib.body_poly(comp, pts, 0, 0.3)
        for s in (-1, 1):
            Bk = 'Backe ' + ('R' if s > 0 else 'L')
            xo = j + d + .2
            pts = [(j, -.2), (j, zc - d), (j + d, zc), (j, zc + d), (j, 3.2), (xo, 3.2), (xo, -.2)]
            if s < 0:
                pts = [(-x, y) for x, y in pts][::-1]
            ty0, ty1 = (-0.048, 0.152) if s > 0 else (-1.352, -1.152)

            def jaw(lib, comp, pts=pts, s=s, ty0=ty0, ty1=ty1):
                lib.body_poly(comp, pts, 0, 0.9)
                x0, x1 = (-0.96, 1.6) if s > 0 else (-1.6, 0.96)
                lib.body_box(comp, (x0, ty0, 0.9), (x1, ty1, 1.35))
                if s < 0:
                    lib.body_box(comp, (-1.6, -1.352, 0.9), (-1.29, -0.2, 1.35))
            self.put(Bk, L.printed('backe' + str(s), 'Greiferbacke %s (PETG)' % ('rechts' if s > 0 else 'links'), jaw),
                     mat((0, Yp, Zl + GRIP_DZ - 0.45), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
            # Zahnstange (PETG, 24DP, Teilungslinie = Teilkreis des 12T-Ritzels), Zaehne zum Ritzel
            rk = L.printed('zahnstange', 'Zahnstange 24DP 2,5" (PETG)', rack24)
            if s > 0:
                self.put(Bk, rk, mat((s * (j - 1.0), pin_y + .25, zr - .15), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
            else:
                self.put(Bk, rk, mat((s * (j - 1.0), pin_y - .25, zr - .15), (-1, 0, 0), (0, -1, 0), (0, 0, 1)))
            self.units[Bk] = self.groups[Bk]
        self.units[S] = self.groups[S]

    # ---------------- Koecher ----------------
    def build_quiver(self):
        G = '10 Rahmen'
        L = self.lib
        yg = self.lift['Yp'] + GRIP_DY
        for x, y in ((3.25, 3.0), (3.25, 5.0), (-3.25, 5.0)):
            self.put(G, L.vex('standoff', L='2.000'), mat((x, y, DECK + T), (1, 0, 0), (0, 0, 1), (0, -1, 0)))
            self.screw(G, (x, y, DECK), (0, 0, 1), 0.25, stage=4)
            self.screw(G, (x, y, QB_TOP), (0, 0, -1), 0.375, stage=9)

        def base(lib, comp):
            lib.body_box(comp, (-3.9, 2.85, 0), (3.4, 5.15, 0.125))
            sk = comp.sketches.add(comp.xYConstructionPlane)
            sk.sketchCurves.sketchCircles.addByCenterRadius(lib._sk_pt(sk, (0, yg, 0)), v(1.7))
            lib._extrude_z(comp, sk.profiles.item(0), 0, 0.125)
            lib.body_ring_sector(comp, 0, yg, 1.7, 1.82, 230, 310, 0.125, 3.3)
            lib.drill_z(comp, [(3.25, 3.0), (3.25, 5.0), (-3.25, 5.0), (-2.75, 4.0)], 0.19)
        self.put(G, L.printed('koecher', 'Koecher (PETG)', base), mat((0, 0, QB_TOP - 0.125), (1, 0, 0), (0, 1, 0), (0, 0, 1)), stage=9)
        # IMU flach auf dem Koecherboden links (steif ueber 2"-Bolzen mit dem Rahmen verbunden), Schraube von oben + Mutter
        org = native_to_world((-2.75, 4.0, QB_TOP + 0.02), (0, 0.5, -0.256), (1, 0, 0), (0, 1, 0), (0, 0, 1))
        self.put('30 Elektronik', L.vex('imu'), mat(org, (1, 0, 0), (0, 1, 0), (0, 0, 1)), stage=10)
        self.screw('30 Elektronik', (-2.75, 4.0, QB_TOP + 0.512), (0, 0, -1), 0.875, nut_gap=0.512 + 0.125, stage=10)
        REPORT.append('Startwinkel Lift %.2f Grad, Greifpunkt Y %.3f Z %.3f' % (TH0, yg, QB_TOP + 4.0))

    # ---------------- Aufnahme: Band, Kipprolle, Kette ----------------
    def build_intake(self):
        G = '40 Aufnahme'
        L = self.lib
        u = (0, math.cos(BAND_A), math.sin(BAND_A))
        pp = (0, -math.sin(BAND_A), math.cos(BAND_A))       # senkrecht zum Band, nach oben/hinten
        p0 = (0, BAND_Y0, BAND_Z0)
        p1 = add(p0, mul(u, BAND_L))
        for s in (-1, 1):
            ra = (s * 2.2, BAND_Y0, BAND_Z0)
            self.channel(G, 24, ra, add(ra, mul(pp, .5)), u, (s, 0, 0))
        # Band (TPU-Gurt vereinfacht) und Mitnehmer
        def belt(lib, comp):
            lib.body_box(comp, (-1.5, -0.05, 0), (1.5, 0.05, BAND_L))
        def cleat(lib, comp):
            lib.body_box(comp, (-1.5, 0, 0), (1.5, 0.45, 0.12))
        bc = L.printed('gurt', 'Foerderband (TPU, vereinfacht)', belt)
        cc = L.printed('mitnehmer', 'Mitnehmer (TPU)', cleat)
        for off in (-1.06, 1.06):
            o = add(p0, mul(pp, off))
            self.put(G, bc, mat(o, (-1, 0, 0), pp, u))
        t = 0.9
        while t < BAND_L - 0.5:
            o = add(add(p0, mul(u, t)), mul(pp, 1.11))
            self.put(G, cc, mat(o, (-1, 0, 0), pp, u))
            t += 1.75
        # Rollen (TPU, D 2") auf Wellen; Kettenrad 16T rechts
        def roller(lib, comp):
            lib.body_cyl_x(comp, 1.0, -1.5, 1.5)
        rc = L.printed('rolle', 'Bandrolle D2 (TPU)', roller)
        for name, c in (('Rolle unten', p0), ('Rolle oben', p1)):
            self.put(name, rc, mat(c, (1, 0, 0), (0, 1, 0), (0, 0, 1)))
            x0 = -(2.45 + 0.26)
            self.stack(name, [('collar', .26)], 2.45, -1, c[1], c[2])
            self.put(name, L.shaft(CHAIN_X + 0.25 - x0), mat((x0, c[1], c[2]), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
            self.put(name, L.vex('spr16'), mat((CHAIN_X, c[1], c[2]), (0, 1, 0), (0, 0, 1), (1, 0, 0)))
            self.units[name] = self.groups[name]
        # Kipprolle zwischen den Tuermen, angetrieben vom Aufnahmemotor rechts aussen
        def tip(lib, comp):
            lib.body_cyl_x(comp, 0.55, -2.4, 2.4)
            for k in range(6):
                a = k * math.pi / 3
                sk_y, sk_z = 0.55 * math.cos(a), 0.55 * math.sin(a)
                lib.body_box(comp, (-2.3, sk_y - 0.03, sk_z - 0.03), (2.3, sk_y + 0.03, sk_z + 0.03))
        K = 'Kipprolle'
        self.put(K, L.printed('kipprolle', 'Kipprolle mit Rippen (TPU)', tip), mat((0, TIP_Y, TIP_Z), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
        self.put(K, L.vex('spr16'), mat((CHAIN_X, TIP_Y, TIP_Z), (0, 1, 0), (0, 0, 1), (1, 0, 0)))
        self.stack(K, [('sp313', .5)], XI + T, -1, TIP_Y, TIP_Z)
        self.stack(K, [('collar', .26)], XI + T + .5, -1, TIP_Y, TIP_Z)
        self.stack(K, [('sp313', .375), ('wash62', .0625)], XI + T, 1, TIP_Y, TIP_Z)
        x0, x1 = -(XI + T + .76), 4.0 + T + 0.4
        self.put(K, L.shaft(x1 - x0), mat((x0, TIP_Y, TIP_Z), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
        self.units[K] = self.groups[K]
        self.intake = dict(p0=p0, p1=p1)
        # Aufnahmemotor: Winkel 1x1 an der hinteren Turmwange rechts, Motor aussen, Welle = Kipprollenwelle
        self.intake_motor_mount()
        # Kette (vereinfacht, Ebene x = CHAIN_X): Aussentangenten um die drei Kettenraeder
        rp = 0.64
        cs = [(p0[1], p0[2]), (TIP_Y, TIP_Z), (p1[1], p1[2])]
        def chain(lib, comp):
            for i in range(3):
                a, b = cs[i], cs[(i + 1) % 3]
                dy, dz = b[0] - a[0], b[1] - a[1]
                ln = math.hypot(dy, dz)
                ny_, nz_ = dz / ln, -dy / ln
                cy = sum(c[0] for c in cs) / 3; cz = sum(c[1] for c in cs) / 3
                if (a[0] + ny_ - cy) * ny_ + (a[1] + nz_ - cz) * nz_ < 0:
                    ny_, nz_ = -ny_, -nz_
                pts = [(a[0] + ny_ * (rp - .05), a[1] + nz_ * (rp - .05)), (b[0] + ny_ * (rp - .05), b[1] + nz_ * (rp - .05)),
                       (b[0] + ny_ * (rp + .05), b[1] + nz_ * (rp + .05)), (a[0] + ny_ * (rp + .05), a[1] + nz_ * (rp + .05))]
                sk = comp.sketches.add(comp.yZConstructionPlane)
                ln_ = sk.sketchCurves.sketchLines
                sp = [lib._sk_pt(sk, (0, y, z)) for y, z in pts]
                for k in range(4):
                    ln_.addByTwoPoints(sp[k], sp[(k + 1) % 4])
                ex = comp.features.extrudeFeatures
                inp = ex.createInput(sk.profiles.item(0), adsk.fusion.FeatureOperations.NewBodyFeatureOperation)
                inp.setSymmetricExtent(adsk.core.ValueInput.createByReal(v(0.17)), True)
                ex.add(inp)
        self.put(G, L.printed('kette', 'Kette 6P (vereinfacht, Laenge im Kopf)', chain), mat((CHAIN_X, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
        # Halteklotz unten (beide Seiten): Band-Profil -> Innenprofil
        tb = 1.5
        hb = add(p0, mul(u, tb))
        def block_lo(lib, comp):
            lib.body_box(comp, (0, -5.25, 1.9), (1.0, -4.75, 2.9))
        bl = L.printed('block_unten', 'Halteklotz Band unten (PETG)', block_lo)
        for s in (-1, 1):
            x0 = 2.45 if s > 0 else -3.45
            self.put(G, bl, mat((x0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)), stage=2)
            self.screw(G, (s * (XI + T), -5.0, AX), (-s, 0, 0), 0.75, stage=2)
            self.screw(G, (s * (1.9 - .087 + .087), hb[1], hb[2]), (s, 0, 0), 0.75, stage=4)
        # Strebe links vom Turm nach vorn + Halteklotz oben (Band links -> Strebe)
        s = -1
        self.channel('10 Rahmen', 12, (s * (XI - .3), PIV_Y, 10.075), (s * (XI - .3), PIV_Y, 10.575), (0, 1, 0), (-s, 0, 0))
        for y in (PIV_Y, PIV_Y + .5):
            for z in (10.075, 10.575):
                self.screw('10 Rahmen', (s * (XI - T), y, z), (s, 0, 0), 0.375, nut_gap=2 * T, stage=4)
        t_hi = round((-1.0 - BAND_Y0) / math.cos(BAND_A) * 2) / 2
        hh = add(p0, mul(u, t_hi))
        def block_hi(lib, comp):
            lib.body_box(comp, (-3.35, -1.25, hh[2] - 0.3), (-2.45, -0.75, 9.825))
        self.put(G, L.printed('block_oben', 'Halteklotz Band oben (PETG)', block_hi), mat((0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)))
        self.screw(G, (-3.15, -1.0, hh[2] - 0.3), (0, 0, 1), 1.25, nut_gap=(9.825 - (hh[2] - 0.3)) + T, stage=4)
        self.screw(G, (-1.9, hh[1], hh[2]), (-1, 0, 0), 0.75, stage=4)
        REPORT.append('Band: untere Rolle (%.2f, %.2f), obere Rolle (%.2f, %.2f)' % (p0[1], p0[2], p1[1], p1[2]))

    def intake_motor_mount(self):
        """Aufnahmemotor rechts aussen: Alu-Winkel 1x1 (4 Loecher) an der hinteren Wange des rechten Turms,
        Motor auf dem abstehenden Schenkel, Welle = Kipprollenwelle."""
        G = '10 Rahmen'
        L = self.lib
        ang = L.vex('angle', n=4)
        for i in range(ang.bRepBodies.count - 1, -1, -1):      # Streukoerper aus der Bibliothek (17" entfernt) entfernen
            b = ang.bRepBodies.item(i)
            if b.boundingBox.minPoint.y / IN > 3:
                try:
                    b.deleteMe()
                except Exception:
                    b.isLightBulbOn = False
        ox, oy, oz = 3.767, TIP_Y - 0.017, TIP_Z + 0.5
        self.put(G, ang, mat((ox, oy, oz), (0, 1, 0), (0, 0, -1), (-1, 0, 0)))
        for z in (oz, oz - 1.5):
            self.screw(G, (ox, oy - 0.296, z), (0, 1, 0), 0.375, nut_gap=2 * T, stage=4)
        # Motor: Vormontage auf dem Winkel (Schrauben vor dem Anschrauben an den Turm setzen)
        self.motor(G, (4.0 + T, TIP_Y, TIP_Z), (-1, 0, 0), (0, 0, -1), stage=0)   # Stufe 0 = Vormontage auf dem Tisch

    # ---------------- Odometrie (Tracking-Raeder) ----------------
    def build_odometry(self):
        """Zwei Einheiten, je: Halter (fest, unter der Brain-Platte) + Schwinge mit Rotation Sensor + Rad auf 1/8"-Welle.
        Montage: jede Einheit auf dem Tisch vormontieren (Sensor- und Drehpunktschrauben), nach dem Brain von oben anschrauben."""
        L = self.lib
        G = '10 Rahmen'
        z = OD_R
        F_S, F_W, Q_S, Q_W = '50 Odometrie Laengs Schwinge', '51 Odometrie Laengs Rad', '52 Odometrie Quer Schwinge', '53 Odometrie Quer Rad'
        fx, fy = F_C
        qx, qy = Q_C
        AX_X, AX_Y = ((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((0, 1, 0), (-1, 0, 0), (0, 0, 1))
        fb = DECK - 0.15                 # Unterkante Halterflansch (Flansch 0,15" unter der Platte)
        MH = [(xn, zn) for xn in (-1.216, 1.216) for zn in (-0.335, 0.335)]   # Sensor-Befestigungsloecher (nativ X, Z)

        # ===== Laengsrad: Welle entlang X; Schwinge + Sensor innen (+x) =====
        sp0 = fx + OD_W + 0.05
        sp1 = sp0 + 0.2
        c, sn = math.cos(math.radians(F_TILT)), math.sin(math.radians(F_TILT))
        u = (0, -c, -sn)                  # Sensor nativ X (langes Ende nach vorn unten)
        nzs = (0, -sn, c)                 # Sensor nativ Z
        so = (sp1 + 0.722, fy + 0.877 * u[1], z + 0.877 * u[2])   # Sensor-Ursprung: Welle liegt bei nativ X = -0,877
        self.put(F_W, L.vex('omni275'), mat((fx, fy, z), (0, -1, 0), (1, 0, 0), (0, 0, 1)), stage=7)
        self.put(F_W, L.vex('collar'), mat((fx - OD_W - 0.13, fy, z), *AX_X), stage=7)
        self.put(F_W, L.vex('wash'), mat((fx + OD_W, fy, z), *AX_X), stage=7)
        self.put(F_W, L.shaft(2.25), mat((sp1 + 0.722 - 2.25, fy, z), *AX_X), stage=7)
        self.put(F_S, L.vex('rot'), mat(so, u, (1, 0, 0), nzs), stage=7)
        holes_f = [(so[1] + xn * u[1] + zn * nzs[1], so[2] + xn * u[2] + zn * nzs[2]) for xn, zn in MH]
        for hy, hz in holes_f:
            self.screw(F_S, (sp1 + 0.801, hy, hz), (-1, 0, 0), 0.875, stage=0)
        py, pz = F_PIV
        arm = (py - fy, pz - z)
        al = math.hypot(*arm)
        xm = (sp0 + sp1) / 2
        sw = L.solid('odo_f_schwinge', 'Odometrie-Schwinge laengs (PETG)', [
            ('obox', (xm, so[1], so[2]), (0, u[1], u[2]), 2.72, 0.2, 0.94),
            ('obox', (xm, (fy + py) / 2, (z + pz) / 2), (0, arm[0] / al, arm[1] / al), al, 0.2, 0.44),
            ('cyl', (sp0, py, pz), (sp1, py, pz), 0.22)],
            [('cyl', (sp0 - 0.1, fy, z), (sp1 + 0.1, fy, z), 0.1),
             ('cyl', (sp0 - 0.1, py, pz), (sp1 + 0.1, py, pz), 0.075)] +
            [('cyl', (sp0 - 0.1, hy, hz), (sp1 + 0.1, hy, hz), 0.075) for hy, hz in holes_f])
        self.put(F_S, sw, adsk.core.Matrix3D.create(), stage=7)
        self.screw(F_S, (sp1 + 0.2, py, pz), (-1, 0, 0), 0.375, stage=0)        # Drehpunkt: durch Lasche in die Schwinge (Gewindeeinsatz)
        ha = L.solid('odo_f_halter', 'Odometrie-Halter laengs (PETG)', [
            ('box', (sp1, py - 0.22, pz - 0.22), (sp1 + 0.2, 5.58, fb)),          # Lasche am Drehpunkt
            ('box', (-2.95, 5.2, fb), (sp1 + 0.2, 5.58, DECK)),                   # Steg hinter dem Rad
            ('box', (-2.95, 4.8, fb), (-2.6, 5.2, DECK)),
            ('box', (-3.4, 4.2, fb), (-2.6, 4.8, DECK))],                         # Fuss mit zwei Gewindeeinsaetzen
            [('cyl', (sp1 - 0.1, py, pz), (sp1 + 0.3, py, pz), 0.085),
             ('cyl', (-2.75, 4.5, fb - 0.1), (-2.75, 4.5, DECK + 0.1), 0.095),
             ('cyl', (-3.25, 4.5, fb - 0.1), (-3.25, 4.5, DECK + 0.1), 0.095)])
        self.put(G, ha, adsk.core.Matrix3D.create(), stage=7)
        for x in (-2.75, -3.25):
            self.screw(G, (x, 4.5, DECK + T), (0, 0, -1), 0.25, stage=7)
        self.units[F_S] = self.groups[F_S]
        self.units[F_W] = self.groups[F_W]

        # ===== Querrad: Welle entlang Y; Schwinge + Sensor hinten (+y), langes Sensorende nach +x =====
        sq0 = qy + OD_W + 0.05
        sq1 = sq0 + 0.2
        sqo = (qx + 0.877, sq1 + 0.722, z)
        self.put(Q_W, L.vex('omni275'), mat((qx, qy, z), (1, 0, 0), (0, 1, 0), (0, 0, 1)), stage=7)
        self.put(Q_W, L.vex('collar'), mat((qx, qy - OD_W - 0.13, z), *AX_Y), stage=7)
        self.put(Q_W, L.vex('wash'), mat((qx, qy + OD_W, z), *AX_Y), stage=7)
        self.put(Q_W, L.shaft(2.25), mat((qx, sq1 + 0.714 - 2.25, z), *AX_Y), stage=7)
        self.put(Q_S, L.vex('rot'), mat(sqo, (1, 0, 0), (0, 1, 0), (0, 0, 1)), stage=7)
        holes_q = [(sqo[0] + xn, z + zn) for xn, zn in MH]
        for hx, hz in holes_q:
            self.screw(Q_S, (hx, sqo[1] + 0.079, hz), (0, -1, 0), 0.875, stage=0)
        px, pz = Q_PIV
        sw = L.solid('odo_q_schwinge', 'Odometrie-Schwinge quer (PETG)', [
            ('box', (qx - 0.487, sq0, z - 0.47), (qx + 2.241, sq1, z + 0.47)),
            ('box', (px - 0.2, sq0, z), (px + 0.2, sq1, pz)),
            ('cyl', (px, sq0, pz), (px, sq1, pz), 0.22)],
            [('cyl', (qx, sq0 - 0.1, z), (qx, sq1 + 0.1, z), 0.1),
             ('cyl', (px, sq0 - 0.1, pz), (px, sq1 + 0.1, pz), 0.075)] +
            [('cyl', (hx, sq0 - 0.1, hz), (hx, sq1 + 0.1, hz), 0.075) for hx, hz in holes_q])
        self.put(Q_S, sw, adsk.core.Matrix3D.create(), stage=7)
        self.screw(Q_S, (px, sq1 + 0.2, pz), (0, -1, 0), 0.375, stage=0)
        ha = L.solid('odo_q_halter', 'Odometrie-Halter quer (PETG)', [
            ('box', (px - 0.22, sq1, pz - 0.22), (px + 0.22, sq1 + 0.2, fb)),     # Lasche am Drehpunkt
            ('box', (px - 0.22, 3.3, fb), (3.42, 4.8, DECK))],                    # Flansch rechts neben dem Brain
            [('cyl', (px, sq1 - 0.1, pz), (px, sq1 + 0.3, pz), 0.085),
             ('cyl', (3.25, 3.5, fb - 0.1), (3.25, 3.5, DECK + 0.1), 0.095),
             ('cyl', (3.25, 4.0, fb - 0.1), (3.25, 4.0, DECK + 0.1), 0.095)])
        self.put(G, ha, adsk.core.Matrix3D.create(), stage=7)
        for y in (3.5, 4.0):
            self.screw(G, (3.25, y, DECK + T), (0, 0, -1), 0.25, stage=7)
        self.units[Q_S] = self.groups[Q_S]
        self.units[Q_W] = self.groups[Q_W]
        self.odom = dict(F=(F_S, F_W, (py, pz), (fy, z)), Q=(Q_S, Q_W, (px, pz), (qx, z)))
        REPORT.append('Odometrie (Roboter-Koordinaten: x vorn, y links, Mitte = Mitte Antriebsraeder): '
                      'parallel_y_in = %.3f, perpendicular_x_in = %.3f, Raddurchmesser %.3f in (nominell, am Roboter kalibrieren)'
                      % (-fx, qy, 2 * OD_R))

    def revolute_axis(self, name, occ, other, p, axis, lim=None):
        """Drehgelenk um eine Weltachse ('x', 'y', 'z') durch Punkt p; lim = (min, max) in rad."""
        planes = {'x': self.root.yZConstructionPlane, 'y': self.root.xZConstructionPlane, 'z': self.root.xYConstructionPlane}
        key = '_jsk_' + axis
        if not hasattr(self, key):
            sk = self.root.sketches.add(planes[axis])
            sk.name = 'Gelenkachsen ' + axis.upper()
            setattr(self, key, sk)
        sk = getattr(self, key)
        pt = sk.sketchPoints.add(sk.modelToSketchSpace(P(p)))
        joints = self.root.asBuiltJoints
        inp = joints.createInput(occ, other, adsk.fusion.JointGeometry.createByPoint(pt))
        inp.setAsRevoluteJointMotion(adsk.fusion.JointDirections.ZAxisJointDirection)
        j = joints.add(inp)
        j.name = name
        self.joints[name] = j
        if lim:
            try:
                rl = j.jointMotion.rotationLimits
                rl.isMinimumValueEnabled = True; rl.minimumValue = lim[0]
                rl.isMaximumValueEnabled = True; rl.maximumValue = lim[1]
            except Exception as e:
                REPORT.append('Grenzen %s: %s' % (name, e))
        return j

    # ---------------- Gelenke ----------------
    def axis_point(self, p):
        """Gelenkpunkt als Skizzenpunkt in der YZ-Ebene (x = 0); Drehachse = Normale = X."""
        if not hasattr(self, '_jsk'):
            self._jsk = self.root.sketches.add(self.root.yZConstructionPlane)
            self._jsk.name = 'Gelenkachsen'
        return self._jsk.sketchPoints.add(self._jsk.modelToSketchSpace(P(p)))

    def revolute(self, name, occ, other, p):
        joints = self.root.asBuiltJoints
        inp = joints.createInput(occ, other, adsk.fusion.JointGeometry.createByPoint(self.axis_point(p)))
        inp.setAsRevoluteJointMotion(adsk.fusion.JointDirections.ZAxisJointDirection)
        j = joints.add(inp)
        j.name = name
        self.joints[name] = j
        return j

    def revolute_z(self, name, occ, other, p):
        if not hasattr(self, '_jskz'):
            self._jskz = self.root.sketches.add(self.root.xYConstructionPlane)
            self._jskz.name = 'Gelenkachsen senkrecht'
        pt = self._jskz.sketchPoints.add(self._jskz.modelToSketchSpace(P(p)))
        joints = self.root.asBuiltJoints
        inp = joints.createInput(occ, other, adsk.fusion.JointGeometry.createByPoint(pt))
        inp.setAsRevoluteJointMotion(adsk.fusion.JointDirections.ZAxisJointDirection)
        j = joints.add(inp); j.name = name; self.joints[name] = j
        return j

    def slider_x(self, name, occ, other, p):
        joints = self.root.asBuiltJoints
        inp = joints.createInput(occ, other, adsk.fusion.JointGeometry.createByPoint(self.axis_point(p)))
        inp.setAsSliderJointMotion(adsk.fusion.JointDirections.ZAxisJointDirection)
        j = joints.add(inp); j.name = name; self.joints[name] = j
        return j

    def gear_link(self, name, j1, j2, deg1, deg2, reverse=True):
        ml = self.root.motionLinks
        inp = ml.createInput(j1, j2)
        inp.valueOne = adsk.core.ValueInput.createByString('%g deg' % deg1)
        inp.valueTwo = adsk.core.ValueInput.createByString('%g deg' % deg2)
        inp.isReversed = reverse
        m = ml.add(inp)
        m.name = name
        return m

    def build_joints(self):
        frame = self.groups['10 Rahmen']
        for name, occ in self.units.items():
            if not (name.startswith('Motorwelle') or name.startswith('Rad')):
                continue
            pos = name.split()[-1][0]
            y = (1.0 if pos == 'V' else -1.0) * (1.0 if name.startswith('Motorwelle') else 4.0)
            self.revolute(name, occ, frame, (0, y, AX))
        if hasattr(self, 'lift'):
            Yp, Zl = self.lift['Yp'], self.lift['Zl']
            ju = self.revolute('Arme unten', self.units['Arme unten'], frame, (0, PIV_Y, ZP_L))
            jo = self.revolute('Arme oben', self.units['Arme oben'], frame, (0, PIV_Y, ZP_U))
            self.revolute('Schlitten unten', self.units['Schlitten'], self.units['Arme unten'], (0, Yp, Zl))
            self.revolute('Schlitten oben', self.units['Schlitten'], self.units['Arme oben'], (0, Yp, Zl + 2.0))
            for sd in ('L', 'R'):
                jr = self.revolute('Liftritzel ' + sd, self.units['Liftritzel ' + sd], frame, (0, PIV_Y, Z_PIN))
                self.gear_link('Lift 12:60 ' + sd, jr, jo, 60, 12)
            try:
                lim = ju.jointMotion.rotationLimits
                lim.isMinimumValueEnabled = True; lim.minimumValue = 0.0
                lim.isMaximumValueEnabled = True; lim.maximumValue = math.radians(60.0 - TH0)
            except Exception as e:
                REPORT.append('Liftgrenzen nicht gesetzt: %s' % e)
        if hasattr(self, 'claw'):
            cl = self.claw
            S = self.units['Schlitten']
            jp = self.revolute_z('Greiferritzel', self.units['Greiferritzel'], S, (0, cl['pin_y'], cl['zr']))
            for sd, sgn in (('R', True), ('L', False)):
                jb = self.slider_x('Backe ' + sd, self.units['Backe ' + sd], S, (0, cl['Yp'], cl['zr']))
                try:
                    ml = self.root.motionLinks
                    inp = ml.createInput(jp, jb)
                    inp.valueOne = adsk.core.ValueInput.createByString('360 deg')
                    inp.valueTwo = adsk.core.ValueInput.createByString('%.4f in' % (math.pi * 0.5))
                    inp.isReversed = sgn
                    m = ml.add(inp); m.name = 'Zahnstange ' + sd
                except Exception as e:
                    REPORT.append('Zahnstangen-Kopplung %s: %s' % (sd, e))
                try:
                    lim = jb.jointMotion.slideLimits
                    lo, hi = (-1.1, 0.3) if sd == 'R' else (-0.3, 1.1)   # schliessen bis Pin (j = 0,19"), oeffnen +0,3"
                    lim.isMinimumValueEnabled = True; lim.minimumValue = v(lo)
                    lim.isMaximumValueEnabled = True; lim.maximumValue = v(hi)
                except Exception as e:
                    REPORT.append('Backengrenzen %s: %s' % (sd, e))
        if hasattr(self, 'intake'):
            ik = self.intake
            jk = self.revolute('Kipprolle', self.units['Kipprolle'], frame, (0, TIP_Y, TIP_Z))
            ju_ = self.revolute('Rolle unten', self.units['Rolle unten'], frame, (0, ik['p0'][1], ik['p0'][2]))
            jo_ = self.revolute('Rolle oben', self.units['Rolle oben'], frame, (0, ik['p1'][1], ik['p1'][2]))
            self.gear_link('Kette Kipprolle-Rolle unten', jk, ju_, 360, 360, reverse=False)
            self.gear_link('Kette Kipprolle-Rolle oben', jk, jo_, 360, 360, reverse=False)
        if hasattr(self, 'odom'):
            fs, fw, (py, pz), (fy, z) = self.odom['F']
            self.revolute_axis('Odometrie Laengs Schwinge', self.units[fs], frame, (0, py, pz), 'x', (-OD_HUB, OD_HUB))
            self.revolute_axis('Odometrie Laengs Rad', self.units[fw], self.units[fs], (0, fy, z), 'x')
            qs, qw, (px, pz), (qx, z) = self.odom['Q']
            self.revolute_axis('Odometrie Quer Schwinge', self.units[qs], frame, (px, 0, pz), 'y', (-OD_HUB, OD_HUB))
            self.revolute_axis('Odometrie Quer Rad', self.units[qw], self.units[qs], (qx, 0, z), 'y')
        # Zahnradkopplung: Motorwelle 84T -> Rad 60T (gegenlaeufig): 60 Grad Motor = 84 Grad Rad
        for s in ('L', 'R'):
            for p in ('V', 'H'):
                self.gear_link('Zahnrad 84:60 %s%s' % (p, s), self.joints['Motorwelle %s%s' % (p, s)],
                               self.joints['Rad %s%s' % (p, s)], 60, 84)


LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else os.environ.get('TEMP', '.'), 'letzter_lauf.txt')


def run(context):
    out = []
    try:
        for i in range(app.documents.count - 1, -1, -1):
            d = app.documents.item(i)
            if d.name.startswith(DOC_NAME) and d.dataFile is None:
                d.close(False)
        doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        doc.name = DOC_NAME
        design = adsk.fusion.Design.cast(app.activeProduct)
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        design.fusionUnitsManager.distanceDisplayUnits = adsk.fusion.DistanceUnits.InchDistanceUnits
        root = design.rootComponent
        lib = Lib(root)
        r = Robot(root, lib)
        r.build_frame()
        r.build_drive()
        r.build_electronics()
        r.build_lift()
        r.build_carriage()
        r.build_quiver()
        r.build_intake()
        if ODOM:
            r.build_odometry()
        lib.hold_occ.isLightBulbOn = False
        r.groups['10 Rahmen'].isGrounded = True
        r.groups['30 Elektronik'].isGrounded = True
        r.groups['40 Aufnahme'].isGrounded = True
        r.build_joints()
        app.activeViewport.fit()
        out.append('Teile: %d | Schrauben: %d | Gelenke: %d | Kopplungen: %d' % (
            root.allOccurrences.count, len(r.screws), root.asBuiltJoints.count, root.motionLinks.count))
        out += REPORT
    except Exception:
        out.append('FEHLER:\n' + traceback.format_exc())
        out += REPORT
    with open(LOG, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    print('\n'.join(out))
