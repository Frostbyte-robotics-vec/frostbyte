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
  function loadObjects(params, field) {
    var b = params.feldobjekte;
    if (!b) return { list: [], share: 0.5 };
    var rc = need(b.radius_in, 'cup', 'feldobjekte.radius_in', true), rp = need(b.radius_in, 'pin', 'feldobjekte.radius_in', true);
    var share = need(b, 'roboter_anteil', 'feldobjekte');
    if (!(share >= 0 && share <= 1)) throw new Error('sim_params feldobjekte.roboter_anteil muss in [0, 1] liegen');
    var list = [], start = field && field.objekte_start;
    if (start) [['cups', rc, 'cup'], ['pins', rp, 'pin']].forEach(function (kind) {
      (start[kind[0]] || []).forEach(function (e) {
        if (typeof e.x_in !== 'number' || typeof e.y_in !== 'number') return;
        if (typeof e.ebene === 'number' && e.ebene !== 0) return;
        list.push({ id: e.id, x_in: e.x_in, y_in: e.y_in, r_in: kind[1], kind: kind[2], standing: e.lage !== 'liegt' });
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
    var voltsAt = opts && opts.voltsAt;
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
        x_in: s.x_in, y_in: s.y_in, h_deg: s.h_deg };
    });
    var fieldIn = field && typeof field.feld_in === 'number' && field.feld_in > 0 ? field.feld_in : null;
    if (fieldIn === null) notes.push('Kollision: field.json fehlt oder ohne feld_in, kein Feldrand');
    var elements = loadElements(params, field, notes);
    var objectCfg = loadObjects(params, field);
    var objects = objectCfg.list.map(function (o) { return Object.assign({}, o); });
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

    function touchObjects(r, name) {
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

    function control() {
      ROBOTS.forEach(function (n) {
        var r = robots[n];
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
          r.contact = {}; r.blockedS = 0; r.blockedSent = false;
        });
        objects = objectCfg.list.map(function (o) { return Object.assign({}, o); });
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
      objects: function () { return objects.map(function (o) { return Object.assign({}, o); }); },
      speed: function (n) { var r = robots[n]; return (r.left.v + r.right.v) / 2; }
    };
  }

  return { create: create, keysToVolts: keysToVolts, wrap: wrap, LOG_COLUMNS: LOG_COLUMNS, penetrationBoxBox: penetrationBoxBox,
    penetrationBoxCircle: penetrationBoxCircle, pushInsideField: pushInsideField };
})();
if (typeof module !== 'undefined') module.exports = FBLive;
