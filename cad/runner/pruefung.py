# Frostbyte Runner – Pruefung: Kollisionen, Gelenke durchfahren, Werkzeugzugang je Schraube, Startvolumen
# Laeuft im aktiven Dokument "Frostbyte Runner". Ergebnis: pruefung.txt neben dem Script.
import adsk.core, adsk.fusion, traceback, os, time, math, re

IN = 2.54
app = adsk.core.Application.get()
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else os.environ.get('TEMP', '.'), 'pruefung.txt')

# Erwartete Beruehrungen (Gewinde, Passungen, Zahneingriffe, Schnappverbindungen)
EXPECT = [('Screw', 'Nut'), ('Screw', 'Motor'), ('Screw', 'Brain'), ('Screw', 'Standoff'), ('Screw', 'Greiferkopf'),
          ('Screw', 'Koecher'), ('Screw', 'Block'), ('Welle', ''), ('Collar', 'Welle'), ('Gear', 'Gear'), ('Gear', 'Rack'),
          ('Battery', 'Clip'), ('Screw', 'Battery'), ('Sprocket', 'Kette'), ('Screw', 'Halteklotz'), ('Gear', 'Zahnstange'),
          ('Zahnstange', 'Greiferbacke'), ('Screw', 'Inertial'), ('Screw', 'Rotation'), ('Screw', 'Odometrie')]
# Montagestufe je Baugruppe (kleiner = frueher montiert)
STAGE = {'10 Rahmen': 1, 'Motorwelle': 3, 'Rad': 3, '30 Elektronik': 5, 'Arme unten': 7, 'Arme oben': 7, 'Liftritzel': 6,
         'Schlitten': 8, 'Greiferritzel': 8, 'Backe': 9, '40 Aufnahme': 4, 'Rolle': 4, 'Kipprolle': 6,
         '50 Odometrie': 7, '51 Odometrie': 7, '52 Odometrie': 7, '53 Odometrie': 7}


def leaf_key(ent):
    """Pfad Gruppe+Teil eines Koerpers/Vorkommens (Ueberschneidungen innerhalb eines Bibliotheksteils zaehlen nicht)."""
    try:
        occ = ent if isinstance(ent, adsk.fusion.Occurrence) else ent.assemblyContext
        return '+'.join(occ.fullPathName.split('+')[:2]) if occ else ''
    except Exception:
        return ''


def comp_set(occ):
    """Alle Komponenten (Tokens) unterhalb eines Vorkommens, fuer Koerper ohne Kontext."""
    s = {occ.component.entityToken}
    for c in occ.component.allOccurrences:
        s.add(c.component.entityToken)
    return s


def side(ent, ca, cb):
    try:
        t = ent.parentComponent.entityToken
    except Exception:
        return '?'
    return ('A' if t in ca else '') + ('B' if t in cb else '')


def expected(a, b):
    for x, y in EXPECT:
        if (x in a and y in b) or (x in b and y in a):
            return True
    return False


def body_box(bs):
    xs = []; ys = []; zs = []
    for b in bs:
        bb = b.boundingBox
        xs += [bb.minPoint.x, bb.maxPoint.x]; ys += [bb.minPoint.y, bb.maxPoint.y]; zs += [bb.minPoint.z, bb.maxPoint.z]
    return (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)) if xs else None


def bodies(o):
    out = [b for b in o.bRepBodies if b.isVisible]
    for c in o.childOccurrences:
        out += bodies(c)
    return out


def overlap(a, b, tol=0.0):
    return a[0] < b[3] - tol and b[0] < a[3] - tol and a[1] < b[4] - tol and b[1] < a[4] - tol and a[2] < b[5] - tol and b[2] < a[5] - tol


def stage_of(group, occ=None, name=''):
    """Montagestufe: Attribut 'frostbyte/stufe' (Schrauben), sonst nach Teil und Baugruppe."""
    if occ is not None:
        for o in (occ, getattr(occ, 'nativeObject', None)):
            try:
                a = o.attributes.itemByName('frostbyte', 'stufe') if o is not None else None
                if a:
                    return int(a.value)
            except Exception:
                pass
    if group == '10 Rahmen':
        for key, st in (('16x5', 4), ('Koecher', 9), ('2.000 Standoff', 4), ('1x1 Angle', 4), ('V5 Motor', 2), ('Bearing', 3), ('12 - 2x', 4)):
            if key in name:
                return st
        return 1
    for k, v in STAGE.items():
        if group.startswith(k):
            return v
    return 5


def leaves(root):
    out = []
    for i in range(root.occurrences.count):
        g = root.occurrences.item(i)
        if g.component.name.startswith('00'):
            continue
        for c in g.childOccurrences:
            bs = bodies(c)
            bb = body_box(bs)
            if bb:
                out.append(dict(occ=c, bodies=bs, box=bb, group=g.component.name, name=c.component.name))
    return out


