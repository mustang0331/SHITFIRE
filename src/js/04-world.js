/* ============================================================ WORLD FEATURES */
// Permanent, seeded, terrain-aware world detail: coast road, dirt paths,
// military facilities, landmarks, and civilian villages. Rebuilt per island
// (terrain seed ^ DEM name), NOT per mission — permanence is what makes the
// features usable for terrain association on the printed sheet.
const WORLD = { roads: [], paths: [], facilities: [], villages: [], civs: [],
                rocks: [], group: null };
const ROCK_NAMES = ['THE TEETH', 'ANVIL RK', 'HOGBACK', 'THE KNUCKLES',
                    'SPLIT RK', 'WIDOW RK', 'THE MOLARS', 'GRAVEYARD'];
function strHash(s) {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) { h ^= s.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}
const _wMats = {
  // E4: metalled surface — near-black bitumen, and deliberately NOT flat-shaded.
  // Flat shading facets a ribbon into visible chevrons as it follows H; a smooth
  // normal is what sells "graded and sealed" against the faceted dirt track.
  asph:  new THREE.MeshLambertMaterial({ color: 0x33322D, flatShading: false, side: THREE.DoubleSide }),
  asphE: new THREE.MeshLambertMaterial({ color: 0x8B8271, flatShading: true, side: THREE.DoubleSide }),
  path:  new THREE.MeshLambertMaterial({ color: 0xB29A6E, flatShading: true, side: THREE.DoubleSide }),
  strip: new THREE.MeshLambertMaterial({ color: 0x59564E, flatShading: true, side: THREE.DoubleSide }),
  mil:   new THREE.MeshLambertMaterial({ color: 0x5C6650, flatShading: true }),
  milB:  new THREE.MeshLambertMaterial({ color: 0x4A5240, flatShading: true }),
  civ:   new THREE.MeshLambertMaterial({ color: 0xC9B891, flatShading: true }),
  civB:  new THREE.MeshLambertMaterial({ color: 0xB5A075, flatShading: true }),
  // E6: rendered masonry. A town is not a big village — it is a DIFFERENT
  // material, pale and cool against the warm thatch of the hut clusters, which
  // is most of what makes the tier readable at 2-3 km before any detail does.
  civM:  new THREE.MeshLambertMaterial({ color: 0xE2DCCB, flatShading: true }),
  civR:  new THREE.MeshLambertMaterial({ color: 0x8C4A38, flatShading: true }),
  drum:  new THREE.MeshLambertMaterial({ color: 0x7A3A2A, flatShading: true }),
  mast:  new THREE.MeshLambertMaterial({ color: 0xB9B4A6, flatShading: true }),
  // E7 — the whole rock layer is ONE material; the colour variation between
  // boulders is baked into vertex colours at build time, so a dozen outcrops
  // cost one draw call and still don't look stamped from one mould.
  rock:  new THREE.MeshLambertMaterial({ vertexColors: true, flatShading: true }),
};
const ROCK_PALETTE = [0x6E675C, 0x7E7669, 0x585249, 0x8A8376, 0x655D51];

/* ---- FIGURES: the three families the observer must tell apart ---------------
   GRAPHICS.md's second governing constraint: legibility beats looks. Three
   independent fixes, none of which moves a figure's ground position:

     1. distance compensation — the mesh grows with range so a figure never
        subtends less than CONFIG.GFX.legFloorMil (see legibilityPass()).
     2. internal contrast — every figure is two-tone, so at least one half
        contrasts whether the backdrop is jungle green, sand, or wet rock.
     3. a contact disc at the feet so the silhouette does not merge into terrain.

   Discrimination is a HARD requirement — collateral damage is an auto-fail, so
   civilian-vs-military must never be a guess. The families therefore differ in
   BOTH palette and silhouette, and the palettes are inverted against each other:

     enemy     dark oxblood body · narrow BRIGHT RUST cap  (dark below / warm above)
     friendly  dark navy body    · narrow BRIGHT ICE cap   (dark below / cool above)
     civilian  PALE bone body    · wide DARK CONICAL hat   (light below / dark above)

   The civilian is also visibly narrower in the body and much wider at the head,
   so the aspect ratio alone separates it at ranges where colour has washed out.

   All figure geometry has its ORIGIN AT THE FEET. Scaling therefore grows a
   figure upward off the ground instead of sinking it, and `rotation.z` lays a
   casualty over about its own boots. Callers set position.y = ground height. */
const FIG = {
  h: { troop: 2.40, civ: 2.08 },        // full silhouette height incl. headgear (m)
  geo: {
    body:    new THREE.BoxGeometry(0.80, 2.00, 0.80).translate(0, 1.00, 0),
    cap:     new THREE.BoxGeometry(0.98, 0.46, 0.98).translate(0, 2.17, 0),
    civBody: new THREE.BoxGeometry(0.54, 1.70, 0.54).translate(0, 0.85, 0),
    civHat:  new THREE.ConeGeometry(0.80, 0.44, 8).translate(0, 1.86, 0),
    disc:    new THREE.CircleGeometry(0.75, 12).rotateX(-Math.PI / 2),
  },
  mat: {
    enemyBody:  new THREE.MeshLambertMaterial({ color: 0x59180F, flatShading: true }),
    enemyCap:   new THREE.MeshLambertMaterial({ color: 0xE0641F, flatShading: true }),
    friendBody: new THREE.MeshLambertMaterial({ color: 0x22406E, flatShading: true }),
    friendCap:  new THREE.MeshLambertMaterial({ color: 0x8FC6F2, flatShading: true }),
    civBody:    new THREE.MeshLambertMaterial({ color: 0xEFE8D4, flatShading: true }),
    civHat:     new THREE.MeshLambertMaterial({ color: 0x46341F, flatShading: true }),
    // unlit + untonemapped so the contact patch stays a constant dark anchor at
    // every time of day; polygonOffset (not a lift) keeps it off the terrain on
    // a slope without floating clear of the boots.
    disc: new THREE.MeshBasicMaterial({ color: 0x0A0D07, transparent: true,
      opacity: 0.40, depthWrite: false, toneMapped: false,
      polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -8 }),
  },
};
// buildWorldFeatures() disposes every geometry it tears down. These are shared
// and live for the life of the page — mark them off-limits.
for (const k in FIG.geo) FIG.geo[k].userData.keep = true;

/* Optics signatures. `vis` on a material overrides what NVG and THERMAL make of
   it; anything without one is derived from its daytime luminance (see VISION).
     nvg  a 0..1 tube brightness
     th   an [r,g,b] thermal signature, 0..1
   Discrimination is a HARD gate — collateral damage and fratricide are both
   auto-fails, so neither device is allowed to turn the three families into a
   guess. The generic derivation would fail that gate: under a monochrome tube
   the enemy and friendly bodies land 0.34 vs 0.46, and under white-hot EVERY
   human body is 1.0. Both are fixed the way the real problem is fixed:

     NVG      An image intensifier sees near-IR, and NIR is not luminance. Live
              vegetation is strongly NIR-reflective (the Wood effect) so jungle
              reads BRIGHT; water absorbs NIR so the sea goes almost black; and
              dyed uniform cloth is NIR-dark, so men read as DARK shapes against
              bright ground. That is what makes the tube worth carrying, and it
              is why these numbers are not derived from the daytime palette — the
              first pass did derive them and measured WORSE than the naked eye at
              night, because the enemy body (0.30) and the jungle (0.34) landed on
              top of each other. Friendly caps are blown to 1.0 — an IR beacon,
              the brightest thing on the island. Civilians keep an inverted read:
              bright cotton body, dark hat.
     THERMAL  friendly caps render CYAN — a thermal CID panel. Civilian hats are
              woven straw, which is a genuine thermal insulator, so a civilian is
              a warm body under a distinctly COLD wide head. Enemy is the only
              family that is bright all the way up.
   Village huts run slightly warm (cooking fires), which makes a no-strike
   village read as a warm cluster in thermal instead of vanishing. */
