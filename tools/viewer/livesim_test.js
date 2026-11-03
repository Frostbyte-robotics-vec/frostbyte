/* Tests für livesim.js. Läuft im Browser (index.html?selftest) und unter Node (test_livesim.py). Kein DOM, keine Ausgabe:
   liefert ein Ergebnisobjekt. Eigene Fixture-Parameter: die Tests hängen nicht an den Schätzwerten aus sim_params.json. */
function runLivesimTests(FBLive) {
  const msgs = [];
  let ok = 0, fail = 0;
  function check(name, cond, info) {
    if (cond) ok++; else { fail++; msgs.push('FEHLER ' + name + (info !== undefined ? ': ' + info : '')); }
  }
  const rel = (a, b, eps) => Math.abs(a - b) <= eps * Math.max(1, Math.abs(b));

  const drive = (spur) => ({ kS_V: 0.5, kV_V_per_in_s: 0.2, kA_V_per_in_s2: 0.05, spurbreite_in: spur, mu: 0.9,
    motor_max_V: 10.0 });
  const params = {
    welt: { schritt_ms: 1, steuer_ms: 10, g_in_s2: 386.1 },
    live: { langsam_faktor: 0.4, ersatzpose_in: { stacker: { x_in: -30, y_in: 10, h_deg: 90 }, runner: { x_in: 30, y_in: -10, h_deg: 0 } } },
    roboter: { stacker: { antrieb: drive(14), akku: { spannung_V: 12.8 } }, runner: { antrieb: drive(11), akku: { spannung_V: 8.0 } } }
  };
  const press = (keys) => Object.assign({ w: false, a: false, s: false, d: false, shift: false }, keys);
  const run = (live, seconds, chunkMs) => { for (let t = 0; t < seconds * 1000; t += chunkMs) live.advance(chunkMs / 1000); };

  // 1 Tasten → Spannungen (eine Funktion)
  let v = FBLive.keysToVolts(press({ w: true }), 10, 0.4);
  check('vor: beide Seiten +Max', v.left_V === 10 && v.right_V === 10, JSON.stringify(v));
  v = FBLive.keysToVolts(press({ s: true }), 10, 0.4);
  check('zurück: beide Seiten −Max', v.left_V === -10 && v.right_V === -10);
  v = FBLive.keysToVolts(press({ a: true }), 10, 0.4);
  check('A dreht gegen den Uhrzeigersinn (rechts schneller)', v.left_V === -10 && v.right_V === 10);
  v = FBLive.keysToVolts(press({ d: true }), 10, 0.4);
  check('D dreht im Uhrzeigersinn (links schneller)', v.left_V === 10 && v.right_V === -10);
  v = FBLive.keysToVolts(press({ w: true, shift: true }), 10, 0.4);
  check('Langsam-Modus skaliert', rel(v.left_V, 4, 1e-12) && rel(v.right_V, 4, 1e-12), JSON.stringify(v));
  v = FBLive.keysToVolts(press({ w: true, s: true }), 10, 0.4);
  check('W+S hebt sich auf', v.left_V === 0 && v.right_V === 0);
  v = FBLive.keysToVolts(press({ w: true, d: true }), 10, 0.4);
  check('W+D übersteigt nie das Maximum', Math.abs(v.left_V) <= 10 && Math.abs(v.right_V) <= 10 && v.left_V > v.right_V, JSON.stringify(v));
  v = FBLive.keysToVolts(press({}), 10, 0.4);
  check('keine Taste: 0 V', v.left_V === 0 && v.right_V === 0);

  // 2 Geradeausfahrt erreicht die stationäre Geschwindigkeit (V − kS)/kV; Akku (8 V) begrenzt den Runner
  let live = FBLive.create(params);
  live.setKeys(press({ w: true }));
  run(live, 5, 10);
  const d = params.roboter.stacker.antrieb;
  let vss = (Math.min(d.motor_max_V, 12.8) - d.kS_V) / d.kV_V_per_in_s;
  check('stationäre Geschwindigkeit Stacker', rel(live.speed('stacker'), vss, 0.01), live.speed('stacker') + ' vs ' + vss);
  const p = live.pose('stacker');
  check('Fahrt entlang des Headings (90° → y wächst, x bleibt)', p.y_in > 10 + 10 && Math.abs(p.x_in - (-30)) < 1e-6, JSON.stringify(p));
  live.toggle();
  live.setKeys(press({ w: true }));
  run(live, 5, 10);
  vss = (Math.min(d.motor_max_V, 8.0) - d.kS_V) / d.kV_V_per_in_s;
  check('Akkuspannung begrenzt Runner', rel(live.speed('runner'), vss, 0.01), live.speed('runner') + ' vs ' + vss);
  live = FBLive.create(params);
  live.setKeys(press({ w: true, shift: true }));
  run(live, 5, 10);
  vss = (0.4 * d.motor_max_V - d.kS_V) / d.kV_V_per_in_s;
  check('Langsam-Modus: stationäre Geschwindigkeit', rel(live.speed('stacker'), vss, 0.01), live.speed('stacker') + ' vs ' + vss);

  // 3 Drehung auf der Stelle ändert x/y nicht
  live = FBLive.create(params);
  const p0 = live.pose('stacker');
  live.setKeys(press({ a: true }));
  run(live, 0.15, 10);
  const p1 = live.pose('stacker');
  check('Drehung ändert x/y nicht', Math.abs(p1.x_in - p0.x_in) < 1e-9 && Math.abs(p1.y_in - p0.y_in) < 1e-9, JSON.stringify(p1));
  const hA = FBLive.wrap(p1.h_deg - p0.h_deg);
  check('A dreht das Heading hoch (gegen Uhrzeigersinn)', hA > 10, hA);
  live.setKeys(press({ d: true }));
  run(live, 0.5, 10);
  const h1 = live.pose('stacker').h_deg;
  run(live, 0.02, 10);
  check('D dreht im Uhrzeigersinn (Heading sinkt)', FBLive.wrap(live.pose('stacker').h_deg - h1) < 0);

  // 4 Determinismus und Unabhängigkeit von der Bildrate
  const script = [[1.0, { w: true }], [0.7, { w: true, d: true }], [0.5, {}], [1.2, { s: true, a: true, shift: true }]];
  const replay = (chunkMs) => {
    const l = FBLive.create(params);
    script.forEach(([sec, keys]) => { l.setKeys(press(keys)); run(l, sec, chunkMs); });
    return l.pose('stacker');
  };
  const a = replay(10), b = replay(10), c = replay(20), e = replay(5);
  check('gleicher Tastenverlauf = gleiche Pose', a.x_in === b.x_in && a.y_in === b.y_in && a.h_deg === b.h_deg);
  check('Bildrate ändert die Pose nicht (20 ms / 5 ms / 10 ms)',
    a.x_in === c.x_in && a.y_in === c.y_in && a.x_in === e.x_in && a.h_deg === e.h_deg, JSON.stringify([a, c, e]));

  // 5 Feste Schritte: Takt hängt nicht an der Bildrate, Rest bleibt im Akkumulator, Hintergrund-Pause
  live = FBLive.create(params);
  let steps = live.advance(0.0005);
  check('unter einem Schritt: nichts, Rest bleibt', steps === 0);
  steps = live.advance(0.0006);
  check('Rest wird nachgeholt (1 Schritt)', steps === 1, steps);
  steps = live.advance(0.016);
  check('16 ms = 16 Schritte', steps === 16, steps);
  steps = live.advance(5);
  check('Riesen-Bild (Tab war weg) wird gekappt', steps > 0 && steps <= 200, steps);
  live.setKeys(press({ w: true }));
  live.pause(true);
  const before = JSON.stringify(live.pose('stacker'));
  steps = live.advance(0.5);
  check('Pause: kein Schritt, Pose unverändert', steps === 0 && JSON.stringify(live.pose('stacker')) === before);
  live.pause(false);
  steps = live.advance(0.01);
  check('nach der Pause weiter, ohne Nachholen', steps === 10, steps);

  // 6 Steuertakt: Spannung wird nur alle steuer_ms neu gesetzt
  live = FBLive.create(params);
  live.advance(0.001);
  live.setKeys(press({ w: true }));
  live.advance(0.004);
  check('Taste wirkt erst am nächsten Steuertakt', live.speed('stacker') === 0, live.speed('stacker'));
  live.advance(0.02);
  check('danach fährt der Roboter', live.speed('stacker') > 0);

  // 7 Tab-Wechsel und Reset
  live = FBLive.create(params);
  check('Start mit Stacker', live.active() === 'stacker');
  live.setKeys(press({ w: true }));
  run(live, 1, 10);
  live.toggle();
  check('Tab wechselt zum Runner', live.active() === 'runner');
  run(live, 3, 10);
  check('Stacker steht still, Taste wirkt jetzt auf Runner', live.speed('stacker') === 0 && live.speed('runner') > 0, live.speed('stacker') + ' / ' + live.speed('runner'));
  live.setKeys(press({}));
  run(live, 2, 10);
  live.reset();
  const ps = live.pose('stacker'), pr = live.pose('runner');
  check('R: beide auf Startpose', ps.x_in === -30 && ps.y_in === 10 && ps.h_deg === 90 && pr.x_in === 30 && pr.y_in === -10 && pr.h_deg === 0, JSON.stringify([ps, pr]));
  check('R: Tempo null', live.speed('stacker') === 0 && live.speed('runner') === 0);

  // 8 Startposen aus field.json; null → Ersatzpose mit Hinweis
  const field = { posen: [{ id: 'START_STACKER', x_in: 5, y_in: 6, h_deg: 45 }, { id: 'START_RUNNER', x_in: null, y_in: null, h_deg: null }] };
  live = FBLive.create(params, field);
  const q1 = live.pose('stacker'), q2 = live.pose('runner');
  check('Startpose aus field.json', q1.x_in === 5 && q1.y_in === 6 && q1.h_deg === 45);
  check('null-Pose → Ersatzpose', q2.x_in === 30 && q2.y_in === -10 && q2.h_deg === 0);
  check('Hinweis nur für die Ersatzpose', live.notes.length === 1 && /START_RUNNER/.test(live.notes[0]), live.notes.join('|'));
  live = FBLive.create(params);
  check('ohne field.json: zwei Hinweise', live.notes.length === 2);

  // 9 Fehlende Parameter melden sich laut
  let threw = false;
  try { FBLive.create({ welt: params.welt, live: params.live, roboter: { stacker: { antrieb: {}, akku: {} }, runner: {} } }); } catch (err) { threw = true; }
  check('unvollständige Parameter → Fehler', threw);

  return { ok, fail, msgs };
}
if (typeof module !== 'undefined') module.exports = runLivesimTests;