def collisions(des, L, subset=None, tol=0.001):
    res = {}
    idx = range(len(L)) if subset is None else subset
    for i in idx:
        for j in range(len(L)):
            if j == i or (subset is None and j < i) or (subset is not None and j in subset and j < i):
                continue
            a, b = L[i], L[j]
            if a['group'] == b['group'] and subset is not None:
                continue
            if not overlap(a['box'], b['box'], tol) or expected(a['name'], b['name']):
                continue
            ents = adsk.core.ObjectCollection.create(); ents.add(a['occ']); ents.add(b['occ'])
            inp = des.createInterferenceInput(ents)
            inp.areCoincidentFacesIncluded = False
            r = des.analyzeInterference(inp)
            if r is None or r.count == 0:
                continue
            vol = 0.0; pt = None
            ca, cb = comp_set(a['occ']), comp_set(b['occ'])
            for k in range(r.count):
                it = r.item(k)
                ka, kb = leaf_key(it.entityOne), leaf_key(it.entityTwo)
                if ka and ka == kb:
                    continue
                sa, sb = side(it.entityOne, ca, cb), side(it.entityTwo, ca, cb)
                if sa == sb and sa in ('A', 'B'):   # Ueberschneidung innerhalb eines Bibliotheksteils (Rollen/Nabe, Gehaeuse/Einsatz)
                    continue
                bd = it.interferenceBody
                if bd:
                    vol += bd.volume / IN ** 3
                    if pt is None:
                        bb = bd.boundingBox
                        pt = ((bb.minPoint.x + bb.maxPoint.x) / 2 / IN, (bb.minPoint.y + bb.maxPoint.y) / 2 / IN, (bb.minPoint.z + bb.maxPoint.z) / 2 / IN)
            if vol < 1e-4:
                continue
            key = tuple(sorted([a['group'] + '/' + a['name'], b['group'] + '/' + b['name']]))
            e = res.setdefault(key, [0, 0.0, pt]); e[0] += 1; e[1] += vol
    return res


def world_copy(tb, b):
    """Temporaere Kopie eines (Proxy-)Koerpers in Weltkoordinaten."""
    a = tb.copy(b)
    bb, ab = b.boundingBox, a.boundingBox
    if abs(bb.minPoint.x - ab.minPoint.x) + abs(bb.minPoint.y - ab.minPoint.y) + abs(bb.minPoint.z - ab.minPoint.z) > 1e-3:
        occ = b.assemblyContext
        if occ is not None:
            tb.transform(a, occ.transform2)
    return a


def tool_access(L, root):
    """Werkzeug (Schraubendreher T15: Spitze r 0,1" bis 3" Abstand) und Steckschluessel (r 0,25" bis 2") pruefen."""
    tb = adsk.fusion.TemporaryBRepManager.get()
    out = []
    screws = [x for x in L if 'Screw' in x['name']]
    for sc in screws:
        t = sc['occ'].transform2
        head = adsk.core.Point3D.create(t.translation.x, t.translation.y, t.translation.z)
        ydir = adsk.core.Vector3D.create(t.getAsCoordinateSystem()[2].x, t.getAsCoordinateSystem()[2].y, t.getAsCoordinateSystem()[2].z)
        # Schaft zeigt nativ +Y -> Werkzeug kommt von -Y
        d = ydir.copy(); d.normalize(); d.scaleBy(-1)
        p0 = head.copy(); v0 = d.copy(); v0.scaleBy(0.12 * IN); p0.translateBy(v0)
        p1 = head.copy(); v1 = d.copy(); v1.scaleBy(3.0 * IN); p1.translateBy(v1)
        tool = tb.createCylinderOrCone(p0, 0.1 * IN, p1, 0.1 * IN)
        tbx = tool.boundingBox
        tbox = (tbx.minPoint.x, tbx.minPoint.y, tbx.minPoint.z, tbx.maxPoint.x, tbx.maxPoint.y, tbx.maxPoint.z)
        blockers = []
        for o in L:
            if o is sc or not overlap(o['box'], tbox):
                continue
            for b in o['bodies']:
                try:
                    a = world_copy(tb, b)
                    tb.booleanOperation(a, tb.copy(tool), adsk.fusion.BooleanTypes.IntersectionBooleanType)
                    if a.volume > 1e-5:
                        blockers.append(o)
                        break
                except Exception:
                    pass
        st = stage_of(sc['group'], sc['occ'], sc['name'])
        if st == 0:   # Vormontage auf dem Tisch: nur die eigene Baugruppe zaehlt
            out.append((sc, [], [], head, True))
            continue
        def inst(o):   # Einbaustufe eines Hindernisses; vormontierte Teile (Stufe 0) kommen mit ihrer Baugruppe
            sb = stage_of(o['group'], o['occ'], o['name'])
            return stage_of(o['group'], None, o['name']) if sb == 0 else sb
        late = [o for o in blockers if inst(o) > st]
        hard = [o for o in blockers if inst(o) <= st]
        out.append((sc, hard, late, head, False))
    return out