function visTag(mat, spec) { mat.userData.vis = spec; return mat; }
visTag(FIG.mat.enemyBody,  { nvg: 0.16, th: [1.00, 1.00, 1.00] });  // NIR-dark cloth
visTag(FIG.mat.enemyCap,   { nvg: 0.32, th: [0.86, 0.86, 0.86] });
visTag(FIG.mat.friendBody, { nvg: 0.20, th: [1.00, 1.00, 1.00] });
visTag(FIG.mat.friendCap,  { nvg: 1.00, th: [0.28, 1.00, 1.00] });  // IR beacon / CID panel
visTag(FIG.mat.civBody,    { nvg: 0.90, th: [0.70, 0.70, 0.70] });  // undyed cotton: NIR-bright
visTag(FIG.mat.civHat,     { nvg: 0.20, th: [0.14, 0.14, 0.14] });  // straw insulates: cold
visTag(FIG.mat.disc,       { nvg: 0.02, th: [0.03, 0.03, 0.03] });
visTag(_wMats.civ,         { nvg: 0.40, th: [0.38, 0.38, 0.38] });  // hut, cooking fire
visTag(_wMats.civB,        { nvg: 0.38, th: [0.36, 0.36, 0.36] });
/* E4 — the road surfaces, and the one genuinely teachable observation cue in
   this row. Sealed asphalt has a high thermal mass and a low albedo: it takes
   the whole day's insolation and re-radiates it for hours after dark, so on a
   night thermal sight the metalled network draws itself as a WARM RIBBON across
   cold ground. That is a real effect and a real technique — the road net is
   often the first thing an observer can orient off through a thermal sight at
   night. Gravel shoulder and dirt track have far less mass and shed their heat
   early, so they stay near the cold floor and the metalled route stands alone.
   Under NVG the ordering inverts: bitumen is strongly NIR-ABSORBING (~0.10) and
   goes near-black, while bare soil is NIR-bright, so the dirt track is the one
   that shows. Two devices, two different answers, both correct. */
// E6 — town fabric. Rendered masonry has more thermal mass than a thatched hut
// so it stays warmer later into the night, and lime render is NIR-bright; a
// tiled roof reads cooler than the wall it sits on. Both stay well clear of the
// human signatures, because a town is exactly where a mistaken civilian call
// gets someone killed.
visTag(_wMats.civM,        { nvg: 0.78, th: [0.46, 0.46, 0.46] });  // lime render
visTag(_wMats.civR,        { nvg: 0.30, th: [0.34, 0.34, 0.34] });  // tile roof
visTag(_wMats.asph,        { nvg: 0.10, th: [0.62, 0.62, 0.62] });  // sealed: NIR-black, holds heat
visTag(_wMats.asphE,       { nvg: 0.34, th: [0.30, 0.30, 0.30] });  // gravel shoulder: sheds heat
visTag(_wMats.path,        { nvg: 0.52, th: [0.24, 0.24, 0.24] });  // bare soil: NIR-bright, cool
/* E7 — bare rock. On a wet tropical island the jungle canopy is the WARM thing
   after dark: it is full of water and it transpires all day. An exposed outcrop
   has nothing over it, a clear view of a cold sky, and radiates straight to it,
   so it reads COLD against the vegetation around it in thermal. Under NVG the
   same contrast runs the other way round from the usual: living foliage is
   strongly NIR-reflective and bare stone is not, so the outcrop is a DARK
   silhouette in a bright field. Either device, the shape is distinctive and it
   is always in the same place — which is the entire point of putting it there. */
visTag(_wMats.rock,        { nvg: 0.14, th: [0.14, 0.14, 0.14] });

function makeFigure(kind) {                 // 'enemy' | 'friendly' | 'civ'
  const civ = kind === 'civ', fr = kind === 'friendly';
  const m = new THREE.Mesh(civ ? FIG.geo.civBody : FIG.geo.body,
    civ ? FIG.mat.civBody : fr ? FIG.mat.friendBody : FIG.mat.enemyBody);
  m.add(new THREE.Mesh(civ ? FIG.geo.civHat : FIG.geo.cap,
    civ ? FIG.mat.civHat : fr ? FIG.mat.friendCap : FIG.mat.enemyCap));
  if (CONFIG.GFX.contactShadow) {
    const d = new THREE.Mesh(FIG.geo.disc, FIG.mat.disc);
    d.position.y = 0.05;
    d.renderOrder = 1;
    m.add(d);
    m.userData.disc = d;
  }
  m.userData.figH = civ ? FIG.h.civ : FIG.h.troop;
  return m;
}

/* E4 — one merged geometry per road CLASS, not one mesh per polyline.
   The network is a coast loop + a handful of spurs + a handful of tracks; drawn
   individually that is a dozen-plus draw calls for a few thousand triangles.
   Merged it is exactly THREE (asphalt surface, gravel shoulder, dirt track)
   whatever the island throws up, which is what keeps the 60 fps rule safe as
   E6 adds settlements to connect. All allocation happens here, at build time —
   nothing in this path runs per frame. */
function ribbonInto(pts, width, lift, pos, idx, base) {
  const n = pts.length;
  for (let i = 0; i < n; i++) {
    const o = pts[Math.max(i - 1, 0)], q = pts[Math.min(i + 1, n - 1)];
    let dx = q.x - o.x, dz = q.z - o.z;
    const l = Math.hypot(dx, dz) || 1; dx /= l; dz /= l;
    const hw = width / 2, px = -dz * hw, pz = dx * hw;
    const x0 = pts[i].x + px, z0 = pts[i].z + pz;
    const x1 = pts[i].x - px, z1 = pts[i].z - pz;
    pos.push(x0, Math.max(H(x0, z0), 0) + lift, z0);
    pos.push(x1, Math.max(H(x1, z1), 0) + lift, z1);
    if (i) {
      const v = base + i * 2;
      idx.push(v - 2, v - 1, v, v - 1, v + 1, v);
    }
  }
  return base + n * 2;
}
function mergedRibbonMesh(lines, width, mat, lift) {
  const pos = [], idx = [];
  let base = 0;
  for (const pts of lines) {
    if (!pts || pts.length < 2) continue;
    base = ribbonInto(pts, width, lift, pos, idx, base);
  }
  if (!idx.length) return null;
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(new Float32Array(pos), 3));
  g.setIndex(idx);
  g.computeVertexNormals();
  return new THREE.Mesh(g, mat);
}

