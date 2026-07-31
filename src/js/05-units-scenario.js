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

/* WORLD3 — path helpers. A scenario path is either the legacy straight line
   { sx, sz, dx, dz, len } or a road polyline { pts, cum, len }. The line
   branch reproduces the legacy expressions exactly, so fixed-seed chapters
   (3.1's authored fallback line, E.2's wade) keep byte-identical geometry. */
function pathPoint(p, d) {
  if (!p.pts) return { x: p.sx + p.dx * d, z: p.sz + p.dz * d };
  let i = 1;
  while (i < p.cum.length - 1 && p.cum[i] < d) i++;
  const seg = p.cum[i] - p.cum[i - 1] || 1;
  const t = clamp((d - p.cum[i - 1]) / seg, 0, 1);
  return { x: lerp(p.pts[i - 1].x, p.pts[i].x, t),
           z: lerp(p.pts[i - 1].z, p.pts[i].z, t) };
}
function pathDir(p, d) {
  if (!p.pts) return { dx: p.dx, dz: p.dz };
  let i = 1;
  while (i < p.cum.length - 1 && p.cum[i] < d) i++;
  const vx = p.pts[i].x - p.pts[i - 1].x, vz = p.pts[i].z - p.pts[i - 1].z;
  const l = Math.hypot(vx, vz) || 1;
  return { dx: vx / l, dz: vz / l };
}
// current head-of-column arc distance; read-only twin of updateScenario's
// piecewise time->distance (state transitions/logs stay in updateScenario)
function convoyHeadD() {
  const S = Scenario;
  const raw = S.speed * (sim.now - S.t0);
  if (raw < S.stop.d || S.stop.tArr === null) return clamp(raw, 0, S.path.len);
  const halted = sim.now - S.stop.tArr;
  if (halted < S.stop.dur) return S.stop.d;
  return clamp(S.stop.d + S.speed * (halted - S.stop.dur), 0, S.path.len);
}

/* TEMPO3 — the diegetic mission clock. Returns { t, label } for scenarios
   that really have a deadline, else null. Both deadlines below already
   existed in code (3.1 convoy escape, E.2 landfall) and were invisible until
   already missed; this reads them out, it does not move them. Read-only:
   derived every call from the same state updateScenario advances, so the
   surfaced number and the actual fail trigger cannot drift apart. */
function scenarioDeadline() {
  const S = Scenario;
  if (!S) return null;   // ENEMY1 — battery has no S.path; the guard is per-branch now
  if (S.type === 'battery' && S.bty && enemyAlive) {
    /* ENEMY1 — the next volley is the deadline TEMPO3's row named in advance.
       Suppression visibly pushes it out; a dead battery clears the clock. */
    return { t: Math.max(0, S.bty.next - (sim.now - scenarioT0)), label: 'VOLLEY' };
  }
  if (S.type === 'defense' && enemyAlive && !S.overrun &&
      sim.now >= (S.suppressedUntil || 0)) {
    /* ENEMY2 — time to the wire at the assault's CURRENT pace. Suppressing or
       attriting them visibly buys time; a halted assault clears the clock. */
    const dOP = dist2(S.enemy.x, S.enemy.z, OP.x, OP.z);
    const pace = S.aSpeed * Math.max(0.25, 1 - Math.min(S.eff, 1));
    if (dOP > 90 && pace > 0)
      return { t: (dOP - 90) / pace, label: 'WIRE' };
  }
  if (!S.path) return null;
  if (S.type === 'convoy' && S.veh && !S.escaped &&
      S.veh.filter(v => !v.dead).length > 1) {
    const dHead = convoyHeadD();
    let t = (S.path.len - dHead) / S.speed;
    // a pit stop still ahead (or in progress) extends the window
    if (S.stop.tArr === null) { if (S.stop.d > dHead) t += S.stop.dur; }
    else if (!S.stop.resumed) t += Math.max(0, S.stop.dur - (sim.now - S.stop.tArr));
    return { t, label: 'CONVOY' };
  }
  if (S.type === 'kaiju' && !S.ashore && enemyAlive) {
    const d = clamp(S.speed * (sim.now - S.t0), 0, S.path.len);
    return { t: (S.path.len - d) / S.speed, label: 'LANDFALL' };
  }
  return null;
}