def mesh_extent(bs):
    """Genaue Huelle ueber Dreiecksnetze (die BRep-Box ist bei gedrehten Teilen zu gross)."""
    lo = [1e9] * 3; hi = [-1e9] * 3
    for b in bs:
        try:
            mc = b.meshManager.createMeshCalculator()
            mc.setQuality(adsk.fusion.TriangleMeshQualityOptions.LowQualityTriangleMesh)
            m = mc.calculate()
            c = m.nodeCoordinatesAsDouble
            bb = b.boundingBox
            for k in range(0, len(c), 3):
                for a in range(3):
                    lo[a] = min(lo[a], c[k + a]); hi[a] = max(hi[a], c[k + a])
        except Exception:
            bb = b.boundingBox
            for a, (u, w) in enumerate(((bb.minPoint.x, bb.maxPoint.x), (bb.minPoint.y, bb.maxPoint.y), (bb.minPoint.z, bb.maxPoint.z))):
                lo[a] = min(lo[a], u); hi[a] = max(hi[a], w)
    return lo, hi


def run(context):
    lines = []
    t0 = time.time()
    try:
        des = adsk.fusion.Design.cast(app.activeProduct)
        root = des.rootComponent
        L = leaves(root)
        lines.append('Teile: %d (ohne Bibliothek)' % len(L))
        # 1) Startvolumen
        lo, hi = mesh_extent([b for x in L for b in x['bodies']])
        bb = body_box([b for x in L for b in x['bodies']])
        if not all(bb[a] - 0.01 <= lo[a] and hi[a] <= bb[a + 3] + 0.01 for a in range(3)):
            lo, hi = list(bb[:3]), list(bb[3:])   # Netz nicht in Weltkoordinaten -> BRep-Box
        dx, dy, dz = (hi[0] - lo[0]) / IN, (hi[1] - lo[1]) / IN, hi[2] / IN
        lines.append('Startvolumen: %.2f x %.2f x %.2f in (Grenze 15 x 15 x 15) -> %s' % (dx, dy, dz, 'OK' if max(dx, dy, dz) <= 15.0 else 'ZU GROSS'))
        # 2) Kollisionen in Startstellung
        res = collisions(des, L)
        lines.append('Kollisionen Startstellung: %d Paare' % len(res))
        for (a, b), (n, vol, pt) in sorted(res.items(), key=lambda kv: -kv[1][1]):
            lines.append('  %2d x %.4f in3 | %s <-> %s | bei (%.2f, %.2f, %.2f)' % (n, vol, a, b, pt[0], pt[1], pt[2]))
        # 3) Lift durchfahren
        js = {root.asBuiltJoints.item(i).name: root.asBuiltJoints.item(i) for i in range(root.asBuiltJoints.count)}
        if 'Arme unten' in js:
            jm = js['Arme unten'].jointMotion
            mx = jm.rotationLimits.maximumValue if jm.rotationLimits.isMaximumValueEnabled else math.radians(78)
            for k in range(1, 7):
                a = mx * k / 6
                jm.rotationValue = a
                adsk.doEvents()
                L2 = leaves(root)
                r2 = collisions(des, L2, subset=[i for i, x in enumerate(L2) if x['group'] in ('Arme unten', 'Arme oben', 'Schlitten', 'Greiferritzel', 'Backe L', 'Backe R')])
                r2 = {k2: v2 for k2, v2 in r2.items() if not (('Arme' in k2[0] and 'Schlitten' in k2[1]) or ('Arme' in k2[1] and 'Schlitten' in k2[0]))}
                lines.append('Lift %+.0f Grad ueber Start: %d Kollisionen' % (math.degrees(a), len(r2)))
                for (p, q), (n, vol, pt) in sorted(r2.items(), key=lambda kv: -kv[1][1])[:6]:
                    lines.append('    %.4f in3 | %s <-> %s | bei (%.2f, %.2f, %.2f)' % (vol, p, q, pt[0], pt[1], pt[2]))
            jm.rotationValue = 0
        # 4) Werkzeugzugang
        acc = tool_access(L, root)
        bad = [x for x in acc if x[1]]
        late = [x for x in acc if not x[1] and x[2]]
        pre = [x for x in acc if x[4]]
        lines.append('Schrauben: %d | frei: %d | Vormontage: %d | erst vor spaeterer Baugruppe montieren: %d | nicht erreichbar: %d' % (
            len(acc), len(acc) - len(bad) - len(late) - len(pre), len(pre), len(late), len(bad)))
        for sc, hard, lt, head, _ in bad:
            lines.append('  NICHT ERREICHBAR %s in %s bei (%.2f, %.2f, %.2f) blockiert von %s' % (
                sc['name'], sc['group'], head.x / IN, head.y / IN, head.z / IN, ', '.join(sorted(set(o['group'] + '/' + o['name'] for o in hard)))))
        for sc, hard, lt, head, _ in late:
            lines.append('  vor %s montieren: %s bei (%.2f, %.2f, %.2f)' % (
                ', '.join(sorted(set(o['group'] for o in lt))), sc['group'], head.x / IN, head.y / IN, head.z / IN))
        lines.append('Dauer %.0f s' % (time.time() - t0))
    except Exception:
        lines.append(traceback.format_exc())
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print('\n'.join(lines[:120]))
