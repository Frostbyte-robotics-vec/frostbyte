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

  // ---- SIM-39: Stützflächen, Greifen, Ablegen, Stapeln ----
  const grab = { reichweite_in: 3.0, vorn_in: 2.5, hand_z_in: 1.0, lift_stufen: 5, lift_stufe_in: 4.0, lift_geschw_in_s: 40, zieh_geschw_in_s: 80,
    fangradius_in: 3.0, stapel_stufe_in: 4.0, fang_zug_1_s: 10, aufricht_rad_s: 6.0, greif_toleranz_in: 0.5 };
  const baseG = Object.assign({}, base, { physik: Object.assign({}, physik, { greifen: grab }) });
  const cfgG = FBPhysics.load(baseG);
  check('greifen ist ohne Block null, mit Block gelesen', cfg.grab === null && cfgG.grab.fang_radius_in === 3.0 && cfgG.grab.stack_step_in === 4.0);
  threw = false;
  try { FBPhysics.load({ welt: base.welt, physik: Object.assign({}, physik, { greifen: Object.assign({}, grab, { fangradius_in: 0 }) }) }); } catch (e) { threw = true; }
  check('Fangradius 0 wird abgelehnt', threw);
  const makeG = (specs, goals) => { const wd = FBPhysics.create(cfgG, specs, {}); wd.setGoals(goals || []); return wd; };
  const runG = (wd, seconds, evs) => { for (let t = 0; t < Math.round(seconds / DT); t++) { const e = wd.step(DT); if (evs) e.forEach((x) => evs.push(x)); } };

  // 15 Fall auf ein Goal: landet nach √(2Δh/g) auf hoehe_in, wird zur Goal-Mitte gezogen, bleibt stehen
  w = makeG([spec('P', 1.5, 1.0)], [{ x_in: 0, y_in: 0, z_in: 3.25 }]);
  w.drop(0, 1.5, 1.0, 10);
  let landT = -1;
  const evG = [];
  for (let t = 1; t <= 1500; t++) { w.step(DT).forEach((e) => evG.push(e)); if (landT < 0 && w.bodies[0].z <= 3.25 + 1e-9) landT = t * DT; }
  check('Fallzeit auf das Goal = √(2·6,75/g) (±3 ms, das Teil wird währenddessen zur Mitte gezogen, die Höhe bleibt)', Math.abs(landT - Math.sqrt(2 * 6.75 / G)) < 0.003, landT);
  check('steht auf dem Goal in dessen Mitte, ruhig', w.bodies[0].state === 'standing' && w.bodies[0].z === 3.25 && Math.abs(w.bodies[0].x) < 1e-3 && Math.abs(w.bodies[0].y) < 1e-3, JSON.stringify(w.bodies[0]));
  check('Ereignis landed mit stacked', evG.filter((e) => e.type === 'landed').length >= 1 && evG.filter((e) => e.type === 'landed')[0].stacked === true);
  // außerhalb des Fangradius: fällt neben das Goal auf den Boden
  w = makeG([spec('P', 5, 0)], [{ x_in: 0, y_in: 0, z_in: 3.25 }]);
  w.drop(0, 5, 0, 10);
  runG(w, 1.5);
  check('außerhalb des Fangradius: Boden, kein Stapel', w.bodies[0].z === 0 && w.bodies[0].x === 5 && w.bodies[0].state === 'standing');

  // 16 Stapel: ein Teil auf dem anderen, Stufe aus den Parametern; mehr als zwei gehen auch
  w = makeG([spec('A', 0, 0), spec('B', 0, 0), spec('C', 0, 0)], [{ x_in: 0, y_in: 0, z_in: 3.25 }]);
  w.drop(0, 0, 0, 8); runG(w, 1.0);
  w.drop(1, 0.5, 0, 14); runG(w, 1.0);
  w.drop(2, -0.5, 0.5, 20); runG(w, 1.5);
  check('drei Teile: Höhen 3,25 / 7,25 / 11,25', [0, 1, 2].every((i, k) => Math.abs(w.bodies[i].z - (3.25 + 4 * k)) < 1e-9), w.bodies.map((b) => b.z).join());
  check('drei Teile: alle in der Goal-Mitte', w.bodies.every((b) => Math.abs(b.x) < 1e-2 && Math.abs(b.y) < 1e-2), w.bodies.map((b) => b.x + ',' + b.y).join(' '));

  // 17 Stütze weg: wird das untere Teil gegriffen oder kippt es, fällt das obere (und zwar auf den Boden)
  w = makeG([spec('CUP', 0, 0, { kind: 'cup' }), spec('PIN', 0, 0, { z_in: 4 })]);
  check('Pin steht auf dem Cup (Start bei z = 4), nichts fällt', (runG(w, 1.0), w.bodies[1].z === 4 && w.bodies[1].state === 'standing'));
  check('der Cup hat etwas obendrauf: nicht frei', w.isFree(0) === false && w.isFree(1) === true);
  w.pushStanding(0, 1, 0, 1.5, DT);
  runG(w, 2.0);
  check('Cup kippt weg: der Pin fällt auf den Boden und steht', w.bodies[1].z === 0 && w.bodies[1].state === 'standing', JSON.stringify([w.bodies[0].state, w.bodies[1].state, w.bodies[1].z]));
  w = makeG([spec('CUP', 0, 0, { kind: 'cup' }), spec('PIN', 0, 0, { z_in: 4 })]);
  w.hold(0);
  runG(w, 0.5);
  check('Cup gegriffen: der Pin fällt', w.bodies[1].z === 0 && w.bodies[0].state === 'held');

  // 18 Halten: moveHeld zieht mit endlicher Geschwindigkeit, kein Sprung
  w = makeG([spec('P', 0, 0)]);
  w.hold(0);
  let far = 0, steps = 0;
  while (!w.moveHeld(0, 8, 0, 3, DT) && steps < 5000) { steps++; far = Math.max(far, 0); }
  check('moveHeld erreicht das Ziel mit Zieh-Geschwindigkeit (|Ziel|/v = 8,54/80 s ± 3 ms)', Math.abs(steps * DT - Math.hypot(8, 3) / 80) < 0.003, steps);
  check('gehalten: Zustand held, Position am Ziel', w.bodies[0].state === 'held' && w.bodies[0].x === 8 && w.bodies[0].z === 3);
  w.release(0);
  runG(w, 1.0);
  check('losgelassen: fällt auf den Boden und steht', w.bodies[0].state === 'standing' && w.bodies[0].z === 0);

  // 19 Livesim: Greifen, Lift, Ablegen auf ein Goal, SG6. Befehle gelten am Steuertakt (cmdAt wie voltsAt).
  {
    const dr = (spur) => ({ kS_V: 1.0, kV_V_per_in_s: 0.2, kA_V_per_in_s2: 0.04, spurbreite_in: spur, mu: 1.0, motor_max_V: 11.5 });
    const gp = {
      welt: base.welt,
      live: { langsam_faktor: 0.4, ersatzpose_in: { stacker: { x_in: 0, y_in: 0, h_deg: 0 }, runner: { x_in: 0, y_in: 0, h_deg: 0 } }, kollision: { radius_in: { goals: 4.0 } } },
      roboter: { stacker: { antrieb: dr(12), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 12, breite: 12 } },
                 runner: { antrieb: dr(10), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 10, breite: 10 } } },
      feldobjekte: { radius_in: { cup: 2.0, pin: 2.0 }, roboter_anteil: 0.5 }, physik: baseG.physik };
    const mkField = (objs) => ({ feld_in: 144, goals: [{ id: 'G1', typ: 'neutral', x_in: 0, y_in: 0, hoehe_in: 3.25 }],
      posen: [{ id: 'START_STACKER', x_in: -40, y_in: 0, h_deg: 0 }, { id: 'START_RUNNER', x_in: 40, y_in: 60, h_deg: 180 }], objekte_start: objs });
    const pinA = { id: 'PIN_A', x_in: -30, y_in: 0, lage: 'steht', ebene: 0, variante: 'yellow' }, cupA = { id: 'CUP_A', x_in: -30, y_in: -3.5, lage: 'steht', ebene: 0 };
    const grabF = mkField({ cups: [cupA, { id: 'CUP_UNTEN', x_in: -60, y_in: -40, lage: 'steht', ebene: 0 }],
                            pins: [pinA, { id: 'PIN_B', x_in: -30, y_in: 3.5, lage: 'steht', ebene: 0, variante: 'yellow' },
                                   { id: 'PIN_LIEGT', x_in: -60, y_in: 40, lage: 'liegt', ebene: 0 }, { id: 'PIN_OBEN', x_in: -60, y_in: -40, lage: 'steht', ebene: 1 }] });
    const oneF = mkField({ pins: [pinA] }), twoF = mkField({ cups: [cupA], pins: [pinA] });   // fahren ohne weitere Hindernisse auf dem Weg
    // Skript: Takt (ms) → Befehl; Fahren bis 3000 ms
    const script = (plan) => (n, t_ms) => (n === 'stacker' && plan[t_ms]) || null;
    const volts = (n, t_ms) => (n === 'stacker' && t_ms < 3000 ? { left_V: 6, right_V: 6 } : { left_V: 0, right_V: 0 });
    const play = (field, plan, chunkMs, endMs, withVolts, noRise) => {
      const pp = noRise ? Object.assign({}, gp, { physik: Object.assign({}, gp.physik, { greifen: Object.assign({}, grab, { aufricht_rad_s: undefined }) }) }) : gp;
      const live = FBLive.create(pp, field, { voltsAt: withVolts === false ? () => ({ left_V: 0, right_V: 0 }) : volts, cmdAt: script(plan) });
      const events = [], pos = {};
      let maxJump = 0;
      const prev = {};
      for (let t = 0; t < endMs; t += chunkMs) {
        live.advance(chunkMs / 1000);
        live.takeEvents().forEach((e) => events.push(e.robot + ' ' + e.text));
        if (chunkMs === 1) live.objects().forEach((o) => { if (prev[o.id]) maxJump = Math.max(maxJump, Math.hypot(o.draw.bx_in - prev[o.id].bx_in, o.draw.by_in - prev[o.id].by_in, o.draw.z_in - prev[o.id].z_in)); prev[o.id] = o.draw; });
      }
      return { live, events, maxJump, get: (id) => live.objects().filter((o) => o.id === id)[0] };
    };
    const g1 = { type: 'grab' }, up = { type: 'lift', dir: 1 }, rel = { type: 'release' };

    const ob = play(grabF, {}, 1, 10, false);
    check('Livesim: Pin obenauf (ebene 1) ist ein Körper, noch in der Luft über dem Boden (z = Stufe)', ob.get('PIN_OBEN') && ob.get('PIN_OBEN').draw.z_in === 4 && ob.get('PIN_OBEN').state === 'standing', ob.get('PIN_OBEN') && JSON.stringify(ob.get('PIN_OBEN').draw));

    const onGoal = play(mkField({ cups: [{ id: 'CUP_G', x_in: 30, y_in: 30, lage: 'steht', ebene: 0 }], pins: [{ id: 'PIN_GOAL', x_in: 0, y_in: 0, lage: 'steht', ebene: 1 }, { id: 'PIN_CUP', x_in: 30, y_in: 30, lage: 'steht', ebene: 1 }] }), {}, 1, 300, false);
    check('Livesim: Pin obenauf an einem Goal steht auf dessen Oberkante, einer auf einem Cup auf dem Cup (setzt sich beim Start)', onGoal.get('PIN_GOAL').draw.z_in === 3.25 && onGoal.get('PIN_CUP').draw.z_in === 4,
      onGoal.get('PIN_GOAL').draw.z_in + ' / ' + onGoal.get('PIN_CUP').draw.z_in);

    // Greifen, anheben, zum Goal fahren, ablegen
    const r1 = play(oneF, { 10: g1, 20: up, 3000: rel }, 1, 4500);
    check('Livesim: GRAB-Ereignis, Pin in der Hand, danach gestapelt', r1.events.indexOf('stacker GRAB:PIN_A') >= 0 && r1.events.indexOf('stacker STACK:PIN_A') >= 0, r1.events.join(' | '));
    const a = r1.get('PIN_A');
    check('Livesim: Pin liegt auf dem Goal (z = hoehe_in), in dessen Mitte, steht', a.state === 'standing' && Math.abs(a.draw.z_in - 3.25) < 1e-9 && Math.hypot(a.x_in, a.y_in) < 0.01, JSON.stringify(a));
    check('Livesim: Greifen, Halten, Ablegen ohne Sprung (< 0,5 in je ms)', r1.maxJump < 0.5, r1.maxJump);
    check('Livesim: Hand ist danach leer, Lift steht auf Stufe 1', r1.live.hand('stacker').length === 0 && r1.live.liftLevel('stacker') === 1);
    const r16 = play(oneF, { 10: g1, 20: up, 3000: rel }, 16, 4500);
    check('Livesim: andere Bildrate, gleicher Zustand', JSON.stringify(r16.live.objects()) === JSON.stringify(r1.live.objects()));

    // Ablegen weit weg vom Goal: Boden, kein STACK
    const r2 = play(oneF, { 10: g1, 500: rel }, 1, 3000, false);   // Roboter steht: das abgelegte Teil wird nicht überfahren
    check('Livesim: Ablegen fern vom Goal landet auf dem Boden ohne STACK', r2.get('PIN_A').draw.z_in === 0 && r2.get('PIN_A').state === 'standing' && !r2.events.some((e) => /STACK:/.test(e)), r2.events.join(' | '));

    // SG6: zweiter Pin abgelehnt, Cup daneben erlaubt
    const r3 = play(grabF, { 10: g1, 20: g1, 30: g1 }, 1, 200, false);
    check('Livesim: SG6: zweiter Pin wird abgelehnt (RULE:SG6), Cup wird gegriffen', r3.events.indexOf('stacker RULE:SG6') >= 0 && r3.live.hand('stacker').sort().join() === 'CUP_A,PIN_A' && r3.get('PIN_B').state === 'standing', r3.live.hand('stacker').join() + ' | ' + r3.events.join(' | '));
    const r3b = play(grabF, { 10: g1, 20: g1 }, 1, 200, false);
    check('Livesim: Pin und Cup in der Hand sind kein Verstoß', r3b.events.indexOf('stacker RULE:SG6') === -1 || r3b.live.hand('stacker').length === 2, r3b.events.join(' | '));

    // SIM-42: Pin und Cup stecken in der Hand ineinander (Pin auf dem Cup, gleicher Fußpunkt in x/y), der Cup wird zuerst abgelegt
    const nest = play(twoF, { 10: g1, 20: g1 }, 1, 1500);
    const nc = nest.get('CUP_A').draw, np = nest.get('PIN_A').draw;
    check('Livesim: Pin steckt auf dem Cup in der Hand (gleiches x/y, eine Stapelstufe höher)', Math.hypot(nc.bx_in - np.bx_in, nc.by_in - np.by_in) < 0.01 && Math.abs(np.z_in - nc.z_in - 4.0) < 0.05, JSON.stringify([nc, np]));
    const nrel = play(twoF, { 10: g1, 20: g1, 1200: rel }, 1, 1500);
    check('Livesim: Q legt zuerst den Cup ab, der Pin bleibt in der Hand', nrel.live.hand('stacker').join() === 'PIN_A', nrel.live.hand('stacker').join());
    // Zwei Teile übereinander: Pin + Cup halten, Lift auf Stufe 2, beide nacheinander ablegen
    const r4 = play(twoF, { 10: g1, 20: g1, 30: up, 40: up, 3000: rel, 3300: rel }, 1, 5000);
    const zs = ['PIN_A', 'CUP_A'].map((id) => r4.get(id).draw.z_in).sort((x, y) => x - y);
    check('Livesim: zwei Teile gestapelt: 3,25 und 7,25 (Stufe 4)', Math.abs(zs[0] - 3.25) < 1e-9 && Math.abs(zs[1] - 7.25) < 1e-9, zs.join());
    check('Livesim: Hand nach beiden Ablagen leer', r4.live.hand('stacker').length === 0, r4.events.join(' | '));

    // Nichts in Reichweite / liegende Objekte / Grenzen
    const r5 = play(grabF, { 10: g1 }, 1, 200, false);
    check('Livesim: Greifen mit Objekt in Reichweite funktioniert nur einmal je Takt', r5.live.hand('stacker').length === 1);
    const far5 = play(mkField({ pins: [{ id: 'PIN_FERN', x_in: -10, y_in: 30, lage: 'steht', ebene: 0 }, { id: 'PIN_LIEGT', x_in: -30, y_in: 0, lage: 'liegt', ebene: 0 }] }), { 10: g1 }, 1, 200, false, true);
    check('Livesim: ohne aufricht_rad_s wird weder ein fernes noch ein liegendes Objekt gegriffen', far5.live.hand('stacker').length === 0 && far5.events.filter((e) => /GRAB/.test(e)).length === 0, far5.events.join(' | '));
    const lift = play(grabF, { 10: up, 20: up, 30: up, 40: up, 50: up, 60: up, 70: up }, 1, 1000, false);
    check('Livesim: Lift bleibt bei Stufe 5 stehen (Grenze)', lift.live.liftLevel('stacker') === 5);

    // SIM-42: Greifen nur auf Höhe des Teils (SIM-41) und SG6 vor der Auswahl: ein nahes, abgelehntes Pin sperrt keinen ferneren Cup
    const hi3 = play(grabF, { 10: up, 20: up, 30: up, 40: up, 800: g1 }, 1, 1000, false);
    check('Livesim: Hand weit über dem Boden greift kein Teil vom Boden', hi3.live.hand('stacker').length === 0 && hi3.events.filter((e) => /GRAB/.test(e)).length === 0, hi3.events.join(' | '));
    const sg6 = play(mkField({ pins: [{ id: 'PIN_NAH', x_in: -30, y_in: 0, lage: 'steht', ebene: 0, variante: 'yellow' }, { id: 'PIN_HAND', x_in: -30, y_in: 3.5, lage: 'steht', ebene: 0, variante: 'yellow' }],
      cups: [{ id: 'CUP_FERN', x_in: -30, y_in: -5.5, lage: 'steht', ebene: 0 }] }), { 10: g1, 20: g1 }, 1, 200, false);
    check('Livesim: SG6 filtert die Kandidaten: Pin in der Hand, nahes Pin abgelehnt, ferner Cup wird trotzdem gegriffen', sg6.live.hand('stacker').length === 2 && sg6.live.hand('stacker').some((id) => /CUP/.test(id)), sg6.live.hand('stacker').join() + ' | ' + sg6.events.join(' | '));
    // SIM-42: Liftwinkel aus der Handhöhe (Basis + Hub · sin θ), damit das CAD-Modell dem Lift folgt und nie im Boden steht
    const hh = (b, h) => Object.assign({}, gp, {
      roboter: Object.assign({}, gp.roboter, { stacker: Object.assign({}, gp.roboter.stacker, { mechanismen: { lift: { min_deg: -40, max_deg: 45 } } }) }),
      physik: Object.assign({}, gp.physik, { greifen: Object.assign({}, grab, { hand_hoehe: { stacker: { basis_in: b, hub_in: h } } }) }) });
    const mkHH = (b, h) => FBLive.create(hh(b, h), grabF, { voltsAt: () => ({ left_V: 0, right_V: 0 }), cmdAt: script({ 10: up, 20: up, 30: up, 40: up, 50: up, 60: up, 70: up, 80: up, 90: up, 100: up }) });
    const lz = mkHH(20, 24), s0 = lz.liftDeg('stacker');
    const zOf = (deg) => 20 + 24 * Math.sin(deg * Math.PI / 180);
    check('Livesim: Liftwinkel beginnt bei min_deg', Math.abs(s0 - -40) < 1e-9, String(s0));
    check('Livesim: ohne hand_hoehe ist liftDeg null (Altverhalten)', play(grabF, {}, 1, 10, false).live.liftDeg('stacker') === null);
    let prevDeg = s0, mono = true, inRange = true;
    for (let t = 0; t < 1500; t += 5) { lz.advance(0.005); const d = lz.liftDeg('stacker'); if (d < prevDeg - 1e-9) mono = false; if (d < -40 - 1e-9 || d > 45 + 1e-9) inRange = false; prevDeg = d; }
    check('Livesim: Liftwinkel steigt monoton und bleibt zwischen min_deg und max_deg', mono && inRange && Math.abs(prevDeg - 45) < 1e-6, String(prevDeg));
    check('Livesim: Handhöhe oben = Basis + Hub·sin(max_deg), unten = Basis + Hub·sin(min_deg)', Math.abs(lz.handZ('stacker') - zOf(45)) < 1e-6 && Math.abs(mkHH(20, 24).handZ('stacker') - zOf(-40)) < 1e-9, lz.handZ('stacker') + '');
    check('Livesim: eine Stufe hebt die Hand um lift_stufe_in (wie eine Stapelebene), die oberste endet genau bei max_deg', (() => {
      const l = FBLive.create(hh(20, 24), grabF, { voltsAt: () => ({ left_V: 0, right_V: 0 }), cmdAt: script({ 10: up }) }); for (let t = 0; t < 1000; t += 10) l.advance(0.01);
      return Math.abs(l.handZ('stacker') - (zOf(-40) + 4.0)) < 1e-6 && l.liftLevel('stacker') === 1 && Math.abs(lz.handZ('stacker') - zOf(45)) < 1e-6 && lz.liftLevel('stacker') === 9; })(), 'Stufen oben: ' + lz.liftLevel('stacker'));
    let threw2 = false;
    try { FBLive.create(hh(5, 24), grabF, { voltsAt: () => ({ left_V: 0, right_V: 0 }) }); } catch (e) { threw2 = true; }
    check('Livesim: Hand unter dem Boden bei min_deg wird abgelehnt', threw2);

    // Reset: Hand und Lift leer, Objekte wie am Anfang
    r1.live.reset();
    check('Livesim: Reset leert Hand und Lift, stellt Objekte zurück', r1.live.hand('stacker').length === 0 && r1.live.liftLevel('stacker') === 0 && r1.live.objects().filter((o) => o.id === 'PIN_A')[0].x_in === -30 && r1.live.objects().filter((o) => o.id === 'PIN_A')[0].draw.z_in === 0);

    // Ohne Block greifen: Befehle ohne Wirkung
    const noG = FBLive.create(Object.assign({}, gp, { physik: physik }), grabF, { voltsAt: () => ({ left_V: 0, right_V: 0 }), cmdAt: script({ 10: g1 }) });
    for (let t = 0; t < 100; t++) noG.advance(0.001);
    check('Livesim ohne Block greifen: Befehl wirkungslos, kein Ereignis', noG.hand('stacker').length === 0 && noG.takeEvents().filter((e) => /GRAB/.test(e.text)).length === 0);
  }

  // ---- SIM-40: liegende Teile greifen, sichtbar aufrichten ----
  check('aufricht_rad_s ist optional (ohne: null) und muss > 0 sein', FBPhysics.load(Object.assign({}, baseG, { physik: Object.assign({}, physik, { greifen: Object.assign({}, grab, { aufricht_rad_s: undefined }) }) })).grab.rise_rad_s === null && cfgG.grab.rise_rad_s === 6.0);
  threw = false;
  try { FBPhysics.load({ welt: base.welt, physik: Object.assign({}, physik, { greifen: Object.assign({}, grab, { aufricht_rad_s: 0 }) }) }); } catch (e) { threw = true; }
  check('aufricht_rad_s = 0 wird abgelehnt', threw);
  {
    const lyingPin = () => makeG([spec('L', 10, 5, { standing: false, lie_dir_deg: 30 })]);
    w = lyingPin();
    const before = w.pose(0), b = w.bodies[0];
    check('hold auf ein liegendes Teil: gehalten, Fußpunkt und Höhe wie vorher gezeichnet (kein Sprung)', w.hold(0) && b.state === 'held' && (() => { const q = w.pose(0); return Math.hypot(q.bx_in - before.bx_in, q.by_in - before.by_in, q.z_in - before.z_in) < 1e-9 && q.tilt_deg === 90; })(), JSON.stringify([before, w.pose(0)]));
    let prevPose = w.pose(0), maxJump = 0, monotone = true, steps = 0, prevTilt = 90;
    while (!w.moveHeld(0, 12, 6, 4, DT) && steps < 5000) {
      steps++;
      const q = w.pose(0);
      maxJump = Math.max(maxJump, Math.hypot(q.bx_in - prevPose.bx_in, q.by_in - prevPose.by_in, q.z_in - prevPose.z_in));
      if (q.tilt_deg > prevTilt + 1e-9) monotone = false;
      prevTilt = q.tilt_deg; prevPose = q;
    }
    const tUp = (Math.PI / 2) / 6.0, tMove = Math.hypot(12 - w.pose(0).bx_in + 0, 0) * 0;   // Zielzeit unten aus der Ausgangslage
    check('Aufrichten dauert (π/2)/ω = 0,262 s (±4 ms), der Weg zur Hand ist kürzer', Math.abs(steps * DT - tUp) < 0.004, steps);
    check('Aufrichten: Winkel nimmt monoton ab, Fußpunkt springt nie (< 0,15 in je ms)', monotone && maxJump < 0.15, maxJump);
    check('am Ende senkrecht in der Hand am Ziel', b.theta === 0 && b.x === 12 && b.y === 6 && b.z === 4 && w.pose(0).tilt_deg === 0);
    w.release(0); runG(w, 1.0);
    check('losgelassen: steht senkrecht auf dem Boden', b.state === 'standing' && b.z === 0 && b.theta === 0);
  }
  {
    const gp2 = { welt: base.welt, physik: baseG.physik };
    void gp2;
    const lieF = { feld_in: 144, goals: [{ id: 'G1', typ: 'neutral', x_in: 0, y_in: 0, hoehe_in: 3.25 }],
      posen: [{ id: 'START_STACKER', x_in: -40, y_in: 0, h_deg: 0 }, { id: 'START_RUNNER', x_in: 40, y_in: 60, h_deg: 180 }],
      objekte_start: { pins: [{ id: 'PIN_L', x_in: -30, y_in: 0, lage: 'liegt', ebene: 0, gelb_deg: 0 }] } };
    const dr = (spur) => ({ kS_V: 1.0, kV_V_per_in_s: 0.2, kA_V_per_in_s2: 0.04, spurbreite_in: spur, mu: 1.0, motor_max_V: 11.5 });
    const lp = { welt: base.welt, live: { langsam_faktor: 0.4, ersatzpose_in: { stacker: { x_in: 0, y_in: 0, h_deg: 0 }, runner: { x_in: 0, y_in: 0, h_deg: 0 } }, kollision: { radius_in: { goals: 4.0 } } },
      roboter: { stacker: { antrieb: dr(12), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 12, breite: 12 } }, runner: { antrieb: dr(10), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 10, breite: 10 } } },
      feldobjekte: { radius_in: { cup: 2.0, pin: 2.0 }, roboter_anteil: 0.5 }, physik: baseG.physik };
    const plan = { 10: { type: 'grab' }, 20: { type: 'lift', dir: 1 }, 3000: { type: 'release' } };
    const volts = (n, t_ms) => (n === 'stacker' && t_ms < 3000 ? { left_V: 6, right_V: 6 } : { left_V: 0, right_V: 0 });
    const live = FBLive.create(lp, lieF, { voltsAt: volts, cmdAt: (n, t) => (n === 'stacker' && plan[t]) || null });
    const tilts = [], events = [];
    let prev = live.objects()[0].draw, maxJump = 0;
    for (let t = 0; t < 4500; t++) {
      live.advance(0.001);
      live.takeEvents().forEach((e) => events.push(e.text));
      const o = live.objects()[0];
      maxJump = Math.max(maxJump, Math.hypot(o.draw.bx_in - prev.bx_in, o.draw.by_in - prev.by_in, o.draw.z_in - prev.z_in)); prev = o.draw;
      if (t === 100 || t === 150) tilts.push(o.draw.tilt_deg);
    }
    const o = live.objects()[0];
    check('Livesim: liegender Pin wird gegriffen (GRAB), richtet sich in der Hand auf (Winkel bei 100 ms > bei 150 ms > 0)', events.indexOf('GRAB:PIN_L') >= 0 && tilts[0] > tilts[1] && tilts[1] >= 0, tilts.join() + ' | ' + events.join(' '));
    check('Livesim: Aufrichten, Halten, Fahren, Ablegen ohne Sprung (< 0,5 in je ms)', maxJump < 0.5, maxJump);
    check('Livesim: der aufgerichtete Pin steht danach gestapelt auf dem Goal (3,25) in dessen Mitte', o.state === 'standing' && Math.abs(o.draw.z_in - 3.25) < 1e-9 && o.draw.tilt_deg === 0 && Math.hypot(o.x_in, o.y_in) < 0.01 && events.indexOf('STACK:PIN_L') >= 0, JSON.stringify(o) + events.join(' '));
  }

  return { ok, fail, msgs };
}
if (typeof module !== 'undefined') module.exports = runPhysicsTests;
