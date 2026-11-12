/* Tests für physics.js (SIM-38). Läuft im Browser (index.html?selftest) und unter Node (test_livesim.py). Kein DOM, keine
   Ausgabe: liefert ein Ergebnisobjekt. Eigene Fixture-Werte, die Tests hängen nicht an den Schätzwerten aus sim_params.json.
   Erwartungen sind analytisch (Fallzeit, Energiebilanz, Gleitweg), nicht aus dem Code abgeschrieben. */
function runPhysicsTests(FBPhysics, FBLive) {
  const msgs = [];
  let ok = 0, fail = 0;
  function check(name, cond, info) {
    if (cond) ok++; else { fail++; msgs.push('FEHLER ' + name + (info !== undefined ? ': ' + info : '')); }
  }
  const G = 386.1, DT = 0.001;
  const physik = { hoehe_in: { cup: 6.5, pin: 6.5 }, schwerpunkt_anteil: { cup: 0.4, pin: 0.5 }, rueckprall: 0.3, stoss_rueckprall: 0.5,
    fall_rueckprall: 0.2, ruhe_omega_rad_s: 0.5, ruhe_v_in_s: 1.0, gleit_reibung: 0.5, kontakt_hoehe_in: 4.0, kick_faktor: 1.0, ausweich_s: 0.05 };
  const base = { welt: { schritt_ms: 1, steuer_ms: 10, log_ms: 20, g_in_s2: G }, physik };
  const cfg = FBPhysics.load(base);
  const spec = (id, x, y, extra) => Object.assign({ id, kind: 'pin', x_in: x, y_in: y, r_in: 1.5, standing: true }, extra);
  const make = (specs, field) => FBPhysics.create(cfg, specs, { field_in: field });
  const run = (w, seconds, evs) => { for (let t = 0; t < Math.round(seconds / DT); t++) { const e = w.step(DT); if (evs) e.forEach((x) => evs.push(x)); } };

  // 1 Laden
  check('ohne Block physik: null', FBPhysics.load({ welt: base.welt }) === null);
  let threw = false;
  try { FBPhysics.load({ welt: base.welt, physik: Object.assign({}, physik, { rueckprall: 1 }) }); } catch (e) { threw = true; }
  check('Rückprall >= 1 wird abgelehnt', threw);
  threw = false;
  try { FBPhysics.load({ welt: base.welt, physik: Object.assign({}, physik, { gleit_reibung: undefined }) }); } catch (e) { threw = true; }
  check('fehlende Zahl wird abgelehnt', threw);

  // 2 Kippwinkel atan(2r/h) bei Schwerpunkt in halber Höhe
  let w = make([spec('P', 0, 0)]);
  check('Kippwinkel = atan(2r/h)', Math.abs(w.bodies[0].crit - Math.atan(2 * 1.5 / 6.5)) < 1e-12, w.bodies[0].crit);

  // 3 Erzwungener Kippwinkel: Umkehrung von f(θ) = hc·sin θ + 2r·(1 − cos θ)
  [0.1, 0.4, 1.0, 1.4].forEach((th) => {
    const depth = 4.0 * Math.sin(th) + 3.0 * (1 - Math.cos(th));
    check('forcedAngle(' + th + ')', Math.abs(FBPhysics.forcedAngle(depth, 4.0, 1.5) - th) < 1e-9);
  });
  check('forcedAngle: keine Tiefe, kein Winkel; sehr tief: 90°', FBPhysics.forcedAngle(0, 4, 1.5) === 0 && FBPhysics.forcedAngle(99, 4, 1.5) === Math.PI / 2);

  // 4 Freier Fall: Landung nach √(2h/g), danach Ruhe auf dem Boden, nie darunter
  w = make([spec('P', 0, 0)]);
  w.drop(0, 0, 0, 20);
  let tLand = -1, minZ = 1e9;
  for (let t = 1; t <= 2000; t++) {
    w.step(DT);
    minZ = Math.min(minZ, w.bodies[0].z);
    if (tLand < 0 && w.bodies[0].z <= 0) tLand = t * DT;
  }
  check('Fallzeit aus 20 in = √(2h/g)', Math.abs(tLand - Math.sqrt(2 * 20 / G)) < 0.002, tLand);
  check('nach dem Fall steht es ruhig auf dem Boden', w.bodies[0].state === 'standing' && w.bodies[0].z === 0 && w.bodies[0].vz === 0, JSON.stringify(w.bodies[0]));
  check('nie unter dem Boden', minZ >= 0, minZ);
  w.drop(0, 0, 0, 20);
  w.setSupport(() => 10);
  run(w, 1.5);
  check('Landung auf einer höheren Stützfläche', w.bodies[0].z === 10 && w.bodies[0].state === 'standing', w.bodies[0].z);

  // 5 Umfallen: Energiebilanz beim Aufschlag. Start bei θ0 über dem Kippwinkel, ohne Anfangsschwung.
  const tipFrom = (th0, dir) => {
    const wd = make([spec('P', 0, 0)]);
    const depth = 4.0 * Math.sin(th0) + 3.0 * (1 - Math.cos(th0));
    wd.pushStanding(0, Math.cos(dir), Math.sin(dir), depth, DT);
    wd.bodies[0].omega = 0;
    return wd;
  };
  const th0 = 0.7;
  w = tipFrom(th0, 0);
  const evs = [];
  run(w, 2.0, evs);
  const b0 = w.bodies[0];
  const imp = evs.filter((e) => e.type === 'impact')[0];
  const wExpect = Math.sqrt(2 * G * (b0.r * Math.sin(th0) + b0.c * Math.cos(th0) - b0.r) / b0.ieff);
  check('Aufschlag-Winkelgeschwindigkeit aus der Energiebilanz (±3 %)', imp && Math.abs(imp.omega_rad_s - wExpect) / wExpect < 0.03, imp && imp.omega_rad_s + ' vs ' + wExpect);
  check('genau ein Ereignis toppled', evs.filter((e) => e.type === 'toppled').length === 1 && evs.filter((e) => e.type === 'toppled')[0].id === 'P');
  check('liegt am Ende, ohne Geschwindigkeit', b0.state === 'lying' && b0.vx === 0 && b0.vy === 0 && b0.theta === Math.PI / 2, b0.state);
  check('Schwerpunkt liegt (r + c) vor dem Standkreis in Kipprichtung', Math.abs(b0.x - (b0.r + b0.c)) < 1e-9 && Math.abs(b0.y) < 1e-9, b0.x + ',' + b0.y);
  let pose = w.pose(0);
  check('liegend: Fuß am Drehpunkt, Höhe r, 90°', Math.abs(pose.bx_in - b0.r) < 1e-9 && pose.z_in === b0.r && pose.tilt_deg === 90, JSON.stringify(pose));
  check('mehrere Aufschläge klingen ab (Rückprall), mindestens zwei', evs.filter((e) => e.type === 'impact').length >= 2, evs.filter((e) => e.type === 'impact').length);

  // 6 Kein Sprung: der Fuß wandert stetig, Höhe und Winkel wachsen ohne Lücke
  w = tipFrom(th0, 1.0);
  let prev = w.pose(0), maxJump = 0, sawMid = false;
  for (let t = 0; t < 800; t++) {
    w.step(DT);
    const p = w.pose(0);
    maxJump = Math.max(maxJump, Math.hypot(p.bx_in - prev.bx_in, p.by_in - prev.by_in, p.z_in - prev.z_in));
    if (p.tilt_deg > 20 && p.tilt_deg < 70) sawMid = true;
    prev = p;
  }
  check('Fußpunkt springt nie (< 0,3 in je ms)', maxJump < 0.3, maxJump);
  check('Zwischenwinkel werden durchlaufen', sawMid);

  // 7 Unter dem Kippwinkel: richtet sich wieder auf, kein Ereignis, nie unter dem Boden
  w = tipFrom(0.3, 0);
  const evs2 = [];
  let minZ2 = 1e9;
  for (let t = 0; t < 2000; t++) { w.step(DT).forEach((e) => evs2.push(e)); minZ2 = Math.min(minZ2, w.pose(0).z_in); }
  check('sanft angekippt: steht wieder', w.bodies[0].state === 'standing' && w.bodies[0].theta === 0 && w.bodies[0].omega === 0, w.bodies[0].state);
  check('sanft angekippt: kein toppled', evs2.filter((e) => e.type === 'toppled').length === 0);
  check('nie unter dem Boden', minZ2 >= 0, minZ2);

  // 8 Ruhe: nach dem Ausschwingen ändert sich nichts mehr (kein Zittern)
  w = tipFrom(th0, 0);
  run(w, 3.0);
  const frozen = JSON.stringify([w.bodies[0], w.pose(0)]);
  run(w, 5.0);
  check('liegendes Objekt bleibt bit-genau ruhig', JSON.stringify([w.bodies[0], w.pose(0)]) === frozen);

  // 9 Gleiten: Weg = v²/(2·μ·g), bleibt im Feld
  w = make([spec('P', 0, 0, { standing: false })]);
  w.kick(0, 30, 0);
  run(w, 3.0);
  check('Gleitweg = v²/(2 μ g) (±2 %)', Math.abs(w.bodies[0].x - 30 * 30 / (2 * 0.5 * G)) / (30 * 30 / (2 * 0.5 * G)) < 0.02, w.bodies[0].x);
  check('zum Stehen gekommen', w.bodies[0].vx === 0 && w.bodies[0].vy === 0);
  w = make([spec('P', 40, 0, { standing: false })], 100);
  w.kick(0, 200, 0);
  run(w, 3.0);
  check('Feldrand hält das Objekt auf (x ≤ 50 − r)', w.bodies[0].x <= 50 - 1.5 + 1e-9 && w.bodies[0].vx === 0, w.bodies[0].x);

  // 10 Objekt gegen Objekt: liegend gegen liegend erhält den Impuls, steht danach nicht in der Wand
  w = make([spec('A', 0, 0, { standing: false }), spec('B', 2.5, 0, { standing: false })]);
  w.kick(0, 20, 0);
  w.step(DT);
  const a = w.bodies[0], b = w.bodies[1];
  check('Stoß: Impuls erhalten (Reibung eines Schritts ausgenommen)', Math.abs((a.vx + b.vx) - 20) < 2 * 0.5 * G * DT + 1e-9, a.vx + ',' + b.vx);
  check('Stoß: B fliegt vor A weg, kein Eindringen', b.vx > a.vx && Math.hypot(b.x - a.x, b.y - a.y) >= 3.0 - 1e-9, a.vx + ',' + b.vx);

  // 11 Ein schnelles liegendes Objekt kippt ein stehendes, ein langsames nicht
  const hit = (speed) => {
    const wd = make([spec('A', 0, 0, { standing: false }), spec('T', 3.5, 0)]);
    wd.kick(0, speed, 0);
    const e = [];
    run(wd, 1.5, e);
    return { world: wd, toppled: e.filter((x) => x.type === 'toppled').length };
  };
  const fast = hit(60), slow = hit(2);
  check('schneller Stoß kippt das stehende Objekt', fast.toppled === 1 && fast.world.bodies[1].state === 'lying' && fast.world.bodies[1].x > 3.5);
  check('langsamer Stoß lässt es stehen', slow.toppled === 0 && slow.world.bodies[1].state === 'standing');

  // 12 Roboter von der Seite: gegen die Kipprichtung wirkt nichts, mit ihr wächst der Winkel
  w = make([spec('P', 0, 0)]);
  w.pushStanding(0, 1, 0, 1.0, DT);
  const th1 = w.bodies[0].theta;
  w.pushStanding(0, -1, 0, 2.5, DT);
  check('Schub gegen die Kipprichtung ändert nichts', w.bodies[0].theta === th1 && w.bodies[0].phi === 0);
  w.pushStanding(0, 1, 0, 2.5, DT);
  check('mehr Eindringtiefe: größerer Winkel', w.bodies[0].theta > th1);

  // 13 Determinismus
  const trace = () => { const wd = tipFrom(th0, 0.4); run(wd, 2.0); return JSON.stringify([wd.bodies, wd.pose(0)]); };
  check('gleiche Eingabe, gleicher Zustand', trace() === trace());

  // 14 Einbindung in den Livesim: Roboter fährt in einen Cup (eigene Fixture), er kippt sichtbar statt zu springen
  {
    const dr = (spur) => ({ kS_V: 1.0, kV_V_per_in_s: 0.2, kA_V_per_in_s2: 0.04, spurbreite_in: spur, mu: 1.0, motor_max_V: 11.5 });
    const lp = {
      welt: base.welt,
      live: { langsam_faktor: 0.4, ersatzpose_in: { stacker: { x_in: 0, y_in: 0, h_deg: 0 }, runner: { x_in: 0, y_in: 0, h_deg: 0 } } },
      roboter: { stacker: { antrieb: dr(12), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 12, breite: 12 } },
                 runner: { antrieb: dr(10), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 10, breite: 10 } } },
      feldobjekte: { radius_in: { cup: 2.0, pin: 2.0 }, roboter_anteil: 0.5 }, physik };
    const lf = { feld_in: 144,
      posen: [{ id: 'START_STACKER', x_in: -40, y_in: 0, h_deg: 0 }, { id: 'START_RUNNER', x_in: 40, y_in: 60, h_deg: 180 }],
      objekte_start: { cups: [{ id: 'CUP_X', x_in: -25, y_in: 0, lage: 'steht', ebene: 0 }, { id: 'CUP_FAR', x_in: 0, y_in: -50, lage: 'steht', ebene: 0 }],
                       pins: [{ id: 'PIN_LIEGT', x_in: 20, y_in: 20, lage: 'liegt', ebene: 0, variante: 'red_yellow', gelb_deg: 90 }] } };
    const volts = (n, t_ms) => (n === 'stacker' && t_ms < 1500 ? { left_V: 6, right_V: 6 } : { left_V: 0, right_V: 0 });
    const drive = (chunkMs) => {
      const live = FBLive.create(lp, lf, { voltsAt: volts });
      const states = [], topples = [];
      let prevPose = live.objects()[0].draw, maxJump = 0;
      for (let t = 0; t < 3000; t += chunkMs) {
        live.advance(chunkMs / 1000);
        live.takeEvents().forEach((e) => { if (/^TOPPLED:/.test(e.text)) topples.push(e.robot + ' ' + e.text); });
        const o = live.objects()[0];
        if (!states.length || states[states.length - 1] !== o.state) states.push(o.state);
        if (chunkMs === 1) { maxJump = Math.max(maxJump, Math.hypot(o.draw.bx_in - prevPose.bx_in, o.draw.by_in - prevPose.by_in, o.draw.z_in - prevPose.z_in)); prevPose = o.draw; }
      }
      return { live, states, topples, maxJump };
    };
    const one = drive(1);
    check('Livesim: Cup durchläuft standing → tipping → lying', one.states.join() === 'standing,tipping,lying', one.states.join());
    check('Livesim: Cup-Fußpunkt springt nie (< 0,5 in je ms)', one.maxJump < 0.5, one.maxJump);
    check('Livesim: ein TOPPLED:CUP_X, dem Roboter zugeordnet, der ihn berührt hat', one.topples.join() === 'stacker TOPPLED:CUP_X', one.topples.join());
    const cup = one.live.objects()[0];
    check('Livesim: Cup liegt vor dem Roboter, auf Höhe r', cup.state === 'lying' && cup.standing === false && cup.x_in > -25 && cup.draw.z_in === 2.0, JSON.stringify(cup));
    check('Livesim: fernes Objekt steht unberührt', one.live.objects()[1].state === 'standing' && one.live.objects()[1].draw.z_in === 0);
    check('Livesim: Pin am Boden liegt von Anfang an', one.live.objects()[2].state === 'lying' && one.live.objects()[2].standing === false);
    const sixteen = drive(16);
    check('Livesim: andere Bildrate, gleicher Zustand', JSON.stringify(sixteen.live.objects()) === JSON.stringify(one.live.objects()));
    one.live.reset();
    check('Livesim: Reset stellt Cup und Pin wieder auf', one.live.objects()[0].state === 'standing' && one.live.objects()[0].x_in === -25 && one.live.objects()[2].state === 'lying');
    const plain = FBLive.create(Object.assign({}, lp, { physik: undefined }), lf, { voltsAt: volts });
    check('Livesim ohne Block physik: Objekte ohne Physik-Felder', plain.objects()[0].draw === undefined && plain.objects()[0].state === undefined);
  }

  return { ok, fail, msgs };
}
if (typeof module !== 'undefined') module.exports = runPhysicsTests;
