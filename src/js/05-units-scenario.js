/* ============================================================ UNITS (friendly compound + enemy) */
const units = { troops: [], vehicles: [], huts: [], flashes: [], smokePuffs: [], flag: null };
{
  const vehMat = new THREE.MeshLambertMaterial({ color: 0x4A4136, flatShading: true });
  const hutMat = new THREE.MeshLambertMaterial({ color: 0xC9B891, flatShading: true });
  /* These two are MIL-RELATION REFERENCE OBJECTS, not scenery. The mil card
     tells the observer a truck is 5 m long and a hut is 3 m high and range is
     size / mils * 1000, so if the geometry disagrees with the card the trainer
     teaches a range error. The truck was 4.6 m (a 5 m call reads 8.7% long) and
     the hut 3.2 m (a 3 m call reads 6.3% short). Rounded to the card, not the
     other way round: a real observer works from remembered round figures, and
     the world should reward that rather than punish it.
     KEEP THESE EQUAL TO THE CARD. */
  const vehGeo = new THREE.BoxGeometry(5.0, 2.1, 2.3);
  const hutGeo = new THREE.BoxGeometry(7, 3.0, 5);
  for (let i = 0; i < 8; i++) { const m = makeFigure('enemy'); scene.add(m); units.troops.push(m); }
  for (let i = 0; i < 4; i++) { const m = new THREE.Mesh(vehGeo, vehMat); scene.add(m); units.vehicles.push(m); }
  units.vehMatAlive = vehMat;
  units.vehMatDead = new THREE.MeshLambertMaterial({ color: 0x211D18, flatShading: true });
  for (let i = 0; i < 3; i++) { const m = new THREE.Mesh(hutGeo, hutMat); scene.add(m); units.huts.push(m); }
  const pole = new THREE.Mesh(new THREE.BoxGeometry(0.3, 7, 0.3),
    new THREE.MeshLambertMaterial({ color: 0x8a8a80 }));
  const banner = new THREE.Mesh(new THREE.BoxGeometry(2.4, 1.4, 0.15),
    new THREE.MeshLambertMaterial({ color: 0x2E5FA0 }));
  scene.add(pole); scene.add(banner);
  units.flag = { pole, banner };
  const flashMat = new THREE.SpriteMaterial({ color: 0xFFE9A0, blending: THREE.AdditiveBlending,
    depthWrite: false, transparent: true });
  for (let i = 0; i < 6; i++) {
    const s = new THREE.Sprite(flashMat); s.scale.setScalar(4); s.visible = false;
    scene.add(s); units.flashes.push({ s, phase: i * 0.31 });
  }
  for (let i = 0; i < 4; i++) {
    const mat = new THREE.MeshLambertMaterial({ color: 0x8A877E, transparent: true,
      opacity: 0, flatShading: true, depthWrite: false });
    const m = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 0), mat);
    scene.add(m); units.smokePuffs.push({ m, off: i * 2.5, bx: 0, by: 0, bz: 0 });
  }
  // known-point marker posts
  units.kpPosts = [];
  const postMat = new THREE.MeshLambertMaterial({ color: 0xE8E4D8, flatShading: true });
  for (let i = 0; i < 2; i++) {
    const m = new THREE.Mesh(new THREE.BoxGeometry(0.5, 2.8, 0.5), postMat);
    m.visible = false;
    scene.add(m); units.kpPosts.push(m);
  }
  // friendly squad (assault scenario)
  units.fSquad = [];
  for (let i = 0; i < 6; i++) {
    const m = makeFigure('friendly');
    m.visible = false;
    scene.add(m); units.fSquad.push(m);
  }
  // bunker
  units.bunker = new THREE.Mesh(new THREE.BoxGeometry(7, 2.6, 7),
    new THREE.MeshLambertMaterial({ color: 0x6E665C, flatShading: true }));
  units.bunker.visible = false;
  scene.add(units.bunker);
  // vehicle burn flames
  units.flames = [];
  const flameMat = new THREE.SpriteMaterial({ color: 0xFF9A40,
    blending: THREE.AdditiveBlending, depthWrite: false, transparent: true });
  for (let i = 0; i < 4; i++) {
    const s = new THREE.Sprite(flameMat);
    s.scale.setScalar(3.5); s.visible = false;
    scene.add(s); units.flames.push(s);
  }
  // optics signatures — see visTag() above. A running engine is the second
  // hottest thing on the island after a man; a burning wreck is the first.
  visTag(vehMat,             { nvg: 0.14, th: [0.90, 0.90, 0.90] });  // painted metal: NIR-dark
  visTag(units.vehMatDead,   { nvg: 1.00, th: [1.00, 1.00, 1.00] });
  visTag(hutMat,             { nvg: 0.40, th: [0.34, 0.34, 0.34] });
  visTag(flashMat,           { nvg: 1.00, th: [1.00, 1.00, 1.00] });
  visTag(flameMat,           { nvg: 1.00, th: [1.00, 1.00, 1.00] });
  visTag(units.bunker.material, { nvg: 0.30, th: [0.30, 0.30, 0.30] });
  visTag(postMat,            { nvg: 0.85, th: [0.16, 0.16, 0.16] });  // painted marker post
}
let enemyAlive = true;

