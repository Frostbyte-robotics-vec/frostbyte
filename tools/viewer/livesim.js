/* Live-Handsteuerung im Browser (SIM-14): vereinfachte Kopie des Fahrmodells aus sim/drive.cpp (ADR-025).
   Kein DOM, keine Zeit-Funktionen; läuft im Browser und unter Node. Alle Modellzahlen kommen aus sim/sim_params.json
   (roboter.<name>.antrieb, akku, welt, live), nichts davon steht hier. Einheiten: Zoll, Grad, Sekunden, Volt. */
var FBLive = (function () {
  'use strict';
  var ROBOTS = ['stacker', 'runner'];
  var START_ID = { stacker: 'START_STACKER', runner: 'START_RUNNER' };
  var MAX_FRAME_MS = 100;   // längere Bildpausen (Tab war im Hintergrund) werden nicht nachgeholt
  var EPS = 1e-9;

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

  function create(params, field) {
    var stepMs = need(params.welt, 'schritt_ms', 'welt', true);
    var ctlMs = need(params.welt, 'steuer_ms', 'welt', true);
    var slow = need(params.live, 'langsam_faktor', 'live', true);
    var ctlEvery = Math.max(1, Math.round(ctlMs / stepMs));
    var notes = [];
    var robots = {};
    ROBOTS.forEach(function (n) {
      var s = startPose(params, field, n, notes);
      robots[n] = { p: loadDrive(params, n), start: s, left: { v: 0 }, right: { v: 0 }, volts: { left_V: 0, right_V: 0 },
        x_in: s.x_in, y_in: s.y_in, h_deg: s.h_deg };
    });
    var keys = { w: false, a: false, s: false, d: false, shift: false };
    var active = 'stacker';
    var acc_ms = 0, stepNo = 0, paused = false;

    function control() {
      ROBOTS.forEach(function (n) {
        var r = robots[n];
        r.volts = n === active ? keysToVolts(keys, Math.min(r.p.motor_max_V, r.p.akku_V), slow) : { left_V: 0, right_V: 0 };
      });
    }

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
        });
        acc_ms = 0; stepNo = 0;
      },
      // dt_s = vergangene Bildzeit; rechnet in festen Schritten (welt.schritt_ms), Steuerung alle welt.steuer_ms. Liefert die Schrittzahl.
      advance: function (dt_s) {
        if (paused) return 0;
        acc_ms += Math.min(Math.max(dt_s * 1000, 0), MAX_FRAME_MS);
        var n = 0;
        while (acc_ms + EPS >= stepMs) {
          if (stepNo % ctlEvery === 0) control();
          ROBOTS.forEach(function (name) { stepRobot(robots[name], stepMs / 1000); });
          acc_ms -= stepMs; stepNo++; n++;
        }
        if (acc_ms < 0) acc_ms = 0;
        return n;
      },
      pose: function (n) { var r = robots[n]; return { x_in: r.x_in, y_in: r.y_in, h_deg: r.h_deg }; },
      speed: function (n) { var r = robots[n]; return (r.left.v + r.right.v) / 2; }
    };
  }

  return { create: create, keysToVolts: keysToVolts, wrap: wrap };
})();
if (typeof module !== 'undefined') module.exports = FBLive;
