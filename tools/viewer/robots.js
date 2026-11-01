/* Robotermodelle Stacker und Runner für den Viewer.
   Zeilen 151–630 unverändert aus reference/frostbyte-lift.html übernommen (Texturen, Materialien, Bauteile, buildStacker,
   buildRunner, build). Ändert sich die Vorlage, diesen Block neu übernehmen; nichts hier von Hand anpassen.
   Modell-Koordinaten: Zoll, y nach oben, Vorderseite des Roboters zeigt nach −z.
   FBRobots(renderer) → { stacker(), runner(), tileTex }; stacker()/runner() → { root, layers, info:{ set(th_rad, t), thMin, thMax, … } } */
function FBRobots(renderer){
const T = 0.045, Y = 2;                       // Blechstärke, Achshöhe (4″-Rad)
/* ---------- Texturen ---------- */
function canvasTex(w, h, draw){ const c = document.createElement('canvas'); c.width = w; c.height = h; draw(c.getContext('2d'), w, h); const t = new THREE.CanvasTexture(c); t.anisotropy = renderer.capabilities.getMaxAnisotropy(); return t; }
const holeBase = canvasTex(64, 64, (g, w) => { g.fillStyle = '#fff'; g.fillRect(0, 0, w, w); const s = Math.round(w * 0.182 / 0.5); g.clearRect((w - s) / 2, (w - s) / 2, s, s); });
holeBase.wrapS = holeBase.wrapT = THREE.RepeatWrapping;
const tileTex = canvasTex(512, 512, (g, w) => {
  g.fillStyle = '#7b8086'; g.fillRect(0, 0, w, w);
  for (let i = 0; i < 4000; i++) { g.fillStyle = Math.random() < .5 ? 'rgba(255,255,255,.035)' : 'rgba(0,0,0,.05)'; g.fillRect(Math.random() * w, Math.random() * w, 2, 2); }
  g.strokeStyle = 'rgba(30,32,35,.55)'; g.lineWidth = 3;
  g.beginPath(); for (let k = 0; k <= 2; k++) { const p = k * w / 2; for (let j = 0; j < 8; j++) { const a = j * w / 8, b = a + w / 8; g.moveTo(p, a); g.lineTo(p, a + w / 32); g.lineTo(p + (j % 2 ? 10 : -10), a + w / 16); g.lineTo(p, a + 3 * w / 32); g.lineTo(p, b); g.moveTo(a, p); g.lineTo(a + w / 32, p); g.lineTo(a + w / 16, p + (j % 2 ? 10 : -10)); g.lineTo(a + 3 * w / 32, p); g.lineTo(b, p); } } g.stroke();
});
tileTex.wrapS = tileTex.wrapT = THREE.RepeatWrapping; tileTex.repeat.set(2, 2);
const screenTex = canvasTex(256, 170, (g, w, h) => {
  g.fillStyle = '#0b2236'; g.fillRect(0, 0, w, h);
  g.fillStyle = '#5fb4ff'; g.font = 'bold 26px sans-serif'; g.fillText('FROSTBYTE', 18, 44);
  g.fillStyle = '#9fd0ff'; g.font = '15px monospace'; g.fillText('Skills · Auton 1', 18, 72);
  ['Drive L', 'Drive R', 'Lift', 'Intake'].forEach((s, i) => { g.fillStyle = '#163b5c'; g.fillRect(18 + (i % 2) * 112, 90 + Math.floor(i / 2) * 36, 102, 28); g.fillStyle = '#cfe6ff'; g.fillText(s, 26 + (i % 2) * 112, 109 + Math.floor(i / 2) * 36); });
});

/* ---------- Materialien ---------- */
const std = (c, r = .6, m = .1, extra = {}) => new THREE.MeshStandardMaterial(Object.assign({color:c, roughness:r, metalness:m}, extra));
const ALU = 0xc4c9cf;
const mAlu = std(ALU, .38, .45), mSteel = std(0x8d939a, .35, .7), mBlack = std(0x1d1f22, .55, .05), mDark = std(0x2b2f33, .7, .1),
  mTire = std(0x262626, .95, 0), mHub = std(0x9aa1a8, .6, .05), mRoller = std(0x30343a, .8, 0), mGear = std(0x2e3134, .6, .05),
  mInsert = std(0x9ea3a8, .5, .1), mGreen = std(0x3e9b4f, .5, .05), mNylon = std(0xe8e5dc, .7, 0), mBear = std(0x161718, .5, .05),
  mBuy = std(0x7f77dd, .5, .05), mBuyDark = std(0x534ab7, .55, .05), mCable = std(0x1a1a1a, .8, 0), mScreen = std(0x0b2236, .3, 0, {map:screenTex, emissive:0xffffff, emissiveMap:screenTex, emissiveIntensity:.55}),
  mAmber = std(0xef9f27, .6, 0), mRed = std(0xc0392b, .5, .05);
const holeCache = {};
function holeMat(u, v){ const k = u.toFixed(2) + 'x' + v.toFixed(2); if (!holeCache[k]) { const t = holeBase.clone(); t.needsUpdate = true; t.repeat.set(u, v); holeCache[k] = std(ALU, .38, .45, {map:t, alphaTest:.5, side:THREE.DoubleSide}); } return holeCache[k]; }

/* ---------- Bauteil-Registrierung ---------- */
let ROOT, LAYERS, BOM, SPIN;
function put(obj, layer){ obj.traverse(o => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } }); LAYERS[layer].add(obj); return obj; }
function count(name, n = 1){ BOM[name] = (BOM[name] || 0) + n; }
function mesh(geo, mat){ return new THREE.Mesh(geo, mat); }

function panel(sx, sy, sz, thin){
  const mats = Array(6).fill(mAlu).slice();
  if (thin === 'x') { const m = holeMat(sz / .5, sy / .5); mats[0] = mats[1] = m; }
  if (thin === 'y') { const m = holeMat(sx / .5, sz / .5); mats[2] = mats[3] = m; }
  if (thin === 'z') { const m = holeMat(sx / .5, sy / .5); mats[4] = mats[5] = m; }
  return mesh(new THREE.BoxGeometry(sx, sy, sz), mats);
}
/* C-Profil 1x2x1: Steg 1″ (2 Löcher), Schenkel 0,5″; lokal: Länge entlang z, Steg in yz-Ebene, Schenkel zeigen nach +x */
function channel(holes){
  const L = holes * .5, g = new THREE.Group();
  g.add(panel(T, 1, L, 'x'));
  [-1, 1].forEach(s => { const f = panel(.5, T, L, 'y'); f.position.set(.25, s * (.5 - T / 2), 0); g.add(f); });
  count(holes === 35 ? 'C-Profil 1x2x1x35' : 'C-Profil 1x2x1x35 (gekürzt)');
  BOM.__holes = (BOM.__holes || 0) + holes;
  return g;
}
function rail(holes, x, z, s){ const c = channel(holes); c.rotation.y = s > 0 ? Math.PI : 0; c.position.set(x, Y, z); return put(c, 'struktur'); }
function cross(holes, z, x = 0){ const inner = channel(holes); inner.rotation.z = Math.PI / 2; const w = new THREE.Group(); w.add(inner); w.rotation.y = Math.PI / 2; w.position.set(x, Y + .5 + T / 2, z); return put(w, 'struktur'); }
function upright(holes, x, yc, z, s){ const inner = channel(holes); inner.rotation.x = -Math.PI / 2; const w = new THREE.Group(); w.add(inner); w.rotation.y = s > 0 ? Math.PI : 0; w.position.set(x, yc, z); return put(w, 'struktur'); }
function plate(holesX, z, y){ const p = panel(holesX * .5, T, 2.5, 'y'); p.position.set(0, y, z); count('Alu-Platte 5x25' + (holesX < 25 ? ' (gekürzt)' : '')); return put(p, 'struktur'); }