/* ============================================================ SCENARIO */
let Scenario = null;
let scenarioT0 = 0;   // sim time the current scenario began (time-to-initiate)

function genScenario(type, seed) {
  const rng = mulberry32((seed * 2654435761 ^ CONFIG.SEED.terrain) >>> 0);
  const M = CONFIG.MISSION;
  const S = { seed, type, difficulty: DIFFICULTY, rng, kps: [], friendlies: [],
              effectRadius: M.effectRadius, hitsNeed: M.hitsToNeutralize,
    alerted: 0, dispersed: 0 };
  const OPT = (activeChapter && activeChapter.scn) || {};

  function findSpot(rMin, rMax, hMin, hMax, needLOS) {
    for (let tries = 0; tries < 300; tries++) {
      const az = rng() * Math.PI * 2;
      const range = lerp(rMin, rMax, rng());
      const x = OP.x + Math.sin(az) * range, z = OP.z - Math.cos(az) * range;
      const h = H(x, z);
      if (h < hMin || h > hMax) continue;
      if (needLOS && !hasLOS(eye.x, eye.y, eye.z, x, h + 2, z)) continue;
      // keep generated targets clear of civilian villages (collateral = fail)
      if (WORLD.villages.some(v => dist2(x, z, v.x, v.z) < v.r + 120)) continue;
      return { x, z };
    }
    return null;
  }
  const brgTo = p => fmtMils(Math.round(radToMils(azTo(OP.x, OP.z, p.x, p.z)) / 100) * 100 % 6400);

  if (type === 'strongpoint') {
    let compound = null, enemy = null;
    for (let tries = 0; tries < 300 && !enemy; tries++) {
      const c = findSpot(M.targetRange[0], M.targetRange[1], 4, 999, true);
      if (!c) break;
      const eAz = rng() * Math.PI * 2;
      const eOff = lerp(M.enemyOffset[0], M.enemyOffset[1], rng());
      const ex = c.x + Math.sin(eAz) * eOff, ez = c.z - Math.cos(eAz) * eOff;
      if (H(ex, ez) < 3) continue;
      if (!hasLOS(eye.x, eye.y, eye.z, ex, H(ex, ez) + 2, ez)) continue;
      compound = c; enemy = { x: ex, z: ez };
    }
    if (!enemy) {
      const az = rng() * Math.PI * 2;
      compound = { x: OP.x + Math.sin(az) * 2200, z: OP.z - Math.cos(az) * 2200 };
      enemy = { x: compound.x + 200, z: compound.z };
    }
    S.compound = compound;
    S.enemy = enemy;
    S.friendlies = [{ x: compound.x, z: compound.z, r: M.fratricideRadius }];
    S.brief = `Friendly strongpoint, grid ${gridOf(compound.x, compound.z)} (grid bearing ~${brgTo(compound)} mils), is under assault. Locate the attackers pressing it and destroy them without hitting the friendlies.`;
  } else if (type === 'troops') {
    if (OPT.mask) {
      // crest-masked: the target must NOT have line of sight from the OP
      let spot = null;
      for (let tries = 0; tries < 400 && !spot; tries++) {
        const az = rng() * Math.PI * 2, range = lerp(1200, 2600, rng());
        const x = OP.x + Math.sin(az) * range, z = OP.z - Math.cos(az) * range;
        const h = H(x, z);
        if (h < 3 || h > 60) continue;
        if (hasLOS(eye.x, eye.y, eye.z, x, h + 2, z)) continue;
        if (WORLD.villages.some(v => dist2(x, z, v.x, v.z) < v.r + 120)) continue;
        spot = { x, z };
      }
      S.enemy = spot || findSpot(M.targetRange[0], M.targetRange[1], 2, 60, false) ||
                { x: OP.x + 2000, z: OP.z };
      S.brief = `Enemy infantry assembling IN DEFILADE near grid ${gridOf(S.enemy.x, S.enemy.z)} — you will not see them or your bursts from the tower. Fight it on the map: plot, fire, listen, adjust off the smoke over the crest.`;
    } else {
      S.enemy = findSpot(M.targetRange[0], M.targetRange[1], 2, 14, true) ||
                { x: OP.x + 2000, z: OP.z };
      S.brief = `Enemy infantry in the open, reported grid bearing ~${brgTo(S.enemy)} mils from your OP. A gift. Do not waste it.`;
    }
  } else if (type === 'wreck') {
    S.enemy = findSpot(1100, 2100, 1, 6, true) || { x: OP.x + 1500, z: OP.z };
    S.effectRadius = 30;
    S.hitsNeed = 2;
    S.brief = `Training target: a derelict landing craft on the beach, grid bearing ~${brgTo(S.enemy)} mils. It cannot shoot back. Neither could the last observer.`;
  } else if (type === 'raid') {
    // raiders MUST be near a village — try every village with LOS from the
    // OP, then relax LOS (fight it on the map) before any random fallback
    function nearVillage(v, needLOS) {
      for (let tries = 0; tries < 300; tries++) {
        const az = rng() * Math.PI * 2, r = 130 + rng() * 70;
        const x = v.x + Math.sin(az) * r, z = v.z - Math.cos(az) * r;
        if (H(x, z) < 2) continue;
        if (needLOS && !hasLOS(eye.x, eye.y, eye.z, x, H(x, z) + 2, z)) continue;
        if (v.huts.some(hh => dist2(x, z, hh.x, hh.z) < 90)) continue;
        return { x, z };
      }
      return null;
    }
    let enemy = null, vil = null, seen = true;
    for (const v of WORLD.villages) {
      enemy = nearVillage(v, true);
      if (enemy) { vil = v; break; }
    }
    if (!enemy) for (const v of WORLD.villages) {
      enemy = nearVillage(v, false);
      if (enemy) { vil = v; seen = false; break; }
    }
    S.enemy = enemy || findSpot(1200, 2800, 2, 60, true) || { x: OP.x + 1800, z: OP.z };
    S.village = vil;
    S.brief = `Raiders are hitting the civilian village${vil ? ' of ' + vil.name : ''}, grid ${gridOf(S.enemy.x, S.enemy.z)} area.` +
      (seen ? '' : ' The village is masked from your tower — work it off the map and the smoke.') +
      ' The huts are NO-STRIKE — one round in the village fails the mission. Cut the raiders down without touching it.';
  } else if (type === 'bunker') {
    S.enemy = findSpot(1200, 3000, 22, 140, true) ||
              findSpot(1200, 3000, 8, 999, true) || { x: OP.x + 1800, z: OP.z };
    S.effectRadius = 35;
    S.hitsNeed = 2;
    S.brief = `Dug-in bunker reported on high ground, grid bearing ~${brgTo(S.enemy)} mils. The effect radius is tight — put it on the roof.`;
  } else if (type === 'convoy') {
    let path = null;
    for (let tries = 0; tries < 400 && !path; tries++) {
      const s0 = findSpot(1400, 3200, 1, 10, false);
      if (!s0) break;
      const paz = rng() * Math.PI * 2;
      const dx = Math.sin(paz), dz = -Math.cos(paz);
      let ok = true, visible = 0;
      for (let d = 0; d <= 1800; d += 150) {
        const x = s0.x + dx * d, z = s0.z + dz * d;
        const h = H(x, z);
        if (h < 0.5 || h > 16) { ok = false; break; }
        if (hasLOS(eye.x, eye.y, eye.z, x, h + 2, z)) visible++;
      }
      if (ok && visible >= 8) path = { sx: s0.x, sz: s0.z, dx, dz, len: 1800 };
    }
    if (!path) path = { sx: OP.x + 1500, sz: OP.z, dx: 0, dz: -1, len: 1800 };
    S.path = path;
    S.speed = 3.5;
    S.veh = [0, 1, 2, 3].map(i => ({ off: i * 40, dead: false, x: path.sx, z: path.sz }));
    S.t0 = sim.now - 220 / S.speed;  // column already strung out on the road
    S.enemy = { x: path.sx, z: path.sz };
    S.escaped = false;
    // pit stop: halt at the facility nearest the route (fuel/ammo/airfield),
    // else a plain crew halt at a seeded point. 1-3 minutes, seeded.
    let stopD = lerp(0.35, 0.6, rng()) * path.len, stopName = 'a crew halt';
    let bestFD = 320;
    for (const f of WORLD.facilities) {
      if (f.kind !== 'fuel' && f.kind !== 'ammo' && f.kind !== 'airfield') continue;
      const t = clamp((f.x - path.sx) * path.dx + (f.z - path.sz) * path.dz, 300, path.len - 300);
      const fd = dist2(f.x, f.z, path.sx + path.dx * t, path.sz + path.dz * t);
      if (fd < bestFD) { bestFD = fd; stopD = t; stopName = 'the ' + f.name.toLowerCase(); }
    }
    const [sLo, sHi] = CONFIG.WORLD.convoyStopDur;
    S.stop = { d: Math.max(stopD, 320), dur: lerp(sLo, sHi, rng()), tArr: null,
               resumed: false, name: stopName };
    S.brief = `Enemy convoy, four vehicles, moving on the low ground near grid ${gridOf(path.sx, path.sz)}. Lead the column and time your fire for effect — or catch them when they pull in somewhere. If the head of the column runs off the end of the road, they are gone.`;
  } else if (type === 'assault') {
    S.enemy = findSpot(1600, 3000, 4, 60, true) || { x: OP.x + 2000, z: OP.z };
    const oAz = azTo(S.enemy.x, S.enemy.z, OP.x, OP.z);  // objective -> OP side
    const off = OPT.fClose ? 520 : 900;                  // fClose: friendlies start inside your sheaf distances
    S.fStart = { x: S.enemy.x + Math.sin(oAz) * off, z: S.enemy.z - Math.cos(oAz) * off };
    S.fAdvAz = azTo(S.fStart.x, S.fStart.z, S.enemy.x, S.enemy.z);
    S.fSpeed = 0.7;
    S.fHold = OPT.fClose ? 120 : 180;
    S.fAdvMax = off - S.fHold;
    S.ft0 = sim.now;
    S.brief = `Friendly infantry advancing from grid ${gridOf(S.fStart.x, S.fStart.z)} onto an enemy-held objective, grid bearing ~${brgTo(S.enemy)} mils.` +
      (OPT.fClose
        ? ' The lines are TANGLED — friendlies are already inside normal safe distances and still moving. DANGER CLOSE proword, creeping corrections, zero slack.'
        : ' This WILL be danger close: your call must include the proword DANGER CLOSE, and a short round lands on our own people.');
  }
  if (OPT.effR) S.effectRadius = OPT.effR;

  // registration / known points for shift missions
  for (let k = 0; k < 2; k++) {
    for (let tries = 0; tries < 200; tries++) {
      const az = rng() * Math.PI * 2;
      const range = 900 + rng() * 2100;
      const x = OP.x + Math.sin(az) * range, z = OP.z - Math.cos(az) * range;
      if (H(x, z) < 3) continue;
      if (!hasLOS(eye.x, eye.y, eye.z, x, H(x, z) + 2, z)) continue;
      if (S.kps.length && dist2(S.kps[0].x, S.kps[0].z, x, z) < 800) continue;
      if (dist2(x, z, S.enemy.x, S.enemy.z) < 400) continue;
      S.kps.push({ id: String(1001 + k), name: KP_NAMES[(seed + k) % KP_NAMES.length], x, z });
      break;
    }
  }
  return S;
}

