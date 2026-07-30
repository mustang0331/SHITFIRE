/* ============================================================ SCENE */
const canvas = document.getElementById('gl');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 1.75));
renderer.setSize(innerWidth, innerHeight);
// 13b. r160 already defaults ColorManagement.enabled = true and outputColorSpace =
// SRGBColorSpace, and every colour in this file is authored as sRGB hex, so the existing
// palette is already being converted correctly. Do not "fix" the colour space — the palette
// was tuned under these defaults and changing it would shift every colour, map sheets included.
if (CONFIG.GFX.toneMap) {
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = CONFIG.GFX.exposure;
}

const scene = new THREE.Scene();
scene.fog = new THREE.Fog(0xE3EADB, 4500, 15000);   // colour driven by TOD, see applyTOD()

const camera = new THREE.PerspectiveCamera(CONFIG.CAMERA.fov, innerWidth / innerHeight, 0.5, 22000);

const sun = new THREE.DirectionalLight(0xfff4e0, 1.25);
scene.add(sun);
const hemi = new THREE.HemisphereLight(0xbfe0ea, 0x5c6b4e, 0.55);
scene.add(hemi);

/* ---- 13c: time of day -----------------------------------------------------
   One TOD value is the lighting authority: sun direction, sun colour and
   intensity, hemisphere sky colour, fog colour, and the Preetham sky
   parameters all derive from it. Nothing else should set those directly.

   'day' deliberately reproduces the pre-13c sun EXACTLY (elev 64.5°, azi 324°,
   the normalized form of the old hard-coded vector) so that landing this row
   does not move the light out from under an already-approved render. GRAPHICS.md
   proposes elev 55 / azi 145 for day, which is a more physically coherent arc
   with dawn/dusk but visibly relights the island — switch `day` to those values
   if that trade is wanted. Keep chapter assignments away from elev < 5: a very
   low sun throws long shadows toward the observer and can hide targets.

   `light` (0..1) is the AMBIENT LEVEL this TOD represents. It is not a lighting
   term — applyTOD does not read it for the sun or the hemisphere. It exists so
   the observer's optics know how much light they are being asked to amplify:
   an image intensifier blooms out in daylight and a thermal sight loses its
   hot/cold separation once the sun has heated the ground. See VISION.
   `hemiGnd` / `hemiInt` are optional overrides for the hemisphere bounce; they
   default to the daytime values so no existing TOD changes.

   'night' is moonlight, and it is deliberately DARK — the first pass ran it at
   inten 0.16 / hemi 0.22 and a harness measured the naked eye doing BETTER at
   night than the image intensifier, which made the whole device pointless. At
   0.09 / 0.13 the jungle renders around sRGB 0.21 and a man on it around 0.06:
   you can tell something is there and you cannot work with it, which is the
   entire argument for [O]. Sun elevation is held at 8 deliberately: below ~5 the
   Lambert terms on the near terrain go flat and target figures stop separating
   from the ground they stand on, which is the long-shadow trap this comment
   already warns about. Preetham cannot draw a night sky — it has no stars and
   no night term — so night is faked by collapsing rayleigh and turbidity until
   the scattering integral bottoms out, which lands a very dark blue-grey dome
   that reads as an overcast moonlit night rather than a starfield. That is the
   honest limit of a one-draw-call sky with no new dependency. */