/* Schraube #8-32 Linsenkopf + Sicherungsmutter; pos = Kopfauflage, dir = Blickrichtung des Kopfes */
const headGeo = new THREE.SphereGeometry(.15, 18, 8, 0, Math.PI * 2, 0, Math.PI / 2); headGeo.scale(1, .55, 1);
const starGeo = new THREE.CylinderGeometry(.05, .05, .02, 6), nutGeo = new THREE.CylinderGeometry(.17, .17, .1, 6), ringGeo = new THREE.CylinderGeometry(.15, .15, .06, 16), shankGeo = new THREE.CylinderGeometry(.07, .07, 1, 10);
function screw(x, y, z, dir, len = .45, nut = true){
  const g = new THREE.Group(); const d = new THREE.Vector3(...dir).normalize();
  g.add(mesh(headGeo, mSteel)); const st = mesh(starGeo, mBlack); st.position.y = .085; g.add(st);
  const sh = mesh(shankGeo, mSteel); sh.scale.y = len; sh.position.y = -len / 2; g.add(sh);
  if (nut) { const n = mesh(nutGeo, mSteel); n.position.y = -len + .06; g.add(n); const r = mesh(ringGeo, mNylon); r.position.y = -len - .02; g.add(r); count('Sicherungsmutter #8-32'); }
  g.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), d); g.position.set(x, y, z);
  count('Schraube #8-32'); return put(g, 'verbindung');
}
/* Flachlager auf einer Profilwand (Normale ±x), 2 Schrauben */
function bearing(x, y, z, nx){
  const b = mesh(new THREE.BoxGeometry(.08, .5, 1.5), mBear); b.position.set(x + nx * .04, y, z); put(b, 'verbindung');
  [-.5, .5].forEach(dz => screw(x + nx * .08, y, z + dz, [nx, 0, 0], .3));
  count('Flachlager');
}
function shaft(x1, x2, y, z, collars = true){
  const L = Math.abs(x2 - x1), s = mesh(new THREE.BoxGeometry(L, .125, .125), mSteel); s.position.set((x1 + x2) / 2, y, z); put(s, 'verbindung');
  if (collars) [x1, x2].forEach(xx => { const c = mesh(new THREE.CylinderGeometry(.27, .27, .2, 20), mSteel); c.rotation.z = Math.PI / 2; c.position.set(xx, y, z); put(c, 'verbindung'); count('Stellring'); });
  count('Welle (aus 12″ gekürzt)'); return s;
}
function spacer(x1, x2, y, z){ const L = Math.abs(x2 - x1); if (L < .05) return; const c = mesh(new THREE.CylinderGeometry(.24, .24, L, 16), mNylon); c.rotation.z = Math.PI / 2; c.position.set((x1 + x2) / 2, y, z); put(c, 'verbindung'); count('Distanzhülse'); }

