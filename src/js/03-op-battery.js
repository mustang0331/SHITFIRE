/* ============================================================ OP */
function findOP() {
  let best = { x: 0, z: 0, h: -1 };
  for (let x = -3000; x <= 3000; x += 120)
    for (let z = -3000; z <= 3000; z += 120) {
      const h = H(x, z);
      if (h > best.h) best = { x, z, h };
    }
  for (let x = best.x - 120; x <= best.x + 120; x += 10)
    for (let z = best.z - 120; z <= best.z + 120; z += 10) {
      const h = H(x, z);
      if (h > best.h) best = { x, z, h };
    }
  return best;
}
const OP = { x: 0, z: 0, h: 0 };
const eye = { x: 0, y: 0, z: 0 };
// OP watchtower: the FO observes from a raised platform (+50 ft) so volcano
// terrain features don't mask the low ground. Eye height drives every LOS
// check, so lasing and crest-masking all inherit the elevation.
const towerMeshes = [];
{
  const legMat = new THREE.MeshLambertMaterial({ color: 0x6B5B43, flatShading: true });
  const deckMat = new THREE.MeshLambertMaterial({ color: 0x7A6A50, flatShading: true });
  const tH = CONFIG.CAMERA.towerHeight;
  for (const [lx, lz] of [[-1.6, -1.6], [1.6, -1.6], [-1.6, 1.6], [1.6, 1.6]]) {
    const m = new THREE.Mesh(new THREE.BoxGeometry(0.35, tH, 0.35), legMat);
    scene.add(m);
    towerMeshes.push({ m, ox: lx, oz: lz, oy: tH / 2 });
  }
  // cross-braces (visual only)
  for (let i = 0; i < 2; i++) {
    const m = new THREE.Mesh(new THREE.BoxGeometry(0.18, tH * 0.55, 0.18), legMat);
    m.rotation.z = i ? 0.42 : -0.42;
    scene.add(m);
    towerMeshes.push({ m, ox: 0, oz: (i ? 1 : -1) * 1.6, oy: tH * 0.4 });
  }
  const deck = new THREE.Mesh(new THREE.BoxGeometry(4.6, 0.3, 4.6), deckMat);
  scene.add(deck);
  towerMeshes.push({ m: deck, ox: 0, oz: 0, oy: tH });
  /* G1 — NO RAILING, deliberately. There were four BoxGeometry(4.6, 0.9, 0.12)
     panels here, which is not a handrail but a SOLID 0.9 m parapet ringing the
     deck, and it was blinding the observer downward.

     Measured. The parapet's top edge sat 1.00 m below eye level and 2.18 m out,
     so it masked everything more than 24.6 deg below horizontal. From an eye
     302 m above the OP's own ground that hid every piece of ground inside 659 m
     horizontal range. Worse in binoculars: to observe ground 600 m out the
     observer pitches down 26.7 deg, and at 9 deg of field the parapet then
     filled 73% of the picture. Even looking dead level it ate the bottom 8.9%
     of the screen.

     The deck alone still masks below 41.7 deg (it hides ground inside 339 m, and
     it must — the observer is standing on it), so removing the parapet gives
     back a full ring of ground from 339 m out to 659 m. The tower still reads as
     a structure from its legs, braces and deck edge.

     Do not put a railing back without re-checking those numbers. A real OP tower
     would have one; this trainer's tower is 300 m tall, which no real one is,
     and at that height a parapet stops being a safety feature and becomes a
     blindfold over the near half of the observer's sector. */
}
function placeOP() {
  const b = findOP();
  OP.x = b.x; OP.z = b.z; OP.h = b.h;
  eye.x = OP.x;
  eye.y = OP.h + CONFIG.CAMERA.towerHeight + CONFIG.CAMERA.eyeHeight;
  eye.z = OP.z;
  camera.position.set(eye.x, eye.y, eye.z);
  for (const e of towerMeshes)
    e.m.position.set(OP.x + e.ox, OP.h + e.oy, OP.z + e.oz);
}
placeOP();

/* ============================================================ FIRING BATTERY */
// Fixed emplacement on flat low ground behind the OP. Cosmetic + map marker +
// time-of-flight range; the gun line itself stays a black box per spec.
function findBattery() {
  let best = null, bestScore = 1e9;
  for (let x = -3400; x <= 3400; x += 150)
    for (let z = -3400; z <= 3400; z += 150) {
      const h = H(x, z);
      if (h < 4 || h > 30) continue;
      const d = dist2(OP.x, OP.z, x, z);
      if (d < 1200 || d > 2800) continue;
      if (Math.abs(H(x + 40, z) - h) > 3 || Math.abs(H(x, z + 40) - h) > 3) continue;
      const score = Math.abs(d - 1800);
      if (score < bestScore) { bestScore = score; best = { x, z }; }
    }
  return best || { x: OP.x + 800, z: OP.z + 800 };
}
const BATTERY = { x: 0, z: 0 };
const batteryMeshes = [];
{
  const gunMat = new THREE.MeshLambertMaterial({ color: 0x3E4A34, flatShading: true });
  const gunGeo = new THREE.BoxGeometry(1.6, 1.4, 5.8);
  for (let i = 0; i < 2; i++) {
    const m = new THREE.Mesh(gunGeo, gunMat);
    m.rotation.y = 0.5 + i * 0.15;
    scene.add(m);
    batteryMeshes.push({ m, ox: i * 16 - 8, oz: i * 7, oy: 0.8 });
  }
  const tent = new THREE.Mesh(new THREE.BoxGeometry(6, 2.6, 4.5),
    new THREE.MeshLambertMaterial({ color: 0x6B6A52, flatShading: true }));
  scene.add(tent);
  batteryMeshes.push({ m: tent, ox: -14, oz: 10, oy: 1.3 });
}
function placeBattery() {
  const b = findBattery();
  BATTERY.x = b.x; BATTERY.z = b.z;
  for (const e of batteryMeshes) {
    const x = BATTERY.x + e.ox, z = BATTERY.z + e.oz;
    e.m.position.set(x, H(x, z) + e.oy, z);
  }
}
placeBattery();

