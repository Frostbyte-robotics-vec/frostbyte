/* Live-Handsteuerung im Browser (SIM-14): vereinfachte Kopie des Fahrmodells aus sim/drive.cpp (ADR-025).
   Kein DOM, keine Zeit-Funktionen; läuft im Browser und unter Node. Alle Modellzahlen kommen aus sim/sim_params.json
   (roboter.<name>.antrieb, akku, welt, live), nichts davon steht hier. Einheiten: Zoll, Grad, Sekunden, Volt. */
var FBLive = (function () {
  'use strict';
  var ROBOTS = ['stacker', 'runner'];
  var START_ID = { stacker: 'START_STACKER', runner: 'START_RUNNER' };
  var MAX_FRAME_MS = 100;   // längere Bildpausen (Tab war im Hintergrund) werden nicht nachgeholt
  var EPS = 1e-9;
  // Spaltenreihenfolge nach docs/log_format.md v1 (test_livesim.py vergleicht mit make_test_logs.COLUMNS)
  var LOG_COLUMNS = ['t_s', 'robot', 'x_in', 'y_in', 'h_deg', 'gx_in', 'gy_in', 'gh_deg', 'g_err_in', 'tx_in', 'ty_in', 'th_deg',
    'lift_deg', 'arm_deg', 'claw_in', 'lift_set_deg', 'arm_set_deg', 'cycle', 'route_step', 'has_cup', 'has_pin', 'pin_yellow_up',
    'score', 'cpu_ms', 'batt_v', 'temp_max_c', 'event'];
  var LOG_BUILD = 'live';

  function wrap(deg) {
    var w = deg - 360 * Math.round(deg / 360);
    return w <= -180 ? w + 360 : w;
  }

  function need(obj, key, where, positive) {
    var v = obj && obj[key];
    if (typeof v !== 'number' || !isFinite(v) || v < 0 || (positive && v === 0)) throw new Error('sim_params ' + where + '.' + key + ' fehlt oder ist ungültig');
    return v;
  }

  /* Tastenzustand → Motorspannungen. Einzige Stelle, die Eingabe in Volt umsetzt (später gegen Gamepad tauschbar).
     keys: {w,a,s,d,shift}; Arcade: vor/zurück + drehen, A = gegen den Uhrzeigersinn (rechts schneller). */
  function keysToVolts(keys, max_V, slowFactor) {
    var fwd = (keys.w ? 1 : 0) - (keys.s ? 1 : 0);
    var turn = (keys.d ? 1 : 0) - (keys.a ? 1 : 0);
    var l = fwd + turn, r = fwd - turn;
    var m = Math.max(1, Math.abs(l), Math.abs(r));
    var scale = max_V * (keys.shift ? slowFactor : 1) / m;
    return { left_V: l * scale + 0, right_V: r * scale + 0 };   // + 0 macht −0 zu 0
  }


  /* ---- Kollisionsgeometrie: 1:1-Übertragung von sim/collision.cpp (SIM-05). Berührung ohne Überlappung ist keine Kollision. ---- */
  var TOUCH_EPS = 1e-12;
  var CONTACT_RELEASE_STEPS = 50;   // wie contact_release_steps in sim/world.cpp
  var BLOCKED_AFTER_S = 0.5;
  var DEG = Math.PI / 180;

  function frameOf(b) {
    var h = b.h_deg * DEG;
    return { u: { x: Math.cos(h), y: Math.sin(h) }, v: { x: -Math.sin(h), y: Math.cos(h) } };
  }
  function cornersOf(b) {
    var f = frameOf(b), hl = b.length_in / 2, hw = b.width_in / 2, c = [];
    [-1, 1].forEach(function (su) {
      [-1, 1].forEach(function (sv) {
        c.push({ x: b.x_in + su * hl * f.u.x + sv * hw * f.v.x, y: b.y_in + su * hl * f.u.y + sv * hw * f.v.y });
      });
    });
    return c;
  }
  function dot(a, b) { return a.x * b.x + a.y * b.y; }
  function project(c, axis) {
    var lo = dot(c[0], axis), hi = lo;
    c.forEach(function (p) { var d = dot(p, axis); lo = Math.min(lo, d); hi = Math.max(hi, d); });
    return { lo: lo, hi: hi };
  }
  // Kürzeste Verschiebung von a, nach der a und b sich nicht mehr überlappen; null, wenn kein Kontakt.
  function penetrationBoxBox(a, b) {
    var ca = cornersOf(a), cb = cornersOf(b), fa = frameOf(a), fb = frameOf(b);
    var axes = [fa.u, fa.v, fb.u, fb.v];
    var best = 0, bestAxis = null;
    for (var i = 0; i < axes.length; i++) {
      var pa = project(ca, axes[i]), pb = project(cb, axes[i]);
      var overlap = Math.min(pa.hi, pb.hi) - Math.max(pa.lo, pb.lo);
      if (overlap <= TOUCH_EPS) return null;
      if (bestAxis === null || overlap < best) { best = overlap; bestAxis = axes[i]; }
    }
    var side = dot({ x: b.x_in - a.x_in, y: b.y_in - a.y_in }, bestAxis);
    var sgn = side >= 0 ? -1 : 1;
    return { x: sgn * best * bestAxis.x, y: sgn * best * bestAxis.y };
  }
  function penetrationBoxCircle(a, c) {
    var f = frameOf(a), hl = a.length_in / 2, hw = a.width_in / 2;
    var rel = { x: c.x_in - a.x_in, y: c.y_in - a.y_in };
    var lu = dot(rel, f.u), lv = dot(rel, f.v);
    var du = lu - Math.min(Math.max(lu, -hl), hl), dv = lv - Math.min(Math.max(lv, -hw), hw);
    var dist = Math.sqrt(du * du + dv * dv);
    if (dist > 0) {
      if (dist >= c.r_in) return null;
      var k = -(c.r_in - dist) / dist;
      return { x: k * (du * f.u.x + dv * f.v.x), y: k * (du * f.u.y + dv * f.v.y) };
    }
    var toU = hl - Math.abs(lu), toV = hw - Math.abs(lv), push;
    if (toU <= toV) {
      push = -(lu >= 0 ? 1 : -1) * (toU + c.r_in);
      return { x: push * f.u.x, y: push * f.u.y };
    }
    push = -(lv >= 0 ? 1 : -1) * (toV + c.r_in);
    return { x: push * f.v.x, y: push * f.v.y };
  }
  // Verschiebung, die die Box (mit allen Ecken) wieder ins Feld [-field/2, field/2] bringt.
  function pushInsideField(a, field_in) {
    var half = field_in / 2, minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    cornersOf(a).forEach(function (p) {
      minX = Math.min(minX, p.x); maxX = Math.max(maxX, p.x); minY = Math.min(minY, p.y); maxY = Math.max(maxY, p.y);
    });
    var d = { x: 0, y: 0 };
    if (maxX > half) d.x = half - maxX; else if (minX < -half) d.x = -half - minX;
    if (maxY > half) d.y = half - maxY; else if (minY < -half) d.y = -half - minY;
    return d;
  }
  // Wie track_contact in sim/world.cpp: true, wenn der Kontakt in diesem Schritt neu beginnt.
  function trackContact(touching, c) {
    if (touching) {
      c.free = 0;
      if (!c.on) { c.on = true; return true; }
    } else if (c.on && ++c.free >= CONTACT_RELEASE_STEPS) {
      c.on = false; c.free = 0;
    }
    return false;
  }

  function sign(v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

  // Eine Seite, wie DriveModel::step_side: V = kS·sgn(v) + kV·v + kA·a, Haftreibung, Schlupfgrenze mu·g.
  function stepSide(side, volts_V, dt_s, p) {
    var v = side.v;
    if (v === 0 && Math.abs(volts_V) <= p.kS_V) return;
    var dir = v !== 0 ? sign(v) : sign(volts_V);
    var a = (volts_V - p.kS_V * dir - p.kV_V_per_in_s * v) / p.kA_V_per_in_s2;
    var aMax = p.mu * p.g_in_s2;
    var next = v + Math.min(Math.max(a, -aMax), aMax) * dt_s;
    if (v !== 0 && next * v < 0) next = 0;
    side.v = next;
  }

  function stepRobot(r, dt_s) {
    var p = r.p;
    var uMax = Math.min(p.motor_max_V, p.akku_V);
    stepSide(r.left, Math.min(Math.max(r.volts.left_V, -uMax), uMax), dt_s, p);
    stepSide(r.right, Math.min(Math.max(r.volts.right_V, -uMax), uMax), dt_s, p);
    var v = (r.left.v + r.right.v) / 2;
    var dh = (r.right.v - r.left.v) / p.spurbreite_in * 180 / Math.PI * dt_s;
    var hMid = (r.h_deg + dh / 2) * Math.PI / 180;
    r.x_in += v * Math.cos(hMid) * dt_s;
    r.y_in += v * Math.sin(hMid) * dt_s;
    r.h_deg = wrap(r.h_deg + dh);
  }

  // Wie DriveModel::set_position (SIM-26): Kollision verschiebt die Pose; Tempo gegen die Verschiebung wird null.
  // Den Radweg führt der Livesim nicht.
  function nudge(r, dx, dy) {
    var h = r.h_deg * Math.PI / 180;
    var along = dx * Math.cos(h) + dy * Math.sin(h);
    [r.left, r.right].forEach(function (s) { if (s.v * along < 0) s.v = 0; });
    r.x_in += dx; r.y_in += dy;
  }

  function loadDrive(params, name) {
    var r = params.roboter && params.roboter[name];
    var where = 'roboter.' + name;
    var a = r && r.antrieb;
    return {
      kS_V: need(a, 'kS_V', where + '.antrieb'), kV_V_per_in_s: need(a, 'kV_V_per_in_s', where + '.antrieb', true),
      kA_V_per_in_s2: need(a, 'kA_V_per_in_s2', where + '.antrieb', true), spurbreite_in: need(a, 'spurbreite_in', where + '.antrieb', true),
      mu: need(a, 'mu', where + '.antrieb', true), motor_max_V: need(a, 'motor_max_V', where + '.antrieb', true),
      akku_V: need(r && r.akku, 'spannung_V', where + '.akku', true), g_in_s2: need(params.welt, 'g_in_s2', 'welt', true)
    };
  }


  function loadOutline(params, name) {
    var r = params.roboter && params.roboter[name];
    var u = r && r.umriss_in;
    return { length_in: need(u, 'laenge', 'roboter.' + name + '.umriss_in', true), width_in: need(u, 'breite', 'roboter.' + name + '.umriss_in', true) };
  }

  /* Feldelemente als Kreise: Radius je Abschnitt aus params.live.kollision.radius_in (optional; ohne Radius kein Element,
     die C++-Simulation kennt keine Feldelement-Kollision). Punkte mit null-Koordinaten werden übersprungen. */
  function loadElements(params, field, notes) {
    var out = [];
    var radii = params.live && params.live.kollision && params.live.kollision.radius_in;
    if (!field || !radii) return out;
    Object.keys(radii).forEach(function (section) {
      var r = radii[section];
      if (typeof r !== 'number' || !isFinite(r) || r <= 0) throw new Error('sim_params live.kollision.radius_in.' + section + ' ist ungültig');
      var list = field[section];
      if (!Array.isArray(list)) { notes.push('Kollision: field.json hat keinen Abschnitt "' + section + '"'); return; }
      list.forEach(function (e) {
        if (typeof e.x_in === 'number' && typeof e.y_in === 'number' && typeof e.id === 'string') out.push({ id: e.id, x_in: e.x_in, y_in: e.y_in, r_in: r });
      });
    });
    return out;
  }

  /* Feldobjekte (SIM-30): wie sim/field_objects.cpp. Nur Objekte am Boden (ebene 0) mit Koordinaten; Pins mit lage "liegt"
     liegen von Anfang an. Ohne params.feldobjekte keine Objekte. */
  function loadObjects(params, field, withLevels) {
    var b = params.feldobjekte;
    if (!b) return { list: [], share: 0.5 };
    var rc = need(b.radius_in, 'cup', 'feldobjekte.radius_in', true), rp = need(b.radius_in, 'pin', 'feldobjekte.radius_in', true);
    var share = need(b, 'roboter_anteil', 'feldobjekte');
    if (!(share >= 0 && share <= 1)) throw new Error('sim_params feldobjekte.roboter_anteil muss in [0, 1] liegen');
    var list = [], start = field && field.objekte_start;
    if (start) [['cups', rc, 'cup'], ['pins', rp, 'pin']].forEach(function (kind) {
      (start[kind[0]] || []).forEach(function (e) {
        if (typeof e.x_in !== 'number' || typeof e.y_in !== 'number') return;
        if (typeof e.ebene === 'number' && e.ebene !== 0 && !withLevels) return;
        list.push({ level: typeof e.ebene === 'number' ? e.ebene : 0, id: e.id, x_in: e.x_in, y_in: e.y_in, r_in: kind[1], kind: kind[2], standing: e.lage !== 'liegt',
          variant: e.variante, yellow_deg: e.gelb_deg, top_color: e.oben_farbe });  // nur für die Anzeige (SIM-37)
      });
    });
    return { list: list, share: share };
  }

  function startPose(params, field, name, notes) {
    var list = (field && field.posen) || [];
    for (var i = 0; i < list.length; i++) {
      var q = list[i];
      if (q.id === START_ID[name] && typeof q.x_in === 'number' && typeof q.y_in === 'number' && typeof q.h_deg === 'number')
        return { x_in: q.x_in, y_in: q.y_in, h_deg: wrap(q.h_deg) };
    }
    var e = params.live.ersatzpose_in[name];
    ['x_in', 'y_in', 'h_deg'].forEach(function (k) {
      if (!e || typeof e[k] !== 'number') throw new Error('sim_params live.ersatzpose_in.' + name + '.' + k + ' fehlt');
    });
    notes.push(START_ID[name] + ' hat keine Koordinaten (null in field.json): Ersatzpose x=' + e.x_in + ', y=' + e.y_in + ', h=' + e.h_deg + '°');
    return { x_in: e.x_in, y_in: e.y_in, h_deg: wrap(e.h_deg) };
  }

  /* opts.voltsAt(name, t_ms) → {left_V, right_V}: ersetzt die Tasten durch einen Eingabeverlauf für beide Roboter
     (SIM-17: gleicher Verlauf wie frostbyte_sim --input; gilt am Steuertakt, t_ms = Zeit seit Start/Reset). */
  function create(params, field, opts) {
    var voltsAt = opts && opts.voltsAt, cmdAt = opts && opts.cmdAt, cmdQueue = { stacker: [], runner: [] };
    var stepMs = need(params.welt, 'schritt_ms', 'welt', true);
    var ctlMs = need(params.welt, 'steuer_ms', 'welt', true);
    var slow = need(params.live, 'langsam_faktor', 'live', true);
    var ctlEvery = Math.max(1, Math.round(ctlMs / stepMs));
    var logEvery = Math.max(1, Math.round(need(params.welt, 'log_ms', 'welt', true) / stepMs));
    var notes = [];
    var robots = {};
    ROBOTS.forEach(function (n) {
      var s = startPose(params, field, n, notes);
      robots[n] = { p: loadDrive(params, n), outline: loadOutline(params, n), start: s, contact: {}, blockedS: 0, blockedSent: false, left: { v: 0 }, right: { v: 0 }, volts: { left_V: 0, right_V: 0 },
        hand: [], lift: { level: 0, z: 0 }, x_in: s.x_in, y_in: s.y_in, h_deg: s.h_deg };
    });
    // SIM-42: Handhöhe aus dem Liftwinkel (basis + hub · sin θ). Stufen teilen den Hubbereich gleich auf; z zählt ab der untersten Stellung.
    function liftGeo(name) {
      var g = physCfg && physCfg.grab, e = g && g.hand_geo[name], m = params.roboter && params.roboter[name] && params.roboter[name].mechanismen;
      if (!e || !m || !m.lift) return null;
      var lo = m.lift.min_deg, hi = m.lift.max_deg, sin = function (d) { return Math.sin(d * DEG); };
      if (typeof lo !== 'number' || typeof hi !== 'number' || !(hi > lo)) throw new Error('sim_params roboter.' + name + '.mechanismen.lift: min_deg/max_deg fehlen');
      var zmin = e.base_in + e.hub_in * sin(lo), zmax = e.base_in + e.hub_in * sin(hi);
      if (zmin < 0) throw new Error('sim_params physik.greifen.hand_hoehe.' + name + ': Hand läge bei min_deg unter dem Boden (' + zmin.toFixed(1) + ' in)');
      return { base_in: e.base_in, hub_in: e.hub_in, min_deg: lo, zmin: zmin, range_in: zmax - zmin };
    }
    function handBase(r) { return r.geo ? r.geo.zmin : holdCfg().hand_z_in; }
    // eine Stufe = lift_stufe_in (wie eine Stapelebene); mit hand_hoehe so viele Stufen, bis der Hubbereich erreicht ist, die oberste endet bei max_deg
    function liftStep(r) { return holdCfg().lift_step_in; }
    function liftLevels(r) { return r.geo ? Math.ceil(r.geo.range_in / liftStep(r) - 1e-9) : holdCfg().lift_levels; }
    var fieldIn = field && typeof field.feld_in === 'number' && field.feld_in > 0 ? field.feld_in : null;
    if (fieldIn === null) notes.push('Kollision: field.json fehlt oder ohne feld_in, kein Feldrand');
    var elements = loadElements(params, field, notes);
    // Ebene-1-Objekte (Pin obenauf) gibt es nur mit Physik und Block greifen: dann stehen sie auf dem Teil darunter (SIM-39).
    var stackable = typeof FBPhysics !== 'undefined' && !!(params.physik && params.physik.greifen);
    var objectCfg = loadObjects(params, field, stackable);
    var objects = objectCfg.list.map(function (o) { return Object.assign({}, o); });
    // Physik der Objekte (SIM-38, ADR-051): nur mit Block physik in den Parametern; ohne ihn bleibt alles wie in SIM-30.
    var physCfg = typeof FBPhysics !== 'undefined' ? FBPhysics.load(params) : null;
    ROBOTS.forEach(function (n) { robots[n].geo = liftGeo(n); });
    if (params.physik && typeof FBPhysics === 'undefined') notes.push('Physik: physics.js fehlt, Objekte kippen ohne Physik (Skript nicht eingebunden?)');
    var phys = null, lastTouch = {};
    function makeWorld() {
      lastTouch = {};
      if (!physCfg) return null;
      var step = physCfg.grab ? physCfg.grab.stack_step_in : 0;
      var world = FBPhysics.create(physCfg, objectCfg.list.map(function (o) {
        return { id: o.id, kind: o.kind, x_in: o.x_in, y_in: o.y_in, r_in: o.r_in, standing: o.standing, lie_dir_deg: o.yellow_deg, z_in: o.level * step };
      }), { field_in: fieldIn === null ? 0 : fieldIn });
      world.setGoals(((field && field.goals) || []).filter(function (g) {
        return typeof g.x_in === 'number' && typeof g.y_in === 'number' && typeof g.hoehe_in === 'number';
      }).map(function (g) { return { x_in: g.x_in, y_in: g.y_in, z_in: g.hoehe_in }; }));
      world.settle();
      return world;
    }
    phys = makeWorld();
    var events = [];
    var keys = { w: false, a: false, s: false, d: false, shift: false };
    var active = 'stacker';
    var acc_ms = 0, stepNo = 0, paused = false;


    function boxOf(r) { return { x_in: r.x_in, y_in: r.y_in, length_in: r.outline.length_in, width_in: r.outline.width_in, h_deg: r.h_deg }; }
    function contactOf(r, key) { return r.contact[key] || (r.contact[key] = { on: false, free: 0 }); }
    function emit(name, text) { events.push({ t_s: stepNo * stepMs / 1000, robot: name, text: text }); logEvent(name, text); }

    // Aufnahme im Log-Format v1: eine Zeile je Roboter alle welt.log_ms; Ereignisse zwischen zwei Zeilen hängen an der nächsten.
    var rows = { stacker: [], runner: [] }, pending = { stacker: [], runner: [] };
    function logEvent(name, text) { pending[name].push(String(text).replace(/[,;\r\n]/g, ' ')); }
    function seedNotes() {
      notes.forEach(function (t) { logEvent(/START_RUNNER/.test(t) ? 'runner' : 'stacker', 'NOTE:' + t); });
    }
    function record() {
      ROBOTS.forEach(function (n) {
        var r = robots[n];
        rows[n].push({ t_s: stepNo * stepMs / 1000, x_in: r.x_in, y_in: r.y_in, h_deg: r.h_deg, event: pending[n].splice(0).join(';') });
      });
    }
    function csvOf(n) {
      var out = ['# frostbyte-log v1 robot=' + n + ' source=sim seed=- build=' + LOG_BUILD, LOG_COLUMNS.join(',')];
      var list = rows[n].slice();
      if (pending[n].length) {   // noch nicht abgeholte Ereignisse nie verwerfen: letzte Zeile mit aktuellem Stand
        var r = robots[n], now = stepNo * stepMs / 1000, last = list[list.length - 1];
        if (last && last.t_s === now) list[list.length - 1] = Object.assign({}, last, { event: [last.event].concat(pending[n]).filter(Boolean).join(';') });
        else list.push({ t_s: now, x_in: r.x_in, y_in: r.y_in, h_deg: r.h_deg, event: pending[n].join(';') });
      }
      list.forEach(function (w) {
        var c = { t_s: w.t_s.toFixed(3), robot: n, x_in: w.x_in.toFixed(3), y_in: w.y_in.toFixed(3), h_deg: w.h_deg.toFixed(3),
          tx_in: w.x_in.toFixed(3), ty_in: w.y_in.toFixed(3), th_deg: w.h_deg.toFixed(3), event: w.event };
        out.push(LOG_COLUMNS.map(function (k) { return c[k] === undefined ? '' : c[k]; }).join(','));
      });
      return out.join('\n') + '\n';
    }
    function restartLog() { rows.stacker.length = 0; rows.runner.length = 0; pending.stacker.length = 0; pending.runner.length = 0; if (stepNo % logEvery === 0) record(); }

    // Mit Physik: ein stehendes Objekt neigt sich, so weit der Roboter es erzwingt (kein Rückweichen des Roboters); ein
    // liegendes wird wie in SIM-30 geschoben (aber als Geschwindigkeit, nicht als Sprung) und gleitet danach aus.
    function touchBodies(r, name) {
      var half = fieldIn === null ? Infinity : fieldIn / 2, share = objectCfg.share, dt = stepMs / 1000;
      phys.bodies.forEach(function (b, i) {
        if (b.z > 0 || b.state === 'falling') return;
        var m = penetrationBoxCircle(boxOf(r), { x_in: b.x, y_in: b.y, r_in: b.r });
        if (!m) return;
        lastTouch[b.id] = name;
        var len = Math.sqrt(m.x * m.x + m.y * m.y);
        if (!(len > 0)) return;   // Überlappungsvektor 0: keine Richtung zum Wegschieben (sonst NaN)
        if (b.state !== 'lying') {
          phys.pushStanding(i, -m.x / len, -m.y / len, len, dt);
          return;
        }
        // Weicht über physik.ausweich_s aus (Geschwindigkeit statt Sprung): das Objekt mit dem Anteil (1 − roboter_anteil) der
        // Überlappung, mindestens aber mit der Fahrgeschwindigkeit des Roboters; der Roboter weicht um seinen Anteil zurück.
        var px = -m.x / len, py = -m.y / len, tau = phys.cfg.evade_s;
        var hr = r.h_deg * DEG, vn = Math.max(0, (r.left.v + r.right.v) / 2 * (Math.cos(hr) * px + Math.sin(hr) * py));
        phys.pushLying(i, px, py, vn + (1 - share) * len / tau);
        var wall = (px > 0 && b.x >= half - b.r - 1e-9) || (px < 0 && b.x <= -half + b.r + 1e-9) || (py > 0 && b.y >= half - b.r - 1e-9) || (py < 0 && b.y <= -half + b.r + 1e-9);
        var back = wall ? len : Math.min(len, share * len * dt / tau);   // an der Wand trägt der Roboter alles
        r.x_in += m.x / len * back; r.y_in += m.y / len * back;
      });
    }

    function touchObjects(r, name) {
      if (phys) { touchBodies(r, name); return; }
      var half = fieldIn === null ? Infinity : fieldIn / 2, share = objectCfg.share;
      objects.forEach(function (o) {
        var m = penetrationBoxCircle(boxOf(r), o);
        if (!m) return;
        if (o.standing) { o.standing = false; emit(name, 'TOPPLED:' + o.id); return; }
        var wantX = o.x_in - m.x * (1 - share), wantY = o.y_in - m.y * (1 - share);
        var nx = Math.min(Math.max(wantX, -half + o.r_in), half - o.r_in), ny = Math.min(Math.max(wantY, -half + o.r_in), half - o.r_in);
        r.x_in += m.x * share - (wantX - nx); r.y_in += m.y * share - (wantY - ny);   // wie DriveModel::displace: Tempo bleibt
        o.x_in = nx; o.y_in = ny;
      });
    }

    // Reihenfolge wie sim/world.cpp: Roboter untereinander (je zur Hälfte), dann Feldrand; Feldelemente zuletzt.
    function collide() {
      var a = robots.stacker, b = robots.runner;
      var mtv = penetrationBoxBox(boxOf(a), boxOf(b));
      if (mtv) {
        nudge(a, mtv.x / 2, mtv.y / 2);
        nudge(b, -mtv.x / 2, -mtv.y / 2);
      }
      var began0 = trackContact(!!mtv, contactOf(a, 'robot')), began1 = trackContact(!!mtv, contactOf(b, 'robot'));
      if (began0) emit('stacker', 'COLLISION:runner');
      if (began1) emit('runner', 'COLLISION:stacker');
      // Feldobjekte (SIM-30): erst umstoßen, dann schieben; Roboter weicht um seinen Anteil (wie sim/field_objects.cpp).
      ROBOTS.forEach(function (n) { touchObjects(robots[n], n); });
      ROBOTS.forEach(function (n) {
        var r = robots[n];
        if (fieldIn !== null) {
          var push = pushInsideField(boxOf(r), fieldIn);
          var wall = push.x !== 0 || push.y !== 0;
          if (wall) nudge(r, push.x, push.y);
          if (trackContact(wall, contactOf(r, 'wall'))) emit(n, 'COLLISION:wall');
        }
        elements.forEach(function (e) {
          var m = penetrationBoxCircle(boxOf(r), e);
          if (m) nudge(r, m.x, m.y);
          if (trackContact(!!m, contactOf(r, e.id))) emit(n, 'COLLISION:' + e.id);
        });
        // BLOCKED: Kontakt hält > 0,5 s bei Fahreingabe (einmal je Kontaktfolge).
        var touching = Object.keys(r.contact).some(function (k) { return r.contact[k].on; });
        var driving = r.volts.left_V !== 0 || r.volts.right_V !== 0;
        if (touching && driving) {
          r.blockedS += stepMs / 1000;
          if (r.blockedS > BLOCKED_AFTER_S && !r.blockedSent) { r.blockedSent = true; emit(n, 'BLOCKED'); }
        } else { r.blockedS = 0; if (!touching) r.blockedSent = false; }
      });
    }

    /* Greifen, Heben, Ablegen (SIM-39): nur mit Physik und Block physik.greifen. Befehle gelten am Steuertakt. Der Roboter hält
       höchstens einen Pin und einen Cup (SG6, RuleGuard: sonst RULE:SG6). Gegriffen wird ein freies, stehendes Teil vor dem Roboter. */
    function holdCfg() { return phys && phys.cfg.grab; }
    // Hand: ein Teil vor dem Roboter, mit dem Fußpunkt greif_hoehe_in unter der Hand; Pin und Cup stecken ineinander (Pin auf dem Cup, eine Stapelstufe höher)
    function handPoint(r, slot, count) {
      var g = holdCfg(), h = r.h_deg * DEG, fwd = r.outline.length_in / 2 + g.front_in;
      var up = (count >= 2 || r.pinRaised) && phys.bodies[r.hand[slot]].kind === 'pin' ? g.stack_step_in : 0;
      return { x: r.x_in + fwd * Math.cos(h), y: r.y_in + fwd * Math.sin(h), z: handBase(r) + r.lift.z - g.grip_in + up };
    }
    function doGrab(name) {
      var r = robots[name], g = holdCfg(), h = r.h_deg * DEG, best = -1, bestD = Infinity;
      if (!g) return;
      var blocked = -1, hz = handBase(r) + r.lift.z;
      phys.bodies.forEach(function (b, i) {
        // Hand auf Höhe des Teils (Fußpunkt bis Oberkante, mit Spielraum): mit hohem Lift wird nichts vom Boden gegriffen
        if ((b.state !== 'standing' && !(b.state === 'lying' && g.rise_rad_s !== null)) || hz < b.z - g.grab_tol_in || hz > b.z + b.h + g.grab_tol_in || !phys.isFree(i)) return;
        var dx = b.x - r.x_in, dy = b.y - r.y_in, lu = dx * Math.cos(h) + dy * Math.sin(h), lv = -dx * Math.sin(h) + dy * Math.cos(h);
        var front = r.outline.length_in / 2;
        if (lu < front - b.r || lu > front + g.reach_in + b.r || Math.abs(lv) > r.outline.width_in / 2 + b.r) return;
        var d = Math.sqrt((lu - front) * (lu - front) + lv * lv);
        // SG6 (RuleGuard) wirkt vor der Auswahl: ein abgelehntes Teil sperrt kein anderes
        if (r.hand.some(function (k) { return phys.bodies[k].kind === b.kind; })) { blocked = i; return; }
        if (d < bestD) { best = i; bestD = d; }
      });
      if (best < 0) { if (blocked >= 0) emit(name, 'RULE:SG6'); return; }
      if (phys.hold(best)) { r.hand.push(best); emit(name, 'GRAB:' + phys.bodies[best].id); }
    }
    function doRelease(name) {
      var r = robots[name];
      if (!r.hand.length) return;
      // der Cup liegt unten und wird zuerst abgelegt; danach hebt der Lift, und der Pin kommt obenauf
      var k = r.hand.findIndex(function (b) { return phys.bodies[b].kind === 'cup'; }), i = r.hand.splice(k < 0 ? r.hand.length - 1 : k, 1)[0];
      r.pinRaised = r.hand.length === 1 && phys.bodies[i].kind === 'cup';   // der Pin bleibt oben, bis er selbst abgelegt wird (sinkt nicht in den Cup)
      phys.release(i);
      emit(name, 'DROP:' + phys.bodies[i].id);
    }
    function doLift(name, dir) {
      var r = robots[name], g = holdCfg();
      if (g) r.lift.level = Math.min(Math.max(r.lift.level + (dir < 0 ? -1 : 1), 0), liftLevels(r));
    }
    function applyCommand(name, c) {
      if (!phys || !holdCfg() || !c) return;
      if (c.type === 'grab') doGrab(name); else if (c.type === 'release') doRelease(name); else if (c.type === 'lift') doLift(name, c.dir);
    }
    // Lift fährt mit lift_geschw_in_s zur Stufe, gehaltene Teile werden mit der Zieh-Geschwindigkeit plus Fahrgeschwindigkeit zur Hand gezogen.
    function updateHands(dt) {
      var g = holdCfg();
      if (!g) return;
      ROBOTS.forEach(function (n) {
        var r = robots[n], goal = Math.min(r.lift.level * liftStep(r), r.geo ? r.geo.range_in : Infinity), d = goal - r.lift.z, step = g.lift_speed_in_s * dt;
        r.lift.z = Math.abs(d) <= step ? goal : r.lift.z + (d > 0 ? step : -step);
        var follow = g.pull_in_s + Math.abs((r.left.v + r.right.v) / 2);
        r.hand.forEach(function (i, slot) {
          var p = handPoint(r, slot, r.hand.length);
          phys.moveHeld(i, p.x, p.y, p.z, dt, follow);
        });
      });
    }

    function control() {
      ROBOTS.forEach(function (n) {
        var r = robots[n];
        if (cmdAt) applyCommand(n, cmdAt(n, stepNo * stepMs)); else cmdQueue[n].splice(0).forEach(function (c) { applyCommand(n, c); });
        if (voltsAt) { var u = voltsAt(n, stepNo * stepMs); r.volts = { left_V: u.left_V + 0, right_V: u.right_V + 0 }; return; }
        r.volts = n === active ? keysToVolts(keys, Math.min(r.p.motor_max_V, r.p.akku_V), slow) : { left_V: 0, right_V: 0 };
      });
    }

    restartLog(); seedNotes();

    return {
      notes: notes,
      setKeys: function (k) { keys = { w: !!k.w, a: !!k.a, s: !!k.s, d: !!k.d, shift: !!k.shift }; },
      active: function () { return active; },
      select: function (n) { if (robots[n]) { active = n; control(); } },
      toggle: function () { active = active === 'stacker' ? 'runner' : 'stacker'; control(); },
      pause: function (on) { paused = !!on; if (paused) acc_ms = 0; },
      reset: function () {
        ROBOTS.forEach(function (n) {
          var r = robots[n];
          r.left.v = 0; r.right.v = 0; r.volts = { left_V: 0, right_V: 0 };
          r.x_in = r.start.x_in; r.y_in = r.start.y_in; r.h_deg = r.start.h_deg;
          r.contact = {}; r.blockedS = 0; r.blockedSent = false; r.hand = []; r.pinRaised = false; r.lift = { level: 0, z: 0 };
          cmdQueue[n].length = 0;
        });
        objects = objectCfg.list.map(function (o) { return Object.assign({}, o); });
        phys = makeWorld();
        acc_ms = 0; stepNo = 0; events.length = 0;
        restartLog(); seedNotes(); // R beginnt eine neue Aufnahme
      },
      // dt_s = vergangene Bildzeit; rechnet in festen Schritten (welt.schritt_ms), Steuerung alle welt.steuer_ms. Liefert die Schrittzahl.
      advance: function (dt_s) {
        if (paused) return 0;
        acc_ms += Math.min(Math.max(dt_s * 1000, 0), MAX_FRAME_MS);
        var n = 0;
        while (acc_ms + EPS >= stepMs) {
          if (stepNo % ctlEvery === 0) control();
          ROBOTS.forEach(function (name) { stepRobot(robots[name], stepMs / 1000); });
          collide();
          if (phys) {
            updateHands(stepMs / 1000);
            phys.step(stepMs / 1000).forEach(function (e) {
              if (e.type === 'toppled') emit(lastTouch[e.id] || ROBOTS[0], 'TOPPLED:' + e.id);
              else if (e.type === 'landed' && e.stacked) emit(lastTouch[e.id] || ROBOTS[0], 'STACK:' + e.id);
            });
          }
          acc_ms -= stepMs; stepNo++; n++;
          if (stepNo % logEvery === 0) record();
        }
        if (acc_ms < 0) acc_ms = 0;
        return n;
      },
      pose: function (n) { var r = robots[n]; return { x_in: r.x_in, y_in: r.y_in, h_deg: r.h_deg }; },
      // Ereignisse seit dem letzten Aufruf: [{t_s, robot, text}] (COLLISION:<mit>, BLOCKED).
      // Eigene Ereignisse ins Log (z. B. später Ausgaben); Komma und Semikolon werden zu Leerzeichen.
      logEvent: function (name, text) { if (rows[name]) emit(name, text); },
      clearLog: function () { restartLog(); },
      // Aufnahme als CSV je Roboter (Log-Format v1): {stacker: text, runner: text}
      exportCsv: function () { return { stacker: csvOf('stacker'), runner: csvOf('runner') }; },
      takeEvents: function () { return events.splice(0, events.length); },
      // Umriss als geschlossenes Vieleck (4 Ecken, Zoll) für die Anzeige.
      outline: function (n) { var c = cornersOf(boxOf(robots[n])); return [c[0], c[1], c[3], c[2]]; },
      elements: function () { return elements.slice(); },
      // Feldobjekte mit aktuellem Zustand (kind = 'cup' | 'pin', standing = steht, sonst liegt), Kopie.
      // Mit Physik (SIM-38) zusätzlich state ('standing' | 'tipping' | 'lying' | 'falling') und draw {bx_in, by_in, z_in, tilt_deg,
      // dir_deg}: Fußpunkt, Neigung und Richtung zum Zeichnen; x_in/y_in bleiben der Kreis der Kollision.
      objects: function () {
        if (!phys) return objects.map(function (o) { return Object.assign({}, o); });
        return phys.bodies.map(function (b, i) {
          return Object.assign({}, objectCfg.list[i], { x_in: b.x, y_in: b.y, standing: b.state === 'standing', state: b.state, draw: phys.pose(i) });
        });
      },
      // Befehl für den nächsten Steuertakt (SIM-39): {type: 'grab'} | {type: 'release'} | {type: 'lift', dir: 1 | -1}. Ohne Physik
      // und Block physik.greifen wirkungslos; liefert, ob es das Greifen gibt.
      command: function (name, c) { if (!cmdQueue[name] || !holdCfg()) return false; cmdQueue[name].push(c); return true; },
      // Ids der gehaltenen Teile und Liftstufe (SIM-39)
      hand: function (name) { return phys ? robots[name].hand.map(function (i) { return phys.bodies[i].id; }) : []; },
      liftLevel: function (name) { return robots[name].lift.level; },
      // Handhöhe über dem Boden und Liftwinkel (SIM-42); liftDeg ist null ohne physik.greifen.hand_hoehe
      handZ: function (name) { var r = robots[name]; return holdCfg() ? handBase(r) + r.lift.z : null; },
      liftDeg: function (name) {
        var r = robots[name], e = r.geo;
        if (!e) return null;
        return Math.asin(Math.min(1, Math.max(-1, (e.zmin + r.lift.z - e.base_in) / e.hub_in))) / DEG;
      },
      // Richtung des oberen Endes eines Objekts, das von Anfang an liegt (nur Anzeige, ändert die Physik nicht).
      setLieDir: function (id, deg) { if (phys) phys.bodies.forEach(function (b, i) { if (b.id === id) phys.setLieDir(i, deg); }); },
      speed: function (n) { var r = robots[n]; return (r.left.v + r.right.v) / 2; }
    };
  }

  return { create: create, keysToVolts: keysToVolts, wrap: wrap, LOG_COLUMNS: LOG_COLUMNS, penetrationBoxBox: penetrationBoxBox,
    penetrationBoxCircle: penetrationBoxCircle, pushInsideField: pushInsideField };
})();
if (typeof module !== 'undefined') module.exports = FBLive;