/* Zahnrad mit Zähnen, 24 DP: Teilkreis-Ø = Z/24 */
const gearCache = {};
function gearGeo(Z){
  if (gearCache[Z]) return gearCache[Z];
  const r = Z / 48, ra = r + 1 / 24, rf = r - 1.25 / 24, st = Math.PI * 2 / Z, sh = new THREE.Shape();
  for (let i = 0; i < Z; i++) { const a = i * st; const pts = [[rf, a - .3 * st], [ra, a - .14 * st], [ra, a + .14 * st], [rf, a + .3 * st]]; pts.forEach(([rr, aa], k) => { const p = [Math.cos(aa) * rr, Math.sin(aa) * rr]; (i === 0 && k === 0) ? sh.moveTo(...p) : sh.lineTo(...p); }); }
  const sq = new THREE.Path(); const q = .13; sq.moveTo(-q, -q); sq.lineTo(q, -q); sq.lineTo(q, q); sq.lineTo(-q, q); sh.holes.push(sq);
  if (Z >= 60) { const n = 6; for (let i = 0; i < n; i++) { const a = i * Math.PI * 2 / n, c = new THREE.Path(); c.absarc(Math.cos(a) * r * .58, Math.sin(a) * r * .58, r * .2, 0, Math.PI * 2, true); sh.holes.push(c); } }
  const g = new THREE.ExtrudeGeometry(sh, {depth:.25, bevelEnabled:false, curveSegments:10}); g.translate(0, 0, -.125); g.rotateY(Math.PI / 2);
  return gearCache[Z] = g;
}
function gear(Z, x, y, z, k){
  const g = new THREE.Group(); g.add(mesh(gearGeo(Z), mGear));
  const ins = mesh(new THREE.BoxGeometry(.27, .5, .5), mInsert); ins.scale.set(1, .62, .62); g.add(ins);
  g.position.set(x, y, z); SPIN.push([g, k]); count('Zahnrad ' + Z + 'T'); return put(g, 'antrieb');
}
/* 4″-Rad (Lauffläche) und 4″-Omnirad */
function tractionWheel(x, y, z, k){
  const g = new THREE.Group();
  const tire = mesh(new THREE.CylinderGeometry(1.95, 1.95, .95, 48), mTire); tire.rotation.z = Math.PI / 2; g.add(tire);
  for (let i = 0; i < 40; i++) { const a = i / 40 * Math.PI * 2, b = mesh(new THREE.BoxGeometry(.8, .08, .14), mTire); b.position.set(0, Math.cos(a) * 1.97, Math.sin(a) * 1.97); b.rotation.x = -a; g.add(b); }
  [-1, 1].forEach(s => { const h = mesh(new THREE.CylinderGeometry(1.3, 1.3, .06, 40), mHub); h.rotation.z = Math.PI / 2; h.position.x = s * .46; g.add(h);
    for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2, c = mesh(new THREE.CylinderGeometry(.24, .24, .07, 16), mDark); c.rotation.z = Math.PI / 2; c.position.set(s * .47, Math.cos(a) * .75, Math.sin(a) * .75); g.add(c); } });
  g.position.set(x, y, z); SPIN.push([g, k]); count('4″-Rad'); return put(g, 'antrieb');
}
function omniWheel(x, y, z, k){
  const g = new THREE.Group();
  [-1, 1].forEach(s => { const p = mesh(new THREE.CylinderGeometry(1.72, 1.72, .1, 36), mHub); p.rotation.z = Math.PI / 2; p.position.x = s * .2; g.add(p); });
  const hubc = mesh(new THREE.CylinderGeometry(.55, .55, .55, 24), mHub); hubc.rotation.z = Math.PI / 2; g.add(hubc);
  [-1, 1].forEach((s, row) => { for (let i = 0; i < 9; i++) { const a = (i + row * .5) / 9 * Math.PI * 2, r = mesh(new THREE.CylinderGeometry(.21, .21, .66, 16), mRoller); r.position.set(s * .32, Math.cos(a) * 1.79, Math.sin(a) * 1.79); r.rotation.x = a + Math.PI / 2; g.add(r);
    const ax = mesh(new THREE.CylinderGeometry(.04, .04, .9, 6), mSteel); ax.position.copy(r.position); ax.rotation.copy(r.rotation); g.add(ax); } });
  g.position.set(x, y, z); SPIN.push([g, k]); count('4″-Omnirad'); return put(g, 'antrieb');
}
/* V5 Smart Motor 11 W: ca. 1,3 × 1,9 × 2,25″, Getriebekassette grün (18:1, 200 rpm) zur Profilwand */
function motor(xFace, s, y, z){
  const g = new THREE.Group(), body = mesh(new THREE.BoxGeometry(1.3, 1.9, 2.25), mDark); g.add(body);
  const cap = mesh(new THREE.CylinderGeometry(.42, .42, .06, 28), mGreen); cap.rotation.z = Math.PI / 2; cap.position.x = s * .66; g.add(cap);
  const top = mesh(new THREE.BoxGeometry(1.1, .06, 2.0), mBlack); top.position.y = .96; g.add(top);
  const port = mesh(new THREE.BoxGeometry(.4, .08, .5), mBlack); port.position.set(-s * .2, .99, -.6); g.add(port);
  const strip = mesh(new THREE.BoxGeometry(1.31, .18, .5), mRed); strip.position.set(0, .3, .9); g.add(strip);
  g.position.set(xFace - s * .65, y, z); put(g, 'motor');
  [-.6, .6].forEach(dz => screw(xFace + s * (T + .02), y + .35, z + dz, [s, 0, 0], .25, false));
  count('V5 Smart Motor'); return new THREE.Vector3(xFace - s * .85, y + 1, z - .6);
}
function cable(points){ const c = new THREE.CatmullRomCurve3(points.map(p => new THREE.Vector3(...p))); put(mesh(new THREE.TubeGeometry(c, 40, .055, 8), mCable), 'elektronik'); count('Smart-Kabel'); }
function standoff(x, y0, y1, z){ const s = mesh(new THREE.CylinderGeometry(.11, .11, y1 - y0, 6), mAlu); s.position.set(x, (y0 + y1) / 2, z); put(s, 'verbindung'); count('Abstandsbolzen'); screw(x, y0 - T, z, [0, -1, 0], .2, false); }
function brain(x, y, z){
  const g = new THREE.Group(); g.add(mesh(new THREE.BoxGeometry(4.7, .95, 3.6), mBlack));
  const scr = mesh(new THREE.PlaneGeometry(2.9, 1.95), mScreen); scr.rotation.x = -Math.PI / 2; scr.position.set(-.5, .48, 0); g.add(scr);
  for (let i = 0; i < 10; i++) { const p = mesh(new THREE.BoxGeometry(.3, .25, .06), mDark); p.position.set(-2 + i * .44, 0, 1.81); g.add(p); const q = p.clone(); q.position.z = -1.81; g.add(q); }
  const btn = mesh(new THREE.CylinderGeometry(.18, .18, .06, 20), mAmber); btn.position.set(1.85, .49, 1.2); g.add(btn);
  g.position.set(x, y, z); put(g, 'elektronik'); count('V5 Brain');
  [[-2, -1.4], [2, -1.4], [-2, 1.4], [2, 1.4]].forEach(([dx, dz]) => standoff(x + dx, Y + .5 + T, y - .48, z + dz));
  return g;
}
function battery(x, y, z){
  const g = new THREE.Group(); g.add(mesh(new THREE.BoxGeometry(4.3, 1.2, 2.4), mBlack));
  const lab = mesh(new THREE.BoxGeometry(2.2, .02, 1.2), mRed); lab.position.y = .61; g.add(lab);
  const clip = mesh(new THREE.BoxGeometry(.3, 1.3, 2.6), mDark); [-1.6, 1.6].forEach(dx => { const c = clip.clone(); c.position.x = dx; g.add(c); });
  g.position.set(x, y, z); put(g, 'elektronik'); count('V5 Akku'); return g;
}
function radio(x, y, z){ const g = new THREE.Group(); g.add(mesh(new THREE.BoxGeometry(.6, 1.1, 1.9), mBlack)); const led = mesh(new THREE.BoxGeometry(.62, .12, .12), std(0x3be37a, .4, 0, {emissive:0x3be37a, emissiveIntensity:.8})); led.position.set(0, .3, .7); g.add(led); g.position.set(x, y, z); put(g, 'elektronik'); count('V5 Funkmodul'); }
function imu(x, y, z){ const g = new THREE.Group(); g.add(mesh(new THREE.BoxGeometry(1.2, .45, 1.2), mBuy)); g.position.set(x, y, z); put(g, 'zukauf'); count('Inertialsensor (IMU)'); }
function trackingWheel(x, y, z, axis){
  const g = new THREE.Group();
  const w = mesh(new THREE.CylinderGeometry(1.375, 1.375, .4, 36), mTire); w.rotation.z = Math.PI / 2; g.add(w);
  const h = mesh(new THREE.CylinderGeometry(.9, .9, .42, 24), mBuy); h.rotation.z = Math.PI / 2; g.add(h);
  const sen = mesh(new THREE.BoxGeometry(.6, 1.2, 1.2), mBuyDark); sen.position.x = .55; g.add(sen);
  if (axis === 'z') g.rotation.y = Math.PI / 2;
  g.position.set(x, y, z); put(g, 'zukauf'); count('Rotationssensor'); count('2,75″-Rad (Tracking)');
  return g;
}
function arrow(z){ const sh = new THREE.Shape(); sh.moveTo(0, -1.4); sh.lineTo(1, .2); sh.lineTo(.35, .2); sh.lineTo(.35, 1.4); sh.lineTo(-.35, 1.4); sh.lineTo(-.35, .2); sh.lineTo(-1, .2); sh.lineTo(0, -1.4);
  const m = mesh(new THREE.ShapeGeometry(sh), std(0xef9f27, .7, 0, {side:THREE.DoubleSide})); m.rotation.x = -Math.PI / 2; m.position.set(0, .01, z); m.scale.set(1.3, 1.3, 1.3); m.receiveShadow = true; ROOT.add(m); }

/* ---------- Lift-Bauteile ---------- */
const XW = (s, v) => s > 0 ? v : -(v + .5);   // Stegposition, damit das Profil |x| v … v+0,5 belegt (Schenkel zeigen nach +x)
function armCh(holes){ const g = new THREE.Group(), c = channel(holes); c.position.z = holes * .25 - .25; g.add(c); return g; }   // Drehpunkt-Loch im Ursprung, Profil entlang +z
function vbar(holes){ const g = new THREE.Group(), c = channel(holes); c.rotation.x = -Math.PI / 2; g.add(c); return g; }
function xbar(holes, x, y, z, parent){ const inner = channel(holes); inner.rotation.z = Math.PI / 2; const w = new THREE.Group(); w.add(inner); w.rotation.y = Math.PI / 2; w.position.set(x, y, z); parent.add(w); return w; }
function shaftX(x1, x2, y, z, parent){
  const s = mesh(new THREE.BoxGeometry(Math.abs(x2 - x1), .125, .125), mSteel); s.position.set((x1 + x2) / 2, y, z); parent.add(s); count('Welle (aus 12″ gekürzt)');
  [x1, x2].forEach(xx => { const c = mesh(new THREE.CylinderGeometry(.27, .27, .2, 20), mSteel); c.rotation.z = Math.PI / 2; c.position.set(xx, y, z); parent.add(c); count('Stellring'); });
}
function gearMesh(Z){ const g = new THREE.Group(); g.add(mesh(gearGeo(Z), mGear)); const ins = mesh(new THREE.BoxGeometry(.27, .5, .5), mInsert); ins.scale.set(1, Z <= 12 ? .52 : .62, Z <= 12 ? .52 : .62); g.add(ins); count('Zahnrad ' + Z + 'T'); return g; }
function gearAt(Z, x, y, z, parent){ const g = gearMesh(Z); g.position.set(x, y, z); parent.add(g); return g; }
/* ---------- Druckteile, Spielobjekte, Greifer v2 ---------- */
const mPETG = std(0x3d8fd6, .5, 0, {side:THREE.DoubleSide}), mTPU = std(0xe0703a, .85, 0),
  mCupClear = std(0xd9eeff, .08, 0, {transparent:true, opacity:.35, depthWrite:false, side:THREE.DoubleSide}), mCupGray = std(0x7f858d, .55, 0, {side:THREE.DoubleSide});
