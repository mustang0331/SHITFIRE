/* ============================================================ PRNG + NOISE */
function mulberry32(a) {
  return function () {
    a |= 0; a = a + 0x6D2B79F5 | 0;
    let t = Math.imul(a ^ a >>> 15, 1 | a);
    t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
    return ((t ^ t >>> 14) >>> 0) / 4294967296;
  };
}
function hash2(ix, iz, seed) {
  let n = Math.imul(ix, 374761393) + Math.imul(iz, 668265263) + Math.imul(seed, 974634211);
  n = Math.imul(n ^ n >>> 13, 1274126177);
  n ^= n >>> 16;
  return (n >>> 0) / 4294967296;
}
function vnoise(x, z, seed) {
  const ix = Math.floor(x), iz = Math.floor(z);
  const fx = x - ix, fz = z - iz;
  const ux = fx * fx * (3 - 2 * fx), uz = fz * fz * (3 - 2 * fz);
  const a = hash2(ix, iz, seed),     b = hash2(ix + 1, iz, seed);
  const c = hash2(ix, iz + 1, seed), d = hash2(ix + 1, iz + 1, seed);
  return a + (b - a) * ux + (c - a) * uz + (a - b - c + d) * ux * uz;
}
function fbm(x, z, seed, oct) {
  let amp = 0.5, f = 1, sum = 0, norm = 0;
  for (let i = 0; i < oct; i++) {
    sum += amp * vnoise(x * f, z * f, seed + i * 131);
    norm += amp; amp *= 0.5; f *= 2;
  }
  return sum / norm;
}
const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
const lerp = (a, b, t) => a + (b - a) * t;
const smoothstep = (a, b, v) => { const t = clamp((v - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };

/* ============================================================ TERRAIN  H(x,z) */
// World coords: x = east, z = south (north is -z). Island centered on origin.
// H dispatches to a loaded DEM when present, else the procedural island —
// everything downstream (mesh, lase, LOS, impacts, map sheets) only sees H.
let DEM = null;
function H(x, z) {
  return DEM ? demSample(x, z) : H_proc(x, z);
}
function demSample(x, z) {
  const n = DEM.n, d = DEM.data;
  const u = (x + 5000) / 10000, v = (z + 5000) / 10000;
  if (u < 0 || u > 1 || v < 0 || v > 1) return CONFIG.TERRAIN.oceanFloor;
  const fx = u * (n - 1), fz = v * (n - 1);
  const i0 = Math.floor(fx), j0 = Math.floor(fz);
  const i1 = Math.min(i0 + 1, n - 1), j1 = Math.min(j0 + 1, n - 1);
  const tx = fx - i0, tz = fz - j0;
  const a = d[j0 * n + i0], b = d[j0 * n + i1];
  const c = d[j1 * n + i0], e = d[j1 * n + i1];
  return a + (b - a) * tx + (c - a) * tz + (a - b - c + e) * tx * tz;
}
function H_proc(x, z) {
  const T = CONFIG.TERRAIN, s = CONFIG.SEED.terrain;
  const r = Math.hypot(x, z);
  const th = Math.atan2(z, x);
  const wob = (vnoise(Math.cos(th) * 2.4 + 40, Math.sin(th) * 2.4 + 40, s) * 2 - 1) * T.radiusWobble;
  const R = T.islandRadius + wob;
  const mask = 1 - smoothstep(0.72 * R, 1.04 * R, r);
  if (mask <= 0) return T.oceanFloor;

  // Ridge fingers: ridged fBm.
  const n = fbm(x * 0.00155, z * 0.00155, s + 7, 5);
  const ridge = Math.pow(1 - Math.abs(2 * n - 1), 1.6) * T.ridgeHeight;
  // Central volcanic massif.
  const dm = Math.hypot(x - T.massifX, z - T.massifZ);
  const massif = T.massifHeight * Math.exp(-(dm * dm) / (T.massifRadius * T.massifRadius));
  let h = mask * (5 + ridge + massif) + (1 - mask) * T.oceanFloor;
  // Coral beach shelf: flatten the low band near the waterline.
  if (h > 0) h *= 0.42 + 0.58 * smoothstep(0, 14, h);
  return h;
}

// Ray vs heightfield: march then bisect. Ocean surface (y=0) counts as a hit.
function groundHit(ox, oy, oz, dx, dy, dz, maxD) {
  const step = 6;
  let tPrev = 0;
  for (let t = step; t <= maxD; t += step) {
    const x = ox + dx * t, y = oy + dy * t, z = oz + dz * t;
    if (y <= Math.max(H(x, z), 0)) {
      let lo = tPrev, hi = t;
      for (let i = 0; i < 9; i++) {
        const m = (lo + hi) / 2;
        const my = oy + dy * m;
        if (my <= Math.max(H(ox + dx * m, oz + dz * m), 0)) hi = m; else lo = m;
      }
      const hx = ox + dx * hi, hz = oz + dz * hi;
      return { x: hx, y: Math.max(H(hx, hz), 0), z: hz, dist: hi };
    }
    tPrev = t;
  }
  return null;
}
function hasLOS(ax, ay, az, bx, by, bz) {
  const d = Math.hypot(bx - ax, by - ay, bz - az);
  const n = Math.ceil(d / 15);
  for (let i = 1; i < n; i++) {
    const t = i / n;
    const x = ax + (bx - ax) * t, y = ay + (by - ay) * t, z = az + (bz - az) * t;
    if (H(x, z) + 0.4 > y) return false;
  }
  return true;
}

/* ============================================================ GRID HELPERS */
function worldToEN(x, z) {
  return { e: CONFIG.MAP.originE + x + 5000, n: CONFIG.MAP.originN + 5000 - z };
}
function enToWorld(e, n) {
  return { x: e - CONFIG.MAP.originE - 5000, z: 5000 - (n - CONFIG.MAP.originN) };
}
function gridOf(x, z) {  // 6-digit, 100 m
  const en = worldToEN(x, z);
  return `${Math.floor(en.e / 100)} ${Math.floor(en.n / 100)}`;
}
function azTo(ax, az, bx, bz) {           // radians, 0 = grid north
  return Math.atan2(bx - ax, -(bz - az));
}
function radToMils(r) { return ((r * MILS_PER_RAD) % 6400 + 6400) % 6400; }
function fmtMils(m) { return String(Math.round(m) % 6400).padStart(4, '0'); }
/* F7 — strip the method-of-engagement prowords out of the TARGET DESCRIPTION.
   The readback was producing "...GRID 245 523, DANGER CLOSE, DANGER CLOSE TROOPS
   IN THE OPEN, OUT." The cause was not a double append: `locStr` gains ", DANGER
   CLOSE" once, correctly, but the description is taken as everything between the
   grid and OVER — so an observer who says "grid 245 523, danger close, troops in
   the open" has the proword captured in his description as well, and both get read
   back. Description means what the target IS, not how to engage it. Duplication
   in the doctrinal readback is the one place cosmetic sloppiness is not cosmetic. */
const descClean = d => (d || '')
  .replace(DC_RE, ' ')
  .replace(/\b(?:at my command|when ready|do not load|cannot observe)\b/g, ' ')
  // G15 — sheaf is method of engagement, not target description
  .replace(/\b(?:converged|open|parallel|linear|circular|special)\s+sheaf\b/g, ' ')
  .replace(/\s{2,}/g, ' ').replace(/^[\s,]+|[\s,]+$/g, '');

/* F6 — the DANGER CLOSE proword, matched with tolerance for what speech
   recognition actually produces. The gate was `raw.includes('danger close')`, an
   exact substring test, and two separate play transcripts show the browser's
   recogniser rendering the phrase as "DANGER CLOTHES". The player was then told
   he had not said the proword — punished for a doctrine violation he did not
   commit, by a transcription error he could not see.

   This is deliberately a NARROW fuzzy match, not a general one. It requires the
   word "danger" and then something close to "close"; it will not fire on unrelated
   text, and "danger" alone is not enough. The cost of a false positive here is
   real — it would let a genuinely undeclared danger-close call through the safety
   gate — so the list is explicit rather than a similarity metric. */
const DC_RE = /\bdanger\s*(?:close|closed|clothes|cloths|clove|cloze|glose)\b/;
const saidDangerClose = raw => DC_RE.test(raw);

/* G2 — grid <-> magnetic. Everything internal to the sim (azTo, corrections, the
   grid, the sheets, the net) is GRID. These two functions exist ONLY at the
   boundary where a number is shown to the observer as an instrument reading, or
   read back off one. Do not use them anywhere else: applying the declination
   twice, or in the wrong direction, is a 124-mil error that looks like nothing. */
/* G4 — the single source of truth for binocular optics. Everything that needs to
   know how magnified the view is (camera fov, reticle scale, mouse sensitivity,
   the tool readout) asks these, so there is exactly one place a zoom change has
   to take effect. */
const binoPower = () => CONFIG.CAMERA.binoZooms[CONFIG.CAMERA.binoZoom];
const binoFovNow = () => CONFIG.CAMERA.fov / binoPower();

/* G17 — who is on the other end of the net. Full form for traffic, short form
   for the vocative ("MUSTANG 12, HACKSAW — ..."), lowercase for parsing the
   observer's own transmissions. Everything asks these; nothing asks CONFIG
   directly, so a chapter's asset is the single switch. */
const fdcCall = () => activeChapter && activeChapter.asset === 'mortar60'
  ? CONFIG.FDC.fdc60 : CONFIG.FDC.fdc;
const fdcShort = () => fdcCall().replace(/\s+FIRES$/, '');

const declMils = () => CONFIG.NAV.declEastDeg / 360 * 6400;
const gridToMag = m => ((m - declMils()) % 6400 + 6400) % 6400;
const magToGrid = m => ((m + declMils()) % 6400 + 6400) % 6400;
const dist2 = (ax, az, bx, bz) => Math.hypot(bx - ax, bz - az);

