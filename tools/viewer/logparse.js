/* Frostbyte-Log lesen (Format docs/log_format.md, v1). Kein DOM; läuft im Browser und unter JavaScriptCore.
   Spalten nur nach Namen, leere Zelle = NaN, Zeilen nach robot-Spalte gruppiert, Abtasten nach t_s. */
var FBLog = (function () {
  'use strict';
  var VERSION = 1;
  var NUM = ['t_s', 'x_in', 'y_in', 'h_deg', 'gx_in', 'gy_in', 'gh_deg', 'g_err_in', 'tx_in', 'ty_in', 'th_deg',
    'lift_deg', 'arm_deg', 'claw_in', 'lift_set_deg', 'arm_set_deg', 'has_cup', 'has_pin', 'pin_yellow_up', 'score',
    'cpu_ms', 'batt_v', 'temp_max_c'];
  var TEXT = ['robot', 'cycle', 'route_step', 'event'];
  var ANGLE = { h_deg: 1, gh_deg: 1, th_deg: 1 };
  var DISCRETE = { has_cup: 1, has_pin: 1, pin_yellow_up: 1, score: 1 };
  var POSE = { odom: ['x_in', 'y_in', 'h_deg'], gps: ['gx_in', 'gy_in', 'gh_deg'], wahr: ['tx_in', 'ty_in', 'th_deg'] };

  function parseHeader(line) {
    var meta = { version: null, robot: '', source: '', seed: '', build: '' };
    var m = /^#\s*frostbyte-log\s+v(\d+)/.exec(line);
    if (m) meta.version = parseInt(m[1], 10);
    line.replace(/(\w+)=(\S*)/g, function (_, k, v) { if (k in meta && k !== 'version') meta[k] = v; return ''; });
    return meta;
  }

  function parseLog(text) {
    var warnings = [];
    var lines = String(text).split(/\r?\n/);
    while (lines.length && lines[lines.length - 1] === '') lines.pop();
    var meta = parseHeader('');
    var li = 0;
    if (lines.length && lines[0].charAt(0) === '#') {
      meta = parseHeader(lines[0]);
      if (meta.version === null) warnings.push('Kopfzeile ohne „frostbyte-log vN“');
      else if (meta.version !== VERSION) warnings.push('Log-Format v' + meta.version + ', Viewer kennt v' + VERSION + ' – Werte evtl. falsch');
      li = 1;
    } else if (lines.length) {
      warnings.push('Keine Kopfzeile „# frostbyte-log v1 …“');
    }
    if (li >= lines.length) { warnings.push('Datei leer oder ohne Spaltenzeile'); return { meta: meta, robots: {}, warnings: warnings }; }
    var cols = lines[li].split(',').map(function (c) { return c.trim(); });
    var idx = {};
    cols.forEach(function (c, i) { idx[c] = i; });
    ['t_s', 'x_in', 'y_in', 'h_deg'].forEach(function (c) { if (!(c in idx)) warnings.push('Spalte ' + c + ' fehlt'); });

    // Zeilen lesen und nach Roboter sammeln
    var groups = {}, order = [];
    for (var r = li + 1; r < lines.length; r++) {
      if (lines[r] === '') continue;
      var cells = lines[r].split(',');
      if (cells.length !== cols.length) { warnings.push('Zeile ' + (r + 1) + ': ' + cells.length + ' statt ' + cols.length + ' Spalten, übersprungen'); continue; }
      var name = ('robot' in idx && cells[idx.robot]) || meta.robot || '?';
      if (!groups[name]) { groups[name] = []; order.push(name); }
      groups[name].push(cells);
    }

    var robots = {};
    order.forEach(function (name) {
      var rows = groups[name];
      var ti = idx.t_s;
      var sorted = true;
      for (var i = 1; i < rows.length; i++) if (+rows[i][ti] < +rows[i - 1][ti]) { sorted = false; break; }
      if (!sorted) {
        warnings.push(name + ': Zeit nicht aufsteigend, sortiert');
        rows = rows.slice().sort(function (a, b) { return a[ti] - b[ti]; });
      }
      var n = rows.length, tr = { name: name, n: n, num: {}, text: {} };
      NUM.forEach(function (c) {
        var a = new Float64Array(n), k = idx[c];
        for (var i = 0; i < n; i++) { var s = k === undefined ? '' : rows[i][k]; a[i] = s === '' ? NaN : +s; }
        tr.num[c] = a;
      });
      TEXT.forEach(function (c) {
        var a = new Array(n), k = idx[c];
        for (var i = 0; i < n; i++) a[i] = k === undefined ? '' : rows[i][k];
        tr.text[c] = a;
      });
      tr.text.robot = tr.text.robot.map(function () { return name; });
      robots[name] = tr;
    });
    return { meta: meta, robots: robots, warnings: warnings };
  }

  // größter Index i mit t[i] <= t (geklemmt auf 0 … n-1)
  function indexAt(tr, t) {
    var a = tr.num.t_s, lo = 0, hi = tr.n - 1;
    if (hi < 0) return -1;
    if (t <= a[0]) return 0;
    if (t >= a[hi]) return hi;
    while (hi - lo > 1) { var mid = (lo + hi) >> 1; if (a[mid] <= t) lo = mid; else hi = mid; }
    return lo;
  }

  function wrap(a) { return ((a + 180) % 360 + 360) % 360 - 180; }

  function sample(tr, col, t) {
    var i = indexAt(tr, t);
    if (i < 0) return NaN;
    if (tr.text[col]) return tr.text[col][i];
    var v = tr.num[col], a = v[i];
    if (DISCRETE[col] || i + 1 >= tr.n || Number.isNaN(a)) return a;
    var b = v[i + 1];
    if (Number.isNaN(b)) return a;
    var t0 = tr.num.t_s[i], t1 = tr.num.t_s[i + 1];
    var f = t1 > t0 ? Math.min(1, Math.max(0, (t - t0) / (t1 - t0))) : 0;
    if (f === 0) return a;
    if (ANGLE[col]) return wrap(a + wrap(b - a) * f);
    return a + (b - a) * f;
  }

  function pose(tr, t, source) {
    var c = POSE[source] || POSE.odom;
    return { x: sample(tr, c[0], t), y: sample(tr, c[1], t), h: sample(tr, c[2], t) };
  }

  function events(log) {
    var out = [];
    Object.keys(log.robots).forEach(function (name) {
      var tr = log.robots[name], ev = tr.text.event;
      for (var i = 0; i < tr.n; i++) {
        if (!ev[i]) continue;
        ev[i].split(';').forEach(function (e) {
          if (e) out.push({ t: tr.num.t_s[i], i: i, robot: name, kind: e.split(':')[0], text: e });
        });
      }
    });
    out.sort(function (a, b) { return a.t - b.t || (a.robot < b.robot ? -1 : a.robot > b.robot ? 1 : 0); });
    return out;
  }

  function timeRange(log) {
    var lo = Infinity, hi = -Infinity;
    Object.keys(log.robots).forEach(function (k) {
      var a = log.robots[k].num.t_s;
      if (a.length) { lo = Math.min(lo, a[0]); hi = Math.max(hi, a[a.length - 1]); }
    });
    return lo <= hi ? [lo, hi] : [0, 0];
  }

  return { VERSION: VERSION, POSE: POSE, parseLog: parseLog, indexAt: indexAt, sample: sample, pose: pose,
    events: events, timeRange: timeRange, wrap: wrap };
})();
if (typeof module !== 'undefined') module.exports = FBLog;