function petg(geo, mat = mPETG){ count('Druckteil PETG'); return mesh(geo, mat); }
function tpu(geo){ count('Druckteil TPU'); return mesh(geo, mTPU); }
function motorMesh(){   // V5-Motor, Kassette zeigt nach +x
  const m = new THREE.Group(); m.add(mesh(new THREE.BoxGeometry(1.3, 1.9, 2.25), mDark));
  const cap = mesh(new THREE.CylinderGeometry(.42, .42, .06, 28), mGreen); cap.rotation.z = Math.PI / 2; cap.position.x = .66; m.add(cap);
  const strip = mesh(new THREE.BoxGeometry(1.31, .18, .5), mRed); strip.position.set(0, .3, .9); m.add(strip);
  count('V5 Smart Motor'); return m;
}
/* Stapel-Einheit: Cup (unten klar, oben grau) mit Pin, dessen rote Hälfte im grauen Cup-Teil steckt. Ursprung = Greifhöhe */
function unitObj(parent, x, y, z){
  const g = new THREE.Group();
  const lo = mesh(new THREE.CylinderGeometry(1.575, 1.5, 3.25, 40, 1, true), mCupClear); lo.position.y = -2.375; g.add(lo);
  const hi = mesh(new THREE.CylinderGeometry(1.575, 1.575, 3.25, 40, 1, true), mCupGray); hi.position.y = .875; g.add(hi);
  const mid = mesh(new THREE.CylinderGeometry(1.55, 1.55, .12, 40), mCupGray); mid.position.y = -.75; g.add(mid);
  const rim = mesh(new THREE.TorusGeometry(1.575, .07, 8, 40), mCupGray); rim.rotation.x = Math.PI / 2; rim.position.y = 2.5; g.add(rim);
  const red = std(0xc0392b, .45, 0), yel = std(0xe8b923, .45, 0);
  [[red, .875], [yel, 4.125]].forEach(([m, cy]) => { const h = mesh(new THREE.CylinderGeometry(.8, .8, 3.25, 32), m); h.position.y = cy; g.add(h); });
  const ring = mesh(new THREE.CylinderGeometry(.9, .9, .14, 32), mBlack); ring.position.y = 2.5; g.add(ring);
  const cap = mesh(new THREE.SphereGeometry(.8, 24, 10, 0, Math.PI * 2, 0, Math.PI / 2), yel); cap.scale.y = .2; cap.position.y = 5.75; g.add(cap);
  g.name = 'unit'; g.traverse(o => { if (o.isMesh) o.userData.noBox = true; });
  g.position.set(x, y, z); parent.add(g); return g;
}
/* Rechte V-Backe (x > 0): 90°-Kerbe bei z = 1,9; j = Abstand der Backenfläche zur Mitte */
function jawGeo(j){
  const zc = 1.9, d = 1.0, xo = j + d + .2, sh = new THREE.Shape();
  sh.moveTo(j, -.6); sh.lineTo(j, zc - d); sh.lineTo(j + d, zc); sh.lineTo(j, zc + d); sh.lineTo(j, 3.0); sh.lineTo(xo, 3.0); sh.lineTo(xo, -.6); sh.lineTo(j, -.6);
  const g = new THREE.ExtrudeGeometry(sh, {depth:.9, bevelEnabled:false}); g.rotateX(Math.PI / 2); g.translate(0, .45, 0); return g;
}
/* Greifer v2: gedruckter Kopf, zwei V-Backen auf Führungswellen, Zahnstangen + 12T-Ritzel, 1 Motor.
   Ursprung O = unterer Gelenkpunkt; Kopf liegt bei −z, Backen und Objekt bei +z. j = 1,29″ fasst den Cup (Ø 3,15″), j = 0,19″ den Pin (Ø 1,6″). */
function clawV2(parent, j = 1.29){
  const g = new THREE.Group(); parent.add(g);
  const head = petg(new THREE.BoxGeometry(5.0, 1.1, .9)); head.position.set(0, .35, -.6); g.add(head);
  [-1, 1].forEach(s => {
    const geo = jawGeo(j); if (s < 0) geo.scale(-1, 1, 1);
    g.add(petg(geo));
    const zc = 1.9;
    [[zc - .5, Math.PI / 4, -1], [zc + .5, -Math.PI / 4, 1]].forEach(([pz, ry, dz]) => { const p = tpu(new THREE.BoxGeometry(.08, .82, 1.3)); p.position.set(s * (j + .5 + .03), 0, pz + dz * .03); p.rotation.y = s * ry; g.add(p); });
    const rack = petg(new THREE.BoxGeometry(2.4, .18, .22)); rack.position.set(s * (j - .2), s > 0 ? -.12 : .12, -.15); g.add(rack);
    const lug = petg(new THREE.BoxGeometry(.28, 1.6, 1.1)); lug.position.set(s * 1.84, 1.4, -.3); g.add(lug);   // Lagerlasche für den oberen Lenker (innen)
  });
  [-.25, .25].forEach(dy => { const r = mesh(new THREE.BoxGeometry(4.9, .125, .125), mSteel); r.position.set(0, dy, -.4); g.add(r); });
  const pin = gearMesh(12); pin.rotation.z = Math.PI / 2; pin.position.set(0, 0, -.15); g.add(pin);
  const m = motorMesh(); m.rotation.set(0, Math.PI / 2, Math.PI / 2); m.position.set(0, -1.2, -.9); g.add(m);   // Motor liegt quer unter dem Kopf, Welle nach oben zum Ritzel
  return g;
}

