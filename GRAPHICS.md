# GRAPHICS.md — Visual Upgrade Plan for SHITFIRE

> **This is the detail spec for SPEC.md stage 13.** [ROADMAP.md](ROADMAP.md) Track A owns the order
> and status; the G-numbers here map to ROADMAP row IDs as `13a = G0.4`, `13b = G0 rest`,
> `13c = G1` … `13i = G7`. **G8 (bloom) is reassigned to stage 11** as row 11d — it is SUNLAMP-only,
> so it ships with the Epilogue, not here. Work one ROADMAP row per commit.

Reference/strategy doc. **No code in `index.html` has been changed by this plan yet** — it was written
while another agent held the file. Every item below is designed as an *additive, independently
landable block* so it can be merged against a moving `index.html` with minimal conflict.

Governing constraint (from CLAUDE.md, unchanged): one file, no build step, no npm, CDN import map
only, 60 fps target, preallocate, no per-frame allocation. **Nothing here adds a dependency** — the
only new imports are `three/addons/`, which the existing import map already resolves
([index.html:156-160](index.html#L156-L160)).

Second governing constraint, specific to this app: **this is a training instrument, not a demo.**
Any graphics change that reduces the observer's ability to (a) resolve a target at 1500–3200 m
through 9° binos, (b) tell a military element from a civilian one, or (c) see exactly where a round
landed relative to the aimpoint, is a regression *even if it looks better*. Several items below are
justified primarily on training value, not looks.

---

## 0. Baseline audit

| Area | Today | Anchor |
|---|---|---|
| Renderer | `antialias: true`, pixelRatio ≤1.75, no tone mapping, no shadows | [index.html:450-452](index.html#L450-L452) |
| Sky | flat `Color(0xBFE0EA)` background | [index.html:455](index.html#L455) |
| Fog | linear `Fog(0xE3EADB, 4500, 15000)` | [index.html:456](index.html#L456) |
| Light | 1 directional + 1 hemisphere, fixed | [index.html:460-463](index.html#L460-L463) |
| Terrain | 300 seg / 10 000 m = **33 m per quad**, vertex colors by height+slope, flat shaded | [index.html:482-514](index.html#L482-L514) |
| Ocean | two flat Lambert planes | [index.html:466-478](index.html#L466-L478) |
| Vegetation | **none** — jungle is a green vertex color | — |
| World features | per-object `Mesh` + `BoxGeometry`, shared materials | [index.html:685-727](index.html#L685-L727) |
| Impact FX | pool of 10: sprite flash, ring, dust hemi, 5 puffs, 6 debris, 14 s life | [index.html:1730-1822](index.html#L1730-L1822) |
| Quality | pixelRatio EMA step-down, 4 tiers | [index.html:3836-3841](index.html#L3836-L3841) |

**Three defects worth naming before any beautification:**

1. **A 33 m terrain facet is larger than the 60 m effect radius.** Rounds land on flat plates, so
   micro-relief gives the observer nothing to judge deviation against. This is the single biggest
   *training* deficiency in the renderer. → §G3.
2. **Adaptive quality can drop pixel ratio to 0.85 while binos are up.** A troop box (2 m) at 3000 m
   subtends ~0.67 mrad; at 9° FOV that's ~4 px tall at full res and ~2 px at tier 3. The quality
   system can silently make targets unresolvable. → §G0.4, a correctness fix, not a graphics one.
3. **No relief shading.** With a single directional light on flat-shaded 33 m facets, ridgelines and
   draws read poorly at range — exactly the terrain-association skill the map sheets are meant to
   train against. → §G2.

---

## 1. Ordering — do these in this order

Ranked by (visual gain + training gain) ÷ risk. **G0–G2 are the plan**; everything after is optional
polish that can be skipped without leaving the codebase in a half-state.

| Stage | What | LOC | Frame cost | Risk |
|---|---|---|---|---|
| **G0** | Config gate, tone mapping, fog tie-in, bino quality pin | ~35 | 0 | low |
| **G1** | Real sky + time-of-day sun | ~60 | ~0 (1 draw call) | low |
| **G2** | **Baked hillshade + AO in terrain vertex colors** | ~55 | **0** (build-time) | low |
| **G3** | Near-field terrain LOD patch | ~40 | +125k tris | med |
| **G4** | Instanced vegetation + scatter | ~120 | 2 draw calls | med |
| **G5** | Shoreline foam + ocean sun glint | ~70 | ~0 | med |
| **G6** | Persistent craters + lingering smoke | ~80 | 1 draw call | low |
| **G7** | Optics presentation (2D canvas only) | ~50 | 0 (no GL) | low |
| **G8** | Optional bloom, Epilogue/SUNLAMP only | ~45 | high, gated | med |

---

## G0 — Config gate and no-regret renderer settings

Everything in this document hangs off one new `CONFIG` block, so the whole plan can be A/B'd or
reverted with one flag. Insert into `CONFIG` at [index.html:302](index.html#L302), after `WORLD`:

```js
  GFX: {
    toneMap: true, exposure: 1.15,
    sky: true,                       // G1
    hillshade: true, aoStrength: 0.35, // G2
    nearLOD: true, nearLODSize: 2400, nearLODSeg: 300, // G3
    veg: true, vegBudget: 4000, vegRadius: 2600,       // G4
    water: true,                     // G5
    craters: 40,                     // G6
    bloom: false,                    // G8 — Epilogue only
  },
```

**G0.1 — Colour management.** r160 already defaults `ColorManagement.enabled = true` and
`outputColorSpace = SRGBColorSpace`, and every colour in the file is set via `setHex`/`new Color(0x…)`,
which are therefore interpreted as sRGB and converted correctly. **Verify this before touching
anything** (`console.log(renderer.outputColorSpace)`); do not "fix" it — the existing palette was
tuned under these defaults and a change would shift every colour in the app, the map sheets included.

**G0.2 — Tone mapping.** Add at [index.html:452](index.html#L452):

```js
if (CONFIG.GFX.toneMap) {
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = CONFIG.GFX.exposure;
}
```

ACES compresses highlights, which stops the additive burst flash and (later) the sun glint from
clipping to flat white — the flash currently blows out and loses its shape. It also desaturates
midtones ~10%, so the jungle greens at [index.html:489](index.html#L489) will need a small saturation
bump to look the same. **Do the exposure/palette rebalance in the same commit** or the tropical
palette will read muddy. `NeutralToneMapping` would be the better fit but does not exist in r160 —
do not bump the three version for it.

**G0.3 — Fog follows the sky.** Once G1 lands, the fog colour must be sampled from the horizon colour
of the current time-of-day, or the horizon will show a hard seam. Keep `Fog` (linear) rather than
`FogExp2`: linear fog gives an explicitly bounded haze envelope (4500–15000 m) that can be tuned to
guarantee a target at max engagement range (3200 m) stays fully legible. `FogExp2` looks marginally
more natural and is harder to bound — not worth the trade here.

**G0.4 — Pin quality while glassing (correctness fix).** In `setBinos`
([index.html:1911](index.html#L1911)) force tier 0 while binos are up and restore on exit; and in the
quality step at [index.html:3852](index.html#L3852) skip step-down entirely when `binos === true`.
Land this one on its own, ahead of the rest — it is a training-fidelity bug, and it is a two-line diff.

---

## G1 — Sky and time of day

Replace the flat background with the Preetham sky already available through the import map:

```js
import { Sky } from 'three/addons/objects/Sky.js';
```

One mesh, one draw call, renders at the far plane. Gains: a real horizon gradient, a sun disc, and
Rayleigh haze that makes the 10 km island read at scale. The cost is negligible; the reason to do it
early is that it becomes the **lighting authority** — sun elevation/azimuth, sun colour, hemisphere
colours and fog colour all derive from one time-of-day value.

```js
const TOD = {
  dawn:  { elev: 6,  azi: 100, sun: 0xFFC08A, inten: 1.05, hemiSky: 0xE8C9A8, fog: 0xE6D6C4 },
  day:   { elev: 55, azi: 145, sun: 0xFFF4E0, inten: 1.25, hemiSky: 0xBFE0EA, fog: 0xE3EADB },
  dusk:  { elev: 9,  azi: 250, sun: 0xFF9A5A, inten: 0.95, hemiSky: 0xD8A9A0, fog: 0xE0C6BA },
  overcast: { elev: 40, azi: 145, sun: 0xE8EAEC, inten: 0.55, hemiSky: 0xC9D2D6, fog: 0xD5DBDD },
};
```

Set per chapter from the `Scenario`, defaulting to `day`. This is nearly free campaign flavour: a
dawn raid, a Volume IV overcast on the black-sand palette, a dusk Epilogue.

**Constraints.** (1) Low sun elevations make G2's hillshade dramatic — good — but also throw long
shadows toward the observer that can hide targets; keep chapter TOD assignments away from `elev < 5`.
(2) The `[M]` map and the printed sheets must not inherit TOD tinting; they are separate 2D canvases
and should stay untouched — check after landing. (3) Re-run `buildTerrain()` when TOD changes, since
G2 bakes the sun direction into vertex colours.

---

## G2 — Baked hillshade + ambient occlusion (highest value item)

**Do this instead of shadow maps.** A shadow map covering a 10 km island needs cascades to be usable
at both 50 m and 3000 m; that is a large, fragile, per-frame-cost change. Because the terrain is
static per mission and `H(x,z)` is cheap and already available, the same visual information — and
almost all of the training value — can be *baked into the existing vertex colour attribute at build
time for zero frame cost*.

Extend `buildTerrain()` ([index.html:482](index.html#L482)). Split the existing single loop into two
passes: pass 1 writes heights into a `Float32Array` (it already computes them), pass 2 computes
shading and writes colours.

```js
// pass 2, per vertex: horizon march toward the sun with exponential step-out
function sunOcclusion(x, z, h, sx, sz, sy) {   // sx,sz = sun azimuth unit vec, sy = tan(elevation)
  let maxAng = 0;
  for (let d = 8; d < 1400; d *= 1.35) {       // ~13 samples, near micro-relief to distant massif
    const dh = H(x + sx * d, z + sz * d) - h;
    const a = dh / d;
    if (a > maxAng) maxAng = a;
  }
  return smoothstep(sy - 0.06, sy + 0.06, maxAng);  // 0 = lit, 1 = shadowed
}
```

Combine three terms into the vertex colour:

- **Lambert hillshade** — `dot(normal, sunDir)`, from the analytic gradient already computed for `sl`
  at [index.html:496](index.html#L496). Cheapest half of the effect; do this even if nothing else.
- **Cast shadow** — `sunOcclusion` above. This is what makes ridgelines and draws legible: the
  shadowed side of a spur is what an observer actually uses to read relief.
- **Valley AO** — sky occlusion, same march repeated over ~6 azimuths at a fixed short radius
  (≤300 m), averaged, scaled by `CONFIG.GFX.aoStrength`. Darkens draws and gullies; strongly improves
  terrain association against the contour sheets.

**Budget.** 301² ≈ 90 k vertices. Cast shadow ≈13 `H()` calls, AO ≈6×5 = 30 → ~3.9 M `H()` calls per
rebuild. `H_proc` is a 5-octave fbm, so expect **300–700 ms** — and `buildTerrain()` runs on *every*
mission regen via `rebuildWorld()` ([index.html:522](index.html#L522)). Mitigations, in preference order:

1. Compute shading on a coarser grid (e.g. 128²) and bilinearly sample it per vertex — cuts the cost
   ~50× and the loss is invisible, since shading is low-frequency by nature.
2. Cache per `(terrainSeed, TOD)`; chapters with fixed seeds then pay once.
3. If it still stalls, run it in an idle callback and swap the colour attribute in when ready —
   the terrain is already visible with flat colours meanwhile.

**Must respect `TERRAIN_PALETTE === 'black'`** (Volume IV black sand,
[index.html:487-490](index.html#L487-L490)). Dark sand plus shadow terms will crush to black; clamp
the shading multiplier to a higher floor (≈0.55 vs ≈0.35) when `blk` is true.

**Verification:** load a chapter, open `[M]`, and confirm a ridge visible in 3D corresponds to the
contours on the sheet. If hillshade is right, resection off the airfield/mast/village gets
*noticeably* easier — that is the acceptance test, not a screenshot.

---

## G3 — Near-field terrain LOD patch

Fixes defect #1. Add a second terrain mesh covering `CONFIG.GFX.nearLODSize` (2400 m) at 300 segments
= **8 m per quad**, centred on the midpoint of the OP→target line, rebuilt when the mission target
changes. Draw it over the base sheet with polygon offset:

```js
material: new THREE.MeshLambertMaterial({ vertexColors: true, flatShading: true,
  polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2 })
```

Both meshes evaluate the same `H()`, so they agree to within interpolation error; the offset kills
z-fighting without needing to punch a hole in the base sheet. Cost is +~180 k triangles (total still
under 300 k, comfortably inside budget for a static scene at 60 fps).

**Alternative if that reads badly at the seam:** raise base `meshSegments` 300 → 420 (25 m/quad, +75%
base tris) and shrink the patch to 1200 m / 300 seg (4 m/quad). Slightly more total cost, no seam
management. Try the patch first; it is the cheaper of the two.

**Caveat:** `groundHit()`/`hasLOS()` ([index.html:400](index.html#L400), [419](index.html#L419)) march
`H()` directly, not the mesh, so LOS and impact placement stay consistent automatically. Don't
"optimise" either of them onto the mesh.

---

## G4 — Instanced vegetation

The island currently has no vegetation at all, which is the largest single gap between what it looks
like and what it is. Two `InstancedMesh` objects, two draw calls:

- **Canopy** — crossed quads (2 quads, 8 tris) with an alpha-**tested** (not blended — avoids sort
  cost and depth ordering entirely) procedural texture. Palm/broadleaf silhouettes drawn once into an
  offscreen `<canvas>` at boot and uploaded as a `CanvasTexture`. **No external image assets** —
  the golden rules forbid them, and a data-URI texture would bloat the file.
- **Scrub/rock scatter** — a single low-poly form, tinted per instance via `instanceColor`.

Placement, driven by the seeded PRNG so scenarios stay reproducible:

```
density = f(H)          — none below 2 m (beach) or above 130 m (rock)
        × f(slope)      — none above the 0.62 slope threshold already used for rock
        × fbm patch     — clumping, reuse vnoise(x*0.012, z*0.012, 999) so veg agrees with the green
        × radial falloff from OP, 0 beyond CONFIG.GFX.vegRadius
        − exclusion discs around every Scenario element, friendly element, village, and road ribbon
```

**Hard rules, all training-driven:**

- **Cap canopy height at ~6 m** and exclude a radius around every target/friendly/civilian element.
  Masked targets are doctrinally interesting, but a trainer where you cannot see your own rounds land
  is a broken trainer. Density and radius stay in `CONFIG.GFX` so this is tunable, not baked.
- **Do not place vegetation on permanent structures or roads** — those must stay identifiable on both
  the 3D view and the map sheets (CLAUDE.md, terrain-association requirement).
- **Civilian/military discrimination must survive.** Re-run the discrimination check (§Verification)
  after landing; if villages get visually noisy, drop village-adjacent density before touching the
  element colours.
- Volume IV black-sand palette: sparse, desaturated, or off entirely.

**Wind sway** is optional and cheap — one `onBeforeCompile` vertex offset driven by a single shared
`uTime` uniform, updated once per frame from the existing loop. One uniform write, no allocation.
Skip it if it costs more than an hour; still frames read fine at this art level.

---

## G5 — Shoreline and water

Two effects, both fragment-side, both roughly free:

**Foam band.** In the terrain material's `onBeforeCompile`, add a term keyed on world-space Y near 0
(≈ −0.5 to +1.2 m) blended toward white, modulated by a scrolling fbm so it breathes. Instantly reads
as "island" instead of "green plate meeting blue plate", and it makes the coastline a usable
terrain-association feature from the OP.

**Sun glint.** The ocean is an unsubdivided plane, so do this in the fragment shader only: a specular
lobe around the reflected sun vector, broken up by a scrolling normal derived from two fbm octaves.
Do **not** subdivide the 44 km plane or add Gerstner vertex displacement — the cost is real and the
gain at these viewing distances is not. `three/addons/objects/Water.js` is available but is heavier
than this needs and would fight the flat-shaded art direction; skip it.

Both need exactly one `uTime` uniform updated per frame in `animate()`. Assign to a preallocated
uniform object — no per-frame allocation.

---

## G6 — Impact persistence

Two additions to the burst system ([index.html:1730](index.html#L1730)), both with direct doctrinal value:

**Persistent craters.** An `InstancedMesh` ring buffer of `CONFIG.GFX.craters` (40) dark discs, lifted
slightly with polygon offset, oldest recycled. Beyond looking right, this lets the observer **see
their own round pattern** — the shot group from a bracket is exactly what an FO is supposed to be
reading, and today it evaporates in 14 s.

**Lingering marker smoke.** Extend the last 2–3 impacts with a thin wisp that fades over 60–90 s, so a
walked bracket stays legible while the correction is being composed and transmitted. Keep it thin
enough not to mask the target — this is a marker, not a smoke screen.

Also worth the ten minutes: the water/ground differentiation at
[index.html:1772-1774](index.html#L1772-L1774) already recolours the dust; give the water case a
taller, narrower column so a round splashing offshore is unmistakably a miss into the sea rather than
an ambiguous puff on the beach. That is a real spotting distinction.

Everything here reuses the existing preallocated-pool pattern. **Do not switch bursts to a
per-impact-allocated particle system.**

---

## G7 — Optics presentation (no WebGL cost)

All of this is drawn into the existing 2D reticle canvas ([index.html:1850](index.html#L1850)) or done
in CSS — zero GL cost, zero risk to frame rate:

- Vignette + slight edge darkening in bino mode; a faint warm/cool fringe at the field stop.
- Keep the mil graduations at full contrast — they are a measuring instrument, and vignetting must
  never encroach on them.
- A small damped sway while glassing, settling over ~1 s, cut to near zero when the laser is held
  ([index.html:1926](index.html#L1926) region). Sells the optic and subtly rewards a steady hold.

**Explicitly skipped:** full-screen heat shimmer / SVG displacement filters. Full-screen CSS filters
on a WebGL canvas are expensive and unpredictable across GPUs, and shimmer actively degrades the
target-resolution that §G0.4 exists to protect.

---

## G8 — Optional bloom, Epilogue only

`EffectComposer` + `UnrealBloomPass` from `three/addons/`, at half resolution, **gated behind
`CONFIG.GFX.bloom` and only enabled for the SUNLAMP directed-energy chapter**, and only at quality
tier 0. A directed-energy weapon is the one thing in this app that genuinely wants bloom.

For everything else, **do not add a composer.** It costs a full-screen render target plus several
passes, breaks the direct `renderer.render()` path the quality system assumes
([index.html:3908](index.html#L3908)), and the additive sprite flashes already fake it well enough.
If bloom lands, the quality step-down must also switch the composer off, not just lower pixel ratio.

Reminder from CLAUDE.md: SUNLAMP **still uses the direct-impact ballistics model**. Bloom is pacing
and presentation only — nothing here touches `applyCorrection` or the impact math.

---

## Do not do

- **No `EffectComposer` on the default path** (see G8).
- **No PBR conversion.** `MeshStandardMaterial` across the scene without an environment map looks
  *worse* than the current Lambert, and with one costs materially more. The flat-shaded low-poly look
  is a deliberate identity and it reads better at 3 km than a half-finished realistic one would.
- **No cascaded shadow maps.** G2 gets ~90% of the relief information for zero frame cost.
- **No external textures, models, or fonts.** Procedural canvas textures only.
- **No trajectory sim, no angle-T, no gun-line rotation** — unchanged from CLAUDE.md; nothing in this
  plan touches ballistics.
- **No per-frame allocation.** Every object added here is preallocated at build time; new per-frame
  work is limited to uniform writes and instance-matrix updates.
- **Don't raise `meshSegments` past ~420** without re-measuring `buildTerrain()`, which runs on every
  mission regen.

---

## Parallel-work / merge notes

Each stage is a self-contained block, gated by one `CONFIG.GFX` flag, touching a bounded set of
functions. Conflict surface against another agent's edits:

| Stage | Functions/regions touched |
|---|---|
| G0 | `CONFIG`, renderer init (450-452), `setBinos`, `QUALITY` step |
| G1 | scene init (454-463), new `TOD` table, `rebuildWorld` |
| G2 | `buildTerrain` only |
| G3 | `buildTerrain` + one new `buildNearPatch`, called from `rebuildWorld` |
| G4 | new `buildVegetation`, called from `buildWorldFeatures` teardown/build |
| G5 | ocean init block, `buildTerrain` material, one uniform write in `animate` |
| G6 | burst pool block (1730-1822) |
| G7 | `drawReticle` / reticle canvas + CSS |
| G8 | renderer init + `animate` render call |

Only **G0** and **G3/G5** overlap regions likely to be under simultaneous edit (`CONFIG`,
`buildTerrain`, `animate`). Land G0 first and separately — it is small, it contains the bino-quality
correctness fix, and it establishes the `CONFIG.GFX` block that everything else references.

---

## Verification checklist

Run after every stage, in Chrome desktop:

1. **Frame rate** — 60 fps at quality tier 0 during a 6-round FFE with binos up. Watch
   `renderer.info.render.calls` (expect < 400) and `.triangles`.
2. **Target resolution** — place a troops-in-the-open scenario at 3000 m; the individual figures must
   be countable through binos at tier 0. This is the gate that fails first when adding haze, bloom, or
   vegetation.
3. **Civilian discrimination** — a civilian village and a military element in the same field of view
   must be unambiguous at 2000 m. Collateral damage is an auto-fail; the graphics must never make that
   a guess.
4. **Impact legibility** — a first round at max deviation must be spottable and its deviation
   measurable off the reticle. Confirm G4 vegetation didn't mask it.
5. **Map sheets unaffected** — `[M]` map, all five printed sheets, and the answer key render
   identically. Structures, roads, and the legend must still be present and correct.
6. **DEM path** — load a heightmap via the DEM ingestion path ([index.html:531](index.html#L531)) and
   confirm G2 hillshade and G4 placement work off DEM-derived `H()`, not just procedural.
7. **Black-sand palette** — run a Volume IV chapter; confirm terrain doesn't crush to black and
   vegetation rules applied.
8. **Rebuild cost** — time `rebuildWorld()`; a mission regen should stay under ~250 ms perceived.
9. **`window.SHITFIRE` interfaces intact** — `H`, `fireMission`, `applyCorrection`, `FDC.say`,
   `Scenario`, `gradeMission`, `TLOG` unchanged in shape ([index.html:3916](index.html#L3916)).

---

## Appendix A — The Graphics Ceiling

Everything above is the *plan* — G0 through G8, ordered by (gain ÷ risk), stopping well short of
maximal. This appendix answers a different question: **if cost and LOC were no object, and only the
CLAUDE.md constraints still applied, how good could this renderer actually get, and where does the
ceiling actually come from?**

### Binding constraint

The obvious candidates are not it:

| Candidate constraint | Actually binding? | Why not |
|---|---|---|
| One file, no build step, no npm | No | GLSL is fully available via `ShaderMaterial` / `onBeforeCompile`, written straight into template literals. No bundler needed to ship a shader. |
| CDN-only, no external deps | No | Everything in §G0–G8 and everything below is `three/addons/` or hand-rolled GLSL, already inside the existing import map. |
| No external assets | No | Textures don't need to be *files* — they can be generated procedurally at boot into render targets (canvas or GPU) and used exactly like a loaded texture from then on. |
| **No artists, no modeled assets, no photogrammetry** | **Yes** | This is the one CLAUDE.md constraint with no procedural substitute. Math can fake a sky. Nothing here can fake a hand-modeled soldier at 2 m from the camera. |

**Binding constraint: asset authoring, not tooling.** And that constraint is sharply asymmetric —
it caps some subsystems hard and barely touches others.

### The asymmetry

Pure-math subsystems (atmosphere, lighting, terrain form, water, explosion FX) need no assets and
can climb close to AAA. Subsystems that read as "an artist made this" up close (vegetation,
characters, vehicles, structures) are hard-capped by the same constraint that leaves the math
subsystems unbounded.

### The gift of this app's camera

SHITFIRE's observer stands on a fixed 15 m tower and never gets closer than ~1500 m to anything —
max engagement range is 3200 m. The categories that are expensive to fake without an artist
(vegetation up close, character/vehicle detail) are exactly the categories this camera never sees up
close. The ceiling therefore lands unusually high *for this specific application* — an FO trainer is
close to the best possible use case for an assets-free renderer. Budget should go almost entirely to
the far field: sky, terrain, water, FX.

### Ceiling by subsystem

Rated out of 10 — 10 being genuinely indistinguishable from a hand-authored AAA scene at the
distances this camera actually uses.

| Subsystem | Ceiling | What gets you there |
|---|---|---|
| Sky / atmosphere / lighting | **9/10** | Physical sky (Preetham via `Sky.js`, or Hosek-Wilkie), in-shader aerial perspective (Rayleigh/Mie), cascaded shadow maps (`three/addons/csm/CSM.js`), SSAO/GTAO, filmic grading, correctly-thresholded bloom, screen-space light shafts. All assetless pure math. **Single biggest realism lever in the project.** |
| Terrain form + texturing | **8.5/10** | Quadtree/clipmap LOD (4 rings, ~1–2 m near resolution out to 10 km, ~1M tris). Triplanar procedural texturing — sand/rock/jungle albedo+normal+roughness baked by GLSL noise into 1–2k render targets at boot. Multi-scale detail normals with distance blending. AO/shadow baked into texture, not vertex colour. |
| Water | **8/10** | Gerstner waves + procedural normal maps + fresnel + sun glint + depth-derived shoreline foam + planar reflection. `Water.js` already covers most of this. Full FFT ocean is overkill at these viewing distances. |
| Explosions / FX | **8/10** | GPU particles as instanced quads driven by a vertex-shader sim or ping-pong data texture; procedural noise-textured billboard smoke; heat-haze refraction pass; secondary fires. Assetless, high visual impact. |
| Vegetation | **6–7/10** | Procedural parameterized trees (trunk + fronds) generated at boot, instanced, with impostor atlases rendered from 8 angles into a boot-time render target for distance LOD. Convincing jungle canopy at 500 m+; obviously procedural at 20 m. **This is where "no assets" bites — but the camera is always far, so it's acceptable.** |
| Characters / vehicles / structures | **4/10 — deliberately** | At 1.5–3.2 km these are a few pixels tall. Training requires an identifiable silhouette, not model fidelity. Spending effort here is the worst gain-per-line in the entire project — this cap is a feature of the design, not a gap to close. |

**Composite, for what this camera actually sees: ~8/10** — a very good stylized-realistic vista,
roughly comparable to an Arma 3 / Ghost Recon vista at range, falling off to obviously-procedural
only at close range, which this app's camera geometry never visits.

### What the max version costs

- **~+3000–4000 LOC**, overwhelmingly GLSL living in template literals — roughly doubling the file.
- **~2–3 s boot budget** for procedural texture baking and erosion (below).
- **Needs a discrete GPU.** The post stack alone (SSAO + bloom + SMAA) is ~4–6 ms at 1080p; 3-cascade
  CSM over a 1M-tri terrain needs a separate shadow LOD or it blows the 16.6 ms frame budget outright.
  Integrated graphics will not hold 60 fps at this tier.
- **The adaptive quality system becomes load-bearing, not a nicety** — the same step-down mechanism
  §G0.4 currently patches for bino correctness ([index.html:3836-3841](index.html#L3836-L3841)) is what
  keeps the max version inside frame budget on mid-tier hardware.
- **Maintainability is the real casualty.** Several thousand lines of GLSL inside an ~8000-line
  single-file HTML document, with no shader compilation step to catch errors early, is genuinely
  painful to debug. This is a cost the LOC/frame-cost numbers don't show.

### Two highest-leverage items not in the main plan

Best gain-per-line in the entire document — neither is in §G0–G8 because both are riskier/heavier
than the "ship it" plan wants, but both dominate on pure ceiling-per-LOC.

1. **Hydraulic erosion at boot.** GPU ping-pong on a 1024² heightfield, ~50–100 iterations, under
   500 ms. This is what separates terrain that reads as *real terrain* — drainage networks, valley
   profiles, sediment deposition — from terrain that reads as fbm blobs. **Biggest single realism
   jump per line of code in the project.** Must run *before* the §G2 hillshade bake, and must keep
   `H(x,z)` as the sole authority afterward so `groundHit`/`hasLOS` stay consistent with what's drawn
   — same constraint §G3 already calls out for the near-field LOD patch.
2. **Real DEM data.** The heightmap is the *one* external input the golden rules already permit
   ([index.html:531](index.html#L531) ingestion path). Dropping in a real island's DEM gives
   geological structure no noise function can fake, for **zero new code.** Cheapest large realism win
   available, full stop.

### Version note

Bumping the pinned three.js version is permitted under CLAUDE.md — it's still CDN-only, no build
step. Later releases bring `WebGPURenderer` and TSL node materials, which raise the ceiling further
(compute shaders, better clustered lighting). **Not recommended here**: it's a renderer rewrite and a
compatibility risk for a training tool that has to just work in Chrome. Separately, flag that the
exact addon pass list — `GTAOPass` in particular — should be re-verified against whichever three
version is actually pinned; this document and §G0–G8 were written against **r160**.

### Training caveat

The training-value curve flattens long before the graphics ceiling does. Several max-tier features
are **net negative** for this trainer because they reduce target legibility at 1500–3200 m: depth of
field, heavy volumetric fog, dense canopy, aggressive bloom, film grain, motion blur, chromatic
aberration. The existing bino-quality pin (§G0.4) exists precisely because target legibility is
fragile at this range — the max version does not get a pass on that fragility just because it looks
better. Any push toward the ceiling must re-run the [§Verification checklist](#verification-checklist),
especially **check 2** (countable figures at 3000 m) and **check 3** (civilian/military discrimination
at 2000 m). A feature that raises the subsystem ceiling but fails either check is a regression, not
an upgrade — same rule the main plan states in its second governing constraint.

### Recommended practical max

**§G0–G6, plus hydraulic erosion, plus a real DEM, plus procedural triplanar terrain texturing.**
That lands around **7/10** — roughly 70% of the ceiling for about 25% of the cost and complexity —
and is approximately the point where further graphics work stops buying training value.