const TOD_TABLE = {
  day:      { elev: 64.5, azi: 324, sun: 0xFFF4E0, inten: 1.25, hemiSky: 0xBFE0EA,
              fog: 0xE3EADB, turbidity: 2.2, rayleigh: 1.0,  mieC: 0.005, mieG: 0.80,
              light: 1.00 },
  dawn:     { elev: 6,    azi: 100, sun: 0xFFC08A, inten: 1.05, hemiSky: 0xE8C9A8,
              fog: 0xE6D6C4, turbidity: 4.0, rayleigh: 2.6,  mieC: 0.008, mieG: 0.82,
              light: 0.45 },
  dusk:     { elev: 9,    azi: 250, sun: 0xFF9A5A, inten: 0.95, hemiSky: 0xD8A9A0,
              fog: 0xE0C6BA, turbidity: 5.0, rayleigh: 2.2,  mieC: 0.007, mieG: 0.82,
              light: 0.40 },
  overcast: { elev: 40,   azi: 145, sun: 0xE8EAEC, inten: 0.55, hemiSky: 0xC9D2D6,
              fog: 0xD5DBDD, turbidity: 10,  rayleigh: 0.6,  mieC: 0.020, mieG: 0.75,
              light: 0.75 },
  night:    { elev: 8,    azi: 205, sun: 0x93A9CE, inten: 0.09, hemiSky: 0x161D2B,
              fog: 0x0A0F18, turbidity: 0.7, rayleigh: 0.10, mieC: 0.0015, mieG: 0.86,
              hemiGnd: 0x0C110D, hemiInt: 0.13, light: 0.05 },
};
const TOD_LIGHT = () => (TOD_TABLE[TOD] && TOD_TABLE[TOD].light !== undefined)
  ? TOD_TABLE[TOD].light : 1;
let TOD = 'day';
let skyMesh = null;
const _sunDir = new THREE.Vector3();

if (CONFIG.GFX.sky) {
  skyMesh = new Sky();
  // Sky is a unit BackSide box with depthWrite off. Scale it to sit inside the
  // 22000 far plane with the observer camera (always within ±5000 of origin)
  // enclosed: half-extent 9000 -> worst-case corner ~15600 + 5000 offset = ~20600,
  // still short of far. NOTE the AAR shot-plot camera pulls back by roughly
  // span * 1.15 and can reach ~8500 out on a very wide shot group; if a plot ever
  // shows a black wedge instead of sky, that camera has left the box — raise this
  // scale and the far plane together, don't shrink the plot framing.
  skyMesh.scale.setScalar(18000);
  scene.add(skyMesh);
}

function todDir(t, out) {
  const el = t.elev * Math.PI / 180, az = t.azi * Math.PI / 180;
  return out.set(Math.cos(el) * Math.sin(az), Math.sin(el), Math.cos(el) * Math.cos(az));
}

function applyTOD(name) {
  TOD = TOD_TABLE[name] ? name : 'day';
  const t = TOD_TABLE[TOD];
  todDir(t, _sunDir);
  sun.position.copy(_sunDir).multiplyScalar(1000);
  sun.color.setHex(t.sun);
  sun.intensity = t.inten;
  hemi.color.setHex(t.hemiSky);
  hemi.groundColor.setHex(t.hemiGnd !== undefined ? t.hemiGnd : 0x5C6B4E);
  hemi.intensity = t.hemiInt !== undefined ? t.hemiInt : 0.55;
  scene.fog.color.setHex(t.fog);
  if (skyMesh) {
    // Sky.js includes <tonemapping_fragment>, so the sky goes through ACES exactly
    // as the fog does — the two stay consistent at the horizon with no CPU pre-tone.
    // The mesh encloses the camera, so scene.background is never visible; leave it null.
    const u = skyMesh.material.uniforms;
    u.turbidity.value = t.turbidity;
    u.rayleigh.value = t.rayleigh;
    u.mieCoefficient.value = t.mieC;
    u.mieDirectionalG.value = t.mieG;
    u.sunPosition.value.copy(_sunDir);   // expects a direction, not a world position
    scene.background = null;
  } else {
    // No sky mesh: fall back to 13b's flat background, pre-toned on the CPU so it
    // still agrees with the tone-mapped fog instead of opening a horizon seam.
    scene.background = CONFIG.GFX.toneMap
      ? acesFilmic(t.hemiSky, CONFIG.GFX.exposure)
      : new THREE.Color(t.hemiSky);
  }
  // 13d bakes sun direction into terrain vertex colours, so a TOD change must
  // re-bake. terrainReady is a hoisted `var`: the boot-time applyTOD('day') runs
  // before the terrain block's `let` bindings exist, and a rebuild attempt there
  // would throw on the TDZ — undefined is falsy, so boot skips it and the first
  // real build owns itself.
  if (CONFIG.GFX.hillshade && terrainReady) buildTerrain();
  // The optics sit DOWNSTREAM of the lighting authority: applyTOD always writes
  // the base sun/hemi/fog first, then applyVision multiplies its gain on top and
  // re-derives the material variants and the overlay for the new ambient level.
  // `var` (not let/const) so this hook is safe to read during the boot-time
  // applyTOD('day') that runs long before the VISION block exists.
  if (visionReady) applyVision();
}
var visionReady = false;
applyTOD('day');