/* DR4B: untere Stufe kippt nach hinten, obere nach vorn → Schlitten fährt senkrecht über dem Turm (z0) */
function dr4b(o){
  const {z0, yP1, L, armHoles} = o, yP2 = yP1 - 2, sides = [], rot = [];
  const car = new THREE.Group(); car.name = 'car'; put(car, 'lift');
  const tH = Math.round((yP1 + .6 - 3.05) * 2);
  [-1, 1].forEach(s => {
    upright(tH, s * 5.5, 3.05 + tH * .25, z0, s);
    [-.25, .25].forEach(dz => screw(s * 5.25, 3.1, z0 + dz, [0, 1, 0], .7));
    const br = armCh(27); br.position.set(XW(s, 5.0), 12, z0 + .5); br.rotation.x = Math.atan2(9.4, 9.5); put(br, 'struktur');
    screw(s * 5.25, 2.6 + T, z0 + 10, [0, 1, 0], .3);
    const la = [yP1, yP2].map(y => { const a = armCh(armHoles); a.position.set(XW(s, 4.4), y, z0); put(a, 'lift'); return a; });
    const statics = new THREE.Group(); put(statics, 'verbindung');
    shaftX(s * 4.3, s * 5.65, yP1, z0, statics); shaftX(s * 4.3, s * 6.3, yP2, z0, statics); shaftX(s * 5.4, s * 6.3, yP2 - 2.5, z0, statics);
    const gb = new THREE.Group(); put(gb, 'lift');
    const g84a = gearAt(84, s * 5.75, yP2, z0, gb), g36 = gearAt(36, s * 5.75, yP2 - 2.5, z0, gb), g84b = gearAt(84, s * 6.1, yP2 - 2.5, z0, gb), g12 = gearAt(12, s * 6.1, yP2 - 4.5, z0, gb);
    rot.push([g84a, -1, 0], [g36, 84 / 36, .05], [g84b, 84 / 36, 0], [g12, -(84 / 36) * 7, .13]);
    const port = motor(s * 6.3, -s, yP2 - 4.5, z0);
    cable([[port.x, port.y, port.z], [s * 7.2, port.y - 1, z0 + 2], [s * 5.9, 7, -2], ...(s < 0 ? [[-3.2, 4.6, 2.6], [-2.7, 4.1, 3.0]] : [[4.2, 7.6, -2.4], [4.0, 3.7, -1.5], [0, 3.8, -1.6], [-1.6, 3.8, -1.6], [-2.62, 3.85, -.85], [-2.65, 3.9, .8], [-2.62, 3.95, 2.25]])]);
    const mb = new THREE.Group(); put(mb, 'lift');
    const bar = vbar(14); bar.position.set(XW(s, 4.95), 1.25, 0); mb.add(bar);
    [[0, 4.3, 5.65], [-2, 4.3, 5.65], [2.5, 3.5, 5.65], [4.5, 3.5, 5.65]].forEach(([dy, a, b]) => shaftX(s * a, s * b, dy, 0, mb));
    const ua = [2.5, 4.5].map(() => { const a = armCh(armHoles); put(a, 'lift'); return a; });
    const ga = gearMesh(60), gc = gearMesh(60); put(ga, 'lift'); put(gc, 'lift');
    const cb = vbar(6); cb.position.set(XW(s, 3.07), 1, 0); car.add(cb);
    [0, 2].forEach(dy => shaftX(s * 3.0, s * 4.2, dy, 0, car));
    const br2 = mesh(new THREE.BoxGeometry(.5, T, 1.8), mAlu); br2.position.set(s * 3.32, 2.55, 1.4); car.add(br2);
    sides.push({s, la, ua, mb, ga, gc, xu:XW(s, 3.6)});
  });
  /* Greifarm: Parallelogramm 5,5″ am Schlitten, Greifer bleibt waagrecht; Antrieb 12T → 84T (7 : 1) am oberen Gelenk */
  const R = 4.7, links = [];
  xbar(14, 0, 2.55, 2.6, car);
  [-1, 1].forEach(s => [[.5, 2.52], [2.0, 2.0]].forEach(([py, xw]) => { const a = armCh(10); a.position.set(XW(s, xw), py, 0); car.add(a); links.push(a); }));   // untere Lenker außen, obere innen: zwei Ebenen, damit sie sich beim Schwenken nicht treffen
  [-1, 1].forEach(s => shaftX(s * 2.35, s * 3.15, .5, 0, car));   // Kurzwellen statt Durchgangswellen: Cup und Pin schwenken zwischen den Lagern durch
  shaftX(1.72, 3.15, 2.0, 0, car); shaftX(-3.15, -1.95, 2.0, 0, car);   // oben rechts trägt das 84T
  const gArm = gearAt(84, 1.8, 2.0, 0, car), pArm = gearAt(12, 1.8, 4.0, 0, car);
  const am = motorMesh(); am.position.set(.975, 4.0, 0); car.add(am);
  const head = new THREE.Group(); head.name = 'head'; car.add(head);
  clawV2(head, 1.29);
  unitObj(head, 0, -.7, 1.9);
  const psiPick = Math.acos(-.6 / R);   // Greifpunkt genau über dem Köcher
  return function set(th, t = 0){
    const k = Math.min(1, Math.max(0, (t - .12) / .28)), psi = psiPick * (1 - k * k * (3 - 2 * k));
    links.forEach(a => a.rotation.x = Math.PI - psi);
    head.position.set(0, .5 - R * Math.sin(psi), -R * Math.cos(psi));
    gArm.rotation.x = -psi; pArm.rotation.x = psi * 7 + .13;
    const c = Math.cos(th), sn = Math.sin(th), A1y = yP1 + L * sn, Az = z0 + L * c;
    sides.forEach(sd => {
      sd.la.forEach(a => a.rotation.x = -th);
      sd.mb.position.set(0, A1y, Az);
      sd.ua.forEach((a, i) => { a.position.set(sd.xu, A1y + 2.5 + i * 2, Az); a.rotation.x = Math.PI + th; });
      sd.ga.position.set(sd.s * 4.25, A1y, Az); sd.ga.rotation.x = -th;
      sd.gc.position.set(sd.s * 4.25, A1y + 2.5, Az); sd.gc.rotation.x = th + .05;
    });
    rot.forEach(([g, k, p]) => g.rotation.x = k * th + p);
    car.position.set(0, yP1 + 2 * L * sn + 2.5, z0);
  };
}

/* Viergelenk (4-bar): Arme zeigen nach vorn, Schlitten bleibt senkrecht */
function fourBar(o){
  const {z0, yP1, L, armHoles} = o, yP2 = yP1 - 2, arms = [], rot = [];
  const car = new THREE.Group(); car.name = 'car'; put(car, 'lift');
  [-1, 1].forEach(s => {
    upright(18, s * 4.5, 7.55, z0, s);
    [-.25, .25].forEach(dz => screw(s * 4.25, 3.1, z0 + dz, [0, 1, 0], .7));
    [yP1, yP2].forEach(y => { const a = armCh(armHoles); a.position.set(XW(s, 3.4), y, z0); put(a, 'lift'); arms.push(a); });
    const statics = new THREE.Group(); put(statics, 'verbindung');
    shaftX(s * 3.3, s * 4.65, yP1, z0, statics); shaftX(s * 3.3, s * 4.95, yP2, z0, statics);
    const gb = new THREE.Group(); put(gb, 'lift');
    const g60 = gearAt(60, s * 4.75, yP2, z0, gb), g12 = gearAt(12, s * 4.75, yP2, z0 - 1.5, gb);
    rot.push([g60, 1, 0], [g12, -5, .13]);
    const port = motor(s * 4.95, -s, yP2, z0 - 1.5);
    cable([[port.x, port.y, port.z], [s * 5.6, port.y + .4, z0 - 3.2], [s * 5.4, 4.6, 1.0], [s * 3.0, 4.4, -1.5], [s * 2.4, 4.3, -3.1]]);   // unter den Liftarmen durch
    const cb = vbar(6); cb.position.set(XW(s, 2.8), 1, 0); car.add(cb);
    [0, 2].forEach(dy => shaftX(s * 2.7, s * 4.0, dy, 0, car));
  });
  xbar(12, 0, -.45, .6, car);   // Querstrebe hinten am Schlitten: frei vom Köcher
  xbar(4, 4.3, .95, -1.1, car);   // Toggle-Mitnehmer vor dem Armgelenk
  const pad = mesh(new THREE.BoxGeometry(.12, 1.1, 1.1), mBlack); pad.position.set(5.36, 1.15, -1.1); car.add(pad);
  const head = new THREE.Group(); head.name = 'head'; head.position.set(0, 2.8, -.3); head.rotation.y = Math.PI; car.add(head);   // Greifer zeigt nach vorn
  clawV2(head, 1.29);
  unitObj(head, 0, -.7, 1.9);
  return function set(th, t = 0){
    arms.forEach(a => a.rotation.x = Math.PI + th);
    rot.forEach(([g, k, p]) => g.rotation.x = k * th + p);
    car.position.set(0, yP2 + L * Math.sin(th), z0 - L * Math.cos(th));
  };
}

/* Antriebsseite: Räder zwischen Außen- und Innenprofil, Zahnradzug im Zwischenraum */
function driveSide(s, cfg){
  const {xo, xi, xw, xg, wheels, motors} = cfg;
  wheels.forEach(([z, omni]) => {
    omni ? omniWheel(s * xw, Y, z, -1.4) : tractionWheel(s * xw, Y, z, -1.4);
    gear(60, s * xg, Y, z, -1.4);
    shaft(s * (xi - .25), s * (xo + .25), Y, z);
    spacer(s * (xg + .13), s * (xw - .5), Y, z); spacer(s * (xw + .5), s * (xo - T), Y, z); spacer(s * (xi + T), s * (xg - .13), Y, z);
    bearing(s * (xo + T / 2), Y, z, s); bearing(s * (xi - T / 2), Y, z, -s);
  });
  const ports = [];
  motors.forEach(([z, y]) => {
    ports.push(motor(s * (xi - T / 2), s, y, z));
    gear(84, s * xg, y, z, 1);
    shaft(s * (xi - .05), s * (xo + .25), y, z);
    spacer(s * (xg + .13), s * (xo - T), y, z);
    bearing(s * (xo + T / 2), y, z, s);
  });
  return ports;
}