function ribbonMesh(pts, width, mat, lift) {
  if (pts.length < 2) return null;
  const n = pts.length;
  const pos = new Float32Array(n * 6);
  const idx = [];
  for (let i = 0; i < n; i++) {
    const o = pts[Math.max(i - 1, 0)], q = pts[Math.min(i + 1, n - 1)];
    let dx = q.x - o.x, dz = q.z - o.z;
    const l = Math.hypot(dx, dz) || 1; dx /= l; dz /= l;
    const hw = width / 2, px = -dz * hw, pz = dx * hw;
    const x0 = pts[i].x + px, z0 = pts[i].z + pz;
    const x1 = pts[i].x - px, z1 = pts[i].z - pz;
    pos[i * 6]     = x0; pos[i * 6 + 1] = Math.max(H(x0, z0), 0) + lift; pos[i * 6 + 2] = z0;
    pos[i * 6 + 3] = x1; pos[i * 6 + 4] = Math.max(H(x1, z1), 0) + lift; pos[i * 6 + 5] = z1;
    if (i) idx.push(i * 2 - 2, i * 2 - 1, i * 2, i * 2 - 1, i * 2 + 1, i * 2);
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setIndex(idx);
  g.computeVertexNormals();
  return new THREE.Mesh(g, mat);
}
/* E6 — every boxy structure on the island is BATCHED and merged per material
   instead of getting its own Mesh. Before this row that was ~25 meshes for the
   facilities and hut clusters; a town of 14-20 buildings would have taken it
   past 45, which is a lot of draw calls to spend on scenery in a renderer that
   has to hold 60 fps with 180k triangles of terrain already on screen.

   Merged, the entire structure layer is one mesh per MATERIAL — six, whatever
   the island builds — and adding a settlement tier costs zero extra draw calls.
   The unit box is non-indexed so the merge is a straight copy with a rotY
   applied; nothing here runs per frame. */
const _boxUnit = new THREE.BoxGeometry(1, 1, 1).toNonIndexed();
_boxUnit.userData.keep = true;
let _boxBatch = [];
function wBox(x, z, w, h, d, mat, rotY) {
  wBoxY(x, Math.max(H(x, z), 0) + h / 2, z, w, h, d, mat, rotY);
}
function wBoxY(x, y, z, w, h, d, mat, rotY) {   // explicit centre height
  _boxBatch.push({ x, y, z, w, h, d, mat, rotY: rotY || 0 });
}
/* E7 — one merged, vertex-coloured, flat-shaded geometry for EVERY boulder on
   the island. A 20-face icosahedron per boulder, squashed and rotated so no two
   read the same, and the whole layer lands as a single draw call. Nothing here
   runs per frame; the arrays are sized once and filled once. */
const _rockUnit = new THREE.IcosahedronGeometry(1, 0);   // non-indexed, 60 verts
_rockUnit.userData.keep = true;
const _rkM = new THREE.Matrix4(), _rkE = new THREE.Euler(), _rkV = new THREE.Vector3();
const _rkC = new THREE.Color();
function mergedRocks(boulders) {
  if (!boulders.length) return null;
  const src = _rockUnit.attributes.position.array;
  const nv = _rockUnit.attributes.position.count;
  const pos = new Float32Array(boulders.length * nv * 3);
  const col = new Float32Array(boulders.length * nv * 3);
  let o = 0;
  for (const b of boulders) {
    _rkE.set(b.rx, b.ry, b.rz);
    _rkM.makeRotationFromEuler(_rkE);
    _rkM.scale(_rkV.set(b.sx, b.sy, b.sz));
    _rkC.setHex(b.col);
    // a little per-boulder value jitter on top of the palette pick, so a pile
    // reads as weathered stone rather than as one solid mass
    const k = b.shade;
    for (let i = 0; i < nv; i++) {
      _rkV.set(src[i * 3], src[i * 3 + 1], src[i * 3 + 2]).applyMatrix4(_rkM);
      pos[o]     = b.x + _rkV.x;
      pos[o + 1] = b.y + _rkV.y;
      pos[o + 2] = b.z + _rkV.z;
      col[o]     = _rkC.r * k;
      col[o + 1] = _rkC.g * k;
      col[o + 2] = _rkC.b * k;
      o += 3;
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  g.computeVertexNormals();
  return new THREE.Mesh(g, _wMats.rock);
}

function flushBoxes(group) {
  const byMat = new Map();
  for (const b of _boxBatch) {
    let a = byMat.get(b.mat);
    if (!a) byMat.set(b.mat, a = []);
    a.push(b);
  }
  _boxBatch = [];
  const bp = _boxUnit.attributes.position.array;
  const bn = _boxUnit.attributes.normal.array;
  const nv = _boxUnit.attributes.position.count;
  for (const [mat, list] of byMat) {
    const pos = new Float32Array(list.length * nv * 3);
    const nor = new Float32Array(list.length * nv * 3);
    let o = 0;
    for (const b of list) {
      const c = Math.cos(b.rotY), s = Math.sin(b.rotY);
      for (let i = 0; i < nv; i++) {
        const vx = bp[i * 3] * b.w, vy = bp[i * 3 + 1] * b.h, vz = bp[i * 3 + 2] * b.d;
        pos[o]     = b.x + vx * c + vz * s;
        pos[o + 1] = b.y + vy;
        pos[o + 2] = b.z - vx * s + vz * c;
        // rotY only: a box's normals stay axis-aligned under a per-axis scale,
        // so they need the rotation and nothing else.
        const nx = bn[i * 3], ny = bn[i * 3 + 1], nz = bn[i * 3 + 2];
        nor[o]     = nx * c + nz * s;
        nor[o + 1] = ny;
        nor[o + 2] = -nx * s + nz * c;
        o += 3;
      }
    }
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
    g.setAttribute('normal', new THREE.BufferAttribute(nor, 3));
    group.add(new THREE.Mesh(g, mat));
  }
}
function isleName(rng) {
  const syl = ['ta', 'va', 'mo', 'ra', 'hi', 'no', 'ke', 'lu', 'pa', 'si', 'ngo', 'fa'];
  let n = '';
  const c = 2 + Math.floor(rng() * 2);
  for (let i = 0; i < c; i++) n += syl[Math.floor(rng() * syl.length)];
  return n.toUpperCase();
}

function buildWorldFeatures() {
  // teardown
  if (WORLD.group) {
    scene.remove(WORLD.group);
    WORLD.group.traverse(o => {
      if (o.geometry && !o.geometry.userData.keep) o.geometry.dispose();
    });
  }
  WORLD.group = new THREE.Group();
  WORLD.roads.length = 0; WORLD.paths.length = 0;
  WORLD.facilities.length = 0; WORLD.villages.length = 0; WORLD.civs.length = 0;
  WORLD.rocks.length = 0;
  const wSeed = (CONFIG.SEED.terrain ^ (DEM ? strHash(DEM.name) : 0)) >>> 0;
  const rng = mulberry32(wSeed);
  rollWind(wSeed);   // 12i — new island, new wind (independent stream)
  // E4+ placement draws from a SECOND stream derived from the same island seed.
  // Everything added from E4 onward is seeded and reproducible, but it takes no
  // draws out of `rng`, so every island built before this row — and every
  // fixed-seed campaign chapter standing on one — regenerates bit-identically.
  const rng2 = mulberry32((Math.imul(wSeed, 2246822519) + 13) >>> 0);
  /* E7 takes a THIRD stream for the same reason E4 took a second one. The rock
     block sits upstream of the road-connector routing, which draws its lateral
     bow from rng2 — so spending rng2 on boulders would have shifted every
     connector's curve on islands that already shipped. A separate stream keeps
     E7 additive: roads, villages and facilities all regenerate bit-identically. */
  const rng3 = mulberry32((Math.imul(wSeed, 3266489917) + 0x9E3779B9) >>> 0);
  const W = CONFIG.WORLD;
  const asphaltLines = [], dirtLines = [];
  _boxBatch = [];                      // E6 — every wBox() lands here, merged below

  // --- coast road: radial scan for the outermost low-shelf point per bearing
  const NA = 128, ring = [];
  for (let a = 0; a < NA; a++) {
    const th = a / NA * Math.PI * 2;
    const ux = Math.cos(th), uz = Math.sin(th);
    let pt = null;
    for (let r = 4700; r >= 400; r -= 40) {
      const h = H(ux * r, uz * r);
      if (h >= 2 && h <= 9) { pt = { x: ux * r, z: uz * r }; break; }
    }
    ring.push(pt);
  }
  for (let a = 0; a < NA; a++) {           // fill gaps from neighbors
    if (ring[a]) continue;
    const p = ring[(a + NA - 1) % NA], q = ring[(a + 1) % NA];
    if (p && q) ring[a] = { x: (p.x + q.x) / 2, z: (p.z + q.z) / 2 };
  }
  let road = ring.filter(Boolean);
  for (let pass = 0; pass < 2; pass++)     // smooth
    road = road.map((p, i) => {
      const a = road[(i + road.length - 1) % road.length], b = road[(i + 1) % road.length];
      return { x: (a.x + p.x * 2 + b.x) / 4, z: (a.z + p.z * 2 + b.z) / 4 };
    });
  /* The 1-2-1 smoothing above cuts corners, and across a bay that puts the
     centreline in the water — measured 9 of 127 points below 0.5 m on the
     default island, one of them 2.9 m UNDER the surface. ribbonMesh clamps y to
     max(H,0), so those points did not look wrong so much as they looked like a
     causeway, and they poisoned the connector routing (a spur anchored on one
     starts underwater and gets rejected). Pull any wet point straight inland
     until it finds land. Measured: 17 points moved, worst case 60 m, and the
     minimum centreline elevation goes -2.86 m -> +1.09 m. */
  for (let i = 0; i < road.length; i++) {
    const p = road[i];
    if (H(p.x, p.z) >= 1.0) continue;
    const l = Math.hypot(p.x, p.z) || 1, ux = p.x / l, uz = p.z / l;
    for (let d = 20; d <= 260; d += 20) {
      const nx = p.x - ux * d, nz = p.z - uz * d;
      if (H(nx, nz) >= 1.0) { road[i] = { x: nx, z: nz }; break; }
    }
  }
  if (road.length > 8) {
    road.push(road[0]);                    // close the loop
    WORLD.roads.push(road);                // the coast road is the metalled route
    asphaltLines.push(road);
  }
  const roadAt = f => road[Math.floor(f * (road.length - 1))] || { x: 0, z: 0 };
  const inland = (p, d) => {               // step toward island center
    const l = Math.hypot(p.x, p.z) || 1;
    return { x: p.x - p.x / l * d, z: p.z - p.z / l * d };
  };

  /* E4 — connector routing. A route leaves the metalled coast road at its
     nearest point and runs to a landmark with a seeded lateral bow so it does
     not read as a drafting-table straight line. It refuses to enter water: no
     bridges are modelled, and a road that walks into the sea would be worse
     than no road on a sheet the observer is meant to resect off. */
  const nearestOnRoad = (tx, tz) => {
    let best = null, bd = Infinity;
    for (const p of road) {
      const d = dist2(p.x, p.z, tx, tz);
      if (d < bd) { bd = d; best = p; }
    }
    return best && { p: best, d: bd };
  };
  const linkTo = (tx, tz, maxLen) => {
    if (road.length < 9) return null;
    const a = nearestOnRoad(tx, tz);
    if (!a || a.d > maxLen || a.d < 8) return null;
    const n = clamp(Math.round(a.d / 45), 4, 40);
    const px = -(tz - a.p.z) / a.d, pz = (tx - a.p.x) / a.d;   // unit normal
    const bow = (rng2() - 0.5) * Math.min(a.d * 0.16, 70);
    const pts = [];
    for (let s = 0; s <= n; s++) {
      const t = s / n, w = Math.sin(t * Math.PI) * bow;
      const x = lerp(a.p.x, tx, t) + px * w, z = lerp(a.p.z, tz, t) + pz * w;
      // s === 0 is the junction with the coast road itself, which the loop above
      // has already guaranteed is on land — testing it again only lets rounding
      // at the waterline throw away an otherwise good route.
      if (s && H(x, z) < 0.5) return null; // no bridges, no fords
      pts.push({ x, z });
    }
    return pts;
  };
  const addRoute = (pts, asphalt) => {
    if (!pts) return false;
    (asphalt ? asphaltLines : dirtLines).push(pts);
    (asphalt ? WORLD.roads : WORLD.paths).push(pts);
    return true;
  };

  // --- facilities (each with a map name; skipped gracefully if no terrain fits)
  function scanBest(score) {
    let best = null, bs = -Infinity;
    for (let x = -3600; x <= 3600; x += 180)
      for (let z = -3600; z <= 3600; z += 180) {
        const s = score(x, z);
        if (s !== null && s > bs) { bs = s; best = { x, z }; }
      }
    return best;
  }
  /* --- airfield.
     MEASURED BUG, fixed here: the old gate demanded 600 m of ground flat to
     within 4.5 m and then took whatever `bestDir` the LAST passing candidate
     happened to leave in the closure — not the winner's heading. On the default
     island (terrain seed 1337) 0 of the 31 candidate sites cleared the gate, so
     there was NO AIRFIELD AT ALL: 2 of 5 sampled seeds shipped an island whose
     printed sheet, convoy pit stops and resection brief all referred to a
     landmark that did not exist.

     Now the strip LENGTH adapts instead of the site being thrown away — 620 m
     if the island can hold it, else 480, else 340, which is how expedient
     strips were actually sited — and the winning heading travels with the
     winning site. Measured over 7 terrain seeds: every one places, longitudinal
     grade 0.41%-0.74%, ~6-11k H() calls for the whole search. */
  {
    let af = null;
    for (const half of [310, 240, 170]) {
      let bs = -Infinity;
      for (let x = -3600; x <= 3600; x += 90)
        for (let z = -3600; z <= 3600; z += 90) {
          const h = H(x, z);
          if (h < 2 || h > 14) continue;          // low, dry, off the beach
          let bd = null;
          // 12 headings, not 4. A coral shelf runs at whatever angle the coast
          // runs at; a 45-degree-only search threw away almost every real site.
          for (let d = 0; d < 12; d++) {
            const th = d * Math.PI / 12, ux = Math.sin(th), uz = -Math.cos(th);
            const px = -uz, pz = ux;
            let dev = 0, wet = false;
            // sample the whole FORMATION, not just the centreline: the strip is
            // 40 m wide and the centreline-only test put half a runway out over
            // the sea, because a 100 m centreline sample can straddle a bay.
            for (let s = -half; s <= half && !wet; s += 60)
              for (let q = -20; q <= 20; q += 20) {
                const hh = H(x + ux * s + px * q, z + uz * s + pz * q);
                if (hh < 1.0) { wet = true; break; }
                dev = Math.max(dev, Math.abs(hh - h));
              }
            if (wet) continue;
            if (!bd || dev < bd.dev) bd = { dev, th };
          }
          // 7 m over 620 m is a 1.1% longitudinal grade — inside what WW2
          // expedient strips were actually built to, and the strip ribbon
          // follows H so the slope is real rather than hidden.
          if (!bd || bd.dev > 7.0) continue;
          const sc = -bd.dev - Math.abs(h - 5) * 0.2;
          if (sc > bs) { bs = sc; af = { x, z, th: bd.th, half }; }
        }
      if (af) break;                              // longest strip this island holds
    }
    if (af) {
      const th = af.th, ux = Math.sin(th), uz = -Math.cos(th);
      const perpX = -uz, perpZ = ux;              // across the strip
      const pts = [];
      for (let s = -af.half; s <= af.half; s += af.half / 5)
        pts.push({ x: af.x + ux * s, z: af.z + uz * s });
      const m = ribbonMesh(pts, 40, _wMats.strip, 0.28);
      if (m) WORLD.group.add(m);
      // apron + taxiway off the midpoint, metalled so it plots as a road
      const apron = [];
      for (let s = 0; s <= 4; s++)
        apron.push({ x: af.x + perpX * (12 + s * 14), z: af.z + perpZ * (12 + s * 14) });
      addRoute(apron, true);
      // the built side: two hangars, an ops/tower block, a windsock mast
      const bx = af.x + perpX * 62, bz = af.z + perpZ * 62;
      // 6.0 m, matching the mil card's "hangar 6 m high". The hangar replaced the
      // watchtower as the card's large reference object: the observer is standing
      // ON the watchtower and can never mil it, and the card's 100 m figure was
      // wrong anyway against a 300 m tower. A hangar is big, hard-edged, plotted
      // on the sheet, and visible across the island — an actual usable reference.
      // KEEP EQUAL TO THE CARD.
      wBox(bx, bz, 16, 6.0, 11, _wMats.mil, th);
      wBox(bx + ux * 26, bz + uz * 26, 16, 6.0, 11, _wMats.mil, th);
      wBox(bx - ux * 30, bz - uz * 30, 9, 8.5, 8, _wMats.milB, th);   // tower/ops
      wBox(bx - ux * 30, bz - uz * 30, 0.4, 5, 0.4, _wMats.mast);     // windsock
      for (let i = 0; i < 3; i++)
        wBox(bx + ux * 46 + perpX * i * 2.6, bz + uz * 46 + perpZ * i * 2.6,
             1.8, 2.0, 1.8, _wMats.drum);
      WORLD.facilities.push({ kind: 'airfield', name: 'AIRFIELD', x: af.x, z: af.z,
                              th, len: af.half * 2 });
    }
  }
  // radio mast: prominent high ground away from the OP — prime resection landmark
  {
    const mp = scanBest((x, z) => {
      const h = H(x, z);
      if (h < 35) return null;
      if (dist2(x, z, OP.x, OP.z) < 900) return null;
      return h;
    });
    if (mp) {
      wBox(mp.x, mp.z, 0.9, 26, 0.9, _wMats.mast);
      wBox(mp.x + 6, mp.z + 4, 5, 2.6, 4, _wMats.mil);
      WORLD.facilities.push({ kind: 'mast', name: 'RDO MAST', x: mp.x, z: mp.z });
    }
  }
  // coastal gun: headland overlooking water
  {
    const cg = scanBest((x, z) => {
      const h = H(x, z);
      if (h < 8 || h > 30) return null;
      const l = Math.hypot(x, z) || 1;
      if (H(x + x / l * 240, z + z / l * 240) > 0) return null;   // water seaward
      return -Math.abs(h - 16);
    });
    if (cg) {
      wBox(cg.x, cg.z, 6, 2.2, 6, _wMats.milB);
      wBox(cg.x, cg.z, 1.2, 1.2, 7.5, _wMats.milB, Math.atan2(cg.x, cg.z));
      WORLD.facilities.push({ kind: 'gun', name: 'CSTL GUN', x: cg.x, z: cg.z });
    }
  }
  // fuel point + ammo depot: on the coast road, well apart
  if (road.length > 8) {
    const fFrac = 0.1 + rng() * 0.3, aFrac = (fFrac + 0.4 + rng() * 0.2) % 1;
    const fp = inland(roadAt(fFrac), 30), ap = inland(roadAt(aFrac), 34);
    if (H(fp.x, fp.z) > 1) {
      wBox(fp.x, fp.z, 8, 3.4, 6, _wMats.mil);
      for (let i = 0; i < 3; i++)
        wBox(fp.x + 7 + i * 2.6, fp.z - 3, 1.8, 2.0, 1.8, _wMats.drum);
      WORLD.facilities.push({ kind: 'fuel', name: 'FUEL PT', x: fp.x, z: fp.z });
    }
    if (H(ap.x, ap.z) > 1) {
      for (let i = 0; i < 3; i++)
        wBox(ap.x + (i - 1) * 11, ap.z + (i % 2) * 7, 8, 2.8, 6, _wMats.milB, i * 0.35);
      WORLD.facilities.push({ kind: 'ammo', name: 'AMMO DEPOT', x: ap.x, z: ap.z });
    }
  }

  // --- civilian villages on the coast road, with wandering civilians
  const usedFracs = [];
  for (let v = 0; v < W.villageCount && road.length > 8; v++) {
    let frac, tries = 0;
    do { frac = rng(); tries++; }
    while (tries < 20 && (usedFracs.some(u => Math.abs(u - frac) < 0.18 || Math.abs(u - frac) > 0.82) ||
           WORLD.facilities.some(f => dist2(f.x, f.z, roadAt(frac).x, roadAt(frac).z) < 500)));
    usedFracs.push(frac);
    const c = inland(roadAt(frac), 55);
    if (H(c.x, c.z) < 1.5) continue;
    const vil = { name: isleName(rng), x: c.x, z: c.z, r: 70, huts: [] };
    const nH = 5 + Math.floor(rng() * 3);
    for (let i = 0; i < nH; i++) {
      const a = rng() * Math.PI * 2, r = 12 + rng() * 42;
      const hx = c.x + Math.cos(a) * r, hz = c.z + Math.sin(a) * r;
      if (H(hx, hz) < 1) continue;
      // 3.0 m high to match the mil card's "hut 3 m", same as the target huts —
      // the observer cannot tell a village hut from an enemy-held one by eye, so
      // they must not mil to different ranges. Was 2.6 m, a 15% range error.
      wBox(hx, hz, 5.2, 3.0, 4.2, rng() < 0.5 ? _wMats.civ : _wMats.civB, rng() * Math.PI);
      vil.huts.push({ x: hx, z: hz });
    }
    if (!vil.huts.length) continue;
    for (let i = 0; i < W.civsPerVillage; i++) {
      const m = makeFigure('civ');
      WORLD.group.add(m);
      WORLD.civs.push({ m, bx: c.x + (rng() - 0.5) * 70, bz: c.z + (rng() - 0.5) * 70,
                        wr: 5 + rng() * 9, phase: rng() * 20, x: c.x, z: c.z });
    }
    WORLD.villages.push(vil);
  }

  /* --- E6: TOWNS. The third settlement tier, and the one the observer is meant
     to be able to name at 2-3 km without a reticle. A village is 5-7 identical
     thatch huts scattered on a 70 m radius off a dirt track. A town is:
       · streets — a main street and a cross street, METALLED, so they plot as
         hard-surface road on the sheet and the block layout is visible from the
         OP as straight lines where the jungle has none;
       · 14-21 buildings of varied footprint and height set back along both
         sides of those streets, not scattered on a circle;
       · rendered masonry (pale, cool) and tiled roofs instead of thatch, which
         is what actually carries the tier at range once detail has washed out;
       · a metalled spur to the coast road, because a town is somewhere trucks
         go.
     It goes into WORLD.villages with tier:'town', so it inherits the existing
     collateral-damage auto-fail and the (CIV) labelling on both map surfaces
     for free — a town is a bigger no-strike area, not a new rule. */
  for (let t = 0; t < W.townCount && road.length > 8; t++) {
    const site = scanBest((x, z) => {
      const h = H(x, z);
      if (h < 3 || h > 45) return null;
      const a = nearestOnRoad(x, z);
      if (!a || a.d > 340 || a.d < 70) return null;
      let dev = 0;
      for (const [ox, oz] of [[-120, 0], [120, 0], [0, -120], [0, 120], [-85, -85], [85, 85]])
        dev = Math.max(dev, Math.abs(H(x + ox, z + oz) - h));
      if (dev > 24) return null;
      for (const v of WORLD.villages) if (dist2(x, z, v.x, v.z) < 900) return null;
      for (const f of WORLD.facilities) if (dist2(x, z, f.x, f.z) < 700) return null;
      return -dev - Math.abs(a.d - 170) * 0.02;
    });
    if (!site) continue;
    const anchor = nearestOnRoad(site.x, site.z);
    // main street runs ACROSS the line to the coast road, so the spur arrives
    // at one end of it instead of running down the middle of the town
    const ax = (site.x - anchor.p.x) / anchor.d, az = (site.z - anchor.p.z) / anchor.d;
    const mx = -az, mz = ax;                       // main-street unit vector
    const town = { name: isleName(rng2), x: site.x, z: site.z, r: W.townR,
                   tier: 'town', huts: [] };
    const streets = [];
    const mkStreet = (ux, uz, half) => {
      const pts = [];
      for (let s = -half; s <= half; s += half / 4) {
        const x = site.x + ux * s, z = site.z + uz * s;
        if (H(x, z) < 1.2) return null;
        pts.push({ x, z });
      }
      return pts;
    };
    const main = mkStreet(mx, mz, W.townStreetHalf);
    if (!main) continue;
    streets.push({ pts: main, ux: mx, uz: mz, half: W.townStreetHalf });
    const cross = mkStreet(ax, az, W.townCrossHalf);
    if (cross) streets.push({ pts: cross, ux: ax, uz: az, half: W.townCrossHalf });
    const want = W.townBuildings[0] +
                 Math.floor(rng2() * (W.townBuildings[1] - W.townBuildings[0]));
    let placed = 0;
    for (const st of streets) {
      const px = -st.uz, pz = st.ux;               // street normal
      for (let side = -1; side <= 1 && placed < want; side += 2) {
        let s = -st.half + 14;
        while (s < st.half - 14 && placed < want) {
          const w = 6.5 + rng2() * 7.5, d = 6 + rng2() * 5.5;
          const twoStorey = rng2() < 0.3;
          const hgt = twoStorey ? 6.4 + rng2() * 2.2 : 3.2 + rng2() * 1.4;
          const off = W.townSetback + rng2() * 5;
          const bx = site.x + st.ux * s + px * off * side;
          const bz = site.z + st.uz * s + pz * off * side;
          const g = H(bx, bz);
          if (g >= 1.2) {
            const rot = Math.atan2(st.ux, st.uz) + (rng2() - 0.5) * 0.12;
            const mat = twoStorey ? _wMats.civM : (rng2() < 0.45 ? _wMats.civM : _wMats.civ);
            wBox(bx, bz, w, hgt, d, mat, rot);
            wBoxY(bx, g + hgt + 0.55, bz, w + 1.3, 1.1, d + 1.3, _wMats.civR, rot);
            town.huts.push({ x: bx, z: bz, w, d });
            placed++;
          }
          s += 20 + rng2() * 12;
        }
      }
    }
    if (placed < 6) continue;                      // not a town; drop it
    /* Built-up-area extent, measured off the buildings actually placed and
       carried on the town so BOTH map surfaces can draw the settlement as a
       generalised block instead of a hundred overlapping dots. A 10 m building
       is 1.2 px at 1:50,000 — plotting them individually produced a solid black
       smear that hid the streets, which is exactly why real sheets generalise a
       town into an outline with the street pattern drawn through it. */
    {
      const nx = -mz, nz = mx;
      let aMin = 1e9, aMax = -1e9, bMin = 1e9, bMax = -1e9;
      for (const b of town.huts) {
        const dx = b.x - site.x, dz = b.z - site.z;
        const a = dx * mx + dz * mz, n = dx * nx + dz * nz;
        if (a < aMin) aMin = a; if (a > aMax) aMax = a;
        if (n < bMin) bMin = n; if (n > bMax) bMax = n;
      }
      const pad = 22;
      town.foot = { ux: mx, uz: mz,
                    aHalf: (aMax - aMin) / 2 + pad, bHalf: (bMax - bMin) / 2 + pad,
                    cx: site.x + mx * (aMax + aMin) / 2 + nx * (bMax + bMin) / 2,
                    cz: site.z + mz * (aMax + aMin) / 2 + nz * (bMax + bMin) / 2 };
    }
    for (const st of streets) { WORLD.roads.push(st.pts); asphaltLines.push(st.pts); }
    for (let i = 0; i < W.civsPerTown; i++) {
      const m = makeFigure('civ');
      WORLD.group.add(m);
      WORLD.civs.push({ m, bx: site.x + (rng2() - 0.5) * W.townR * 1.4,
                        bz: site.z + (rng2() - 0.5) * W.townR * 1.4,
                        wr: 6 + rng2() * 11, phase: rng2() * 20, x: site.x, z: site.z });
    }
    WORLD.villages.push(town);
  }

  /* --- E7: ROCK OUTCROPS.
     Everything else on this island is man-made, which means every landmark the
     observer has is something someone chose to build near the water. Inland he
     has a wall of identical jungle. Outcrops are the natural, permanent, plotted
     features that make resection work away from the coast road.

     An outcrop wants ground that is STEEP and CONVEX — a spur shoulder or a
     ridge crest, where weathering strips the soil off and leaves the bedrock
     standing. Flat ground grows over it; a hollow collects the soil that buries
     it. So the score is slope x convexity, gated on elevation to keep them out
     of the surf and off the massif summit.

     MEASURED, and the reason the sector quota exists: terrain score ALONE
     bunches the whole set onto whichever bearings the ridge fingers run. On the
     default island that left 2289 mils of empty arc — an observer facing west
     had no rock to resect off at all. Taking the best candidate in each 800-mil
     sector before taking any second one closes it to 1288 mils, and does not
     override the terrain test: a sector with no qualifying ground still gets
     nothing. Measured over 6 terrain seeds: 14/14 placed on every one. */
  {
    const RK = W.rock;
    const rockScore = (x, z) => {
      const h = H(x, z);
      if (h < 18 || h > 150) return null;
      const d = 55;
      const hE = H(x + d, z), hW = H(x - d, z), hN = H(x, z - d), hS = H(x, z + d);
      const slope = Math.hypot(hE - hW, hN - hS) / (2 * d);
      const conv = h - (hE + hW + hN + hS) / 4;      // > 0 = standing proud
      if (slope < 0.045 || conv < 0.4) return null;
      return slope * 26 + conv * 1.5;
    };
    const cands = [];
    for (let x = -3400; x <= 3400; x += 100)
      for (let z = -3400; z <= 3400; z += 100) {
        const s = rockScore(x, z);
        if (s !== null) cands.push({ x, z, s: s + rng3() * 0.9 });
      }
    cands.sort((a, b) => b.s - a.s);
    const tooClose = (x, z) => {
      if (dist2(x, z, OP.x, OP.z) < 300) return true;
      for (const r of WORLD.rocks) if (dist2(x, z, r.x, r.z) < RK.sep) return true;
      for (const f of WORLD.facilities) if (dist2(x, z, f.x, f.z) < 200) return true;
      for (const v of WORLD.villages) if (dist2(x, z, v.x, v.z) < 220) return true;
      return false;
    };
    const take = c => {
      if (tooClose(c.x, c.z)) return false;
      WORLD.rocks.push({ x: c.x, z: c.z, s: c.s, h: H(c.x, c.z),
                         r: RK.rMin + rng3() * (RK.rMax - RK.rMin), name: null });
      return true;
    };
    const SECT = 8, sMil = 6400 / SECT;
    for (let s0 = 0; s0 < SECT && WORLD.rocks.length < RK.count; s0++)
      for (const c of cands) {
        const mil = (Math.atan2(c.x - OP.x, OP.z - c.z) * MILS_PER_RAD + 6400) % 6400;
        if (Math.floor(mil / sMil) !== s0) continue;
        if (take(c)) break;
      }
    for (const c of cands) {
      if (WORLD.rocks.length >= RK.count) break;
      take(c);
    }
    /* The most prominent few get names and a label on both map surfaces. Naming
       all fourteen would bury the sheet in text and make none of them memorable;
       three is enough to say "the enemy is 400 short of THE TEETH" and be
       understood. Named outcrops also join nearestLandmark(), so the SALUTE spot
       report can cue off them. */
    const byScore = WORLD.rocks.slice().sort((a, b) => b.s - a.s);
    const pool = ROCK_NAMES.slice();
    for (let i = 0; i < RK.named && i < byScore.length && pool.length; i++)
      byScore[i].name = pool.splice(Math.floor(rng3() * pool.length), 1)[0];

    // --- boulders. One merged, vertex-coloured mesh for the whole island.
    const boulders = [];
    for (const rk of WORLD.rocks) {
      const n = RK.nMin + Math.floor(rng3() * (RK.nMax - RK.nMin + 1));
      for (let i = 0; i < n; i++) {
        // i === 0 is the dominant block, dead centre; the rest ring it. A pile
        // with one clear high point reads as a single feature at 2 km instead of
        // as scattered noise, which is what makes it identifiable on a bearing.
        const ang = rng3() * Math.PI * 2;
        const rad = i ? rk.r * (0.35 + rng3() * 0.65) : 0;
        const bx = rk.x + Math.cos(ang) * rad, bz = rk.z + Math.sin(ang) * rad;
        const sz0 = (i ? 0.26 + rng3() * 0.26 : 0.46 + rng3() * 0.14) * rk.r;
        const sx = sz0 * (0.8 + rng3() * 0.5), sy = sz0 * (0.7 + rng3() * 0.6),
              szz = sz0 * (0.8 + rng3() * 0.5);
        // Sit it on the LOWEST ground it covers, then sink it half its own
        // height. On a 33 m terrain facet a boulder placed on the centre sample
        // floats off the downhill edge; taking the minimum buries the uphill
        // side instead, which is what a real block on a slope does.
        let gy = H(bx, bz);
        for (let q = 0; q < 4; q++) {
          const a2 = q * Math.PI / 2;
          gy = Math.min(gy, H(bx + Math.cos(a2) * sx, bz + Math.sin(a2) * szz));
        }
        boulders.push({
          x: bx, y: Math.max(gy, 0) + sy * 0.5, z: bz,
          sx, sy, sz: szz,
          rx: (rng3() - 0.5) * 0.7, ry: rng3() * Math.PI * 2, rz: (rng3() - 0.5) * 0.7,
          col: ROCK_PALETTE[Math.floor(rng3() * ROCK_PALETTE.length)],
          shade: 0.82 + rng3() * 0.34,
        });
      }
    }
    const rm = mergedRocks(boulders);
    if (rm) WORLD.group.add(rm);
  }

  // --- dirt paths: villages/facilities up toward the interior (first ridge shoulder)
  const pathTargets = [...WORLD.villages.filter(v => v.tier !== 'town'),
                       ...WORLD.facilities.filter(f => f.kind === 'mast' || f.kind === 'gun')];
  for (let i = 0; i + 1 < pathTargets.length; i += 2) {
    const a = pathTargets[i], b = pathTargets[i + 1];
    const pts = [];
    const n = 26;
    for (let s = 0; s <= n; s++) {
      const t = s / n;
      const x = lerp(a.x, b.x, t) + Math.sin(t * 9 + i) * 26;
      const z = lerp(a.z, b.z, t) + Math.cos(t * 7 - i) * 26;
      if (H(x, z) < 0.5) { pts.length = 0; break; }   // refuse water crossings
      pts.push({ x, z });
    }
    if (pts.length) { WORLD.paths.push(pts); dirtLines.push(pts); }
  }

  /* --- E4: the two classes get their own network.
     ASPHALT reaches the things that need a truck: the airfield, the ammo depot
     and the fuel point (the three convoy pit stops in SPEC — a column halts
     where the metalled route lets it). DIRT reaches everything a truck does not
     need to reach: the villages, the coastal gun, the mast, and the track up to
     the OP. That split is the terrain-association lesson, not decoration: on the
     sheet the observer can tell which routes a vehicle column can actually use. */
  for (const f of WORLD.facilities) {
    if (f.kind === 'airfield') addRoute(linkTo(f.x, f.z, 3000), true);
    else if (f.kind === 'fuel' || f.kind === 'ammo') addRoute(linkTo(f.x, f.z, 260), true);
    else if (f.kind === 'gun' || f.kind === 'mast') addRoute(linkTo(f.x, f.z, 2200), false);
  }
  for (const v of WORLD.villages)
    addRoute(linkTo(v.x, v.z, v.tier === 'town' ? 600 : 700), v.tier === 'town');
  addRoute(linkTo(OP.x, OP.z, 6000), false);          // the track up to the OP

  // --- one merged mesh per class. Three draw calls for the whole network.
  const netMeshes = [
    mergedRibbonMesh(asphaltLines, W.roadWidth + W.shoulderW * 2, _wMats.asphE, 0.20),
    mergedRibbonMesh(asphaltLines, W.roadWidth, _wMats.asph, 0.26),
    mergedRibbonMesh(dirtLines, W.pathWidth, _wMats.path, 0.18),
  ];
  for (const m of netMeshes) if (m) WORLD.group.add(m);
  flushBoxes(WORLD.group);             // E6 — one mesh per material, not per box

  buildVegetation();   // 13f — instanced canopy + scatter, seeded per island

  scene.add(WORLD.group);
  // Everything in here is brand new geometry with no `_m0` yet. If the observer
  // is on NVG or thermal when an island is rebuilt, re-derive the variants or
  // the whole world layer renders in daylight colours inside the tube.
  if (visionReady) applyVision();
}

/* ============================================================ 13f VEGETATION */
/* Two InstancedMesh objects, two draw calls (GRAPHICS.md §G4): alpha-TESTED
   crossed-quad canopy (no blending, no sort) and a low-poly scrub/rock
   scatter, both tinted per instance. Placement draws from its OWN seeded
   stream (wSeed ^ golden ratio) so adding vegetation cannot shift a single
   existing road, facility or village on any island already in play.

   Every hard rule here is training-driven, not aesthetic: canopy tops out at
   CONFIG.GFX.vegMaxH (6 m) so it can never hide a burst plume; roads, paths,
   structures, villages and the named rocks keep clear margins so the map
   sheet's terrain-association features stay identifiable in 3D; and
   vegMissionCull() zeroes instances around each mission's elements so the
   observer can always SEE what he is shooting at and what he must not hit. */
/* 12i — the island's wind, seeded with the island. ONE vector that every
   smoke system obeys (burst columns, marker wisps, screens, the illum flare's
   chute), which is what makes drift READABLE AS A TOOL: the observer watches
   any smoke on the island and knows the wind everywhere on it. Rounds are
   NOT wind-affected — impact = aimpoint + error stands (CLAUDE.md ballistics
   rule); wind here is observation, the same honesty as TOD. `fromMils` is the
   direction the wind blows FROM (how wind is reported); dx/dz is the drift
   direction smoke actually travels (downwind), in m/s. */
const WIND = { dx: 0, dz: 0, speed: 0, fromMils: 0 };
function rollWind(seed) {
  const r = mulberry32((seed ^ 0x517CC1B7) >>> 0);
  const from = r() * Math.PI * 2;
  WIND.speed = 1.5 + r() * 4.5;                       // 1.5–6 m/s
  WIND.fromMils = Math.round(radToMils(from) / 100) * 100 % 6400;
  WIND.dx = -Math.sin(from) * WIND.speed;             // FROM az → TO vector
  WIND.dz = Math.cos(from) * WIND.speed;
}

const VEG = { canopy: null, scatter: null, placements: [] };
let _vegTex = null;
function vegTexture() {
  if (_vegTex) return _vegTex;
  // procedural silhouette — no image assets (golden rule). White shape on
  // transparent; per-instance colour supplies the green.
  const cv = document.createElement('canvas');
  cv.width = cv.height = 128;
  const g = cv.getContext('2d');
  g.clearRect(0, 0, 128, 128);
  g.fillStyle = '#fff';
  g.fillRect(60, 6, 8, 122);                       // trunk
  for (let i = 0; i < 26; i++) {                   // blobby broadleaf crown
    const a = (i / 26) * Math.PI * 2;
    const r = 26 + 14 * Math.abs(Math.sin(i * 2.7));
    g.beginPath();
    g.ellipse(64 + Math.cos(a) * r * 0.9, 34 + Math.sin(a) * r * 0.55,
              17, 12, a, 0, Math.PI * 2);
    g.fill();
  }
  _vegTex = new THREE.CanvasTexture(cv);
  return _vegTex;
}
function crossedQuadGeo() {
  // two vertical quads crossed at 90° + one horizontal crown cap, origin at
  // the BASE so instances sit on H. The cap is what makes a tree readable
  // from the OP: the watchtower looks DOWN at most of the island, and
  // vertical cards foreshorten to nothing from above. Its UV window samples
  // just the crown blob of the texture.
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array([
    -0.5, 0, 0,  0.5, 0, 0,  0.5, 1, 0,  -0.5, 1, 0,
    0, 0, -0.5,  0, 0, 0.5,  0, 1, 0.5,  0, 1, -0.5,
    -0.42, 0.74, -0.42,  0.42, 0.74, -0.42,  0.42, 0.74, 0.42,  -0.42, 0.74, 0.42]), 3));
  geo.setAttribute('uv', new THREE.BufferAttribute(new Float32Array([
    0, 0, 1, 0, 1, 1, 0, 1,  0, 0, 1, 0, 1, 1, 0, 1,
    0.18, 0.52, 0.82, 0.52, 0.82, 0.96, 0.18, 0.96]), 2));
  // Foliage cards must not light by their geometric normals: horizontal
  // normals dot an overhead sun to ~0 and every tree renders black at noon —
  // and DoubleSide is no fix, because back faces get the normal FLIPPED and
  // go black from the other direction instead. So: all normals point UP (the
  // canopy takes the same Lambert term as the ground it stands on), and each
  // face is drawn twice with reversed winding so both view sides are FRONT
  // faces. FrontSide material, no flipping, correct hue at every TOD.
  geo.setIndex([0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11,
                2, 1, 0, 3, 2, 0, 6, 5, 4, 7, 6, 4, 10, 9, 8, 11, 10, 8]);
  const nrm = new Float32Array(36);
  for (let i = 1; i < 36; i += 3) nrm[i] = 1;
  geo.setAttribute('normal', new THREE.BufferAttribute(nrm, 3));
  return geo;
}
function buildVegetation() {
  VEG.placements.length = 0;
  // old GEOMETRY dies with WORLD.group's teardown; the materials (and their
  // optics variants) are ours to dispose or they accumulate per island change
  for (const m of [VEG.canopy, VEG.scatter]) {
    if (!m) continue;
    if (m.material._vis)
      for (const k in m.material._vis) m.material._vis[k].dispose();
    m.material.dispose();
  }
  VEG.canopy = VEG.scatter = null;
  if (!CONFIG.GFX.veg) return;
  const W = CONFIG.WORLD, G = CONFIG.GFX;
  const blk = TERRAIN_PALETTE === 'black';
  const budget = Math.round(G.vegBudget * (blk ? 0.3 : 1));
  const wSeed = (CONFIG.SEED.terrain ^ (DEM ? strHash(DEM.name) : 0)) >>> 0;
  const rng = mulberry32((wSeed ^ 0x9E3779B9) >>> 0);

  // exclusion primitives, gathered once. Lines use true point-to-segment
  // distance — road points can be a hundred metres apart, and a point-only
  // test would happily plant a tree in the middle of the carriageway.
  const lines = [];
  for (const rd of WORLD.roads) lines.push({ pts: rd, r: W.roadWidth / 2 + 10 });
  for (const p of WORLD.paths) lines.push({ pts: p, r: 8 });
  const segDist2 = (x, z, ax, az, bx, bz) => {
    const dx = bx - ax, dz = bz - az;
    const t = clamp(((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz || 1), 0, 1);
    const px = ax + dx * t - x, pz = az + dz * t - z;
    return px * px + pz * pz;
  };
  const onLine = (x, z) => {
    for (const L of lines) {
      const r2 = L.r * L.r, P = L.pts;
      for (let i = 1; i < P.length; i++)
        if (segDist2(x, z, P[i - 1].x, P[i - 1].z, P[i].x, P[i].z) < r2) return true;
    }
    return false;
  };
  const discs = [];
  for (const v of WORLD.villages) discs.push({ x: v.x, z: v.z, r: (v.r || 120) + 30 });
  for (const f of WORLD.facilities)
    discs.push({ x: f.x, z: f.z, r: f.kind === 'airfield' ? 320 : 100 });
  for (const rk of WORLD.rocks) discs.push({ x: rk.x, z: rk.z, r: (rk.r || 30) + 20 });
  discs.push({ x: OP.x, z: OP.z, r: 70 }, { x: BATTERY.x, z: BATTERY.z, r: 90 });
  const inDisc = (x, z) => {
    for (const d of discs) if (dist2(x, z, d.x, d.z) < d.r) return true;
    return false;
  };

  const R = G.vegRadius;
  for (let tries = budget * 5; tries > 0 && VEG.placements.length < budget; tries--) {
    const az = rng() * Math.PI * 2, rr = Math.sqrt(rng()) * R;
    const x = OP.x + Math.sin(az) * rr, z = OP.z - Math.cos(az) * rr;
    const h = H(x, z);
    if (h < 2 || h > 130) continue;                          // beach and bare rock
    const sl = Math.hypot(H(x + 9, z) - h, H(x, z + 9) - h) / 9;
    if (sl > 0.62) continue;                                 // the terrain's own rock threshold
    // clumping: the same noise field that patches the ground green, so the
    // trees stand where the terrain already says vegetation lives
    const clump = vnoise(x * 0.012, z * 0.012, 999);
    if (rng() > clump * 1.5 - 0.12) continue;
    if (rng() > 1 - (rr / R) * (rr / R) * 0.6) continue;     // radial thinning
    if (onLine(x, z) || inDisc(x, z)) continue;
    VEG.placements.push({ x, z, h,
      type: rng() < 0.68 ? 0 : 1,                            // 0 canopy, 1 scatter
      s: 0.55 + rng() * 0.45, rot: rng() * Math.PI * 2, t: rng() });
  }
  const canopyN = VEG.placements.filter(p => p.type === 0).length;
  const scatterN = VEG.placements.length - canopyN;
  if (!VEG.placements.length) return;

  // canopy sits in the terrain's own jungle band, a half-step darker — trees
  // must read as VEGETATION against the grass, not as black noise under ACES
  const cCanA = gfxPal(blk ? 0x42503A : 0x4C7A3E), cCanB = gfxPal(blk ? 0x353F2E : 0x3A6230);
  const cScrA = gfxPal(blk ? 0x4A473F : 0x7A7052), cScrB = gfxPal(blk ? 0x3C3A34 : 0x5E6B45);
  // canopy: NIR-bright foliage under NVG (the E3 rule), cool mass in thermal
  const canMat = visTag(new THREE.MeshLambertMaterial({
    map: vegTexture(), alphaTest: 0.5 }),   // FrontSide — see crossedQuadGeo
    { nvg: 0.85, th: [0.24, 0.24, 0.24] });
  const scrMat = visTag(new THREE.MeshLambertMaterial({ flatShading: true }),
    { nvg: 0.50, th: [0.27, 0.27, 0.27] });
  VEG.canopy = new THREE.InstancedMesh(crossedQuadGeo(), canMat, canopyN);
  VEG.scatter = new THREE.InstancedMesh(
    new THREE.IcosahedronGeometry(1, 0).scale(1, 0.55, 1), scrMat, scatterN);
  const col = new THREE.Color();
  let ci = 0, si = 0;
  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), up = new THREE.Vector3(0, 1, 0);
  for (const p of VEG.placements) {
    q.setFromAxisAngle(up, p.rot);
    if (p.type === 0) {
      const hgt = G.vegMaxH * (0.6 + 0.4 * p.s);             // hard cap: never over vegMaxH
      m4.compose(new THREE.Vector3(p.x, p.h, p.z), q, new THREE.Vector3(hgt * 0.9, hgt, hgt * 0.9));
      p.mi = ci; VEG.canopy.setMatrixAt(ci, m4);
      VEG.canopy.setColorAt(ci++, col.lerpColors(cCanA, cCanB, p.t));
    } else {
      const s = 0.8 + p.s * 1.6;
      m4.compose(new THREE.Vector3(p.x, p.h, p.z), q, new THREE.Vector3(s, s, s));
      p.mi = si; VEG.scatter.setMatrixAt(si, m4);
      VEG.scatter.setColorAt(si++, col.lerpColors(cScrA, cScrB, p.t));
    }
  }
  WORLD.group.add(VEG.canopy, VEG.scatter);
}
/* Per-mission cull: hide instances around the elements of THIS mission so
   targets, friendlies and the fall of shot stay observable — "a trainer where
   you cannot see your own rounds land is a broken trainer" (GRAPHICS.md §G4).
   The placements array is permanent; only the instance scales change. */
function vegMissionCull() {
  if (!VEG.canopy || !Scenario) return;
  const ex = [];
  const S = Scenario;
  if (S.enemy) ex.push({ x: S.enemy.x, z: S.enemy.z, r: 80 });
  for (const f of (S.friendlies || [])) ex.push({ x: f.x, z: f.z, r: 80 });
  if (S.fStart) {   // the assault corridor: friendlies walk this line under fire
    const n = Math.ceil(dist2(S.fStart.x, S.fStart.z, S.enemy.x, S.enemy.z) / 60);
    for (let i = 0; i <= n; i++) {
      const t = i / n;
      ex.push({ x: S.fStart.x + (S.enemy.x - S.fStart.x) * t,
                z: S.fStart.z + (S.enemy.z - S.fStart.z) * t, r: 55 });
    }
  }
  if (S.path)       // convoy route (WORLD3: road polyline or legacy line)
    for (let d = 0; d <= S.path.len; d += 80)
      ex.push({ ...pathPoint(S.path, d), r: 40 });
  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), up = new THREE.Vector3(0, 1, 0);
  const G = CONFIG.GFX;
  for (const p of VEG.placements) {
    let hide = false;
    for (const e of ex) if (dist2(p.x, p.z, e.x, e.z) < e.r) { hide = true; break; }
    q.setFromAxisAngle(up, p.rot);
    const mesh = p.type === 0 ? VEG.canopy : VEG.scatter;
    if (hide) m4.compose(new THREE.Vector3(p.x, -50, p.z), q, new THREE.Vector3(0.001, 0.001, 0.001));
    else if (p.type === 0) {
      const hgt = G.vegMaxH * (0.6 + 0.4 * p.s);
      m4.compose(new THREE.Vector3(p.x, p.h, p.z), q, new THREE.Vector3(hgt * 0.9, hgt, hgt * 0.9));
    } else {
      const s = 0.8 + p.s * 1.6;
      m4.compose(new THREE.Vector3(p.x, p.h, p.z), q, new THREE.Vector3(s, s, s));
    }
    mesh.setMatrixAt(p.mi, m4);
  }
  VEG.canopy.instanceMatrix.needsUpdate = true;
  VEG.scatter.instanceMatrix.needsUpdate = true;
}
buildWorldFeatures();

// per-frame: civilians wander their village; positions kept for collateral checks
function updateCivilians(t) {
  for (const c of WORLD.civs) {
    c.x = c.bx + Math.sin(t * 0.14 + c.phase) * c.wr;
    c.z = c.bz + Math.cos(t * 0.11 + c.phase * 1.7) * c.wr;
    c.m.position.set(c.x, Math.max(H(c.x, c.z), 0), c.z);   // origin is at the feet
  }
}