// Ocean
/* 13g — the water uniforms, preallocated ONCE (the row's gate: one uTime
   uniform, no per-frame allocation). The render loop writes three scalars a
   frame; the GLSL below reads them. Optics variants are material CLONES and
   clone() does not carry onBeforeCompile, so NVG/thermal see plain water —
   which is also physically the right answer (no glint through a tube; the
   E3 shoreline readability comes from the shallows band, not the foam). */
const WATER_U = {
  uTime: { value: 0 },
  uSun: { value: new THREE.Vector3(0, 1, 0) },
  uSunI: { value: 1 },
};
// tiny value-noise shared by both patches; scrolled by uTime for movement
const GLSL_NOISE = `
  uniform float uTime;
  float h13(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
  float vnz(vec2 p){
    vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h13(i), h13(i + vec2(1, 0)), f.x),
               mix(h13(i + vec2(0, 1)), h13(i + vec2(1, 1)), f.x), f.y);
  }`;
// vertex-side: world position, computed from `transformed` so it is correct
// for every mesh the material lands on (base sheet and 13e patch alike)
function waterVertexPatch(sh) {
  sh.vertexShader = 'varying vec3 vWpos13;\n' + sh.vertexShader.replace(
    '#include <begin_vertex>',
    '#include <begin_vertex>\n  vWpos13 = (modelMatrix * vec4(transformed, 1.0)).xyz;');
}
function terrainFoamPatch(sh) {
  sh.uniforms.uTime = WATER_U.uTime;
  waterVertexPatch(sh);
  sh.fragmentShader = ('varying vec3 vWpos13;\n' + GLSL_NOISE + '\n' + sh.fragmentShader)
    .replace('#include <opaque_fragment>', `
  {
    // 13g — surf: a breathing white band where the ground meets the sea
    float nz = vnz(vWpos13.xz * 0.11 + vec2(uTime * 0.35, uTime * 0.22));
    float band = smoothstep(1.5, 0.55, vWpos13.y) * smoothstep(-0.8, -0.1, vWpos13.y);
    outgoingLight = mix(outgoingLight, vec3(0.93), band * smoothstep(0.32, 0.78, nz) * 0.85);
  }
  #include <opaque_fragment>`);
}
function oceanGlintPatch(sh) {
  sh.uniforms.uTime = WATER_U.uTime;
  sh.uniforms.uSun = WATER_U.uSun;
  sh.uniforms.uSunI = WATER_U.uSunI;
  waterVertexPatch(sh);
  sh.fragmentShader = ('varying vec3 vWpos13;\nuniform vec3 uSun;\nuniform float uSunI;\n' +
    GLSL_NOISE + '\n' + sh.fragmentShader)
    .replace('#include <opaque_fragment>', `
  {
    // 13g — sun glint: a specular lobe off a noise-perturbed water normal,
    // gated by sun elevation so it dies with the light instead of glowing
    // at night. Fragment-only: the ocean plane has no subdivisions to move.
    vec3 vd = normalize(cameraPosition - vWpos13);
    vec2 gp = vWpos13.xz * 0.045 + vec2(uTime * 0.55, -uTime * 0.42);
    vec3 wn = normalize(vec3(vnz(gp) - 0.5, 5.5, vnz(gp * 1.7 + 31.0) - 0.5));
    float g = pow(max(dot(reflect(-uSun, wn), vd), 0.0), 90.0);
    outgoingLight += vec3(1.0, 0.95, 0.8) * g * 1.3 * uSunI * max(uSun.y, 0.0);
  }
  #include <opaque_fragment>`);
}