/* Elektronik hochkant an den Seiten, damit die Mitte für Band und Köcher frei bleibt */
function brainV(x, y, z){
  const g = new THREE.Group(); g.add(mesh(new THREE.BoxGeometry(.95, 3.6, 4.7), mBlack));
  const scr = mesh(new THREE.PlaneGeometry(2.9, 1.95), mScreen); scr.rotation.y = -Math.PI / 2; scr.position.set(-.48, .3, -.5); g.add(scr);
  for (let i = 0; i < 10; i++) { const pp = mesh(new THREE.BoxGeometry(.25, .06, .3), mDark); pp.position.set(0, 1.81, -2 + i * .44); g.add(pp); const q = pp.clone(); q.position.y = -1.81; g.add(q); }
  const btn = mesh(new THREE.CylinderGeometry(.18, .18, .06, 20), mAmber); btn.rotation.z = Math.PI / 2; btn.position.set(-.49, 1.2, 1.85); g.add(btn);
  g.position.set(x, y, z); put(g, 'elektronik'); count('V5 Brain'); return g;
}
function batteryV(x, y, z){
  const g = new THREE.Group(); g.add(mesh(new THREE.BoxGeometry(1.2, 2.4, 4.3), mBlack));
  const lab = mesh(new THREE.BoxGeometry(.02, 1.2, 2.2), mRed); lab.position.x = .61; g.add(lab);
  [-1.6, 1.6].forEach(dz => { const c = mesh(new THREE.BoxGeometry(1.3, 2.6, .3), mDark); c.position.z = dz; g.add(c); });
  g.position.set(x, y, z); put(g, 'elektronik'); count('V5 Akku'); return g;
}
function tray(x0, x1, z0, z1, s){   // gedruckte Wanne unter dem Innenprofil, mit Aufhängung
  const t = petg(new THREE.BoxGeometry(Math.abs(x1 - x0), .12, z1 - z0)); t.position.set((x0 + x1) / 2, .72, (z0 + z1) / 2); put(t, 'aufnahme');
  [z0 + .3, z1 - .3].forEach(zh => { if (Math.abs(Math.abs(zh) - 3) < 1.35) zh = zh < 3 ? 1.6 : 4.4; const h = petg(new THREE.BoxGeometry(.12, .8, .4)); h.position.set(s * 5.38, 1.08, zh); put(h, 'aufnahme'); });   // Aufhängungen nur neben den Antriebsmotoren
}
function intakeStacker(){
  const A = LAYERS.aufnahme, M = LAYERS.motor;
  /* Förderband: unten an der offenen Rückseite, oben über dem Köcher */
  const z1 = 7.4, y1 = 1.1, z2 = -2.6, y2 = 8.3, len = Math.hypot(z1 - z2, y2 - y1), ang = Math.atan2(y2 - y1, z1 - z2);
  const band = new THREE.Group(); band.position.set(0, (y1 + y2) / 2, (z1 + z2) / 2); band.rotation.x = Math.PI + ang; A.add(band);
  [-1, 1].forEach(s => { const r = channel(25); r.position.set(XW(s, 1.9), 0, 0); band.add(r); });
  [-len / 2, len / 2].forEach(z => {
    const p = tpu(new THREE.CylinderGeometry(1, 1, 3.0, 32)); p.rotation.z = Math.PI / 2; p.position.z = z; band.add(p);
    shaftX(-2.6, 3.95, 0, z, band);
    const sp = mesh(new THREE.CylinderGeometry(.65, .65, .12, 20), mDark); sp.rotation.z = Math.PI / 2; sp.position.set(3.95, 0, z); band.add(sp); count('Kettenrad');
  });
  [-1.03, 1.03].forEach(y => { const b = mesh(new THREE.BoxGeometry(3.0, .1, len), mDark); b.position.y = y; band.add(b); });
  count('Förderband-Glied', Math.round((2 * len + 2 * Math.PI) / .75));
  for (let z = -len / 2 + .9; z < len / 2 - .5; z += 1.75) { const c = tpu(new THREE.BoxGeometry(3.0, .45, .12)); c.position.set(0, -1.3, z); band.add(c); }
  [-1, 1].forEach(s => { const post = vbar(4); post.position.set(XW(s, 1.9), 4.05, 0); A.add(post); screw(s * 2.15, 3.1, 0, [0, 1, 0], .3); });   // steht auf dem mittleren Querträger
  const defl = petg(new THREE.CylinderGeometry(1.55, 1.55, 3.2, 24, 1, true, Math.PI / 2, Math.PI / 2)); defl.rotation.z = Math.PI / 2; defl.position.set(0, y2, z2); A.add(defl);
  /* Fangtrichter an der Rückseite: richtet quer liegende Pins längs aus */
  [-1, 1].forEach(s => { const w = petg(new THREE.BoxGeometry(.12, 3.0, 2.5)); w.position.set(s * 3.65, 1.8, 8.175); w.rotation.y = Math.atan2(s * 2.9, 1.15); A.add(w); });
  /* Kipprolle oben: legt stehende Cups auf das Band */
  const roll = new THREE.Group(); roll.position.set(0, 6.6, 7.6); A.add(roll);
  const rc = tpu(new THREE.CylinderGeometry(.6, .6, 6.0, 28)); rc.rotation.z = Math.PI / 2; roll.add(rc);
  for (let i = 0; i < 8; i++) { const a = i / 8 * Math.PI * 2, f = mesh(new THREE.BoxGeometry(5.8, .45, .05), mTPU); f.position.set(0, Math.cos(a) * .82, Math.sin(a) * .82); f.rotation.x = -a; roll.add(f); }
  shaftX(-5.6, 5.6, 6.6, 7.6, A);
  const rsp = mesh(new THREE.CylinderGeometry(.9, .9, .12, 20), mDark); rsp.rotation.z = Math.PI / 2; rsp.position.set(3.95, 6.6, 7.6); A.add(rsp); count('Kettenrad');
  /* Antrieb: 1 Motor, Kette auf x = 3,95″ zu Kipprolle und beiden Bandrollen */
  const im = motorMesh(); im.rotation.y = Math.PI; im.position.set(4.69, 4.6, 5.9); M.add(im);
  const msp = mesh(new THREE.CylinderGeometry(.65, .65, .12, 20), mDark); msp.rotation.z = Math.PI / 2; msp.position.set(3.95, 4.6, 5.9); A.add(msp); count('Kettenrad');
  const loop = new THREE.CatmullRomCurve3([[3.95, 7.5, 7.6], [3.95, 4.6, 6.6], [3.95, 3.9, 5.9], [3.95, .4, 7.0], [3.95, .2, 7.9], [3.95, 2.1, 7.8], [3.95, 9.0, -2.0], [3.95, 9.3, -3.1], [3.95, 7.4, 8.4]].map(v => new THREE.Vector3(...v)), true);
  A.add(mesh(new THREE.TubeGeometry(loop, 160, .07, 6, true), mBlack)); count('Kettenglied', Math.round(loop.getLength() / .375));
  cable([[4.69, 4.9, 4.75], [3.4, 4.2, 3.6], [3.0, 3.7, -1.2], [0, 3.8, -1.7], [-1.6, 3.8, -1.7], [-2.62, 3.85, -.9], [-2.65, 3.9, .8], [-2.62, 3.95, 2.2]]);
  /* Köcher: Rückwand, Boden, Stützen zum vorderen Querträger; Schalter meldet „Cup da“ */
  const qz = -6, arc = petg(new THREE.CylinderGeometry(1.82, 1.82, 6.25, 36, 1, true, -Math.PI * 2 / 9, Math.PI * 4 / 9)); arc.position.set(0, .35 + 3.125, qz); A.add(arc);
  const base = petg(new THREE.CylinderGeometry(1.95, 1.95, .12, 36)); base.position.set(0, .3, qz); A.add(base);
  [-1, 1].forEach(s => { const st = petg(new THREE.BoxGeometry(.3, 2.15, .3)); st.position.set(s * 1.6, 1.375, -7.6); A.add(st); });   // von unten an den vorderen Querträger
  const bump = new THREE.Group(); bump.add(mesh(new THREE.BoxGeometry(.6, .5, .35), mBlack)); const nub = mesh(new THREE.BoxGeometry(.3, .25, .12), mRed); nub.position.z = -.2; bump.add(nub);
  bump.position.set(0, .7, qz + 1.9); A.add(bump); count('Bumper-Schalter');
  const opt = mesh(new THREE.BoxGeometry(.8, .6, .45), mBuy); opt.position.set(0, 4.6, qz + 2.1); put(opt, 'zukauf'); count('Optischer Sensor');
}

