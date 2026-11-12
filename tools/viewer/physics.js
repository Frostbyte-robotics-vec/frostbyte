/* Physik der Spielobjekte im Browser-Livesim (SIM-38, ADR-051): Cups und Pins kippen um die Bodenkante, fallen unter
   Schwerkraft, schlagen auf, prallen zurück, gleiten aus und kommen zur Ruhe. Kein DOM, keine Zeit-Funktionen, keine
   Bibliothek; läuft im Browser und unter Node. Feste Schritte: derselbe Aufruf mit demselben dt ergibt denselben Zustand.
   Alle Modellzahlen aus sim/sim_params.json (physik, welt.g_in_s2, feldobjekte.radius_in), nichts davon steht hier.
   Einheiten: Zoll, Grad (nur an der Schnittstelle), Radiant und Sekunden (intern).

   Modell eines Körpers (Zylinder mit Radius r, Höhe h, Schwerpunkt c über dem Boden):
   - standing: steht auf dem Boden (Kreis um x, y).
   - tipping: dreht sich um die Bodenkante P = (x, y) + r·(cos phi, sin phi) um den Winkel theta (0 = aufrecht, 90° = liegt).
     Drehmoment der Schwerkraft m·g·(c·sin θ − r·cos θ): über dem Kippwinkel atan(r/c) fällt er um, darunter richtet er sich auf.
   - lying: liegt, Kreis um den Schwerpunkt (x, y), gleitet mit Reibung aus.
   - falling: freier Fall in z (Ablegen, SIM-39), Landung auf der Stützfläche.
   - held: von einem Roboter gehalten (SIM-39); Lage setzt der Aufrufer mit moveHeld, keine Kollision.
   Stützfläche (SIM-39): Boden, ein Goal (hoehe_in) oder ein stehendes Teil darunter (dessen z + Stapelstufe), jeweils nur innerhalb
   des Fangradius um dessen Mitte. Ein stehendes Teil über dem Boden, dessen Stütze verschwindet (gegriffen, gekippt), fällt. */
