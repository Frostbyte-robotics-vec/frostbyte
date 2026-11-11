/* Tests für fieldmodels.js. Läuft im Browser (index.html?selftest) und unter Node (test_fieldmodels.py). Kein DOM; eigene
   Fixture-Manifeste, keine echten CAD-Maße. */
function runFieldmodelsTests(FBModels) {
  const msgs = [];
  let ok = 0, fail = 0;
  function check(name, cond, info) {
    if (cond) ok++; else { fail++; msgs.push('FEHLER ' + name + (info !== undefined ? ': ' + info : '')); }
  }
  const part = (name, rgba, y0, y1) => ({ name, rgba, y_min_in: y0, y_max_in: y1 });
  const RED = [0.75, 0.22, 0.17, 1], BLUE = [0.12, 0.42, 0.68, 1], YEL = [0.88, 0.7, 0.25, 1];
  const manifest = { version: 1, einheiten: 'Zoll', modelle: {
    pin_red_blue: { kind: 'pin', height_in: 6.5, teile: [part('b', BLUE, -0.2, 3.25), part('r', RED, -3.25, 0.2)] },
    pin_red_neutral: { kind: 'pin', height_in: 6.5, teile: [part('r', RED, -0.2, 3.25), part('y', YEL, -3.25, 0.2)] },
    pin_blue_neutral: { kind: 'pin', height_in: 6.5, teile: [part('y', YEL, -0.2, 3.25), part('b', BLUE, -3.25, 0.2)] },
    pin_neutral_neutral: { kind: 'pin', height_in: 6.5, teile: [part('y', YEL, -0.2, 3.25), part('y2', YEL, -3.25, 0.2)] },
    cup: { kind: 'cup', height_in: 6.48, teile: [] },
    goal_red: { kind: 'goal', height_in: 3.94, teile: [] } } };
  const payload = { manifest, glb: { cup: 'Z2xURg==', pin_red_blue: 'Z2xURg==' } };

  // 1 Schlüssel
  check('Pin-Varianten aus field.json', FBModels.pinKey('red_yellow') === 'pin_red_neutral' && FBModels.pinKey('blue_yellow') === 'pin_blue_neutral'
    && FBModels.pinKey('red_blue') === 'pin_red_blue' && FBModels.pinKey('yellow') === 'pin_neutral_neutral');
  check('Pin ohne Variante: gelb/gelb', FBModels.pinKey(undefined) === 'pin_neutral_neutral');
  check('Goal-Typen', FBModels.goalKey('alliance_red') === 'goal_red' && FBModels.goalKey('alliance_blue') === 'goal_blue'
    && FBModels.goalKey('neutral') === 'goal_neutral' && FBModels.goalKey('midfield') === 'goal_midfield');
  check('unbekannter Goal-Typ: kein Modell', FBModels.goalKey('x') === null);
  check('Loader nach Farbe', FBModels.loaderKey('red') === 'loader_red' && FBModels.loaderKey('blue') === 'loader_blue' && FBModels.loaderKey('x') === null);
  check('Objekt: Cup und Pin', FBModels.objectKey({ kind: 'cup' }) === 'cup' && FBModels.objectKey({ kind: 'pin', variant: 'red_blue' }) === 'pin_red_blue');
  check('Objekt: unbekannte Art', FBModels.objectKey({ kind: 'ball' }) === null);

  // 2 Verfügbarkeit und Fallback
  check('ohne Nutzlast nichts verfügbar', FBModels.usable(undefined) === false && FBModels.usable(null) === false);
  check('falsche Version nicht verfügbar', FBModels.usable({ manifest: { version: 2, modelle: {} }, glb: {} }) === false);
  check('Nutzlast ohne glb nicht verfügbar', FBModels.usable({ manifest: manifest }) === false);
  check('passende Nutzlast verfügbar', FBModels.usable(payload) === true);
  check('hat: nur Schlüssel mit GLB', FBModels.has(payload, 'cup') === true && FBModels.has(payload, 'goal_red') === false && FBModels.has(undefined, 'cup') === false);

  // 3 Höhen: aus dem Manifest, sonst Manual-Wert
  check('Höhe aus Manifest', FBModels.heightIn(payload, 'cup', 4.5) === 6.48);
  check('Höhe ohne Modell: Rückfallwert', FBModels.heightIn(undefined, 'cup', 6.5) === 6.5);
  check('Höhe: Pin über Variante', FBModels.heightIn(payload, 'pin_red_blue', 1) === 6.5);

  // 4 Pin: obere Hälfte, Wenden, liegend
  check('obere Farbe rot/blau', FBModels.topColor(manifest.modelle.pin_red_blue) === 'blue');
  check('obere Farbe rot/gelb', FBModels.topColor(manifest.modelle.pin_red_neutral) === 'red');
  check('Wenden: rot gewünscht, blau oben', FBModels.needsFlip(manifest.modelle.pin_red_blue, 'red') === true);
  check('Wenden: blau gewünscht, blau oben', FBModels.needsFlip(manifest.modelle.pin_red_blue, 'blue') === false);
  check('Wenden: keine Angabe', FBModels.needsFlip(manifest.modelle.pin_red_blue, undefined) === false);
  check('gelb unten bei rot/gelb', FBModels.yellowDown(manifest.modelle.pin_red_neutral) === true);
  check('gelb oben bei blau/gelb', FBModels.yellowDown(manifest.modelle.pin_blue_neutral) === false);
  let ax = FBModels.lyingAxis(manifest.modelle.pin_red_neutral, 0);
  check('liegend: gelb unten, gelb nach +x → Achse nach −x', Math.abs(ax[0] + 1) < 1e-9 && ax[1] === 0 && Math.abs(ax[2]) < 1e-9, JSON.stringify(ax));
  ax = FBModels.lyingAxis(manifest.modelle.pin_blue_neutral, 90);
  check('liegend: gelb oben, gelb nach +y (Feld) → z negativ', Math.abs(ax[0]) < 1e-9 && Math.abs(ax[2] + 1) < 1e-9, JSON.stringify(ax));
  ax = FBModels.lyingAxis(manifest.modelle.pin_red_blue, undefined);
  check('liegend ohne Richtung: Standardachse', ax[0] === 1 && ax[1] === 0 && ax[2] === 0);

  // 5 base64
  const buf = FBModels.decodeBase64('Z2xURg==');
  check('base64 → Bytes', buf.byteLength === 4 && new Uint8Array(buf)[0] === 0x67, buf.byteLength);

  return { ok, fail, msgs };
}
if (typeof module !== 'undefined') module.exports = runFieldmodelsTests;