/* Runner-Aufnahme (Rev. E): gleiche Bauweise wie beim Stacker, aber steiler, weil der Rahmen nur 14,5″ lang ist.
   Hinten unten nimmt das Band vom Boden und aus dem Loader auf, oben fällt alles über den Abweiser in den Köcher vorne über dem Brain. */
function intakeRunner(){
  const A = LAYERS.aufnahme, M = LAYERS.motor, cx = 2.65;
  const z1 = 5.9, y1 = 1.1, z2 = -.3, y2 = 11.2, len = Math.hypot(z1 - z2, y2 - y1), ang = Math.atan2(y2 - y1, z1 - z2);
  const band = new THREE.Group(); band.position.set(0, (y1 + y2) / 2, (z1 + z2) / 2); band.rotation.x = Math.PI + ang; A.add(band);
  [-1, 1].forEach(s => { const r = channel(24); r.position.set(XW(s, 1.9), 0, 0); band.add(r); });
  [-len / 2, len / 2].forEach(z => {
    const p = tpu(new THREE.CylinderGeometry(1, 1, 3.0, 32)); p.rotation.z = Math.PI / 2; p.position.z = z; band.add(p);
    shaftX(-2.6, cx + .15, 0, z, band);
    const sp = mesh(new THREE.CylinderGeometry(.65, .65, .12, 20), mDark); sp.rotation.z = Math.PI / 2; sp.position.set(cx, 0, z); band.add(sp); count('Kettenrad');
  });
  [-1.03, 1.03].forEach(y => { const b = mesh(new THREE.BoxGeometry(3.0, .1, len), mDark); b.position.y = y; band.add(b); });
  count('Förderband-Glied', Math.round((2 * len + 2 * Math.PI) / .75));
  for (let z = -len / 2 + .9; z < len / 2 - .5; z += 1.75) { const c = tpu(new THREE.BoxGeometry(3.0, .45, .12)); c.position.set(0, -1.3, z); band.add(c); }
  /* Stützen: vorne zum Bandende, hinten zum Boden-Querstück */
  [-1, 1].forEach(s => {
    const post = vbar(14); post.position.set(XW(s, 1.9), 2.55 + 3.5, -.55); A.add(post); screw(s * 2.15, 2.6, -.55, [0, 1, 0], .3);
  });
  const defl = petg(new THREE.CylinderGeometry(1.55, 1.55, 3.2, 24, 1, true, Math.PI / 2, Math.PI / 2)); defl.rotation.z = Math.PI / 2; defl.position.set(0, y2, z2); A.add(defl);
  /* kurzer Fangtrichter hinten (bleibt im 15″-Startwürfel) */
  [-1, 1].forEach(s => { const w = petg(new THREE.BoxGeometry(.12, 2.6, 1.3)); w.position.set(s * 3.2, 1.6, 6.85); w.rotation.y = s * .55; A.add(w); });
  /* Kipprolle: legt stehende Cups und Loader-Einheiten auf das Band, Welle in den Lifttürmen gelagert */
  const ry = 6.6, rz = 6.45, roll = new THREE.Group(); roll.position.set(0, ry, rz); A.add(roll);
  const rc = tpu(new THREE.CylinderGeometry(.6, .6, 4.9, 28)); rc.rotation.z = Math.PI / 2; roll.add(rc);
  for (let i = 0; i < 8; i++) { const a = i / 8 * Math.PI * 2, f = mesh(new THREE.BoxGeometry(4.7, .4, .05), mTPU); f.position.set(0, Math.cos(a) * .8, Math.sin(a) * .8); f.rotation.x = -a; roll.add(f); }
  shaftX(-4.75, 4.75, ry, rz, A);
  const rsp = mesh(new THREE.CylinderGeometry(.9, .9, .12, 20), mDark); rsp.rotation.z = Math.PI / 2; rsp.position.set(cx, ry, rz); A.add(rsp); count('Kettenrad');
  /* Antrieb: 1 Motor rechts zwischen den Antriebsmotoren, Kette auf x = 2,65″ */
  const im = motorMesh(); im.rotation.y = Math.PI; im.position.set(cx + .77, 4.0, 2.2); M.add(im);
  const msp = mesh(new THREE.CylinderGeometry(.65, .65, .12, 20), mDark); msp.rotation.z = Math.PI / 2; msp.position.set(cx, 4.0, 2.2); A.add(msp); count('Kettenrad');
  const loop = new THREE.CatmullRomCurve3([[cx, 11.85, -.3], [cx, 11.25, -.95], [cx, 4.0, 1.55], [cx, 3.35, 2.2], [cx, .45, 5.9], [cx, 1.1, 6.55], [cx, 5.9, 7.2], [cx, 7.5, 6.45], [cx, 11.4, .3]].map(v => new THREE.Vector3(...v)), true);
  A.add(mesh(new THREE.TubeGeometry(loop, 160, .07, 6, true), mBlack)); count('Kettenglied', Math.round(loop.getLength() / .375));
  cable([[cx + .77, 5.1, 1.5], [cx + .4, 5.2, .4], [2.95, 3.8, -1.3], [2.55, 3.7, -2.9]]);
  /* Köcher vorn über dem Brain: Boden, niedrige Rückwand (Cup fällt hinein, Backen greifen seitlich daran vorbei), Stützen zum Querträger */
  const qz = -5.3, qy = 4.12, arc = petg(new THREE.CylinderGeometry(1.7, 1.7, 3.3, 36, 1, true, -Math.PI * 2 / 9, Math.PI * 4 / 9)); arc.position.set(0, qy + .06 + 1.65, qz); A.add(arc);
  const base = petg(new THREE.CylinderGeometry(1.95, 1.95, .12, 36)); base.position.set(0, qy, qz); A.add(base);
  [-1, 1].forEach(s => { const st = petg(new THREE.BoxGeometry(.3, qy - .06 - 2.57, .3)); st.position.set(s * 1.0, (qy - .06 + 2.57) / 2, -6.95); A.add(st); });
  const bump = new THREE.Group(); bump.add(mesh(new THREE.BoxGeometry(.6, .5, .35), mBlack)); const nub = mesh(new THREE.BoxGeometry(.3, .25, .36), mRed); nub.position.z = -.18; bump.add(nub);   // Taster ragt durch einen Schlitz der Rückwand
  bump.position.set(0, qy + .31, qz + 1.95); A.add(bump); count('Bumper-Schalter');
}

