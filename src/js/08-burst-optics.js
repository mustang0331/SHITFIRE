/* ============================================================ BURST EFFECTS (preallocated pool) */
const bursts = [];
{
  const dustGeoH = new THREE.SphereGeometry(1, 10, 6, 0, Math.PI * 2, 0, Math.PI / 2);
  const ringGeo = new THREE.RingGeometry(0.78, 1, 28).rotateX(-Math.PI / 2);
  const puffGeo = new THREE.IcosahedronGeometry(1, 0);
  const debrisGeo = new THREE.BoxGeometry(0.5, 0.5, 0.5);
  const debrisMat = new THREE.MeshLambertMaterial({ color: 0x4a443c, flatShading: true });
  for (let b = 0; b < 10; b++) {
    const group = new THREE.Group();
    const flash = new THREE.Sprite(new THREE.SpriteMaterial({ color: 0xFFF2C0,
      blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
    flash.position.y = 3;
    const ring = new THREE.Mesh(ringGeo, new THREE.MeshBasicMaterial({ color: 0xE8DCB8,
      blending: THREE.AdditiveBlending, depthWrite: false, transparent: true,
      side: THREE.DoubleSide }));
    ring.position.y = 0.6;
    const dust = new THREE.Mesh(dustGeoH, new THREE.MeshLambertMaterial({ color: 0xB9A98C,
      transparent: true, flatShading: true, depthWrite: false }));
    const puffs = [];
    for (let i = 0; i < 5; i++) {
      const m = new THREE.Mesh(puffGeo, new THREE.MeshLambertMaterial({ color: 0x8A877E,
        transparent: true, flatShading: true, depthWrite: false }));
      puffs.push(m); group.add(m);
    }
    const debris = [];
    for (let i = 0; i < 6; i++) {
      const m = new THREE.Mesh(debrisGeo, debrisMat);
      debris.push({ m, vx: 0, vy: 0, vz: 0 }); group.add(m);
    }
    group.add(flash); group.add(ring); group.add(dust);
    group.visible = false;
    scene.add(group);
    // optics signatures: the muzzle/impact flash and the ring are the brightest
    // things on the net; the dust column is genuinely hot for a few seconds.
    visTag(flash.material, { nvg: 1.00, th: [1.00, 1.00, 1.00] });
    visTag(ring.material,  { nvg: 0.95, th: [0.95, 0.95, 0.95] });
    visTag(dust.material,  { nvg: 0.85, th: [0.62, 0.62, 0.62] });
    for (const p of puffs) visTag(p.material, { nvg: 0.60, th: [0.44, 0.44, 0.44] });
    bursts.push({ group, flash, ring, dust, puffs, debris, t0: 0, active: false, seen: true });
  }
  visTag(debrisMat, { nvg: 0.20, th: [0.40, 0.40, 0.40] });
}

/* ============================================================ OBSERVER OPTICS (DAY / NVG / THERMAL) */
/* Three vision modes on the observer, cycled with [O]. Always on — not gated to
   binoculars — because the device is worn, not part of the glass, and because
   finding the target with the naked eye at night is the thing the device exists
   to make possible.

   IMPLEMENTATION CONSTRAINT (GRAPHICS.md §Do not do): no EffectComposer on the
   default render path. It costs a full-screen render target plus several passes
   and breaks the direct renderer.render() call the quality system assumes. So
   there is no post-processing here at all. Instead:

     1. MATERIAL VARIANTS. Every material in the scene gets an NVG clone and a
        THERMAL clone, cached on the source material itself (the _vis property).
        Toggling walks the scene once and reassigns .material. That is a
        one-time cost per toggle — a few hundred assignments, no allocation in
        the render loop — and it is the only thing that touches the GPU path.
     2. LIGHTS + FOG + BACKGROUND. applyTOD stays the lighting authority and
        always writes the base; applyVision multiplies the tube gain on top,
        retints the fog, hides the Preetham sky, and swaps in a pre-toned flat
        background so the horizon does not seam (the 13b lesson).
     3. A 2D CANVAS OVERLAY for grain, vignette, tube tint, and halo — the three
        things a material swap genuinely cannot do. 320x180 backing store, CSS
        stretched, redrawn at CONFIG.OPTICS.hz (24), every layer prebuilt at
        boot and drawn with drawImage. Nothing in it allocates per frame.

   DAYLIGHT BALANCE — the choice, stated: both devices stay AVAILABLE at every
   time of day and simply stop working. Hiding them from the menu in daylight
   would teach nothing; blooming them out teaches the actual lesson, which is
   that an image intensifier is a night device and a thermal sight loses its
   hot/cold separation once the sun has cooked the ground to body temperature.
   Above CONFIG.OPTICS.nvgBloomAt the NVG image collapses toward flat pale green;
   above thermalWashAt the thermal cold floor rises until terrain and men are the
   same grey. At full daylight both are worse than the naked eye, which is
   correct, and neither can trivialise a day mission.

   DISCRIMINATION IS A HARD GATE. See visTag() for how friendly, enemy, and
   civilian stay separable under a monochrome tube and under white-hot. */
const VISION = {
  mode: 'day',
  modes: ['day', 'nvg', 'thermal'],
  cv: document.getElementById('optics'),
  ctx: null,
  layer: { nvg: null, thermal: null },   // prebuilt tint + vignette + bloom veil
  grain: [],                             // prebuilt noise frames
  halo: { nvg: null, thermal: null },    // prebuilt radial blob
  src: [],                               // halo emitters (sprites), preallocated
  gi: 0, last: 0,
};
const _hv = new THREE.Vector3();          // halo projection scratch — never reallocated

/* --- pure colour maths (harness-verified; see the commit body) --------------- */
function visLum(c) { return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b; }
function visClamp01(v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
// how far past the "this device is out of its depth" threshold the ambient is
function visDegrade(ambient, at) {
  return at >= 1 ? 0 : visClamp01((ambient - at) / (1 - at));
}
// NVG: monochrome P43 green, gamma-lifted so shadow detail comes up the way a
// tube's response curve does. Blooms toward flat pale green in daylight.
function visNvgRGB(lum, bloom, out) {
  const g = visClamp01(Math.pow(visClamp01(lum), 0.55));
  const k = bloom * 0.85;
  out.r = g * 0.14 + (0.72 - g * 0.14) * k;
  out.g = g * 1.00 + (0.95 - g * 1.00) * k;
  out.b = g * 0.20 + (0.72 - g * 0.20) * k;
  return out;
}
// THERMAL: white-hot. Tagged signatures pass through; untagged materials get a
// faint luminance-derived cold grey so terrain relief still reads (an FO has to
// judge a burst against micro-relief, so a flat black ground would be a
// regression). The cold floor rises with ambient until the separation is gone.
function visThermalRGB(spec, lum, wash, out) {
  if (spec && spec.th) {
    // a tagged signature is pulled toward one mid grey as the ground heats up
    out.r = spec.th[0] + (0.80 - spec.th[0]) * wash * 0.85;
    out.g = spec.th[1] + (0.80 - spec.th[1]) * wash * 0.85;
    out.b = spec.th[2] + (0.80 - spec.th[2]) * wash * 0.85;
  } else {
    // untagged = cold; the floor lifts with ambient until cold meets hot
    const g = 0.05 + 0.13 * visClamp01(lum) + 0.62 * wash;
    out.r = out.g = out.b = g;
  }
  out.r = visClamp01(out.r); out.g = visClamp01(out.g); out.b = visClamp01(out.b);
  return out;
}

const _vo = { r: 0, g: 0, b: 0 };         // variant colour scratch
const _lc = { r: 0, g: 0, b: 0 };         // source colour scratch, sRGB
function visVariantOf(m0, mode) {
  if (mode === 'day') return m0;
  /* The registry lives in DIRECT properties, never in userData. r160's
     Material.copy deep-copies userData via JSON.stringify, and this registry is
     circular by construction (_vis -> variant -> _visBase -> source): parking it
     in userData made the SECOND variant clone of any material throw "Converting
     circular structure to JSON" - i.e. [O][O] day->NVG->THERMAL hard-crashed
     applyVision. Found by tools/shots.js the first time anything actually BOOTED
     the file; the parse gate can never see this class. Direct properties are
     invisible to clone(), which is also semantically right: a clone should not
     inherit another material's variant cache. */
  const cache = m0._vis || (m0._vis = {});
  if (!cache[mode]) {
    const v = m0.clone();
    v._visBase = m0;
    // a variant is a flat signature: vertex colours would multiply the terrain
    // palette back in and neither device would be monochrome
    if (v.vertexColors) { v.vertexColors = false; v.needsUpdate = true; }
    cache[mode] = v;
  }
  return cache[mode];
}
function visTintVariant(m0, mode, bloom, wash) {
  const v = visVariantOf(m0, mode);
  if (!v.color || !m0.color) return v;
  const spec = m0.userData.vis;
  // r160 keeps Color components in the LINEAR working space, but every number in
  // visNvgRGB/visThermalRGB is authored perceptually. Read and write in sRGB
  // explicitly so a "0.05 cold" really renders near-black instead of mid-grey.
  m0.color.getRGB(_lc, THREE.SRGBColorSpace);
  if (mode === 'nvg') {
    const lum = spec && spec.nvg !== undefined ? spec.nvg : visLum(_lc);
    visNvgRGB(lum, bloom, _vo);
  } else {
    visThermalRGB(spec, visLum(_lc), wash, _vo);
  }
  v.color.setRGB(_vo.r, _vo.g, _vo.b, THREE.SRGBColorSpace);
  return v;
}

/* Sky / fog / background per mode. Raw (pre-tone) hexes; the background is
   pushed through acesFilmic() exactly as 13b/13c do, and the fog is given the
   RAW value, so the two still agree at the horizon and no seam opens. */
function visSkyHex(mode, ambient) {
  if (mode === 'nvg') {
    const w = visDegrade(ambient, CONFIG.OPTICS.nvgBloomAt);
    return w > 0.5 ? 0x9FC79F : 0x0A2410;              // bloomed out vs a dark green dome
  }
  const w = visDegrade(ambient, CONFIG.OPTICS.thermalWashAt);
  return w > 0.5 ? 0x8A8A8A : 0x06080C;                // washed vs cold sky
}

function applyVision(forceMode) {
  if (!CONFIG.OPTICS.enabled) return;
  const mode = forceMode || VISION.mode;
  const t = TOD_TABLE[TOD], ambient = TOD_LIGHT();
  const bloom = visDegrade(ambient, CONFIG.OPTICS.nvgBloomAt);
  const wash = visDegrade(ambient, CONFIG.OPTICS.thermalWashAt);

  // ---- lights, fog, background (applyTOD has already written the base) ----
  if (mode === 'day') {
    sun.intensity = t.inten;
    sun.color.setHex(t.sun);
    hemi.intensity = t.hemiInt !== undefined ? t.hemiInt : 0.55;
    hemi.color.setHex(t.hemiSky);
    hemi.groundColor.setHex(t.hemiGnd !== undefined ? t.hemiGnd : 0x5C6B4E);
    scene.fog.color.setHex(t.fog);
    if (skyMesh) { skyMesh.visible = true; scene.background = null; }
  } else if (mode === 'nvg') {
    // Automatic gain control. A tube does not output "the scene times N" — it
    // drives its own screen to a roughly FIXED brightness whatever it is fed,
    // which is why an intensified image looks the same at midnight and at dusk.
    // So the rig is absolute, not a multiplier on the TOD. Amplification lives
    // in the pow(lum, 0.55) curve in visNvgRGB; multiplying here as well was
    // double-counting and pushed the whole image into saturation, where the
    // transfer curve flattens and contrast dies. Daylight degradation is carried
    // entirely by the bloom term, not by the rig.
    sun.intensity = 0.42;
    sun.color.setHex(0xFFFFFF);            // the phosphor colour lives in the materials
    hemi.intensity = 0.30;
    hemi.color.setHex(0xFFFFFF);
    hemi.groundColor.setHex(0x9A9A9A);
    const hex = visSkyHex('nvg', ambient);
    scene.fog.color.setHex(hex);
    if (skyMesh) skyMesh.visible = false;
    scene.background = CONFIG.GFX.toneMap
      ? acesFilmic(hex, CONFIG.GFX.exposure) : new THREE.Color(hex);
  } else {
    // Thermal has no sun, so the rig exists only to make a signature render as
    // itself — but with enough directional term left that slope relief still
    // reads. An FO judges a burst against micro-relief, and a perfectly flat
    // ground would be a regression. Real thermal sights do show relief
    // (differential heating and emissivity), so this is not a cheat.
    sun.intensity = 0.90;
    sun.color.setHex(0xFFFFFF);
    hemi.intensity = 0.55;
    hemi.color.setHex(0xFFFFFF);
    hemi.groundColor.setHex(0xFFFFFF);
    const hex = visSkyHex('thermal', ambient);
    scene.fog.color.setHex(hex);
    if (skyMesh) skyMesh.visible = false;
    scene.background = CONFIG.GFX.toneMap
      ? acesFilmic(hex, CONFIG.GFX.exposure) : new THREE.Color(hex);
  }

  // ---- material variants ----
  scene.traverse(o => {
    const cur = o.material;
    if (!cur || Array.isArray(cur)) return;
    if (o === skyMesh) return;
    let m0 = o.userData._m0;
    if (!m0) m0 = o.userData._m0 = cur._visBase || cur;
    if (!m0.color) return;                                  // ShaderMaterial etc.
    o.material = mode === 'day' ? m0 : visTintVariant(m0, mode, bloom, wash);
  });

  // ---- overlay ----
  VISION.cv.classList.toggle('on', mode !== 'day');
  if (mode !== 'day') { visBuildLayer(mode, ambient); drawOptics(); }
}

/* --- overlay construction (all prebuilt; drawOptics only calls drawImage) --- */
function visMakeCanvas(w, h) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  return c;
}
function visBuildLayer(mode, ambient) {
  const O = CONFIG.OPTICS, W = O.cvW, Hh = O.cvH;
  const c = VISION.layer[mode] || (VISION.layer[mode] = visMakeCanvas(W, Hh));
  const g = c.getContext('2d');
  g.clearRect(0, 0, W, Hh);
  if (mode === 'nvg') {
    const bloom = visDegrade(ambient, O.nvgBloomAt);
    // tube tint: a light green wash, heavy when the tube is blooming out
    g.fillStyle = `rgba(120,255,150,${0.06 + 0.42 * bloom})`;
    g.fillRect(0, 0, W, Hh);
  } else {
    const wash = visDegrade(ambient, O.thermalWashAt);
    g.fillStyle = `rgba(210,215,225,${0.03 + 0.34 * wash})`;
    g.fillRect(0, 0, W, Hh);
  }
  // heavy vignette — much heavier for NVG, which is looking down a tube
  const rg = g.createRadialGradient(W / 2, Hh / 2, Hh * 0.16, W / 2, Hh / 2, Hh * 0.82);
  const edge = mode === 'nvg' ? 0.90 : 0.62;
  rg.addColorStop(0, 'rgba(0,0,0,0)');
  rg.addColorStop(0.55, `rgba(0,0,0,${edge * 0.22})`);
  rg.addColorStop(1, `rgba(0,0,0,${edge})`);
  g.fillStyle = rg;
  g.fillRect(0, 0, W, Hh);
}
function visBuildGrain() {
  const O = CONFIG.OPTICS, W = O.cvW, Hh = O.cvH;
  for (let f = 0; f < 6; f++) {
    const c = visMakeCanvas(W, Hh);
    const g = c.getContext('2d');
    const img = g.createImageData(W, Hh);
    const d = img.data;
    for (let i = 0; i < d.length; i += 4) {
      const n = Math.random();
      const v = n * 255;
      d[i] = v * 0.55; d[i + 1] = v; d[i + 2] = v * 0.6;
      d[i + 3] = n > 0.72 ? 255 : 0;         // sparse scintillation, not TV snow
    }
    g.putImageData(img, 0, 0);
    VISION.grain.push(c);
  }
}
function visBuildHalo() {
  for (const mode of ['nvg', 'thermal']) {
    const c = visMakeCanvas(64, 64);
    const g = c.getContext('2d');
    const rg = g.createRadialGradient(32, 32, 0, 32, 32, 32);
    const col = mode === 'nvg' ? '190,255,205' : '255,255,255';
    rg.addColorStop(0, `rgba(${col},0.95)`);
    rg.addColorStop(0.35, `rgba(${col},0.34)`);
    rg.addColorStop(1, `rgba(${col},0)`);
    g.fillStyle = rg;
    g.fillRect(0, 0, 64, 64);
    VISION.halo[mode] = c;
  }
}
function visCollectSources() {
  VISION.src.length = 0;
  for (const f of units.flashes) VISION.src.push(f.s);
  for (const s of units.flames) VISION.src.push(s);
  for (const b of bursts) VISION.src.push(b.flash);
}
function drawOptics() {
  const mode = VISION.mode;
  if (mode === 'day' || !VISION.ctx) return;
  const O = CONFIG.OPTICS, W = O.cvW, Hh = O.cvH, ctx = VISION.ctx;
  ctx.clearRect(0, 0, W, Hh);
  // 1. halo on every bright emitter that is on screen and in front of us. NVG
  //    tubes and thermal cores both bloom around a point source; the overlay
  //    runs at 24 Hz, so a halo lags the camera by ~40 ms, which reads as tube
  //    persistence rather than as a bug.
  const halo = VISION.halo[mode];
  if (halo) {
    ctx.globalCompositeOperation = 'lighter';
    const r = O.haloR;
    for (let i = 0; i < VISION.src.length; i++) {
      const s = VISION.src[i];
      if (!s.visible || !s.parent || !s.parent.visible) continue;
      s.getWorldPosition(_hv);
      _hv.project(camera);
      if (_hv.z > 1 || _hv.x < -1.3 || _hv.x > 1.3 || _hv.y < -1.3 || _hv.y > 1.3) continue;
      const px = (_hv.x * 0.5 + 0.5) * W, py = (-_hv.y * 0.5 + 0.5) * Hh;
      ctx.drawImage(halo, px - r, py - r, r * 2, r * 2);
    }
  }
  // 2. sensor grain — cycled from prebuilt frames, never generated per draw
  if (VISION.grain.length) {
    VISION.gi = (VISION.gi + 1 + (Math.random() * 3 | 0)) % VISION.grain.length;
    ctx.globalAlpha = mode === 'nvg' ? O.grain : O.grain * 0.45;
    ctx.drawImage(VISION.grain[VISION.gi], 0, 0);
    ctx.globalAlpha = 1;
  }
  // 3. tint + vignette (+ daylight bloom veil), prebuilt by visBuildLayer
  ctx.globalCompositeOperation = 'source-over';
  const layer = VISION.layer[mode];
  if (layer) ctx.drawImage(layer, 0, 0);
}

function setVision(mode) {
  if (!CONFIG.OPTICS.enabled) return;
  if (VISION.modes.indexOf(mode) < 0) mode = 'day';
  VISION.mode = mode;
  applyVision();
  refreshTool();
  refreshStatus();
}
const VIS_LABEL = { day: 'DAY OPTIC', nvg: 'NVG', thermal: 'THERMAL' };
function cycleVision() {
  const i = VISION.modes.indexOf(VISION.mode);
  const next = VISION.modes[(i + 1) % VISION.modes.length];
  setVision(next);
  const a = TOD_LIGHT();
  if (next === 'day') {
    log('', 'Optics: naked eye / daylight glass.', 'sys');
  } else if (next === 'nvg') {
    log('', a > CONFIG.OPTICS.nvgBloomAt
      ? 'NIGHT VISION UP — and the tube is blooming out. There is far too much light for an image intensifier; you are looking at a green fog. This is a night device.'
      : 'NIGHT VISION UP. Green, grainy, and short on contrast — but you can see. Bright sources will halo; that is the tube, not the enemy.', 'sys');
  } else {
    log('', a > CONFIG.OPTICS.thermalWashAt
      ? 'THERMAL UP — and the ground is as warm as the men standing on it. The sun has cooked your contrast away. Thermal is for the dark.'
      : 'THERMAL UP. White-hot: bodies and running engines burn, terrain stays cold. Friendly markers read CYAN. Civilians are warm bodies under a COLD straw hat — check the head before you send, over.', 'sys');
  }
}

/* ---- 13h: impact persistence (GRAPHICS.md §G6) ------------------------------
   The burst itself evaporates in 14 s, but the OBSERVER'S READ of the shot
   group must not: persistent craters (an InstancedMesh ring buffer, oldest
   recycled) keep the round pattern on the ground — a walked bracket stays
   readable exactly like the plotted AAR — and a thin marker wisp lingers on
   the last few impacts (~75 s) while the correction is composed. Everything
   preallocated; nothing allocated per impact. DECLARED ABOVE THE BOOT BLOCK:
   initCraters/initWisps run from it, and `let` bindings below it would still
   be in their temporal dead zone (this file boots at module eval). */
let CRATERS = null, craterCursor = 0;
const _crM4 = new THREE.Matrix4(), _crQ = new THREE.Quaternion(),
      _crV = new THREE.Vector3(), _crN = new THREE.Vector3(), _crUp = new THREE.Vector3(0, 1, 0);
function initCraters() {
  const n = CONFIG.GFX.craters;
  if (!n) return;
  // fresh-turned earth: dark by day, WARM in thermal (churned ground holds heat)
  const mat = visTag(new THREE.MeshLambertMaterial({
    color: 0x2A2118, transparent: true, opacity: 0.8, depthWrite: false,
    polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2 }),
    { nvg: 0.22, th: [0.6, 0.6, 0.6] });
  CRATERS = new THREE.InstancedMesh(
    new THREE.CircleGeometry(1, 12).rotateX(-Math.PI / 2), mat, n);
  _crM4.compose(_crV.set(0, -400, 0), _crQ.identity(), _crN.set(0.001, 0.001, 0.001));
  for (let i = 0; i < n; i++) CRATERS.setMatrixAt(i, _crM4);
  scene.add(CRATERS);
}
function addCrater(x, z) {
  if (!CRATERS) return;
  const h = H(x, z);
  // conform to the slope so the disc lies ON the ground, not through it
  _crN.set(-(H(x + 6, z) - h) / 6, 1, -(H(x, z + 6) - h) / 6).normalize();
  _crQ.setFromUnitVectors(_crUp, _crN);
  const s = 2.6 + Math.random() * 1.7;
  _crM4.compose(_crV.set(x, h + 0.12, z), _crQ, _crN.set(s, 1, s));
  CRATERS.setMatrixAt(craterCursor++ % CRATERS.count, _crM4);
  CRATERS.instanceMatrix.needsUpdate = true;
}
function craterClear() {   // island rebuild: old coordinates are a different map
  if (!CRATERS) return;
  _crM4.compose(_crV.set(0, -400, 0), _crQ.identity(), _crN.set(0.001, 0.001, 0.001));
  for (let i = 0; i < CRATERS.count; i++) CRATERS.setMatrixAt(i, _crM4);
  CRATERS.instanceMatrix.needsUpdate = true;
  craterCursor = 0;
  for (const w of WISPS) w.t0 = -1e9;
}
const WISPS = [];
function initWisps() {
  for (let i = 0; i < 3; i++) {
    const m = new THREE.Sprite(visTag(new THREE.SpriteMaterial({
      color: 0xB9B4A8, transparent: true, opacity: 0, depthWrite: false }),
      { nvg: 0.4, th: [0.34, 0.34, 0.34] }));
    m.visible = false;
    scene.add(m);
    WISPS.push({ m, t0: -1e9, x: 0, y: 0, z: 0 });
  }
}
function markWisp(x, y, z) {
  let w = WISPS[0];
  for (const c of WISPS) if (c.t0 < w.t0) w = c;   // recycle the oldest
  w.t0 = sim.now; w.x = x; w.y = y; w.z = z;
}

/* ---- 12h: smoke screens + illumination -------------------------------------
   SCREENS: six slots of three sprites each, oldest recycled — a smoke round
   builds a standing white wall for EFFECTS.screen.sec, and a screen laid on
   the enemy suppresses them by obscuration (they cannot shoot what they
   cannot see), which is what makes an immediate-smoke mission accomplishable.
   ILLUM: ONE pooled point light + flare sprite — a real light source, so at
   night the terrain under it genuinely illuminates (the reason this row was
   blocked on 13c). Everything preallocated; per-round work is slot reuse. */
const SCREENS = [];
function initScreens() {
  for (let i = 0; i < 6; i++) {
    const sprs = [];
    for (let j = 0; j < 3; j++) {
      const m = new THREE.Sprite(visTag(new THREE.SpriteMaterial({
        color: 0xE9E7DE, transparent: true, opacity: 0, depthWrite: false }),
        { nvg: 0.6, th: [0.30, 0.30, 0.30] }));
      m.visible = false;
      scene.add(m); sprs.push(m);
    }
    SCREENS.push({ sprs, t0: -1e9, x: 0, y: 0, z: 0 });
  }
}
function deployScreen(x, y, z) {
  let s = SCREENS[0];
  for (const c of SCREENS) if (c.t0 < s.t0) s = c;
  s.t0 = sim.now; s.x = x; s.y = y; s.z = z;
  const S = Scenario, E = CONFIG.EFFECTS;
  // obscuration is suppression: a screened gun cannot serve its target
  if (S && S.enemy && enemyAlive &&
      dist2(x, z, S.enemy.x, S.enemy.z) < E.screen.radius) {
    S.everSuppressed = true;
    S.suppressedUntil = Math.max(S.suppressedUntil, sim.now + E.screen.sec);
  }
}
function updateScreens() {
  const E = CONFIG.EFFECTS;
  for (const s of SCREENS) {
    const a = (sim.now - s.t0) / E.screen.sec;
    if (a < 0 || a >= 1) { for (const m of s.sprs) m.visible = false; continue; }
    const grow = Math.min(1, a * 6);                    // builds over the first sixth
    const fade = Math.min(1, (1 - a) * 4);              // collapses over the last quarter
    for (let j = 0; j < s.sprs.length; j++) {
      const m = s.sprs[j];
      m.visible = true;
      m.position.set(s.x + (j - 1) * 13, s.y + 9 + j * 4 + a * 6, s.z + (j - 1) * 5);
      const sc = (18 + 20 * grow + a * 8) * (0.85 + j * 0.15);
      m.scale.set(sc, sc * 0.72, 1);
      m.material.opacity = 0.5 * grow * fade;
    }
  }
}
const ILLUM = { t0: -1e9, x: 0, z: 0, y: 0, light: null, spr: null };
function initIllum() {
  // r160 lights are PHYSICAL units (candela): a flare must push six figures
  // to pool light on ground 300 m below it — a real M485 is ~a million cd
  ILLUM.light = new THREE.PointLight(0xFFEDC0, 0, 2600, 2);
  scene.add(ILLUM.light);
  ILLUM.spr = new THREE.Sprite(visTag(new THREE.SpriteMaterial({
    color: 0xFFF6D8, transparent: true, opacity: 0, depthWrite: false,
    blending: THREE.AdditiveBlending }), { nvg: 1.0, th: [1.0, 1.0, 1.0] }));
  ILLUM.spr.visible = false;
  scene.add(ILLUM.spr);
}
function igniteIllum(x, z) {
  ILLUM.t0 = sim.now; ILLUM.x = x; ILLUM.z = z;
  ILLUM.y = Math.max(H(x, z), 0) + 330;                 // burst height above the point
}
function updateIllum(dt) {
  const t = sim.now - ILLUM.t0;
  const dead = t < 0 || t > 55 || ILLUM.y < Math.max(H(ILLUM.x, ILLUM.z), 0) + 35;
  if (dead) {
    if (ILLUM.light.intensity) { ILLUM.light.intensity = 0; ILLUM.spr.visible = false; }
    return;
  }
  ILLUM.y -= 6.5 * dt;                                  // the flare rides its chute down
  ILLUM.x += 1.6 * dt;                                  // a hint of drift
  ILLUM.light.position.set(ILLUM.x, ILLUM.y, ILLUM.z);
  ILLUM.light.intensity = 240000 * (0.9 + 0.1 * Math.sin(sim.now * 13));
  ILLUM.spr.visible = true;
  ILLUM.spr.position.copy(ILLUM.light.position);
  ILLUM.spr.scale.setScalar(11 + Math.sin(sim.now * 9) * 1.5);
  ILLUM.spr.material.opacity = 0.95;
}

/* --- boot ------------------------------------------------------------------ */
{
  VISION.ctx = VISION.cv.getContext('2d');
  VISION.cv.width = CONFIG.OPTICS.cvW;
  VISION.cv.height = CONFIG.OPTICS.cvH;
  visBuildGrain();
  visBuildHalo();
  visCollectSources();
  initCraters();              // 13h — before applyVision so _m0 is established
  initWisps();
  initScreens();              // 12h
  initIllum();
  visionReady = true;
  applyVision();              // 'day' — establishes _m0 on everything already built
}

/* Assign a day material to an object without losing the current vision mode, and
   recolour a day material so its live variant follows. Anything that writes
   `.material =` or `.material.color` on a scene object MUST go through these two
   or it will either be overwritten on the next toggle or permanently corrupt a
   cached variant. */
function visSetMat(obj, dayMat) {
  obj.userData._m0 = dayMat;
  obj.material = VISION.mode === 'day' ? dayMat
    : visTintVariant(dayMat, VISION.mode,
        visDegrade(TOD_LIGHT(), CONFIG.OPTICS.nvgBloomAt),
        visDegrade(TOD_LIGHT(), CONFIG.OPTICS.thermalWashAt));
}
function visSetColor(obj, hex) {
  const m0 = obj.userData._m0 || obj.material;
  m0.color.setHex(hex);
  if (VISION.mode !== 'day')
    obj.material = visTintVariant(m0, VISION.mode,
      visDegrade(TOD_LIGHT(), CONFIG.OPTICS.nvgBloomAt),
      visDegrade(TOD_LIGHT(), CONFIG.OPTICS.thermalWashAt));
}
function spawnBurst(x, y, z) {
  const seen = hasLOS(eye.x, eye.y, eye.z, x, y + 2, z);
  let b = bursts.find(b => !b.active) || bursts[0];
  b.active = true; b.t0 = sim.now; b.seen = seen;
  b.group.visible = true;
  b.group.position.set(x, y, z);
  const water = H(x, z) < 0;
  b.water = water;   // 13h — the water case reshapes the column in updateBursts
  if (!water) { addCrater(x, z); markWisp(x, y, z); }   // 13h — the sea keeps no scars
  visSetColor(b.dust, water ? 0xDCE9E6 : 0xB9A98C);
  for (const p of b.puffs) visSetColor(p, water ? 0xCFDEDD : 0x8A877E);
  b.debris.forEach(d => {
    const a = Math.random() * Math.PI * 2, sp = 8 + Math.random() * 22;
    d.vx = Math.sin(a) * sp; d.vz = Math.cos(a) * sp; d.vy = 14 + Math.random() * 18;
    d.m.position.set(0, 1, 0); d.m.visible = seen;
  });
  b.flash.material.opacity = seen ? 1 : 0;
  b.ring.material.opacity = seen ? 0.7 : 0;
  boom(dist2(eye.x, eye.z, x, z) / 343, dist2(eye.x, eye.z, x, z));
}
function updateBursts(dt) {
  for (const b of bursts) {
    if (!b.active) continue;
    const t = sim.now - b.t0;
    if (t > 14) { b.active = false; b.group.visible = false; continue; }
    // flash
    if (b.seen) {
      const ft = clamp(1 - t / 0.18, 0, 1);
      b.flash.material.opacity = ft;
      b.flash.scale.setScalar(14 + 60 * (1 - ft));
      // ring
      const rt = clamp(t / 1.3, 0, 1);
      b.ring.scale.setScalar(4 + 66 * rt);
      b.ring.material.opacity = 0.7 * (1 - rt);
    }
    // dust hemisphere — 13h: a sea splash is a tall, narrow white column, so a
    // round off the coast is unmistakably a wet miss and not a puff on the beach
    const dtn = clamp(t / 3, 0, 1);
    const ds = 6 + 36 * dtn;
    if (b.water) b.dust.scale.set(ds * 0.38, ds * 1.9, ds * 0.38);
    else b.dust.scale.setScalar(ds);
    b.dust.material.opacity = 0.85 * (1 - dtn);
    // debris (ballistic, first 1.8 s)
    if (t < 1.8 && b.seen) {
      for (const d of b.debris) {
        d.m.position.x += d.vx * dt; d.m.position.z += d.vz * dt;
        d.vy -= 30 * dt; d.m.position.y += d.vy * dt;
        if (d.m.position.y < 0.3) d.m.visible = false;
      }
    } else b.debris.forEach(d => d.m.visible = false);
    // smoke column
    for (let i = 0; i < b.puffs.length; i++) {
      const p = b.puffs[i];
      const pt = clamp((t - i * 0.55) / 11, 0, 1);
      if (pt <= 0) { p.visible = false; continue; }
      p.visible = true;
      p.position.set(4 * Math.sin(i * 2.1) + pt * 26, 2 + pt * 105, 4 * Math.cos(i * 1.7) + pt * 9);
      p.scale.setScalar(5 + pt * 24);
      p.material.opacity = 0.66 * (1 - pt);
    }
  }
  // 13h — marker wisps: thin, slow, ~75 s. A marker, not a smoke screen — the
  // opacity ceiling keeps the target legible through its own markers.
  for (const w of WISPS) {
    const a = (sim.now - w.t0) / 75;
    if (a < 0 || a >= 1) { w.m.visible = false; continue; }
    w.m.visible = true;
    w.m.position.set(w.x, w.y + 8 + a * 26, w.z);
    w.m.scale.set(2.5 + a * 5, 8 + a * 14, 1);
    w.m.material.opacity = 0.32 * (1 - a) * (VISION.mode === 'day' ? 1 : 0.5);
  }
  updateScreens();   // 12h
  updateIllum(dt);
}

