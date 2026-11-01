/* Tests für logparse.js. Läuft im Browser (index.html?selftest) und unter JavaScriptCore
   (osascript -l JavaScript, aufgerufen von test_logparse.py). Kein DOM, keine Ausgabe: liefert ein Ergebnisobjekt. */
function runLogparseTests(FBLog) {
  const msgs = [];
  let ok = 0, fail = 0;
  function check(name, cond, info) {
    if (cond) ok++; else { fail++; msgs.push('FEHLER ' + name + (info !== undefined ? ': ' + info : '')); }
  }
  const near = (a, b, eps) => Math.abs(a - b) <= (eps || 1e-9);
  const COLS = 't_s,robot,x_in,y_in,h_deg,gx_in,gy_in,gh_deg,g_err_in,tx_in,ty_in,th_deg,lift_deg,arm_deg,claw_in,' +
    'lift_set_deg,arm_set_deg,cycle,route_step,has_cup,has_pin,pin_yellow_up,score,cpu_ms,batt_v,temp_max_c,event';
  const HEAD = '# frostbyte-log v1 robot=stacker source=sim seed=42 build=abc123';
  const row = (t, robot, x, y, h, extra) => {
    const r = { t_s: t, robot, x_in: x, y_in: y, h_deg: h, cycle: 'BEREIT', score: 0, has_cup: 0 };
    Object.assign(r, extra || {});
    return COLS.split(',').map(c => (r[c] === undefined ? '' : r[c])).join(',');
  };
  const csv = (head, lines) => [head, COLS].concat(lines).join('\n') + '\n';

  // 1 Kopfzeile
  let L = FBLog.parseLog(csv(HEAD, [row(0, 'stacker', 0, 0, 0), row(0.02, 'stacker', 1, 2, 10)]));
  check('version', L.meta.version === 1, L.meta.version);
  check('meta robot/source/seed', L.meta.robot === 'stacker' && L.meta.source === 'sim' && L.meta.seed === '42' && L.meta.build === 'abc123');
  check('keine Warnungen', L.warnings.length === 0, L.warnings.join(' | '));
  check('ein Roboter', Object.keys(L.robots).join() === 'stacker');
  const S = L.robots.stacker;
  check('n Zeilen', S.n === 2, S.n);
  check('Zahl gelesen', S.num.y_in[1] === 2);

  // 2 leere Zellen = NaN
  check('leer = NaN', Number.isNaN(S.num.tx_in[0]) && Number.isNaN(S.num.gx_in[1]));
  check('Text-Spalte', S.text.cycle[0] === 'BEREIT');

  // 3 andere Version → Warnung, trotzdem gelesen
  L = FBLog.parseLog(csv('# frostbyte-log v2 robot=runner source=robot seed=- build=x', [row(0, 'runner', 0, 0, 0)]));
  check('v2 warnt', L.warnings.some(w => /v2/.test(w)), L.warnings.join(' | '));
  check('v2 gelesen', L.robots.runner && L.robots.runner.n === 1);
  check('seed - bleibt Text', L.meta.seed === '-');

  // 4 fehlende Kopfzeile → Warnung, Spaltenzeile trotzdem erkannt
  L = FBLog.parseLog([COLS, row(0, 'stacker', 1, 1, 0)].join('\n'));
  check('ohne Kopfzeile warnt', L.warnings.length > 0);
  check('ohne Kopfzeile gelesen', L.robots.stacker && L.robots.stacker.num.x_in[0] === 1);

  // 5 Spalten nach Namen, nicht nach Position
  const swapped = '# frostbyte-log v1 robot=stacker source=sim seed=1 build=x\ny_in,t_s,x_in,h_deg,robot\n5,0,7,90,stacker\n';
  L = FBLog.parseLog(swapped);
  check('nach Namen', L.robots.stacker.num.x_in[0] === 7 && L.robots.stacker.num.y_in[0] === 5);
  check('fehlende Spalte = NaN', Number.isNaN(L.robots.stacker.num.lift_deg[0]));

  // 6 zwei Roboter in einer Datei
  L = FBLog.parseLog(csv(HEAD, [row(0, 'stacker', 0, 0, 0), row(0, 'runner', 9, 9, 0), row(0.02, 'stacker', 1, 0, 0), row(0.02, 'runner', 8, 9, 0)]));
  check('zwei Roboter', Object.keys(L.robots).sort().join() === 'runner,stacker');
  check('Zuordnung', L.robots.runner.num.x_in[1] === 8 && L.robots.stacker.num.x_in[1] === 1);

  // 7 robot-Spalte leer → Name aus Kopfzeile
  L = FBLog.parseLog(csv(HEAD, [row(0, '', 0, 0, 0)]));
  check('Name aus Kopfzeile', L.robots.stacker && L.robots.stacker.n === 1);

  // 8 CRLF
  L = FBLog.parseLog(csv(HEAD, [row(0, 'stacker', 3, 0, 0)]).replace(/\n/g, '\r\n'));
  check('CRLF', L.robots.stacker.num.x_in[0] === 3 && L.robots.stacker.text.event[0] === '');

  // 9 unsortierte Zeit → sortiert mit Warnung
  L = FBLog.parseLog(csv(HEAD, [row(0.04, 'stacker', 4, 0, 0), row(0, 'stacker', 0, 0, 0), row(0.02, 'stacker', 2, 0, 0)]));
  check('sortiert', Array.from(L.robots.stacker.num.x_in).join() === '0,2,4');
  check('sortiert warnt', L.warnings.some(w => /sortiert/.test(w)));

  // 10 Interpolation
  L = FBLog.parseLog(csv(HEAD, [
    row(0, 'stacker', 0, 0, 170, { gx_in: 0, score: 0, cycle: 'BEREIT' }),
    row(1, 'stacker', 10, -4, -170, { gx_in: '', score: 6, cycle: 'HEBEN' }),
    row(2, 'stacker', 20, -8, -150, { gx_in: 5, score: 6, cycle: 'SETZEN' }),
  ]));
  const T = L.robots.stacker;
  check('linear', near(FBLog.sample(T, 'x_in', 0.5), 5) && near(FBLog.sample(T, 'y_in', 1.25), -5));
  check('Winkel über ±180', near(Math.abs(FBLog.sample(T, 'h_deg', 0.5)), 180, 1e-6), FBLog.sample(T, 'h_deg', 0.5));
  check('Winkel normal', near(FBLog.sample(T, 'h_deg', 1.5), -160, 1e-6), FBLog.sample(T, 'h_deg', 1.5));
  check('vor Anfang', FBLog.sample(T, 'x_in', -3) === 0);
  check('nach Ende', FBLog.sample(T, 'x_in', 99) === 20);
  check('diskret ohne Interpolation', FBLog.sample(T, 'score', 0.99) === 0 && FBLog.sample(T, 'score', 1) === 6);
  check('Text', FBLog.sample(T, 'cycle', 1.7) === 'HEBEN' && FBLog.sample(T, 'cycle', 2) === 'SETZEN');
  check('NaN danach → halten', FBLog.sample(T, 'gx_in', 0.5) === 0);
  check('NaN selbst → NaN', Number.isNaN(FBLog.sample(T, 'gx_in', 1.5)));
  check('index', FBLog.indexAt(T, 1.5) === 1 && FBLog.indexAt(T, -1) === 0 && FBLog.indexAt(T, 5) === 2);

  // 11 Posen-Quelle
  L = FBLog.parseLog(csv(HEAD, [row(0, 'stacker', 1, 2, 3, { gx_in: 4, gy_in: 5, gh_deg: 6, tx_in: 7, ty_in: 8, th_deg: 9 })]));
  const P = L.robots.stacker;
  const p = s => { const q = FBLog.pose(P, 0, s); return [q.x, q.y, q.h].join(); };
  check('pose odom/gps/wahr', p('odom') === '1,2,3' && p('gps') === '4,5,6' && p('wahr') === '7,8,9');

  // 12 Ereignisse
  L = FBLog.parseLog(csv(HEAD, [row(0, 'stacker', 0, 0, 0), row(0.02, 'stacker', 0, 0, 0, { event: 'PLACED:NEUTRAL_R1:2;RULE:SG6' }), row(0.04, 'runner', 0, 0, 0, { event: 'BLOCKED' })]));
  const ev = FBLog.events(L);
  check('Ereignisse', ev.length === 3, ev.length);
  check('Ereignis-Felder', ev[0].t === 0.02 && ev[0].robot === 'stacker' && ev[0].kind === 'PLACED' && ev[0].text === 'PLACED:NEUTRAL_R1:2');
  check('Ereignis-Reihenfolge', ev[2].robot === 'runner' && ev[2].kind === 'BLOCKED');

  // 13 Zeitspanne
  check('Zeitspanne', FBLog.timeRange(L).join() === '0,0.04');

  // 14 leere Datei
  L = FBLog.parseLog('');
  check('leer warnt', L.warnings.length > 0 && Object.keys(L.robots).length === 0);

  // 15 kaputte Zeile (falsche Spaltenzahl) → Warnung, Rest gelesen
  L = FBLog.parseLog(csv(HEAD, [row(0, 'stacker', 0, 0, 0), '1,stacker,3', row(0.04, 'stacker', 4, 0, 0)]));
  check('kaputte Zeile warnt', L.warnings.some(w => /Zeile 4/.test(w)), L.warnings.join(' | '));
  check('kaputte Zeile übersprungen', L.robots.stacker.n === 2);

  return { ok, fail, msgs };
}
if (typeof module !== 'undefined') module.exports = runLogparseTests;