/* WORLD3 — a skirmish convoy route is a seeded window of a real road. The
   legacy corridor search (13 steps of 150 m, all at 0.5-16 m elevation, >=8
   visible) is unsatisfiable on this island — measured ~0.07% per attempt,
   0/9000 in a tolerance sweep — so every skirmish convoy since the type
   shipped drove the identical fallback line. Roads traverse drivable ground
   by construction and are spread across the island; the coast loop alone is
   several km, so seeded windows give visibly different routes. */
function roadConvoyRoute(rng) {
  const L = 1800, cands = [];
  for (const r of WORLD.roads) {
    if (r.length < 2) continue;
    const cum = [0];
    for (let i = 1; i < r.length; i++)
      cum.push(cum[i - 1] + Math.hypot(r[i].x - r[i - 1].x, r[i].z - r[i - 1].z));
    const total = cum[cum.length - 1];
    if (total < L * 0.75) continue;         // too short to string a column on
    const closed = r[0].x === r[r.length - 1].x && r[0].z === r[r.length - 1].z;
    cands.push({ r, cum, total, closed });
  }
  if (!cands.length) return null;
  const wsum = cands.reduce((s, c) => s + c.total, 0);
  const at = (c, d) => {                     // point at arc distance d on cand c
    let i = 1;
    while (i < c.cum.length - 1 && c.cum[i] < d) i++;
    const seg = c.cum[i] - c.cum[i - 1] || 1;
    const t = clamp((d - c.cum[i - 1]) / seg, 0, 1);
    return { x: lerp(c.r[i - 1].x, c.r[i].x, t), z: lerp(c.r[i - 1].z, c.r[i].z, t) };
  };
  let best = null;
  for (let tries = 0; tries < 40; tries++) {
    // length-weighted road pick, then a seeded start offset along it
    let w = rng() * wsum, c = cands[cands.length - 1];
    for (const cc of cands) { if (w < cc.total) { c = cc; break; } w -= cc.total; }
    const span = Math.min(L, c.closed ? c.total * 0.9 : c.total);
    const d0 = rng() * (c.closed ? c.total : Math.max(c.total - span, 1));
    const pts = [];
    for (let d = 0; d <= span; d += 30)
      pts.push(at(c, c.closed ? (d0 + d) % c.total : Math.min(d0 + d, c.total)));
    const cum = [0];
    for (let i = 1; i < pts.length; i++)
      cum.push(cum[i - 1] + Math.hypot(pts[i].x - pts[i - 1].x, pts[i].z - pts[i - 1].z));
    const route = { pts, cum, len: cum[cum.length - 1] };
    // the old workability standard, kept: >=8 of 13 column samples visible
    let vis = 0, n = 0;
    for (let d = 0; d <= route.len; d += 150) {
      const q = pathPoint(route, d);
      n++;
      if (hasLOS(eye.x, eye.y, eye.z, q.x, H(q.x, q.z) + 2, q.z)) vis++;
    }
    if (vis >= Math.ceil(n * 8 / 13)) return route;
    if (!best || vis > best.vis) best = { route, vis };
  }
  return best.route;                         // most-visible window found
}

