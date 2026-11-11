/* Auswahl der CAD-Modelle für Spielelemente (SIM-37). Kein DOM, kein three.js; läuft im Browser und unter Node.
   Die Modelle selbst (models/models.js, window.FB_MODELS) entstehen mit tools/export_field_models.py aus dem Feld-CAD und
   liegen im Repo (VEX hat die Weitergabe bestätigt, ADR-045). Fehlen sie, zeichnet der Viewer wie bisher einfache Körper. */
var FBModels = (function () {
  'use strict';
  var VERSION = 1;
  var PIN = { yellow: 'pin_neutral_neutral', red_yellow: 'pin_red_neutral', blue_yellow: 'pin_blue_neutral', red_blue: 'pin_red_blue' };
  var GOAL = { alliance_red: 'goal_red', alliance_blue: 'goal_blue', neutral: 'goal_neutral', midfield: 'goal_midfield' };
  var LOADER = { red: 'loader_red', blue: 'loader_blue' };

  function pinKey(variant) { return PIN[variant] || PIN.yellow; }
  function goalKey(typ) { return GOAL[typ] || null; }
  function loaderKey(farbe) { return LOADER[farbe] || null; }
  function objectKey(o) {
    if (o.kind === 'cup') return 'cup';
    if (o.kind === 'pin') return pinKey(o.variant);
    return null;
  }

  function usable(payload) {
    return !!(payload && payload.manifest && payload.manifest.version === VERSION && payload.manifest.modelle && payload.glb);
  }
  function has(payload, key) { return usable(payload) && !!payload.glb[key] && !!payload.manifest.modelle[key]; }
  function heightIn(payload, key, fallback) {
    return has(payload, key) ? payload.manifest.modelle[key].height_in : fallback;
  }

  // Farbname eines Teils (rot / blau / gelb) aus dem Farbwert
  function colorName(rgba) {
    var r = rgba[0], g = rgba[1], b = rgba[2];
    if (r > 0.6 && g > 0.55 && b < 0.4) return 'yellow';
    if (r >= b && r > g) return 'red';
    if (b > r && b > g) return 'blue';
    return 'other';
  }
  function partAt(entry, top) {
    var best = null;
    (entry.teile || []).forEach(function (t) {
      var mid = (t.y_min_in + t.y_max_in) / 2;
      if (best === null || (top ? mid > best.mid : mid < best.mid)) best = { mid: mid, t: t };
    });
    return best && best.t;
  }
  function topColor(entry) { var t = partAt(entry, true); return t ? colorName(t.rgba) : null; }
  function needsFlip(entry, wantTop) { return !!wantTop && topColor(entry) !== null && topColor(entry) !== wantTop; }
  function yellowDown(entry) { var t = partAt(entry, false); return !!t && colorName(t.rgba) === 'yellow'; }

  // Modellachse (+y) eines liegenden Pins im Viewer-System (x = x_in, z = −y_in). yellow_deg: Richtung der gelben Seite im
  // Feld (0° = +x, 90° = +y). Ohne Angabe: Achse entlang x.
  function lyingAxis(entry, yellowDeg) {
    if (typeof yellowDeg !== 'number') return [1, 0, 0];
    var a = yellowDeg * Math.PI / 180, dx = Math.cos(a), dz = -Math.sin(a), s = yellowDown(entry) ? -1 : 1;
    return [s * dx, 0, s * dz];
  }

  function decodeBase64(str) {
    var bytes;
    if (typeof atob === 'function') {
      var bin = atob(str);
      bytes = new Uint8Array(bin.length);
      for (var i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    } else {
      var b = Buffer.from(str, 'base64');
      bytes = new Uint8Array(b);
    }
    return bytes.buffer;
  }

  return { VERSION: VERSION, pinKey: pinKey, goalKey: goalKey, loaderKey: loaderKey, objectKey: objectKey, usable: usable,
    has: has, heightIn: heightIn, topColor: topColor, needsFlip: needsFlip, yellowDown: yellowDown, lyingAxis: lyingAxis,
    decodeBase64: decodeBase64 };
})();
if (typeof module !== 'undefined') module.exports = FBModels;
