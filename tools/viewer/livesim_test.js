/* Tests für livesim.js. Läuft im Browser (index.html?selftest) und unter Node (test_livesim.py). Kein DOM, keine Ausgabe:
   liefert ein Ergebnisobjekt. Eigene Fixture-Parameter: die Tests hängen nicht an den Schätzwerten aus sim_params.json. */
function runLivesimTests(FBLive, FBLog) {
  const msgs = [];
  let ok = 0, fail = 0;
  function check(name, cond, info) {
    if (cond) ok++; else { fail++; msgs.push('FEHLER ' + name + (info !== undefined ? ': ' + info : '')); }
  }
  const rel = (a, b, eps) => Math.abs(a - b) <= eps * Math.max(1, Math.abs(b));

  const drive = (spur) => ({ kS_V: 0.5, kV_V_per_in_s: 0.2, kA_V_per_in_s2: 0.05, spurbreite_in: spur, mu: 0.9,
    motor_max_V: 10.0 });
  const outline = { laenge: 10, breite: 10 };
  const params = {
    welt: { schritt_ms: 1, steuer_ms: 10, log_ms: 20, g_in_s2: 386.1 },
    live: { langsam_faktor: 0.4, ersatzpose_in: { stacker: { x_in: -30, y_in: 10, h_deg: 90 }, runner: { x_in: 30, y_in: -10, h_deg: 0 } } },
    roboter: { stacker: { antrieb: drive(14), akku: { spannung_V: 12.8 }, umriss_in: outline }, runner: { antrieb: drive(11), akku: { spannung_V: 8.0 }, umriss_in: outline } }
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
  const field = { feld_in: 144, posen: [{ id: 'START_STACKER', x_in: 5, y_in: 6, h_deg: 45 }, { id: 'START_RUNNER', x_in: null, y_in: null, h_deg: null }] };
  live = FBLive.create(params, field);
  const q1 = live.pose('stacker'), q2 = live.pose('runner');
  check('Startpose aus field.json', q1.x_in === 5 && q1.y_in === 6 && q1.h_deg === 45);
  check('null-Pose → Ersatzpose', q2.x_in === 30 && q2.y_in === -10 && q2.h_deg === 0);
  check('Hinweis nur für die Ersatzpose', live.notes.length === 1 && /START_RUNNER/.test(live.notes[0]), live.notes.join('|'));
  live = FBLive.create(params);
  check('ohne field.json: zwei Hinweise zu Startposen', live.notes.filter(n => /START_/.test(n)).length === 2);


  // 10 Kollisionsgeometrie: dieselben Fälle wie test/test_sim_collision.cpp (SIM-05)
  const box = (x, y, h) => ({ x_in: x, y_in: y, length_in: 10, width_in: 10, h_deg: h });
  const pbb = FBLive.penetrationBoxBox, pbc = FBLive.penetrationBoxCircle, pif = FBLive.pushInsideField;
  const near = (a, b) => Math.abs(a - b) < 1e-6;
  check('Box/Box: getrennt', pbb(box(0, 0, 0), box(11, 0, 0)) === null && pbb(box(0, 0, 45), box(0, 20, 45)) === null);
  check('Box/Box: Berührung der Kanten = kein Kontakt', pbb(box(0, 0, 0), box(10, 0, 0)) === null);
  let m = pbb(box(0, 0, 0), box(8, 0, 0));
  check('Box/Box: Überlappung 2 Zoll, a geht nach −x', m && near(m.x, -2) && near(m.y, 0), JSON.stringify(m));
  check('Box/Box: 45°-Box nur bei echtem Kontakt', pbb(box(0, 0, 0), box(11.5, 0, 45)) !== null && pbb(box(0, 0, 0), box(12.5, 0, 45)) === null
    && pbb(box(0, 0, 0), box(11, 11, 45)) === null);
  const ab = pbb(box(0, 0, 10), box(7, 3, 30)), ba = pbb(box(7, 3, 30), box(0, 0, 10));
  check('Box/Box: Verschiebung symmetrisch', ab && ba && near(ab.x, -ba.x) && near(ab.y, -ba.y));
  m = pif(box(0, 0, 0), 144);
  check('Rand: Box innen → (0, 0)', m.x === 0 && m.y === 0);
  m = pif(box(70, 0, 0), 144);
  check('Rand: Box über die Wand → −3', near(m.x, -3) && near(m.y, 0), JSON.stringify(m));
  m = pif(box(66, 0, 45), 144);
  check('Rand: gedrehte Box zählt mit der Ecke', near(m.x, -(66 + 7.0710678 - 72)) || Math.abs(m.x + (66 + 7.0710678 - 72)) < 1e-6, m.x);
  m = pif(box(70, -70, 0), 144);
  check('Rand: Ecke → beide Achsen', near(m.x, -3) && near(m.y, 3));
  const circ = { x_in: 20, y_in: 0, r_in: 3 };
  m = pbc(box(0, 0, 0), circ);
  check('Kreis: kein Kontakt', m === null);
  m = pbc(box(14, 0, 0), circ);
  check('Kreis: 2 Zoll zurück', m && near(m.x, -2) && Math.abs(m.y) < 1e-9, JSON.stringify(m));
  check('Kreis: Ecke trifft, Kante nicht', pbc(box(0, 0, 0), { x_in: 8, y_in: 8, r_in: 3 }) === null && pbc(box(2, 2, 0), { x_in: 8, y_in: 8, r_in: 3 }) !== null);
  m = pbc(box(0, 0, 0), { x_in: 3, y_in: 0, r_in: 1 });
  check('Kreis: Mittelpunkt in der Box → über die nächste Seite', m && near(m.x, -3) && near(m.y, 0), JSON.stringify(m));

  // 11 Kollisionen im Live-Lauf: Pose bleibt im Feld, Ereignis nur beim Kontaktbeginn
  const cField = (extra) => Object.assign({ feld_in: 144, posen: [{ id: 'START_STACKER', x_in: 50, y_in: 0, h_deg: 0 }, { id: 'START_RUNNER', x_in: 0, y_in: -50, h_deg: 0 }] }, extra);
  live = FBLive.create(params, cField());
  live.setKeys(press({ w: true }));
  run(live, 3, 10);
  let ev = live.takeEvents();
  const ps2 = live.pose('stacker');
  check('Wand: Pose bleibt im Feld', ps2.x_in <= 72 - 5 + 1e-6 && ps2.x_in > 60, JSON.stringify(ps2));
  check('Wand: genau ein COLLISION:wall (nicht jeder Takt)', ev.filter(e => e.text === 'COLLISION:wall' && e.robot === 'stacker').length === 1, JSON.stringify(ev));
  check('Wand: BLOCKED nach > 0,5 s Fahreingabe, einmal', ev.filter(e => e.text === 'BLOCKED').length === 1 && ev.length === 2, JSON.stringify(ev));
  check('takeEvents leert die Liste', live.takeEvents().length === 0);
  live.setKeys(press({ s: true }));
  run(live, 1, 10);
  live.setKeys(press({ w: true }));
  run(live, 2, 10);
  ev = live.takeEvents();
  check('nach Lösen vom Rand und erneutem Anfahren: neuer Kontakt, neues Ereignis', ev.filter(e => e.text === 'COLLISION:wall').length === 1, JSON.stringify(ev));
  check('Rückwärts-Lösen erzeugt keine Ereignisse außer dem neuen Kontakt', ev.every(e => e.text === 'COLLISION:wall' || e.text === 'BLOCKED'), JSON.stringify(ev));

  live = FBLive.create(params, cField({ posen: [{ id: 'START_STACKER', x_in: 0, y_in: 0, h_deg: 0 }, { id: 'START_RUNNER', x_in: 25, y_in: 0, h_deg: 180 }] }));
  live.setKeys(press({ w: true }));
  run(live, 1.5, 10);
  ev = live.takeEvents();
  check('Roboter–Roboter: beide bekommen ein Ereignis mit dem Namen des anderen',
    ev.filter(e => e.robot === 'stacker' && e.text === 'COLLISION:runner').length === 1 && ev.filter(e => e.robot === 'runner' && e.text === 'COLLISION:stacker').length === 1, JSON.stringify(ev));
  const pa = live.pose('stacker'), pb = live.pose('runner');
  check('Roboter durchdringen sich nicht', Math.abs(pa.x_in - pb.x_in) >= 10 - 1e-6, JSON.stringify([pa, pb]));

  // 12 Feldelemente: nur mit Radius in den Parametern, null-Koordinaten übersprungen
  const pEl = Object.assign({}, params, { live: Object.assign({}, params.live, { kollision: { radius_in: { goals: 3 } } }) });
  const fEl = cField({ goals: [{ id: 'G1', x_in: 60, y_in: 0 }, { id: 'G_NULL', x_in: null, y_in: null }] });
  live = FBLive.create(pEl, fEl);
  check('Elemente: null-Koordinaten übersprungen', live.elements().length === 1 && live.elements()[0].id === 'G1');
  live.setKeys(press({ w: true }));
  run(live, 2, 10);
  ev = live.takeEvents();
  check('Element: COLLISION:G1, Roboter bleibt davor', ev.filter(e => e.text === 'COLLISION:G1').length === 1 && live.pose('stacker').x_in <= 60 - 3 - 5 + 1e-6, JSON.stringify([ev, live.pose('stacker')]));
  live = FBLive.create(params, fEl);
  check('Ohne Radius in den Parametern: keine Feldelemente', live.elements().length === 0);

  // 13 Umriss für die Anzeige und Reset
  live = FBLive.create(params, cField());
  const ol = live.outline('stacker');
  check('Umriss: 4 Ecken um die Pose', ol.length === 4 && near(ol[0].x + ol[2].x, 100) && near(ol[1].x + ol[3].x, 100));
  live.setKeys(press({ w: true }));
  run(live, 3, 10);
  live.reset();
  check('R: Ereignisse und Kontakte zurückgesetzt', live.takeEvents().length === 0);
  live.setKeys(press({ w: true }));
  run(live, 3, 10);
  check('nach R kann dieselbe Wand wieder ein Ereignis erzeugen', live.takeEvents().filter(e => e.text === 'COLLISION:wall').length === 1);

  // 10 CSV-Export im Log-Format v1 (SIM-16): Export → logparse.js → gleiche Posen und Ereignisse
  if (FBLog) {
    live = FBLive.create(params);   // ohne field.json: zwei START_-Hinweise + kein Feldrand → nur NOTE-Token
    const poses = [live.pose('stacker')];
    live.setKeys(press({ w: true }));
    for (let k = 0; k < 20; k++) { live.advance(0.02); poses.push(live.pose('stacker')); }
    live.setKeys(press({ w: true, d: true }));   // Kurve: Heading ändert sich
    for (let k = 0; k < 20; k++) { live.advance(0.02); poses.push(live.pose('stacker')); }
    check('Kurve ändert das Heading', Math.abs(FBLive.wrap(poses[40].h_deg - poses[0].h_deg)) > 1);
    let csv = live.exportCsv();
    const head = csv.stacker.split('\n')[0];
    check('Kopfzeile v1', /^# frostbyte-log v1 robot=stacker source=sim seed=- build=\S+$/.test(head), head);
    check('zweite Zeile: Spaltennamen', csv.stacker.split('\n')[1].split(',').join() === FBLive.LOG_COLUMNS.join(), csv.stacker.split('\n')[1]);
    let L = FBLog.parseLog(csv.stacker);
    check('Parser: keine Warnungen', L.warnings.length === 0, L.warnings.join('|'));
    const tr = L.robots.stacker;
    check('Raster 20 ms, eine Zeile mehr als Bilder', tr.n === 41 && Math.abs(tr.num.t_s[1] - 0.02) < 1e-9 && Math.abs(tr.num.t_s[40] - 0.8) < 1e-9, tr.n);
    let maxErr = 0;
    for (let k = 0; k < 41; k++) maxErr = Math.max(maxErr, Math.abs(tr.num.x_in[k] - poses[k].x_in), Math.abs(tr.num.y_in[k] - poses[k].y_in), Math.abs(FBLive.wrap(tr.num.th_deg[k] - poses[k].h_deg)), Math.abs(FBLive.wrap(tr.num.h_deg[k] - poses[k].h_deg)));
    check('Posen des Exports = Posen der Live-Fahrt', maxErr < 1e-3, maxErr);
    check('wahre Pose und Odometrie-Spalte gleich', Math.abs(tr.num.tx_in[40] - tr.num.x_in[40]) < 1e-9);
    const ev0 = FBLog.events(L).filter(e => e.robot === 'stacker');
    check('Hinweise stehen als NOTE-Token im Log', ev0.some(e => e.kind === 'NOTE' && /START_STACKER/.test(e.text)), JSON.stringify(ev0));
    check('Runner-Datei hat nur Runner-Zeilen', FBLog.parseLog(csv.runner).robots.runner.n === 41 && Object.keys(FBLog.parseLog(csv.runner).robots).join() === 'runner');

    // Fahrt gegen die Wand: Ereignisse im Export = takeEvents
    const fld = { feld_in: 144, posen: [] };
    live = FBLive.create(params, fld);
    live.setKeys(press({ w: true }));
    const seen = [];
    for (let k = 0; k < 400; k++) { live.advance(0.02); live.takeEvents().forEach(e => seen.push(e.text)); }
    csv = live.exportCsv();
    L = FBLog.parseLog(csv.stacker);
    const evs = FBLog.events(L).filter(e => e.robot === 'stacker' && e.kind !== 'NOTE').map(e => e.text);
    check('Ereignisse im Export = Ereignisse der Fahrt (COLLISION:wall, BLOCKED)',
      evs.join() === seen.join() && evs.indexOf('COLLISION:wall') >= 0 && evs.indexOf('BLOCKED') >= 0, evs.join() + ' | ' + seen.join());
    check('Pose im Export bleibt im Feld', Array.from(L.robots.stacker.num.y_in).every(y => y <= 72 + 1e-6));

    // Unbekannte Token und Sonderzeichen werden nicht verworfen und zerstören das CSV nicht
    live = FBLive.create(params, fld);
    live.logEvent('stacker', 'OUTPUT:motor,links;rechts');
    live.advance(0.04);
    check('logEvent erscheint auch in takeEvents (Anzeige)', live.takeEvents().some(e => /^OUTPUT:motor/.test(e.text)));
    L = FBLog.parseLog(live.exportCsv().stacker);
    const un = FBLog.events(L).filter(e => e.kind === 'OUTPUT');
    check('unbekannter Token bleibt erhalten, Komma/Semikolon entschärft', un.length === 1 && L.warnings.length === 0 && /^OUTPUT:motor/.test(un[0].text), JSON.stringify(un));

    // Log leeren, R beginnt neue Aufnahme, Ereignis ganz am Ende geht beim Export nicht verloren
    live = FBLive.create(params, fld);
    live.setKeys(press({ w: true }));
    for (let k = 0; k < 10; k++) live.advance(0.02);
    live.clearLog();
    L = FBLog.parseLog(live.exportCsv().stacker);
    check('Log leeren: nur noch eine Zeile (aktuelle Zeit)', L.robots.stacker.n === 1 && Math.abs(L.robots.stacker.num.t_s[0] - 0.2) < 1e-9, L.robots.stacker.n);
    // SIM-19: Log leeren mitten im Raster → keine Zeile abseits des 20-ms-Rasters (nur die Schlusszeile darf abweichen)
    live = FBLive.create(params, fld);
    for (let k = 0; k < 10; k++) live.advance(0.02);
    live.advance(0.005);   // ein Schritt hinter dem Raster
    live.clearLog();
    L = FBLog.parseLog(live.exportCsv().stacker);
    check('Log leeren zwischen Rasterpunkten: keine Zeile', L.robots.stacker === undefined || L.robots.stacker.n === 0, L.robots.stacker && L.robots.stacker.n);
    for (let k = 0; k < 5; k++) live.advance(0.02);
    L = FBLog.parseLog(live.exportCsv().stacker);
    const tt = Array.from(L.robots.stacker.num.t_s);
    check('nach Leeren mitten im Raster liegen alle Zeilen auf dem Raster', tt.length === 5 && tt.every(t => Math.abs(t / 0.02 - Math.round(t / 0.02)) < 1e-6), tt.join());
    live.reset();
    L = FBLog.parseLog(live.exportCsv().stacker);
    check('R: neue Aufnahme ab t = 0', L.robots.stacker.n === 1 && L.robots.stacker.num.t_s[0] === 0);
    live.logEvent('runner', 'BLOCKED');
    live.advance(0.005);
    const evEnd = FBLog.events(FBLog.parseLog(live.exportCsv().runner)).filter(e => e.kind === 'BLOCKED');
    check('Ereignis vor dem nächsten Rasterpunkt geht beim Export nicht verloren', evEnd.length === 1, evEnd.length);
  }

  // 9 Fehlende Parameter melden sich laut
  let threw = false;
  try { FBLive.create({ welt: params.welt, live: params.live, roboter: { stacker: { antrieb: {}, akku: {} }, runner: {} } }); } catch (err) { threw = true; }
  check('unvollständige Parameter → Fehler', threw);

  return { ok, fail, msgs };
}

/* SIM-17: Abgleich gegen frostbyte_sim. fx = {params, field, input, reference} (Fixture-Dateien aus test/fixtures, Text bzw. JSON).
   Spielt den Eingabeverlauf (t_ms,robot,left_V,right_V) im Livesim ab und vergleicht die wahre Pose alle 20 ms und die
   COLLISION-Ereignisse mit der Referenz, die frostbyte_sim schreibt. Liefert zusätzlich die gemessene größte Abweichung. */
function runLivesimReferenceTest(FBLive, fx, tolPos_in, tolHead_deg) {
  const msgs = [];
  let ok = 0, fail = 0, maxPos = 0, maxHead = 0, collisions = 0;
  function check(name, cond, info) {
    if (cond) ok++; else { fail++; msgs.push('FEHLER ' + name + (info !== undefined ? ': ' + info : '')); }
  }
  const lines = (t) => t.split(/\r?\n/).map((l) => l.trim()).filter((l) => l !== '' && l[0] !== '#');
  const sched = { stacker: [], runner: [] };
  lines(fx.input).slice(1).forEach((l) => {
    const c = l.split(',');
    sched[c[1]].push({ t_ms: Number(c[0]), left_V: Number(c[2]), right_V: Number(c[3]) });
  });
  const voltsAt = (name, t_ms) => {
    let cur = { left_V: 0, right_V: 0 };
    sched[name].forEach((r) => { if (r.t_ms <= t_ms) cur = r; });
    return cur;
  };
  const live = FBLive.create(fx.params, fx.field, { voltsAt });
  check('Startposen kommen aus der Fixture-field.json (keine Ersatzpose)', live.notes.length === 0, live.notes.join('|'));

  const ref = { stacker: [], runner: [] };
  lines(fx.reference).slice(1).forEach((l) => {
    const c = l.split(',');
    ref[c[1]].push({ t_ms: Math.round(Number(c[0]) * 1000), x: Number(c[2]), y: Number(c[3]), h: Number(c[4]), event: c[5] || '' });
  });
  const total = ref.stacker[ref.stacker.length - 1].t_ms;
  const got = { stacker: {}, runner: {} };
  const events = { stacker: [], runner: [] };
  for (let t = 0; t <= total; t++) {
    if (t > 0) {
      const n = live.advance(0.001);
      if (n !== 1) { check('ein Schritt je ms', false, 't=' + t + ' n=' + n); break; }
    }
    if (t % 20 === 0) ['stacker', 'runner'].forEach((n) => { got[n][t] = live.pose(n); });
    live.takeEvents().forEach((e) => {
      if (/^COLLISION:/.test(e.text)) events[e.robot].push({ row_ms: Math.ceil((Math.round(e.t_s * 1000) + 1) / 20) * 20, text: e.text });
    });
  }
  ['stacker', 'runner'].forEach((n) => {
    ref[n].forEach((r) => {
      const g = got[n][r.t_ms];
      if (!g) { check(n + ' t=' + r.t_ms + ' ms fehlt', false); return; }
      const dp = Math.hypot(g.x_in - r.x, g.y_in - r.y), dh = Math.abs(FBLive.wrap(g.h_deg - r.h));
      maxPos = Math.max(maxPos, dp); maxHead = Math.max(maxHead, dh);
      if (dp > tolPos_in || dh > tolHead_deg) check(n + ' t=' + r.t_ms + ' ms', false, 'Δpos=' + dp.toFixed(4) + ' in, Δh=' + dh.toFixed(4) + '°');
      else ok++;
      const want = r.event.split(';').filter((e) => /^COLLISION:/.test(e));
      const have = events[n].filter((e) => e.row_ms === r.t_ms).map((e) => e.text);
      if (want.join() !== have.join()) check(n + ' Ereignis t=' + r.t_ms + ' ms', false, 'Referenz [' + want + '] Livesim [' + have + ']');
      collisions += want.length;
    });
  });
  check('Referenz enthält Kollisionen (Roboter und Rand)', collisions === 4, collisions);
  check('Livesim erzeugt keine zusätzlichen Kollisionsereignisse', events.stacker.length + events.runner.length === collisions,
    JSON.stringify(events));

  // 13 Feldobjekte (SIM-30): erst umstoßen, dann schieben. Gleiche Fixture-Werte wie test/test_sim_field_objects.cpp
  // (sim_params_objects.json, field_objects.json, Stacker 6 V bis 1,5 s); Endpose aus frostbyte_sim.
  {
    const dr = (spur, mass) => ({ kS_V: 1.0, kV_V_per_in_s: 0.2, kA_V_per_in_s2: mass, spurbreite_in: spur, mu: 1.0, motor_max_V: 11.5 });
    const objParams = {
      welt: { schritt_ms: 1, steuer_ms: 10, log_ms: 20, g_in_s2: 386.1 },
      live: { langsam_faktor: 0.4, ersatzpose_in: { stacker: { x_in: 0, y_in: 0, h_deg: 0 }, runner: { x_in: 0, y_in: 0, h_deg: 0 } } },
      roboter: { stacker: { antrieb: dr(12, 0.04), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 12, breite: 12 } },
                 runner: { antrieb: dr(10, 0.03), akku: { spannung_V: 12.6 }, umriss_in: { laenge: 10, breite: 10 } } },
      feldobjekte: { radius_in: { cup: 2.0, pin: 2.0 }, roboter_anteil: 0.5 }
    };
    const objField = { feld_in: 144,
      posen: [{ id: 'START_STACKER', x_in: -40, y_in: 0, h_deg: 0 }, { id: 'START_RUNNER', x_in: 40, y_in: 60, h_deg: 180 }],
      objekte_start: { cups: [{ id: 'CUP_X', x_in: -25, y_in: 0, lage: 'steht', ebene: 0 }, { id: 'CUP_FAR', x_in: 0, y_in: -50, lage: 'steht', ebene: 0 },
                              { id: 'CUP_NULL', x_in: null, y_in: null, lage: 'steht', ebene: 0 }],
                       pins: [{ id: 'PIN_LIEGT', x_in: 20, y_in: 20, lage: 'liegt', ebene: 0 }, { id: 'PIN_OBEN', x_in: -25, y_in: 0, lage: 'steht', ebene: 1 }] } };
    const volts = (n, t_ms) => (n === 'stacker' && t_ms < 1500 ? { left_V: 6, right_V: 6 } : { left_V: 0, right_V: 0 });
    const live = FBLive.create(objParams, objField, { voltsAt: volts });
    check('Objekte: nur Bodenobjekte mit Koordinaten (CUP_X, CUP_FAR, PIN_LIEGT)', live.objects().map((o) => o.id).join() === 'CUP_X,CUP_FAR,PIN_LIEGT',
      live.objects().map((o) => o.id).join());
    check('Objekte: Pin am Boden liegt, Cup steht', live.objects()[2].standing === false && live.objects()[0].standing === true);
    const topples = [];
    for (let t = 0; t < 3000; t++) { live.advance(0.001); live.takeEvents().forEach((e) => { if (/^TOPPLED:/.test(e.text)) topples.push(e.text); }); }
    check('Objekte: genau ein TOPPLED:CUP_X, sonst keins', topples.join() === 'TOPPLED:CUP_X', topples.join());
    const cup = live.objects()[0];
    check('Objekte: Cup liegt und wurde nach vorn geschoben', cup.standing === false && cup.x_in > -25 + 1 && Math.abs(cup.y_in) < 1e-6, JSON.stringify(cup));
    check('Objekte: fernes Objekt unberührt', live.objects()[1].standing === true && live.objects()[1].x_in === 0);
    const q = live.pose('stacker');
    check('Objekte: Endpose wie frostbyte_sim (-18.643 in)', Math.abs(q.x_in - (-18.643)) <= 0.1, q.x_in);
    live.reset();
    check('Reset stellt die Objekte wieder auf', live.objects()[0].standing === true && live.objects()[0].x_in === -25);
    const none = FBLive.create(Object.assign({}, objParams, { feldobjekte: undefined }), objField, { voltsAt: volts });
    check('Ohne feldobjekte in sim_params keine Objekte', none.objects().length === 0);
  }

  return { ok, fail, msgs, maxPos_in: maxPos, maxHead_deg: maxHead };
}
if (typeof module !== 'undefined') module.exports = runLivesimTests;