let oceanShallows = null;
{
  // water absorbs near-IR (almost black under a tube) and has a high heat
  // capacity (slightly WARMER than the land it borders at night, so the
  // shoreline stays readable in thermal). Both are physical, and both happen to
  // keep the coast usable as a terrain-association feature after dark.
  const oceanMat = visTag(new THREE.MeshLambertMaterial({ color: gfxPal(0x1E5A7A) }),
                          { nvg: 0.06, th: [0.22, 0.22, 0.22] });
  if (CONFIG.GFX.water) {
    oceanMat.onBeforeCompile = oceanGlintPatch;
    oceanMat.customProgramCacheKey = () => 'ocean13g';
  }
  const ocean = new THREE.Mesh(
    new THREE.PlaneGeometry(44000, 44000).rotateX(-Math.PI / 2), oceanMat);
  ocean.position.y = 0;
  scene.add(ocean);
  oceanShallows = new THREE.Mesh(
    new THREE.CircleGeometry(CONFIG.TERRAIN.islandRadius + 900, 48).rotateX(-Math.PI / 2),
    visTag(new THREE.MeshLambertMaterial({ color: gfxPal(0x2E7DA0) }),
           { nvg: 0.09, th: [0.25, 0.25, 0.25] }));
  oceanShallows.position.y = 0.15;
  scene.add(oceanShallows);
}

/* Terrain mesh — a low-poly base sheet plus, since 13e, ONE high-res patch
   over the mission target area. 33 m facets cannot show the micro-relief a
   burst gets judged against (H's finest octave is ~40 m — the base mesh sits
   at its aliasing edge), so the patch re-samples the SAME H() at facet/PATCH_DIV
   resolution. The base mesh gets a hole cut on its own grid lines and the
   patch's boundary vertices are placed ON those lines with heights lerped
   between the base corners' H — the two meshes share the boundary polyline
   exactly, so there is no crack, no overlap, and nothing to z-fight.
   groundHit/hasLOS/impacts still read the analytic H, untouched. */