function buildStacker(){
  const cfg = {xo:8.5, xi:5.5, xw:7, xg:6.1, wheels:[[-6, true], [0, false], [6, true]], motors:[[-3, Y], [3, Y]]};
  const ports = [];
  [-1, 1].forEach(s => { rail(35, s * cfg.xo, 0, s); rail(35, s * cfg.xi, 0, s); ports.push(...driveSide(s, cfg)); });
  cross(35, -8.25); [8.25, 5.25].forEach(xx => [-1, 1].forEach(s => [-.25, .25].forEach(dz => screw(s * xx, Y + .5 + T * 2, -8.25 + dz, [0, 1, 0], .3))));
  [-1, 1].forEach(s => {   // Rückseite offen: kurze Querstücke außen, Portal für die Kipprolle
    cross(7, 8.25, s * 7.0); [8.25, 5.4].forEach(xx => screw(s * xx, Y + .5 + T * 2, 8.25, [0, 1, 0], .3));
    upright(11, s * 5.5, 5.8, 7.6, s);
  });
  xbar(23, 0, 8.6, 7.6, LAYERS.struktur);
  cross(23, 0); [-1, 1].forEach(s => [-.25, .25].forEach(dz => screw(s * 5.25, Y + .5 + T * 2, dz, [0, 1, 0], .3)));
  brainV(-3.0, 2.65, 4.6); tray(-5.4, -2.9, .25, 6.95, -1);
  radio(-3.3, 1.35, 1.2);
  batteryV(3.1, 2.1, 4.75); tray(2.45, 5.4, 2.6, 6.4, 1);
  imu(0, Y + .5 + T + .25, 0);
  trackingWheel(1.6, 1.375, 2.2, 'x'); trackingWheel(-1.6, 1.375, 1.1, 'z');
  [[2.75, .5, 2.2], [-2.45, .5, 1.7]].forEach(([x, z0, z1]) => { const a = mesh(new THREE.BoxGeometry(.5, T, Math.abs(z1 - z0)), mAlu); a.position.set(x, Y + .5 - T / 2, (z0 + z1) / 2); put(a, 'struktur'); const v = mesh(new THREE.BoxGeometry(.5, 1.3, T), mAlu); v.position.set(x, Y + .5 - .65, z1); put(v, 'struktur'); count('Alu-Winkel 1x1 (gekürzt)'); screw(x, Y + .5 + T * 1.5, z0 - .25, [0, 1, 0], .3); });
  const under = [[0, 3.8, -1.5], [-1.6, 3.8, -1.5], [-2.62, 3.85, -.8], [-2.65, 3.9, .8], [-2.62, 3.9, 2.3]];   // rechts → links unter dem Band durch (z = −1,5″)
  ports.forEach(p => cable(p.x < 0 ? [[p.x, p.y, p.z], [p.x, p.y + .9, p.z], [-3.7, 3.4, (p.z + 3) / 2], [-2.8, 3.3, 3.0]] : [[p.x, p.y, p.z], [p.x, p.y + 1, p.z], [3.4, 3.6, -1.5], ...under]));
  cable([[3.1, 3.4, 2.8], [2.9, 3.6, -1.5], ...under]);
  cable([[-3.3, 1.9, 1.6], [-3.1, 2.3, 2.0], [-2.9, 1.6, 2.4]]);
  intakeStacker();
  const set = dr4b({z0:-8.5, yP1:22.19, L:12, armHoles:25});
  arrow(-17.0);
  return {set, thMin:-40, thMax:45, start:24, title:'Stacker komplett', specs:[['Rahmen', '17,5″ × 17,5″, hinten offen'], ['Startvolumen', '≤ 24″ × 24″ × 24″'], ['Lift', 'DR4B, Turm vorne, 2 × 12″'], ['Liftwinkel', '−40° … +45°'], ['Greifarm', 'Parallelarm 4,7″ · 1 Motor 7 : 1'], ['Greifer', 'V-Backen, Zahnstange · 1 Motor'], ['Aufnahme', 'Band + Kipprolle hinten · 1 Motor'], ['Boden / Loader', 'beides über die Aufnahme, rückwärts an den Loader'], ['Übergabe', 'Köcher vorne innen, Cup + Pin'], ['Motoren', '9 = 4 Antrieb, 2 Lift, 3 Mechanik'], ['Fahrtempo', '≈ 1,49 m/s']]};
}
function buildRunner(){
  const cfg = {xo:7, xi:4.5, xw:5.75, xg:4.95, wheels:[[-3, true], [3, false]], motors:[[0, Y], [4.5, Y + 2.598]]};
  const ports = [];
  [-1, 1].forEach(s => { rail(29, s * cfg.xo, 0, s); rail(29, s * cfg.xi, 0, s); upright(6, s * cfg.xo, 4, 4.5, s); upright(6, s * cfg.xi, 4, 4.5, s);
    [cfg.xo, cfg.xi].forEach(xr => [-.25, .25].forEach(dz => screw(s * (xr - .25), Y + .5 + T * 1.5, 4.5 + dz * 0 + (dz > 0 ? .25 : -.25), [0, -1, 0], .2)));
    ports.push(...driveSide(s, cfg)); });
  cross(29, -6.75); [6.75, 4.25].forEach(xx => [-1, 1].forEach(s => [-.25, .25].forEach(dz => screw(s * xx, Y + .5 + T * 2, -6.75 + dz, [0, 1, 0], .3))));
  [-1, 1].forEach(s => { cross(5, 6.75, s * 5.75); [6.75, 4.75].forEach(xx => screw(s * xx, Y + .5 + T * 2, 6.75, [0, 1, 0], .3)); });   // Rückseite offen für die Aufnahme
  [[-4.6, 18], [1.2, 12]].forEach(([z, hx]) => { plate(hx, z, Y + .5 + T / 2); [-1, 1].forEach(s => [-.75, .75].forEach(dz => screw(s * 4.25, Y + .5 + T * 1.5, z + dz, [0, 1, 0], .3))); });
  brain(0, 3.55, -4.6);
  radio(-3.05, Y + .5 + T + .55, -5.0);
  battery(0, Y + .5 + T + .6, 1.2);
  imu(0, Y + .5 + T + 1.2 + .25, 1.2);
  ports.forEach(p => { const sx = Math.sign(p.x); cable(p.z > 2 ? [[p.x, p.y, p.z], [p.x, p.y + .8, p.z], [sx * 3.0, 5.4, 1.0], [sx * 2.95, 3.6, -1.2], [sx * 2.5, 3.6, -2.9]] : [[p.x, p.y, p.z], [p.x, p.y + .8, p.z], [sx * 2.95, 3.4, -1.2], [sx * 2.5, 3.6, -2.9]]); });   // außen an den Bandstützen vorbei zum Brain
  cable([[-1.2, 3.8, .0], [-1.0, 3.95, -1.6], [-1.6, 3.7, -2.9]]);
  intakeRunner();
  const set = fourBar({z0:6.3, yP1:11.5, L:10, armHoles:21});
  arrow(-10.4);
  return {set, thMin:-20, thMax:60, start:15, title:'Runner komplett', specs:[['Rahmen', '14,5″ × 14,5″, hinten offen'], ['Startvolumen', '≤ 15″ × 15″ × 15″'], ['Lift', 'Viergelenk, 10″'], ['Liftwinkel', '−20° … +60°'], ['Liftantrieb', '2 Motoren, 12T → 60T (5 : 1)'], ['Toggle', 'Mitnehmer am Schlitten'], ['Greifer', 'V-Backen, Zahnstange · 1 Motor'], ['Aufnahme', 'Band + Kipprolle hinten · 1 Motor'], ['Boden / Loader', 'beides über die Aufnahme, rückwärts an den Loader'], ['Übergabe', 'Köcher vorne über dem Brain'], ['Motoren', '8 = 4 Antrieb, 2 Lift, 2 Mechanik (1 Zukauf)'], ['Fahrtempo', '≈ 1,49 m/s']]};
}

const LAYER_KEYS = ['lift', 'aufnahme', 'struktur', 'antrieb', 'motor', 'verbindung', 'elektronik', 'zukauf'];
function build(fn){
  ROOT = new THREE.Group(); LAYERS = {}; BOM = {}; SPIN = [];
  LAYER_KEYS.forEach(k => { LAYERS[k] = new THREE.Group(); ROOT.add(LAYERS[k]); });
  const info = fn();
  ROOT.traverse(o => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
  return {root:ROOT, layers:LAYERS, bom:BOM, spin:SPIN, info};
}
return { stacker: () => build(buildStacker), runner: () => build(buildRunner), tileTex };
}