// Current world positions of all friendly elements (for fratricide + danger close).
function friendlyPositions() {
  const S = Scenario;
  if (!S) return [];
  if (S.type === 'strongpoint') return S.friendlies;
  if (S.type !== 'assault') return [];
  const adv = Math.min(S.fSpeed * (sim.now - S.ft0), S.fAdvMax);
  const fx = Math.sin(S.fAdvAz), fz = -Math.cos(S.fAdvAz);
  const rx = Math.cos(S.fAdvAz), rz = Math.sin(S.fAdvAz);
  const out = [];
  for (let i = 0; i < 6; i++) {
    const l = (i - 2.5) * 8;
    out.push({ x: S.fStart.x + fx * adv + rx * l,
               z: S.fStart.z + fz * adv + rz * l, r: 40 });
  }
  return out;
}

// Per-frame scenario dynamics (convoy movement, advancing friendlies).
function updateScenario() {
  const S = Scenario;
  if (!S) return;
  if (S.type === 'convoy') {
    // piecewise time->distance with the pit stop in the middle
    const raw = S.speed * (sim.now - S.t0);
    let dHead;
    if (raw < S.stop.d) dHead = raw;
    else {
      if (S.stop.tArr === null) {
        S.stop.tArr = sim.now;
        log('', `The column has pulled in at ${S.stop.name}. Engines idling. This window will not stay open.`, 'sys');
      }
      const halted = sim.now - S.stop.tArr;
      if (halted < S.stop.dur) dHead = S.stop.d;
      else {
        if (!S.stop.resumed) {
          S.stop.resumed = true;
          log('', 'The column is rolling again.', 'sys');
        }
        dHead = S.stop.d + S.speed * (halted - S.stop.dur);
      }
    }
    dHead = clamp(dHead, 0, S.path.len);
    let cxSum = 0, czSum = 0, alive = 0;
    for (let i = 0; i < S.veh.length; i++) {
      const v = S.veh[i];
      if (!v.dead) {
        const d = clamp(dHead - v.off, 0, S.path.len);
        v.x = S.path.sx + S.path.dx * d;
        v.z = S.path.sz + S.path.dz * d;
        cxSum += v.x; czSum += v.z; alive++;
      }
      const m = units.vehicles[i];
      m.position.set(v.x, H(v.x, v.z) + 1.05, v.z);
      m.rotation.y = Math.atan2(S.path.dx, S.path.dz) + Math.PI / 2;
      const fl = units.flames[i];
      fl.visible = v.dead && ((sim.now * 7 + i) % 1) < 0.6;
      if (v.dead) fl.position.set(v.x, H(v.x, v.z) + 2.6, v.z);
    }
    if (alive) { S.enemy.x = cxSum / alive; S.enemy.z = czSum / alive; }
    if (dHead >= S.path.len && !S.escaped) {
      S.escaped = true;
      if (S.veh.filter(v => !v.dead).length > 1) {
        log('', 'The convoy has reached the end of the road and dispersed. Mission window closed.', 'sys');
        if (mission && !mission.done) {
          mission.done = true; mission.failReason = 'escaped'; mission.tEnd = sim.now;
          schedule(sim.now + 2, showAAR);
        }
      }
    }
  } else if (S.type === 'assault') {
    const adv = Math.min(S.fSpeed * (sim.now - S.ft0), S.fAdvMax);
    const fx = Math.sin(S.fAdvAz), fz = -Math.cos(S.fAdvAz);
    const rx = Math.cos(S.fAdvAz), rz = Math.sin(S.fAdvAz);
    for (let i = 0; i < units.fSquad.length; i++) {
      const l = (i - 2.5) * 8;
      const x = S.fStart.x + fx * adv + rx * l;
      const z = S.fStart.z + fz * adv + rz * l;
      units.fSquad[i].position.set(x, H(x, z), z);   // figure origin is at the feet
    }
  }
}