let TERRAIN_PALETTE = null;   // null = tropical; 'black' = Volume IV black sand
let patchCx = null, patchCz = null;   // 13e — current patch center (world), null = no patch
function setTerrainFocus(x, z) {
  // called at mission start with the target area; rebuild only on a real move
  if (patchCx !== null && Math.hypot(x - patchCx, z - patchCz) < 250) return;
  patchCx = x; patchCz = z;
  if (terrainReady) buildTerrain();
}
function buildTerrain() {
  const seg = CONFIG.TERRAIN.meshSegments, size = CONFIG.MAP.size;
  const cell = size / seg;
  const geo = new THREE.PlaneGeometry(size, size, seg, seg).rotateX(-Math.PI / 2);
  const pos = geo.attributes.position;
  const colors = new Float32Array(pos.count * 3);
  const blk = TERRAIN_PALETTE === 'black';
  // 13b: palette runs through gfxPal so ACES's midtone desaturation is pre-compensated.
  const cSandLo = gfxPal(blk ? 0x3A3733 : 0xC7B383), cSand = gfxPal(blk ? 0x4A4640 : 0xD8C89A);
  const cJunA = gfxPal(blk ? 0x3E5A34 : 0x4E7A3D), cJunB = gfxPal(blk ? 0x2E4526 : 0x3B5E2E);
  const cRokA = gfxPal(blk ? 0x403C38 : 0x5A544E), cRokB = gfxPal(blk ? 0x524D46 : 0x6E665C);
  /* 13d — baked hillshade + ambient occlusion (GRAPHICS.md G2, the plan's
     highest-value row). Lambert on flat-shaded 33 m facets gives each facet ONE
     shade, so ridgelines and draws vanish at range — exactly the relief the
     printed contours promise. Baking a per-vertex sun term + a concavity term
     into the vertex colours costs zero per frame and makes the 3D island agree
     with its own map sheet.

       shade  one-sided gradient from the two slope samples already taken (no
              extra H calls), dotted with the CURRENT TOD sun via todDir —
              which is why applyTOD re-bakes on a TOD change.
       AO     concavity against a 48 m ring (4 extra H calls): ground sitting
              below its surroundings is a draw and darkens by up to aoStrength.

     hillFloor clamps the COMBINED multiplier: the Volume IV black-sand palette
     starts dark, and shade*occlusion could push it below legibility — the gate
     on this row says the palette must not crush, so the floor is config, not
     hope. Water and beach are left unshaded (flat anyway, and the shallows
     band is doing its own work). Build cost: 7 H() per vertex vs 3 before. */
  const doShade = CONFIG.GFX.hillshade;
  let sunX = 0, sunY = 1, sunZ = 0;
  if (doShade) {
    const sd = todDir(TOD_TABLE[TOD] || TOD_TABLE.day, _bakeSun);
    sunX = sd.x; sunY = sd.y; sunZ = sd.z;
  }
  const aoK = CONFIG.GFX.aoStrength, mFloor = CONFIG.GFX.hillFloor;
  const tmp = new THREE.Color();
  // one vertex's colour + baked shade, shared by the base sheet and the 13e
  // patch so the two can never disagree about what the ground looks like
  const vcol = (x, z, h) => {
    const hx = H(x + 9, z), hz = H(x, z + 9);
    const sl = Math.hypot(hx - h, hz - h) / 9;
    const patch = vnoise(x * 0.012, z * 0.012, 999);
    if (h < 1.2)      tmp.copy(cSandLo);
    else if (h < 5.5) tmp.copy(cSand);
    else if (sl > 0.62 || h > 120) tmp.lerpColors(cRokA, cRokB, patch);
    else              tmp.lerpColors(cJunA, cJunB, patch);
    if (doShade && h > 1.2) {
      const gx = (hx - h) / 9, gz = (hz - h) / 9;
      const inv = 1 / Math.sqrt(gx * gx + gz * gz + 1);
      const dot = (-gx * sunX + sunY - gz * sunZ) * inv;
      let m = 0.74 + 0.40 * Math.max(0, dot);            // sun term, floored
      const ring = (H(x + 48, z) + H(x - 48, z) + H(x, z - 48) + H(x, z + 48)) * 0.25;
      m *= 1 - clamp((ring - h) / 22, 0, 1) * aoK;       // concavity term
      m = Math.max(mFloor, m);
      tmp.r *= m; tmp.g *= m; tmp.b *= m;
    }
    return tmp;
  };
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i), z = pos.getZ(i);
    const h = H(x, z);
    pos.setY(i, h);
    const c0 = vcol(x, z, h);
    colors[i * 3] = c0.r; colors[i * 3 + 1] = c0.g; colors[i * 3 + 2] = c0.b;
  }
  geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));

  /* 13e — the patch rect, snapped OUTWARD to the base grid so the hole edge
     and the patch edge are the same polyline of base grid lines. */
  const half = size / 2, T13 = CONFIG.TERRAIN;
  let i0 = 0, i1 = 0, j0 = 0, j1 = 0;
  if (patchCx !== null) {
    const ph = T13.patchSize / 2;
    i0 = clamp(Math.floor((patchCx + half - ph) / cell), 0, seg);
    i1 = clamp(Math.ceil((patchCx + half + ph) / cell), 0, seg);
    j0 = clamp(Math.floor((patchCz + half - ph) / cell), 0, seg);
    j1 = clamp(Math.ceil((patchCz + half + ph) / cell), 0, seg);
  }
  const hasPatch = i1 > i0 && j1 > j0;
  if (hasPatch) {
    // cut the hole: PlaneGeometry lays its index out 6 entries per cell in
    // row-major order, so cell (ix, iy) owns index slots [(iy*seg+ix)*6 .. +6)
    const oldIdx = geo.getIndex().array;
    const keep = new Uint32Array(oldIdx.length);
    let k = 0;
    for (let iy = 0; iy < seg; iy++)
      for (let ix = 0; ix < seg; ix++) {
        if (ix >= i0 && ix < i1 && iy >= j0 && iy < j1) continue;   // inside the hole
        const s0 = (iy * seg + ix) * 6;
        for (let s = 0; s < 6; s++) keep[k++] = oldIdx[s0 + s];
      }
    geo.setIndex(new THREE.BufferAttribute(keep.subarray(0, k).slice(), 1));
  }
  geo.computeVertexNormals();

  // 13e — the patch itself: PATCH_DIV sub-cells per base cell, boundary
  // vertices ON the base grid lines with heights lerped between the base
  // corners' H — crack-free by construction, no z-fight because the two
  // surfaces only share the boundary polyline.
  let pGeo = null;
  if (hasPatch) {
    const div = T13.patchDiv;
    const nx = (i1 - i0) * div, nz = (j1 - j0) * div;
    const pPos = new Float32Array((nx + 1) * (nz + 1) * 3);
    const pCol = new Float32Array((nx + 1) * (nz + 1) * 3);
    // coordinates are computed in the SAME float expression the base mesh uses
    // (index * cell - half), so a patch-boundary vertex and its base grid line
    // agree bit-for-bit — a*step accumulation would drift by ulps and open a
    // hairline gap along the seam
    for (let b = 0; b <= nz; b++)
      for (let a = 0; a <= nx; a++) {
        const x = (i0 + a / div) * cell - half, z = (j0 + b / div) * cell - half;
        let h;
        if (a === 0 || a === nx || b === 0 || b === nz) {
          // boundary: sit exactly on the base mesh's edge segments
          if (a === 0 || a === nx) {
            const zA = (j0 + Math.floor(b / div)) * cell - half, t = (b % div) / div;
            h = t === 0 ? H(x, zA) : H(x, zA) * (1 - t) + H(x, zA + cell) * t;
          } else {
            const xA = (i0 + Math.floor(a / div)) * cell - half, t = (a % div) / div;
            h = t === 0 ? H(xA, z) : H(xA, z) * (1 - t) + H(xA + cell, z) * t;
          }
        } else h = H(x, z);
        const vi = (b * (nx + 1) + a) * 3;
        pPos[vi] = x; pPos[vi + 1] = h; pPos[vi + 2] = z;
        const c1 = vcol(x, z, h);
        pCol[vi] = c1.r; pCol[vi + 1] = c1.g; pCol[vi + 2] = c1.b;
      }
    const pIdx = new Uint32Array(nx * nz * 6);
    let q = 0;
    for (let b = 0; b < nz; b++)
      for (let a = 0; a < nx; a++) {
        const v = b * (nx + 1) + a;
        pIdx[q++] = v; pIdx[q++] = v + nx + 1; pIdx[q++] = v + 1;
        pIdx[q++] = v + 1; pIdx[q++] = v + nx + 1; pIdx[q++] = v + nx + 2;
      }
    pGeo = new THREE.BufferGeometry();
    pGeo.setAttribute('position', new THREE.BufferAttribute(pPos, 3));
    pGeo.setAttribute('color', new THREE.BufferAttribute(pCol, 3));
    pGeo.setIndex(new THREE.BufferAttribute(pIdx, 1));
    pGeo.computeVertexNormals();
  }

  if (terrainMesh) {
    scene.remove(terrainMesh);
    terrainMesh.geometry.dispose();
    terrainMesh.material.dispose();
  }
  if (terrainPatchMesh) {
    scene.remove(terrainPatchMesh);
    terrainPatchMesh.geometry.dispose();   // material is shared with the base sheet
    terrainPatchMesh = null;
  }
  // The terrain is the one material in the scene whose colour lives in a vertex
  // attribute, not in material.color — so the generic optics derivation would read
  // white and blow the ground out. Tag it explicitly. (visTag is hoisted; the tag
  // is inert until VISION exists.) A rebuild makes a NEW material, so the optics
  // have to be re-derived or a mid-mission island swap leaves the ground in day
  // colours under a night device. The 13e patch SHARES this material — one
  // dispose, one optics derivation, and the two sheets can never diverge.
  const tMat = visTag(
    new THREE.MeshLambertMaterial({ vertexColors: true, flatShading: true }),
    { nvg: 0.55, th: [0.14, 0.14, 0.14] });   // vegetation is NIR-bright
  if (CONFIG.GFX.water) {
    // 13g — the surf band rides the terrain material, so it lands on the base
    // sheet and the 13e patch alike (they share this material)
    tMat.onBeforeCompile = terrainFoamPatch;
    tMat.customProgramCacheKey = () => 'terra13g';
  }
  terrainMesh = new THREE.Mesh(geo, tMat);
  scene.add(terrainMesh);
  if (pGeo) {
    terrainPatchMesh = new THREE.Mesh(pGeo, tMat);
    scene.add(terrainPatchMesh);
  }
  if (visionReady) applyVision();
}
let terrainPatchMesh = null;   // 13e — high-res patch over the target area
let terrainMesh = null;
const _bakeSun = new THREE.Vector3();   // 13d — todDir scratch for the bake
var terrainReady = false;               // 13d — var: hoisted, boot-safe (see applyTOD)
buildTerrain();
terrainReady = true;