var FBPhysics = (function () {
  'use strict';
  var HALF_PI = Math.PI / 2;
  var RAD = 180 / Math.PI;
  var KINDS = ['cup', 'pin'];
  var BISECT_STEPS = 40;

  function need(obj, key, where, positive) {
    var v = obj && obj[key];
    if (typeof v !== 'number' || !isFinite(v) || v < 0 || (positive && v === 0)) throw new Error('sim_params ' + where + '.' + key + ' fehlt oder ist ungültig');
    return v;
  }

  /* Liest den Block physik; null, wenn er fehlt (dann gibt es keine Physik, der Livesim läuft wie vorher). */
  function load(params) {
    var p = params && params.physik;
    if (!p) return null;
    var cfg = { g_in_s2: need(params.welt, 'g_in_s2', 'welt', true), height_in: {}, cg: {} };
    KINDS.forEach(function (k) {
      cfg.height_in[k] = need(p.hoehe_in, k, 'physik.hoehe_in', true);
      cfg.cg[k] = need(p.schwerpunkt_anteil, k, 'physik.schwerpunkt_anteil', true);
      if (cfg.cg[k] >= 1) throw new Error('sim_params physik.schwerpunkt_anteil.' + k + ' muss kleiner als 1 sein');
    });
    cfg.bounce = need(p, 'rueckprall', 'physik');
    cfg.stoss_bounce = need(p, 'stoss_rueckprall', 'physik');
    cfg.fall_bounce = need(p, 'fall_rueckprall', 'physik');
    [['rueckprall', cfg.bounce], ['stoss_rueckprall', cfg.stoss_bounce], ['fall_rueckprall', cfg.fall_bounce]].forEach(function (e) {
      if (e[1] >= 1) throw new Error('sim_params physik.' + e[0] + ' muss kleiner als 1 sein');
    });
    cfg.rest_omega_rad_s = need(p, 'ruhe_omega_rad_s', 'physik', true);
    cfg.rest_v_in_s = need(p, 'ruhe_v_in_s', 'physik', true);
    cfg.friction = need(p, 'gleit_reibung', 'physik', true);
    cfg.contact_h_in = need(p, 'kontakt_hoehe_in', 'physik', true);
    cfg.kick = need(p, 'kick_faktor', 'physik', true);
    cfg.evade_s = need(p, 'ausweich_s', 'physik', true);
    // Greifen und Stapeln (SIM-39): optional; ohne Block gibt es keine Hand, keine Stapel und keine Stützflächen.
    var g = p.greifen;
    cfg.grab = !g ? null : {
      reach_in: need(g, 'reichweite_in', 'physik.greifen', true), front_in: need(g, 'vorn_in', 'physik.greifen'),
      hand_z_in: need(g, 'hand_z_in', 'physik.greifen'), lift_levels: need(g, 'lift_stufen', 'physik.greifen', true),
      lift_step_in: need(g, 'lift_stufe_in', 'physik.greifen', true), lift_speed_in_s: need(g, 'lift_geschw_in_s', 'physik.greifen', true),
      pull_in_s: need(g, 'zieh_geschw_in_s', 'physik.greifen', true), fang_radius_in: need(g, 'fangradius_in', 'physik.greifen', true),
      stack_step_in: need(g, 'stapel_stufe_in', 'physik.greifen', true), snap_rate_1_s: need(g, 'fang_zug_1_s', 'physik.greifen', true)
    };
    return cfg;
  }

  /* Kippwinkel, den ein Roboter erzwingt, der depth_in tief in den Standkreis eingedrungen ist und das Objekt in
     Höhe contact_in berührt: Berührpunkt (2r vom Drehpunkt, contact_in hoch) wandert um contact·sin θ + 2r·(1 − cos θ). */
  function forcedAngle(depth_in, contact_in, r_in) {
    if (!(depth_in > 0)) return 0;
    if (depth_in >= contact_in + 2 * r_in) return HALF_PI;
    var lo = 0, hi = HALF_PI;
    for (var i = 0; i < BISECT_STEPS; i++) {
      var mid = (lo + hi) / 2;
      if (contact_in * Math.sin(mid) + 2 * r_in * (1 - Math.cos(mid)) < depth_in) lo = mid; else hi = mid;
    }
    return (lo + hi) / 2;
  }

  /* spec: {id, kind ('cup'|'pin'), x_in, y_in, r_in, standing, lie_dir_deg}; opts.field_in: Seitenlänge des Felds (sonst
     unbegrenzt). Liegt ein Objekt von Anfang an, ist (x, y) sein Schwerpunkt und lie_dir_deg die Richtung, in die sein
     oberes Ende zeigt. */
  function create(cfg, specs, opts) {
    var half = opts && opts.field_in > 0 ? opts.field_in / 2 : Infinity;
    var G = cfg.g_in_s2;
    var goals = [];
    var bodies = specs.map(function (s) {
      var h = cfg.height_in[s.kind], c = cfg.cg[s.kind] * h, r = s.r_in;
      var k2 = r * r / 4 + h * h / 12;   // Massenträgheit je Masse um die Querachse durch den Schwerpunkt (Vollzylinder)
      return { id: s.id, kind: s.kind, x: s.x_in, y: s.y_in, r: r, h: h, c: c, ieff: k2 + r * r + c * c, crit: Math.atan2(r, c),
        state: s.standing ? 'standing' : 'lying', theta: s.standing ? 0 : HALF_PI, omega: 0, phi: (s.lie_dir_deg || 0) / RAD,
        vx: 0, vy: 0, z: s.z_in || 0, vz: 0, fallen: false, pristine: !s.standing };
    });

    function clampField(b) {
      var m = half - b.r;
      if (b.x > m) { b.x = m; if (b.vx > 0) b.vx = 0; } else if (b.x < -m) { b.x = -m; if (b.vx < 0) b.vx = 0; }
      if (b.y > m) { b.y = m; if (b.vy > 0) b.vy = 0; } else if (b.y < -m) { b.y = -m; if (b.vy < 0) b.vy = 0; }
    }

    // Aufgeschlagen: der Schwerpunkt liegt jetzt c vor dem Drehpunkt; der Kreis zieht vom Standkreis dorthin.
    function toLying(b) {
      var dx = Math.cos(b.phi), dy = Math.sin(b.phi);
      b.x += (b.r + b.c) * dx; b.y += (b.r + b.c) * dy;
      b.state = 'lying'; b.theta = HALF_PI; b.omega = 0; b.vx = 0; b.vy = 0;
      clampField(b);
    }

    // Höhe der Stützfläche unter b und, wenn es eine Mitte gibt, zu der das Teil hingezogen wird (Goal oder Teil darunter).
    function supportAt(self) {
      var best = { z: 0, sx: null, sy: null }, g = cfg.grab;
      if (!g) return best;
      var f2 = g.fang_radius_in * g.fang_radius_in;
      goals.forEach(function (goal) {
        var dx = goal.x_in - self.x, dy = goal.y_in - self.y;
        if (dx * dx + dy * dy <= f2 && goal.z_in > best.z) best = { z: goal.z_in, sx: goal.x_in, sy: goal.y_in };
      });
      bodies.forEach(function (o) {
        if (o === self || o.state !== 'standing' || !(o.z < self.z - 1e-9)) return;
        var dx = o.x - self.x, dy = o.y - self.y, top = o.z + g.stack_step_in;
        if (dx * dx + dy * dy <= f2 && top > best.z + 1e-9) best = { z: top, sx: o.x, sy: o.y };
      });
      return best;
    }

    function integrate(b, dt, ev) {
      if (b.state === 'tipping') {
        b.omega += G * (b.c * Math.sin(b.theta) - b.r * Math.cos(b.theta)) / b.ieff * dt;
        var next = b.theta + b.omega * dt;
        if (next >= HALF_PI) {
          var wIn = Math.max(b.omega, 0);
          ev.push({ type: 'impact', id: b.id, omega_rad_s: wIn });
          b.theta = HALF_PI;
          if (wIn < cfg.rest_omega_rad_s) toLying(b); else b.omega = -cfg.bounce * wIn;
        } else if (next <= 0) {
          var wUp = Math.max(-b.omega, 0);
          b.theta = 0;
          if (wUp < cfg.rest_omega_rad_s) { b.state = 'standing'; b.omega = 0; } else b.omega = cfg.bounce * wUp;
        } else b.theta = next;
        if (b.state === 'tipping' && !b.fallen && b.theta > b.crit) { b.fallen = true; ev.push({ type: 'toppled', id: b.id }); }
      } else if (b.state === 'lying') {
        var sp = Math.sqrt(b.vx * b.vx + b.vy * b.vy);
        if (sp > 0) {
          var s2 = Math.max(0, sp - cfg.friction * G * dt);
          b.vx *= s2 / sp; b.vy *= s2 / sp;
          b.x += b.vx * dt; b.y += b.vy * dt;
          clampField(b);
        }
      } else if (b.state === 'falling') {
        b.vz -= G * dt;
        b.z += b.vz * dt;
        var sup = supportAt(b);
        if (sup.sx !== null) {   // über dem Goal oder Stapel: zur Mitte ziehen
          var k = Math.min(1, cfg.grab.snap_rate_1_s * dt);
          b.x += (sup.sx - b.x) * k; b.y += (sup.sy - b.y) * k;
        }
        if (b.z <= sup.z) {
          var vIn = -b.vz;
          b.z = sup.z;
          if (vIn < cfg.rest_v_in_s) {
            b.vz = 0; b.state = 'standing';
            if (sup.sx !== null) { b.x = sup.sx; b.y = sup.sy; }
            ev.push({ type: 'landed', id: b.id, z_in: b.z, stacked: sup.z > 0 });
          } else b.vz = cfg.fall_bounce * vIn;
        }
      } else if (b.state === 'standing' && b.z > 0) {
        if (supportAt(b).z < b.z - 1e-6) { b.state = 'falling'; b.vz = 0; }   // Stütze weg
      }
    }

    function moving(b) { return b.state === 'lying' && (b.vx !== 0 || b.vy !== 0); }

    // Ein liegendes, bewegtes Objekt a trifft b. Zwei liegende tauschen den Stoß (gleiche Massen), ein stehendes wird
    // angekippt (Schwung wirkt auf den Berührpunkt in Höhe contact), das bewegte prallt ab.
    function collide(a, b) {
      if (b.state === 'falling' || b.state === 'held' || b.z > 0 || a.z > 0) return;
      var dx = b.x - a.x, dy = b.y - a.y, dist = Math.sqrt(dx * dx + dy * dy), min = a.r + b.r;
      if (dist >= min) return;
      var nx = dist > 0 ? dx / dist : 1, ny = dist > 0 ? dy / dist : 0, over = min - dist;
      if (b.state === 'lying') {
        a.x -= nx * over / 2; a.y -= ny * over / 2; b.x += nx * over / 2; b.y += ny * over / 2;
        var vn = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
        if (vn < 0) {
          var j = -(1 + cfg.stoss_bounce) * vn / 2;
          a.vx -= j * nx; a.vy -= j * ny; b.vx += j * nx; b.vy += j * ny;
        }
        clampField(a); clampField(b);
        return;
      }
      a.x -= nx * over; a.y -= ny * over;
      var s = a.vx * nx + a.vy * ny;
      if (s <= 0) { clampField(a); return; }
      if (b.state === 'standing') { b.state = 'tipping'; b.phi = Math.atan2(ny, nx); }
      if (Math.cos(b.phi) * nx + Math.sin(b.phi) * ny > 0) b.omega = Math.max(b.omega, cfg.kick * s / Math.min(cfg.contact_h_in, b.h));
      a.vx -= (1 + cfg.stoss_bounce) * s * nx; a.vy -= (1 + cfg.stoss_bounce) * s * ny;
      clampField(a);
    }

    var world = {
      bodies: bodies,
      cfg: cfg,
      // Ein Schritt dt_s; liefert die Ereignisse {type: 'toppled' | 'impact', id}.
      step: function (dt_s) {
        var ev = [];
        bodies.forEach(function (b) { integrate(b, dt_s, ev); });
        for (var i = 0; i < bodies.length; i++) {
          var a = bodies[i];
          if (!moving(a)) continue;
          for (var j = 0; j < bodies.length; j++) {
            if (j === i || (j < i && moving(bodies[j]))) continue;
            collide(a, bodies[j]);
            if (!moving(a)) break;
          }
        }
        return ev;
      },
      // Roboter drückt von der Seite: (nx, ny) = Schubrichtung (Roboter → Objekt, Länge 1), depth_in = Eindringtiefe in den
      // Standkreis. Das Objekt neigt sich mindestens so weit, wie der Roboter es erzwingt, mit dessen Geschwindigkeit.
      pushStanding: function (i, nx, ny, depth_in, dt_s) {
        var b = bodies[i];
        if (b.z > 0 || (b.state !== 'standing' && b.state !== 'tipping')) return;
        if (b.state === 'tipping' && Math.cos(b.phi) * nx + Math.sin(b.phi) * ny <= 0) return;   // gegen die Kipprichtung
        var want = forcedAngle(depth_in, Math.min(cfg.contact_h_in, b.h), b.r);
        if (want <= b.theta) return;
        if (b.state === 'standing') { b.state = 'tipping'; b.phi = Math.atan2(ny, nx); }
        b.omega = Math.max(b.omega, (want - b.theta) / dt_s);
        b.theta = want;
      },
      // Liegendes Objekt bekommt die Geschwindigkeit (Roboter schiebt es an); es gleitet danach mit Reibung aus.
      kick: function (i, vx_in_s, vy_in_s) {
        var b = bodies[i];
        if (b.state !== 'lying') return;
        b.vx = vx_in_s; b.vy = vy_in_s;
      },
      // Roboter schiebt ein liegendes Objekt in Richtung (nx, ny): die Geschwindigkeit entlang der Schubrichtung ist mindestens
      // v_in_s, quer dazu bleibt sie. Es gleitet danach mit Reibung aus.
      pushLying: function (i, nx, ny, v_in_s) {
        var b = bodies[i];
        if (b.state !== 'lying') return;
        var vn = b.vx * nx + b.vy * ny;
        if (vn >= v_in_s) return;
        b.vx += (v_in_s - vn) * nx; b.vy += (v_in_s - vn) * ny;
      },
      // Objekt fällt aus Höhe z_in an (x, y), aufrecht, auf die Stützfläche darunter (Boden, Goal, Teil).
      drop: function (i, x_in, y_in, z_in) {
        var b = bodies[i];
        b.x = x_in; b.y = y_in; b.z = z_in; b.vz = 0; b.vx = 0; b.vy = 0; b.theta = 0; b.omega = 0; b.state = 'falling';
      },
      // Goals als Stützflächen: [{x_in, y_in, z_in (Oberkante)}]
      setGoals: function (list) { goals = list.slice(); },
      // Setzt Teile, die über dem Boden beginnen (spec.z_in > 0), von unten nach oben auf ihre Stützfläche; ohne Stütze bleibt z.
      settle: function () {
        bodies.filter(function (b) { return b.state === 'standing' && b.z > 0; }).sort(function (a, b) { return a.z - b.z; }).forEach(function (b) {
          var declared = b.z;
          b.z = Infinity;
          var sup = supportAt(b);
          b.z = sup.z > 0 ? sup.z : declared;
        });
      },
      // Ein stehendes Teil hat nichts obendrauf (nur dann ist es greifbar).
      isFree: function (i) {
        var b = bodies[i], g = cfg.grab, f2 = g ? g.fang_radius_in * g.fang_radius_in : 0;
        return !bodies.some(function (o) {
          var dx = o.x - b.x, dy = o.y - b.y;
          return o !== b && o.state === 'standing' && o.z > b.z + 1e-9 && dx * dx + dy * dy <= f2;
        });
      },
      // Roboter nimmt ein stehendes Teil in die Hand; es bleibt, wo es ist, bis moveHeld es zur Hand zieht.
      hold: function (i) {
        var b = bodies[i];
        if (b.state !== 'standing') return false;
        b.state = 'held'; b.vx = 0; b.vy = 0; b.vz = 0; b.theta = 0; b.omega = 0;
        return true;
      },
      // Zieht ein gehaltenes Teil mit höchstens speed_in_s (Standard: physik.greifen.zieh_geschw_in_s) zum Ziel; true, wenn dort.
      moveHeld: function (i, tx, ty, tz, dt_s, speed_in_s) {
        var b = bodies[i], dx = tx - b.x, dy = ty - b.y, dz = tz - b.z, d = Math.sqrt(dx * dx + dy * dy + dz * dz);
        var step = (speed_in_s === undefined ? cfg.grab.pull_in_s : speed_in_s) * dt_s;
        if (d <= step) { b.x = tx; b.y = ty; b.z = tz; return true; }
        b.x += dx / d * step; b.y += dy / d * step; b.z += dz / d * step;
        return false;
      },
      // Lässt ein gehaltenes Teil los; es fällt aus der jetzigen Lage.
      release: function (i) {
        var b = bodies[i];
        if (b.state !== 'held') return false;
        b.state = 'falling'; b.vz = 0;
        return true;
      },
      // Richtung (Feld, Grad) des oberen Endes eines Objekts, das von Anfang an liegt und noch nie berührt wurde.
      setLieDir: function (i, deg) {
        var b = bodies[i];
        if (b.pristine && b.state === 'lying') b.phi = deg / RAD;
      },
      // Lage zum Zeichnen: Fußpunkt (bx, by, z), Neigung tilt_deg aus der Senkrechten in Richtung dir_deg (Feld).
      pose: function (i) {
        var b = bodies[i], dx = Math.cos(b.phi), dy = Math.sin(b.phi);
        if (b.state === 'tipping') {
          var shift = b.r * (1 - Math.cos(b.theta));
          return { bx_in: b.x + shift * dx, by_in: b.y + shift * dy, z_in: b.z + b.r * Math.sin(b.theta), tilt_deg: b.theta * RAD, dir_deg: b.phi * RAD };
        }
        if (b.state === 'lying') return { bx_in: b.x - b.c * dx, by_in: b.y - b.c * dy, z_in: b.r, tilt_deg: 90, dir_deg: b.phi * RAD };
        return { bx_in: b.x, by_in: b.y, z_in: b.z, tilt_deg: 0, dir_deg: b.phi * RAD };
      }
    };
    return world;
  }

  return { load: load, create: create, forcedAngle: forcedAngle };
})();
if (typeof module !== 'undefined') module.exports = FBPhysics;