function placeUnits() {
  const S = Scenario;
  const rng = mulberry32((S.seed * 68168169 + 17) >>> 0);
  // hide/reset everything mission-specific
  units.troops.forEach(m => m.visible = false);
  units.vehicles.forEach(m => {
    m.visible = false; visSetMat(m, units.vehMatAlive);
    m.rotation.set(0, 0, 0); m.scale.set(1, 1, 1);
  });
  units.huts.forEach(m => m.visible = false);
  units.flag.pole.visible = units.flag.banner.visible = false;
  units.flashes.forEach(f => f.s.visible = false);
  units.fSquad.forEach(m => m.visible = false);
  units.bunker.visible = false;
  visSetColor(units.bunker, 0x6E665C);
  units.flames.forEach(s => s.visible = false);
  units.smokePuffs.forEach(p => { p.on = false; p.m.visible = false; });
  enemyAlive = true;

  function troopCluster(cx, cz, n, spread) {
    units.troops.forEach((m, i) => {
      if (i >= n) return;
      const a = rng() * Math.PI * 2, r = 4 + rng() * spread;
      const x = cx + Math.sin(a) * r, z = cz + Math.cos(a) * r;
      m.visible = true;
      m.position.set(x, H(x, z), z);          // figure origin is at the feet
      m.rotation.set(0, rng() * Math.PI * 2, 0);
      if (i < units.flashes.length)
        units.flashes[i].s.position.set(x, H(x, z) + 1.6, z);
    });
  }

  if (S.type === 'strongpoint') {
    troopCluster(S.enemy.x, S.enemy.z, 8, 22);
    for (let i = 0; i < 2; i++) {
      const m = units.vehicles[i];
      const a = rng() * Math.PI * 2, r = 12 + rng() * 20;
      const x = S.enemy.x + Math.sin(a) * r, z = S.enemy.z + Math.cos(a) * r;
      m.visible = true;
      m.position.set(x, H(x, z) + 1.05, z);
      m.rotation.set(0, rng() * Math.PI * 2, 0);
    }
    const c = S.compound;
    const hutOff = [[0, 0], [10, 7], [-9, 8]];
    units.huts.forEach((m, i) => {
      const x = c.x + hutOff[i][0], z = c.z + hutOff[i][1];
      m.visible = true;
      m.position.set(x, H(x, z) + 1.6, z);
      m.rotation.set(0, (i * 0.7) % Math.PI, 0);
    });
    units.flag.pole.visible = units.flag.banner.visible = true;
    const fx = c.x + 4, fz = c.z - 5, fh = H(fx, fz);
    units.flag.pole.position.set(fx, fh + 3.5, fz);
    units.flag.banner.position.set(fx + 1.2, fh + 6.3, fz);
    units.smokePuffs.forEach(p => {
      p.on = true; p.bx = c.x - 6; p.by = H(c.x - 6, c.z + 3); p.bz = c.z + 3;
    });
  } else if (S.type === 'troops') {
    troopCluster(S.enemy.x, S.enemy.z, 8, 26);
  } else if (S.type === 'bunker') {
    const e = S.enemy;
    units.bunker.visible = true;
    units.bunker.position.set(e.x, H(e.x, e.z) + 1.3, e.z);
    troopCluster(e.x + 14, e.z + 9, 3, 8);
    units.flashes[0].s.position.set(e.x, H(e.x, e.z) + 2.4, e.z);
  } else if (S.type === 'convoy') {
    units.vehicles.forEach(m => m.visible = true);  // positions driven per-frame
  } else if (S.type === 'assault') {
    troopCluster(S.enemy.x, S.enemy.z, 8, 20);
    units.fSquad.forEach(m => m.visible = true);    // positions driven per-frame
  } else if (S.type === 'wreck') {
    const m = units.vehicles[0];                    // derelict landing craft
    m.visible = true;
    m.position.set(S.enemy.x, H(S.enemy.x, S.enemy.z) + 0.9, S.enemy.z);
    m.rotation.set(0, rng() * Math.PI, 0.35);
    m.scale.set(2.2, 1.3, 1.6);
    visSetMat(m, units.vehMatDead);
  } else if (S.type === 'raid') {
    troopCluster(S.enemy.x, S.enemy.z, 8, 18);
    const m = units.vehicles[0];
    m.visible = true;
    const vx = S.enemy.x + 14, vz = S.enemy.z - 10;
    m.position.set(vx, H(vx, vz) + 1.05, vz);
    m.rotation.set(0, rng() * Math.PI * 2, 0);
  }
  units.kpPosts.forEach((m, i) => {
    const kp = S.kps[i];
    m.visible = !!kp;
    if (kp) m.position.set(kp.x, H(kp.x, kp.z) + 1.4, kp.z);
  });
  legibilityPass(eye.x, eye.y, eye.z);
}