/* ============================================================ DEM INGESTION */
// Drop any grayscale heightmap image (real-island DEM export, e.g. Iwo Jima)
// onto the window, or load it from the mission menu. Luminance -> elevation,
// border faded into ocean so every DEM reads as an island ringed by water.
function rebuildWorld() {
  buildTerrain();
  buildMinimap();
  placeOP();
  placeBattery();
  buildWorldFeatures();
  oceanShallows.visible = !DEM;
  /* G21 — recorded targets do not survive an island change. RECTGT holds WORLD
     COORDINATES; on a new heightfield those coordinates are an arbitrary point
     that could be open water, a friendly position, or a village. "SUPPRESS
     TARGET AB7101" firing at where a target USED to be on a different island is
     the audit finding this row existed to catch. Same-island missions keep
     their targets — that is what "on file" means. */
  wipeRecordedTargets();
  newMission(false);
}
function wipeRecordedTargets() {
  let n = 0;
  for (const k in RECTGT) { delete RECTGT[k]; n++; }
  if (n) log('', `Recorded target list wiped (${n}) — target numbers do not transfer between islands.`, 'sys');
}
function loadDEMFile(file) {
  const url = URL.createObjectURL(file);
  const img = new Image();
  img.onload = () => {
    URL.revokeObjectURL(url);
    const n = CONFIG.TERRAIN.demRes;
    const cv = document.createElement('canvas');
    cv.width = n; cv.height = n;
    const c2 = cv.getContext('2d');
    c2.drawImage(img, 0, 0, n, n);
    const px = c2.getImageData(0, 0, n, n).data;
    const data = new Float32Array(n * n);
    const T = CONFIG.TERRAIN, sea = T.demSeaLum;
    for (let i = 0; i < n * n; i++) {
      const lum = 0.2126 * px[i * 4] + 0.7152 * px[i * 4 + 1] + 0.0722 * px[i * 4 + 2];
      data[i] = lum <= sea
        ? lerp(T.oceanFloor, 0, lum / sea)
        : (lum - sea) / (255 - sea) * T.demMaxElev;
    }
    for (let j = 0; j < n; j++)
      for (let i = 0; i < n; i++) {
        const dEdge = Math.min(i, j, n - 1 - i, n - 1 - j) / n * 10000;
        const m = smoothstep(0, 350, dEdge);
        data[j * n + i] = data[j * n + i] * m + T.oceanFloor * (1 - m);
      }
    DEM = { data, n, name: file.name.replace(/\.[^.]+$/, '') };
    rebuildWorld();
    log('', `DEM loaded: ${file.name} (${img.width}×${img.height} → ${n}×${n}, max elev ${T.demMaxElev} m). World rebuilt — new mission on the new island.`, 'sys');
  };
  img.onerror = () => {
    URL.revokeObjectURL(url);
    log('', 'Could not read that file as an image heightmap (use a grayscale PNG/JPG).', 'sys');
  };
  img.src = url;
}
window.addEventListener('dragover', e => e.preventDefault());
window.addEventListener('drop', e => {
  e.preventDefault();
  if (e.dataTransfer.files && e.dataTransfer.files[0]) loadDEMFile(e.dataTransfer.files[0]);
});