function genScenario(type, seed) {
  const rng = mulberry32((seed * 2654435761 ^ CONFIG.SEED.terrain) >>> 0);
  const M = CONFIG.MISSION;
  /* G13 — graded effects state. tgtClass picks the CONFIG.EFFECTS band set
     ('personnel' or 'point'; convoy is assessed by vehicle kills instead).
     posture scales personnel casualties (FM 7-90 App. B); dispersal flips it
     to prone. eff accumulates casualty/damage %; everSuppressed records that
     suppression was achieved at some point (the verdict term), while
     suppressedUntil is the live window (the enemy's behaviour). */
  const S = { seed, type, difficulty: DIFFICULTY, rng, kps: [], friendlies: [],
              tgtClass: 'personnel', posture: 'standing', effScale: 1,
              eff: 0, everSuppressed: false, suppressedUntil: 0,
    alerted: 0, dispersed: 0 };
  const OPT = (activeChapter && activeChapter.scn) || {};

  /* WORLD1 — skirmish targets disperse across the whole island instead of the
     1500-3200 m annulus around the OP (user: "targets are mostly at the ends of
     the island"). Skirmish ONLY: with a chapter active the draw below is the
     classic annulus, untouched, so every fixed-seed chapter's rng sequence —
     and therefore its authored target position and par — stays identical.
     The spread draw is area-uniform over the island disc (sqrt radius), then
     clamped to CONFIG.MISSION.skirmishRange from the OP. Height, LOS and
     village-exclusion checks are shared by both draws. */
  const spread = !activeChapter;
  function findSpot(rMin, rMax, hMin, hMax, needLOS) {
    /* The disc draw's acceptance rate is inherently lower than the annulus's —
       it also samples water and masked far coast — so spread mode gets more
       tries before the fixed fallback point. Runs once per mission; cheap. */
    const tryMax = spread ? 1500 : 300;
    for (let tries = 0; tries < tryMax; tries++) {
      let x, z;
      if (spread) {
        const az = rng() * Math.PI * 2;
        const r = CONFIG.TERRAIN.islandRadius * Math.sqrt(rng());
        x = Math.sin(az) * r; z = -Math.cos(az) * r;
        const d = dist2(x, z, OP.x, OP.z);
        if (d < M.skirmishRange[0] || d > M.skirmishRange[1]) continue;
      } else {
        const az = rng() * Math.PI * 2;
        const range = lerp(rMin, rMax, rng());
        x = OP.x + Math.sin(az) * range; z = OP.z - Math.cos(az) * range;
      }
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
      /* WORLD1 — the 14 m elevation ceiling was the second half of "targets at
         the ends of the island": on a 95 m-ridge island it admits only the
         coastal fringe. Skirmish troops may now stand anywhere on the island;
         chapters keep the authored coastal placement. */
      S.enemy = findSpot(M.targetRange[0], M.targetRange[1], 2, spread ? 999 : 14, true) ||
                { x: OP.x + 2000, z: OP.z };
      S.brief = `Enemy infantry in the open, reported grid bearing ~${brgTo(S.enemy)} mils from your OP. A gift. Do not waste it.`;
    }
  } else if (type === 'wreck') {
    S.enemy = findSpot(1100, 2100, 1, 6, true) || { x: OP.x + 1500, z: OP.z };
    S.tgtClass = 'point';
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
    // 14b — the DIALOGUE_REVISIONS §6 copy pass: the clauses now read as one
    // brief instead of three concatenated fragments; content unchanged
    S.brief = `Raiders are hitting the civilian village${vil ? ' of ' + vil.name : ''} — grid ${gridOf(S.enemy.x, S.enemy.z)} area.` +
      (seen ? '' : ' The village is masked from your tower; work it off the map and the smoke.') +
      ' The huts are NO-STRIKE: one round inside the village fails the mission, full stop. Cut the raiders down without touching it.';
  } else if (type === 'bunker') {
    S.enemy = findSpot(1200, 3000, 22, 140, true) ||
              findSpot(1200, 3000, 8, 999, true) || { x: OP.x + 1800, z: OP.z };
    S.tgtClass = 'point';
    S.brief = `Dug-in bunker reported on high ground, grid bearing ~${brgTo(S.enemy)} mils. The effect bands are tight — put it on the roof.`;
  } else if (type === 'convoy') {
    let path = null;
    if (spread) {
      /* WORLD3 — skirmish convoys drive the built roads (see roadConvoyRoute).
         Chapters keep the legacy draw below untouched, so 3.1's rng sequence
         and authored fallback-line path stay identical. */
      path = roadConvoyRoute(rng);
    } else {
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
    }
    if (!path) path = { sx: OP.x + 1500, sz: OP.z, dx: 0, dz: -1, len: 1800 };
    S.path = path;
    const p0 = pathPoint(path, 0);
    S.speed = 3.5;
    S.veh = [0, 1, 2, 3].map(i => ({ off: i * 40, dead: false, d: 0, x: p0.x, z: p0.z }));
    S.t0 = sim.now - 220 / S.speed;  // column already strung out on the road
    S.enemy = { x: p0.x, z: p0.z };
    S.escaped = false;
    // pit stop: halt at the facility nearest the route (fuel/ammo/airfield),
    // else a plain crew halt at a seeded point. 1-3 minutes, seeded.
    let stopD = lerp(0.35, 0.6, rng()) * path.len, stopName = 'a crew halt';
    let bestFD = 320;
    for (const f of WORLD.facilities) {
      if (f.kind !== 'fuel' && f.kind !== 'ammo' && f.kind !== 'airfield') continue;
      if (path.pts) {
        // polyline: nearest sampled point on the route (roads run to the
        // facilities by construction, so real pit stops finally happen)
        for (let t = 300; t <= path.len - 300; t += 40) {
          const q = pathPoint(path, t);
          const fd = dist2(f.x, f.z, q.x, q.z);
          if (fd < bestFD) { bestFD = fd; stopD = t; stopName = 'the ' + f.name.toLowerCase(); }
        }
      } else {
        const t = clamp((f.x - path.sx) * path.dx + (f.z - path.sz) * path.dz, 300, path.len - 300);
        const fd = dist2(f.x, f.z, path.sx + path.dx * t, path.sz + path.dz * t);
        if (fd < bestFD) { bestFD = fd; stopD = t; stopName = 'the ' + f.name.toLowerCase(); }
      }
    }
    const [sLo, sHi] = CONFIG.WORLD.convoyStopDur;
    S.stop = { d: Math.max(stopD, 320), dur: lerp(sLo, sHi, rng()), tArr: null,
               resumed: false, name: stopName };
    S.brief = `Enemy convoy, four vehicles, moving on the ${path.pts ? 'road' : 'low ground'} near grid ${gridOf(p0.x, p0.z)}. Lead the column and time your fire for effect — or catch them when they pull in somewhere. If the head of the column runs off the end of the road, they are gone.`;
  } else if (type === 'battery') {
    /* ENEMY1 — counterbattery. An enemy mortar section is firing on a friendly
       position WHILE the observer works: the first target with a real reason
       to hurry. The gun does not need LOS from the tower — the graded skill is
       locating it from its effects (muzzle signature at the firing point, the
       impacts around the friendly position). Enemy rounds obey the same law
       ours do: impact = their aimpoint + error. No trajectory on either side.
       Grading (user decision 2026-07-30): friendly casualties from enemy fire
       COST score/time — they never auto-fail. Fratricide/collateral by OUR
       rounds stay the only auto-fails, unchanged. */
    let fb = null;
    for (let tries = 0; tries < 300 && !fb; tries++)
      fb = findSpot(M.targetRange[0], M.targetRange[1], 3, 999, true);
    fb = fb || { x: OP.x + 1600, z: OP.z };
    let gun = null;
    for (let tries = 0; tries < 600 && !gun; tries++) {
      const az = rng() * Math.PI * 2, r = lerp(1500, 3000, rng());
      const x = fb.x + Math.sin(az) * r, z = fb.z - Math.cos(az) * r;
      if (H(x, z) < 2) continue;
      const dOP = dist2(x, z, OP.x, OP.z);
      // the gun must be engageable: inside the skirmish band from the OP
      if (dOP < M.skirmishRange[0] || dOP > M.skirmishRange[1]) continue;
      if (WORLD.villages.some(v => dist2(x, z, v.x, v.z) < v.r + 120)) continue;
      gun = { x, z };
    }
    gun = gun || { x: OP.x + 2400, z: OP.z + 600 };
    S.enemy = gun;
    S.fireBase = fb;
    S.friendlies = [{ x: fb.x, z: fb.z, r: M.fratricideRadius }];
    S.tgtClass = 'point';          // a gun position: tight bands, dug-in crew
    S.posture = 'dug-in';
    S.btyCas = 0;                  // rounds landed inside the friendly position
    // first volley soon after the brief lands; period is seeded per scenario
    S.bty = { next: 12 + rng() * 8, period: lerp(24, 38, rng()), tof: lerp(9, 14, rng()) };
    S.brief = `Friendly position at grid ${gridOf(fb.x, fb.z)} is taking sustained indirect fire. The gun is somewhere out there — find it by its signature and its fall of shot, and silence it. Every volley that lands while you work is friendly casualties on your clock.`;
  } else if (type === 'defense') {
    /* ENEMY2 — the OP watchtower base is the objective. The sharpest
       danger-close problem in the trainer: the friendly element is the tower
       the observer is standing on, the OT range shrinks as the enemy closes,
       and every correction walks fire toward yourself. S.friendlies is the OP
       itself, so the F2 danger-close gate and the fratricide check both key
       on your own position with zero special cases. */
    let en = null;
    for (let tries = 0; tries < 500 && !en; tries++) {
      const az = rng() * Math.PI * 2, r = lerp(1000, 1600, rng());
      const x = OP.x + Math.sin(az) * r, z = OP.z - Math.cos(az) * r;
      const h = H(x, z);
      if (h < 2) continue;
      if (!hasLOS(eye.x, eye.y, eye.z, x, h + 2, z)) continue;
      if (WORLD.villages.some(v => dist2(x, z, v.x, v.z) < v.r + 120)) continue;
      en = { x, z };
    }
    en = en || { x: OP.x + 1200, z: OP.z };
    S.enemy = en;
    S.friendlies = [{ x: OP.x, z: OP.z, r: M.fratricideRadius }];
    S.aSpeed = 0.7;            // a deliberate assault pace: ~25 min to the wire
    S.lastT = null;            // incremental advance (speed varies with losses)
    S.overrun = false;
    S.brief = `You are the objective. Assault force forming up near grid ${gridOf(en.x, en.z)}, grid bearing ~${brgTo(en)} mils, advancing on YOUR TOWER. Your own position is the friendly element — expect DANGER CLOSE, expect creeping corrections, and do not drop one on yourself. Break the assault before it reaches the wire.`;
  } else if (type === 'chow') {
    /* 11a — Epilogue E.1. The war is won; the flock is not aware. A seagull
       flock (personnel-class target — FM 6-30 has no column for wingspan)
       masses on the tideline near the general's barbecue pit; the COOKS at
       the pit are a no-fire line with the standard fratricide rules. The
       absurd is treated as routine: the scenario machinery is the ordinary
       one, which is the joke working as doctrine. Chapter-only — never in
       the skirmish rotation. */
    let site = null;
    for (let tries = 0; tries < 300 && !site; tries++) {
      const az = rng() * Math.PI * 2, range = lerp(1200, 2200, rng());
      const x = OP.x + Math.sin(az) * range, z = OP.z - Math.cos(az) * range;
      const h = H(x, z);
      if (h < 2 || h > 9) continue;                    // a beach terrace fit for grilling
      if (!hasLOS(eye.x, eye.y, eye.z, x, h + 2, z)) continue;
      if (WORLD.villages.some(v => dist2(x, z, v.x, v.z) < v.r + 120)) continue;
      site = { x, z };
    }
    site = site || { x: OP.x + 1400, z: OP.z };
    const fAz = rng() * Math.PI * 2, fOff = 150 + rng() * 60;
    let ex = site.x + Math.sin(fAz) * fOff, ez = site.z - Math.cos(fAz) * fOff;
    if (H(ex, ez) < 1) { ex = site.x + 160; ez = site.z; }
    S.enemy = { x: ex, z: ez };
    S.bbq = site;
    S.friendlies = [{ x: site.x, z: site.z, r: 60 }];
    S.brief = `The general is grilling at grid ${gridOf(site.x, site.z)}. A seagull flock, several hundred strong, is assaulting the pit from the tideline — massing near grid ${gridOf(ex, ez)}. The COOKS are a NO-FIRE line: one round on Private Dombrowski or his potato salad is FRATRICIDE, same rules, no appeals. This close to the pit you WILL say DANGER CLOSE. Destroy the flock.`;
  } else if (type === 'kaiju') {
    /* 11b — Epilogue E.2 CLAWS OUT. A B-movie crab the size of a church wades
       ashore toward the nearest village. Mechanically it is an honest moving
       point target: convoy-style path (offshore → the village), point-class
       bands scaled UP (it is enormous — easier to HIT) with an armor divisor
       (it is chitin — harder to HURT), so stopping it takes sustained,
       well-led fire on the move. Landfall = mission failed and over, the
       convoy-escape precedent. Nobody in the fiction finds any of this
       unusual. Chapter-only. */
    let vil = WORLD.villages[0] || { x: 0, z: 0 };
    for (const v of WORLD.villages)
      if (dist2(v.x, v.z, OP.x, OP.z) < dist2(vil.x, vil.z, OP.x, OP.z)) vil = v;
    // landfall line: from the village straight out to sea
    const outAz = Math.atan2(vil.x, vil.z);            // away from island center
    let sx = vil.x, sz = vil.z, steps = 0;
    while (H(sx, sz) > -4 && steps++ < 400) { sx += Math.sin(outAz) * 25; sz += Math.cos(outAz) * 25; }
    sx += Math.sin(outAz) * 300; sz += Math.cos(outAz) * 300;   // start well offshore
    const len = Math.max(dist2(sx, sz, vil.x, vil.z) - 60, 400);
    const dxn = (vil.x - sx) / dist2(sx, sz, vil.x, vil.z);
    const dzn = (vil.z - sz) / dist2(sx, sz, vil.x, vil.z);
    S.path = { sx, sz, dx: dxn, dz: dzn, len };
    S.speed = 2.1;                                     // wading; the sea resists
    S.t0 = sim.now;
    S.enemy = { x: sx, z: sz };
    S.village = vil;
    S.ashore = false;
    S.tgtClass = 'point';
    S.effScale = 2;                                    // church-sized: bands double
    S.armor = 3;                                       // chitin: three volleys' worth
    S.brief = `A crab, dark red, approximately the size of a church, is wading ashore toward the village of ${vil.name || 'the coast'} from grid ${gridOf(sx, sz)}. It is a MOVING TARGET — lead it. It is also a crab. The village is NO-STRIKE as always. Stop it before landfall; nobody will discuss this afterwards.`;
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
  /* G13 — the legacy chapter knob `effR` was an absolute effect radius against
     the old default of 60 m; it now scales the graded distance bands by the
     same ratio, so chapter 4.2's effR:30 still means "half the normal effect
     envelope — precision work". G18 replaces this with per-asset band sets. */
  if (OPT.effR) S.effScale = OPT.effR / 60;

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
  /* ENEMY2 (bug found during its build): this returned [] for every type
     except strongpoint/assault — so chow's COOKS no-fire line and battery's
     position were never actually fratricide-protected or danger-close gated,
     despite E.1's brief promising "FRATRICIDE, same rules, no appeals".
     Static friendlies now come straight off S.friendlies for every type;
     assault keeps its computed advancing positions below. */
  if (S.type !== 'assault') return S.friendlies || [];
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
        const q = pathPoint(S.path, d);
        v.x = q.x; v.z = q.z; v.d = d;   // d kept so a dead hull holds its heading
        cxSum += v.x; czSum += v.z; alive++;
      }
      const m = units.vehicles[i];
      const dir = pathDir(S.path, v.d || 0);   // WORLD3: heading follows the road
      m.position.set(v.x, H(v.x, v.z) + 1.05, v.z);
      m.rotation.y = Math.atan2(dir.dx, dir.dz) + Math.PI / 2;
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
  } else if (S.type === 'kaiju') {
    // 11b — the crab wades until it is dead or ashore
    const m = units.bunker;
    if (!enemyAlive) {
      m.rotation.z = 0.6;                              // it settles; the tide handles the rest
      m.position.y = Math.max(H(S.enemy.x, S.enemy.z), 0) + 2.2;
    } else {
      const d = clamp(S.speed * (sim.now - S.t0), 0, S.path.len);
      const q = pathPoint(S.path, d);                  // line form: legacy math exactly
      S.enemy.x = q.x;
      S.enemy.z = q.z;
      const bob = Math.sin(sim.now * 1.6) * 0.9;       // the gait of a determined crab
      const kd = pathDir(S.path, d);
      m.position.set(S.enemy.x, Math.max(H(S.enemy.x, S.enemy.z), 0) + 4.4 + bob, S.enemy.z);
      m.rotation.y = Math.atan2(kd.dx, kd.dz);
      if (d >= S.path.len && !S.ashore) {
        S.ashore = true;
        log('', 'LANDFALL. The crab is in the village. There is no doctrinal term for what happens next, and the mission is over.', 'sys');
        if (mission && !mission.done) {
          mission.done = true; mission.failReason = 'escaped'; mission.tEnd = sim.now;
          schedule(sim.now + 2, showAAR);
        }
      }
    }
  } else if (S.type === 'battery' && S.bty) {
    /* ENEMY1 — the volley loop. Fires only while the crew is alive and not
       suppressed, so silencing the battery VISIBLY stops the shelling:
       suppression pauses it (and the pause is watchable), neutralized or
       destroyed ends it. bty.next counts seconds since scenario start. */
    const tS = sim.now - scenarioT0;
    if (enemyAlive && tS >= S.bty.next) {
      if (sim.now < (S.suppressedUntil || 0)) {
        // heads down: the crew waits out the suppression, then relays
        S.bty.next = (S.suppressedUntil - scenarioT0) + 6;
      } else {
        S.bty.next = tS + S.bty.period;
        enemyVolley(S);
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
  } else if (S.type === 'defense' && !S.overrun) {
    /* ENEMY2 — the assault closes on the tower. Advance is incremental so it
       honestly responds to what the observer does: suppression halts it (and
       the halt is watchable), casualties slow it in proportion, a broken
       assault (enemyAlive false) stops for good. */
    const dt2 = S.lastT === null ? 0 : sim.now - S.lastT;
    S.lastT = sim.now;
    const dOP = dist2(S.enemy.x, S.enemy.z, OP.x, OP.z);
    if (enemyAlive && sim.now >= (S.suppressedUntil || 0) && dt2 > 0) {
      const pace = S.aSpeed * Math.max(0.25, 1 - Math.min(S.eff, 1));
      const step = Math.min(pace * dt2, Math.max(dOP - 60, 0));
      if (dOP > 60) {
        S.enemy.x += (OP.x - S.enemy.x) / dOP * step;
        S.enemy.z += (OP.z - S.enemy.z) / dOP * step;
      }
    }
    // figures: line abreast on the advance azimuth, at the feet of S.enemy
    const aAz = Math.atan2(OP.x - S.enemy.x, -(OP.z - S.enemy.z));
    const rx = Math.cos(aAz), rz = Math.sin(aAz);
    for (let i = 0; i < 8; i++) {
      const m = units.troops[i];
      if (!m.visible) continue;
      const l = (i - 3.5) * 9;
      const x = S.enemy.x + rx * l, z = S.enemy.z + rz * l;
      m.position.set(x, H(x, z), z);
      if (i < units.flashes.length)
        units.flashes[i].s.position.set(x, H(x, z) + 1.6, z);
    }
    if (dOP <= 90 && !S.overrun) {
      S.overrun = true;
      log('', 'They are through the wire and on the tower stairs. The OP is overrun — mission over.', 'sys');
      if (mission && !mission.done) {
        mission.done = true; mission.failReason = 'overrun'; mission.tEnd = sim.now;
        schedule(sim.now + 2, showAAR);
      }
    }
  }
}

/* ENEMY1 — one enemy volley: muzzle signature at the gun, three rounds around
   the friendly position after a seeded presentation delay (a pacing constant,
   like our own tofFor — nothing is computed from physics). Enemy aimpoint is
   the friendly position; error is drawn from the scenario stream. A round
   inside the position radius is casualties — a COST at grading, never a fail. */
function enemyVolley(S) {
  const fb = S.fireBase;
  spawnBurst(S.enemy.x, H(S.enemy.x, S.enemy.z), S.enemy.z, true);   // signature
  for (let i = 0; i < 3; i++) {
    const ix = fb.x + gauss(S.rng) * 55, iz = fb.z + gauss(S.rng) * 55;
    schedule(sim.now + S.bty.tof + i * 0.8, () => {
      if (Scenario !== S) return;              // mission changed while in flight
      spawnBurst(ix, Math.max(H(ix, iz), 0), iz);
      if (dist2(ix, iz, fb.x, fb.z) < S.friendlies[0].r) {
        S.btyCas++;
        if (S.btyCas === 1)
          log('', 'Rounds inside the friendly position. They are taking casualties down there — kill that gun.', 'sys');
      }
    });
  }
}

function placeUnits() {
  const S = Scenario;
  const rng = mulberry32((S.seed * 68168169 + 17) >>> 0);
  // hide/reset everything mission-specific (scale too — 11a shrinks the
  // troop figures into gulls, and a stale scale must not leak forward)
  units.troops.forEach(m => { m.visible = false; m.scale.set(1, 1, 1); });
  units.vehicles.forEach(m => {
    m.visible = false; visSetMat(m, units.vehMatAlive);
    m.rotation.set(0, 0, 0); m.scale.set(1, 1, 1);
  });
  units.huts.forEach(m => m.visible = false);
  units.flag.pole.visible = units.flag.banner.visible = false;
  units.flashes.forEach(f => f.s.visible = false);
  units.fSquad.forEach(m => m.visible = false);
  units.bunker.visible = false;
  units.bunker.scale.set(1, 1, 1);   // 11b — the crab scales it up; must not leak
  units.bunker.rotation.set(0, 0, 0);
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
  } else if (S.type === 'defense') {
    // ENEMY2 — eight figures; positions are driven per-frame as they advance
    troopCluster(S.enemy.x, S.enemy.z, 8, 20);
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
  } else if (S.type === 'battery') {
    /* ENEMY1 — the gun position: bunker mesh as the emplacement, crew of
       three, one truck standing off. The friendly position gets the huts and
       the flag — a marked, map-plotted friendly location, same courtesy the
       strongpoint compound gets. */
    const e = S.enemy;
    units.bunker.visible = true;
    units.bunker.position.set(e.x, H(e.x, e.z) + 1.3, e.z);
    troopCluster(e.x + 12, e.z + 8, 3, 8);
    const vm = units.vehicles[0];
    const va = rng() * Math.PI * 2;
    const vx = e.x + Math.sin(va) * 26, vz = e.z + Math.cos(va) * 26;
    vm.visible = true;
    vm.position.set(vx, H(vx, vz) + 1.05, vz);
    vm.rotation.set(0, rng() * Math.PI * 2, 0);
    const c = S.fireBase;
    const hutOff = [[0, 0], [10, 7], [-9, 8]];
    units.huts.forEach((hm, i) => {
      const x = c.x + hutOff[i][0], z = c.z + hutOff[i][1];
      hm.visible = true;
      hm.position.set(x, H(x, z) + 1.6, z);
      hm.rotation.set(0, (i * 0.7) % Math.PI, 0);
    });
    units.flag.pole.visible = units.flag.banner.visible = true;
    const fx = c.x + 4, fz = c.z - 5, fh = H(fx, fz);
    units.flag.pole.position.set(fx, fh + 3.5, fz);
    units.flag.banner.position.set(fx + 1.2, fh + 6.3, fz);
  } else if (S.type === 'kaiju') {
    // 11b — the crab is the bunker mesh, enormous and crimson; position is
    // driven per-frame by updateScenario as it wades
    units.bunker.visible = true;
    units.bunker.scale.set(4.6, 3.1, 5.4);
    visSetColor(units.bunker, 0x8A3428);
    units.bunker.position.set(S.enemy.x, 4, S.enemy.z);
  } else if (S.type === 'chow') {
    // 11a — the flock: the ordinary troop figures at gull scale (legibility
    // still applies; a trainer where you cannot see the target is a broken
    // trainer, even when the target has feathers)
    troopCluster(S.enemy.x, S.enemy.z, 8, 16);
    units.troops.forEach(m => { if (m.visible) m.scale.set(0.5, 0.42, 0.5); });
    // the cooks at the pit, and the grill smoking like a position marker
    units.fSquad.forEach((m, i) => {
      if (i >= 3) return;
      const a = i * 2.1, r = 5 + i * 3;
      const x = S.bbq.x + Math.sin(a) * r, z = S.bbq.z + Math.cos(a) * r;
      m.visible = true;
      m.position.set(x, Math.max(H(x, z), 0), z);
    });
    units.smokePuffs.forEach(p => {
      p.on = true; p.bx = S.bbq.x; p.by = Math.max(H(S.bbq.x, S.bbq.z), 0); p.bz = S.bbq.z;
    });
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