/* ---- distance-compensated silhouettes ---------------------------------------
   Hold a floor on apparent size: a figure is scaled until it subtends at least
   CONFIG.GFX.legFloorMil, clamped at legMaxScale and never applied inside
   legOnset metres. VISUAL ONLY — the mesh transform is read by nothing else.
   Effect radius, fratricide, and collateral all measure against Scenario.enemy,
   friendlyPositions(), and WORLD.civs[].x/z, which this never touches.

   Caveat worth knowing: this deliberately breaks mil-relation ranging off a MAN
   (a scaled figure no longer subtends its true height). The mil card lists huts,
   trucks, the mast and the tower instead — a 1.8 m man at 2 km is 0.9 mils, too
   small to measure on a 1-mil reticle anyway. Deviation measurement is an angle
   BETWEEN two points and is unaffected, so 12b's OT-factor workflow is intact.

   Takes an explicit camera position so the AAR bird's-eye plot can size figures
   for its own range instead of inheriting the observer's. Allocation-free. */
function figScale(d, baseH) {
  const G = CONFIG.GFX;
  if (!G.legibility || d < G.legOnset) return 1;
  const s = (G.legFloorMil * d / 1000) / baseH;
  return s < 1 ? 1 : (s > G.legMaxScale ? G.legMaxScale : s);
}
function _legOne(m, cx, cy, cz) {
  if (!m || !m.visible) return;
  const dx = m.position.x - cx, dy = m.position.y - cy, dz = m.position.z - cz;
  m.scale.setScalar(figScale(Math.sqrt(dx * dx + dy * dy + dz * dz),
                             m.userData.figH || FIG.h.troop));
  // the contact disc is a child, so counter-rotate it to stay flat on the
  // ground when a casualty or a prone man lays the parent over
  const disc = m.userData.disc;
  if (disc) disc.rotation.z = -m.rotation.z;
}
function legibilityPass(cx, cy, cz) {
  for (let i = 0; i < units.troops.length; i++) _legOne(units.troops[i], cx, cy, cz);
  for (let i = 0; i < units.fSquad.length; i++) _legOne(units.fSquad[i], cx, cy, cz);
  for (let i = 0; i < WORLD.civs.length; i++)   _legOne(WORLD.civs[i].m, cx, cy, cz);
}

