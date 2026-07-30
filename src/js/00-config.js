import * as THREE from 'three';
import { Sky } from 'three/addons/objects/Sky.js';   // 13c — Preetham sky, one draw call

/* ============================================================ CONFIG */
const CONFIG = {
  SEED: { terrain: 1337, mission: 1 },
  MAP: { size: 10000, originE: 20000, originN: 50000 }, // SW corner easting/northing (m)
  TERRAIN: {
    meshSegments: 300, islandRadius: 3900, radiusWobble: 800,
    ridgeHeight: 95, massifHeight: 165, massifRadius: 950,
    massifX: -520, massifZ: -360, oceanFloor: -30,
    demRes: 512, demMaxElev: 240, demSeaLum: 10,
    /* 13e — near-field LOD patch over the mission target area: patchSize m
       square re-sampled at patchDiv sub-cells per base facet (33.3/4 ≈ 8.3 m),
       so burst deviation is judged against relief the base sheet aliases away. */
    patchSize: 1200, patchDiv: 4,
  },
  CAMERA: { fov: 60, eyeHeight: 2.2, towerHeight: 300,   // 50 ft OP watchtower
            /* G4 — three optical powers, cycled with [Z] or the mouse wheel while
               glassing. MAGNIFICATION is the authored number and the field of view
               is DERIVED from it (fov = naked-eye fov / power), which is what keeps
               the mil reticle honest: drawReticle() computes px-per-mil from
               whatever the current fov is, so one number changes and the
               graduations re-scale to stay true. Author a fov directly and the two
               can drift apart, and a reticle that lies about mils is worse than no
               reticle — every range estimate the observer makes runs through it.

                 4X   fov 15.0  — scanning. The old 9 deg was too tight to find
                                  anything with; this is the "expand the FOV" ask.
                 7X   fov  8.57 — the M22 the doctrine assumes. Default.
                14X   fov  4.29 — precision mil measurement on a small target. */
            binoZooms: [4, 7, 14], binoZoom: 1,
            /* 13i — hand-held sway amplitude in mils (constant angle; higher
               power magnifies it optically, as real glass does). 0 disables. */
            swayMil: 0.65,
            mouseSens: 0.0022, pitchClamp: 1.35 },
  BALLISTICS: {
    tofDivisor: 300, tofBase: 8, tofMin: 15, tofMax: 40, splashLead: 5,
    firstRound: { easy: [40, 90], normal: [60, 150], hard: [90, 220] },   // m, uniform
    followUp:   { easy: { range: 12, defl: 7 }, normal: { range: 18, defl: 10 },
                  hard: { range: 26, defl: 15 } },                        // m, 1-sigma
    ffeRounds: 6, ffeStagger: [0.25, 0.9],
    /* G3 — TEST AID. `false` makes every round land exactly on the aimpoint, so
       the observer's own arithmetic can be checked without dispersion muddying
       the result. Toggled live with SHIFT+D.

       A mission fired with this off is NOT A GRADED MISSION and the code treats
       it that way: no stars are recorded, the AAR says so in the verdict line,
       the TLOG entry carries noDisp, and the HUD shows NO DISP the whole time.
       An unmarked no-dispersion run would silently look like a perfect one,
       which would corrupt the only record of how the observer is actually
       doing — the whole point of the trainer. */
    dispersion: true,
  },
  MISSION: {
    fratricideRadius: 80,
    passMaxAdjustRounds: 4, targetRange: [1500, 3200], enemyOffset: [150, 250],
    /* F2 — danger-close distance per firing asset (BALLISTICS_RESEARCH.md §6:
       JFIRE's tables put mortars at roughly a third to half of the 155's
       figure; the flat single-band gate itself is a correct simplification). */
    dangerClose: { battery: 600, mortar60: 250 },
  },
  /* G13 — graded terminal effects. Replaces the old binary model (N hits inside
     one radius = "neutralized"). Every HE round now contributes a casualty /
     damage percentage by how far from the target it lands, and the accumulated
     figure grades into the three doctrinal outcomes:

         SUPPRESSED   — fire is landing close enough to force heads down; the
                        effect lasts only during and briefly after the fires
                        (FM 6-30 §4-14: temporary by definition)
         NEUTRALIZED  — >=10% casualties: the unit is combat-ineffective
                        (FM 6-30 §4-14 / App. E-2)
         DESTROYED    — >=30% casualties: permanently out of action
                        (FM 6-30 §4-14: "normally renders a unit ineffective")

     Distance bands are unclassified proxies (the real JMEM effect tables are
     classified — FM 6-30 §4-14e): personnel bands lean on the 155mm lethal-area
     figures and ATP 3-09.30 Table 1-1 FPF widths recorded in DOCTRINE.md
     §Pre-mission and effects data; posture swing (standing vs prone) comes from
     FM 7-90 App. B, where the same volley clears neutralization on a standing
     platoon and fails it on a prone one. G18 refines these per asset (60mm vs
     155); until then these are the 155 battery's numbers and chapter `effR`
     scales the bands for the precision shoots.

     This models terminal EFFECT at the impact point only — where the round
     lands is still impact = aimpoint + error, never a trajectory. */
  EFFECTS: {
    suppressSec: 90,                        // suppression outlives the last round by this
    // 12h — a smoke screen laid within `radius` of the enemy suppresses by
    // obscuration for `sec` (a screened gun cannot serve its target)
    screen: { radius: 120, sec: 90 },
    posture: { standing: 1.0, prone: 0.4 }, // FM 7-90 App. B: posture alone swings ~2.5x
    /* G18 — per-asset band sets: a 60mm round and a 155 do not do the same
       thing. Per-round contribution (%) at full effect, and the distance
       bands: full inside rFull, half to rHalf, quarter (suppression credit)
       to rSupp. 'point' is hard targets (bunker, derelict) on a
       structural-damage scale.

       arty (155): bands from the lethal-area/FPF proxies (DOCTRINE.md §G18 —
       the true JMEM figures are classified, flagged ⚠ there).
       mortar60: bands are FM 7-90 App. B verbatim (20 m "probably
       suppressed", 35 m 50%, beyond 50 m little effect); perRound 2 makes the
       manual's own check case come out right — a section volley of 10 rounds
       on a standing platoon ≈ 20% (neutralized), the same volley prone 8%
       (fails, suppression only). Mortars suppress and neutralize; destruction
       with sixties is meant to take several volleys. */
    arty: {
      personnel: { rFull: 30, rHalf: 50, rSupp: 75, perRound: 8,
                   neutralizePct: 10, destroyPct: 30 },
      point:     { rFull: 15, rHalf: 25, rSupp: 45, perRound: 55,
                   neutralizePct: 40, destroyPct: 100 },
    },
    mortar60: {
      personnel: { rFull: 20, rHalf: 35, rSupp: 50, perRound: 2,
                   neutralizePct: 10, destroyPct: 30 },
      point:     { rFull: 10, rHalf: 18, rSupp: 35, perRound: 25,
                   neutralizePct: 40, destroyPct: 100 },
    },
  },
  /* G2 — declination. The world, the grid, the map sheet and every azimuth on the
     fire net are GRID (true) mils. The observer's own instruments — compass, HUD
     heading, binocular heading — read MAGNETIC, because a real compass does.

     SIGN CONVENTION, stated once because getting it backwards is silent and
     costly: declEastDeg is degrees that magnetic north lies EAST of grid north.
     With MN east of GN, a fixed direction measured from MN reads SMALLER than
     measured from GN, so

         magnetic = grid - declination
         grid     = magnetic + declination

     7 deg = 124.4 mils, which is far bigger than any correction the observer
     will ever send. Transmitting a magnetic azimuth as OT direction is therefore
     a real and very findable mistake, and handleDirection() calls it out by name
     rather than as a generic "you are off by N mils". */
  NAV: { declEastDeg: 7 },
  /* G17 — one net, two fire units. HELLHOUND FIRES is the 155 battery;
     HACKSAW FIRES is the 60mm mortar section (Volume IV precision shoots).
     Different units answer different calls, and the observer is expected to
     address the right one — strict net enforces it. HACKSAW chosen to be
     phonetically far from HELLHOUND so speech recognition cannot confuse the
     two, and clear of every existing name (MUSTANG, GUNNY, SUNLAMP). */
  FDC: { obs: 'MUSTANG 12', fdc: 'HELLHOUND FIRES', fdc60: 'HACKSAW FIRES',
         readbackDelay: 1.3, shotDelay: 4.0 },
  VOICE: { lang: 'en-US', ttsLang: 'en-IN', rate: 1.05, pitch: 0.8, volume: 1.0,
           ttsEnabled: true,
           preferredVoices: ['ravi', 'heera', 'en-in', 'india',
                             'david', 'mark', 'guy', 'male', 'en-us', 'english'] },
  GAMEPAD: { lookSpeed: 2.8, deadzone: 0.16 },
  // Spot report: the cue that tells the observer WHERE TO LOOK without telling
  // him where the target IS. See sendSpotReport().
  SPOT: { enabled: true, lead: 2.4, gap: 2.9, saluteChance: 0.7, landmarkMax: 2600 },
  // Observer optics: DAY / NVG / THERMAL, cycled with [O]. See VISION.
  OPTICS: {
    enabled: true,
    hz: 24,             // overlay redraw rate (grain, vignette, halo). NOT the frame rate.
    cvW: 320, cvH: 180, // overlay backing store; CSS stretches it to fill. Chunky on
                        // purpose — grain and halo want to be soft, and this makes the
                        // whole overlay cost ~58k pixels instead of 2M.
    grain: 0.16,        // NVG scintillation amplitude, 0..1
    haloR: 26,          // halo radius in overlay pixels at 1x
    // Daylight degradation. Both devices stay AVAILABLE at every time of day and
    // simply stop working well — see VISION's header for why that beats hiding them.
    nvgBloomAt: 0.35,   // ambient above this starts blooming the tube out
    thermalWashAt: 0.30,// ambient above this starts cooking the terrain hot
  },
  UI: { laseHold: 6 },
  WORLD: {
    // E4 — two road CLASSES, not one ribbon with two widths.
    //   asphalt  metalled route between the major landmarks. Wide, dark, smooth,
    //            and laid on a pale gravel shoulder 2*shoulderW wider than the
    //            carriageway, which is what gives it a defined edge at range.
    //   dirt     unimproved track to huts, villages and the OP. Narrower, lighter,
    //            browner, no shoulder.
    roadWidth: 8, shoulderW: 3.2, pathWidth: 2.8,
    // E6 — three settlement tiers, so "the village" and "the town" are
    // different answers to "what am I looking at". A village is a hut cluster
    // off a dirt track; a town has streets, masonry, and a metalled spur.
    villageCount: 2, civsPerVillage: 6,
    townCount: 1, townBuildings: [14, 21], townR: 135, civsPerTown: 10,
    townStreetHalf: 135, townCrossHalf: 88, townSetback: 21,
    // E7 — rock outcrops / boulder piles. Terrain-association landmarks on
    // ground that carries nothing else: the ridges and spurs between the coast
    // road and the massif, where a jungle island otherwise offers the observer
    // no named feature to resect off.
    rock: { count: 14, sep: 460, rMin: 11, rMax: 27, nMin: 4, nMax: 10, named: 3 },
    convoyStopDur: [60, 180],          // pit-stop length range (s)
    hutCollateralR: 24, civCollateralR: 28,
  },
  // Visual overhaul (SPEC stage 13 / GRAPHICS.md). Every row hangs off one flag here so the
  // whole plan can be A/B'd or reverted without touching code.
  GFX: {
    toneMap: true,      // 13b — ACES filmic tone mapping
    exposure: 1.15,     // 13b — renderer.toneMappingExposure
    satComp: 1.0,       // 13b — palette saturation trim; see gfxPal(). 1.0 = neutral.
    /* 13f — instanced vegetation. vegMaxH caps the canopy so foliage can never
       hide a burst plume (training rule, GRAPHICS.md §G4); the budget and the
       OP-centred radius are perf knobs. Black-sand palette runs at 30% budget. */
    veg: true, vegBudget: 4000, vegRadius: 2600, vegMaxH: 6,
    // --- forward declarations: inert until the row that consumes them lands ---
    sky: true,          // 13c — Sky.js + time of day
    // --- target legibility -------------------------------------------------
    // A 2.4 m silhouette at 3200 m subtends 0.75 mils; through the 9-degree bino
    // FOV that is ~5 px at 1080p, which is below the threshold where a human can
    // COUNT bodies — the skill this app exists to train. legFloorMil is the
    // minimum angular height a figure is allowed to subtend; the mesh is scaled
    // up to hold it. Purely visual: every scoring path (effect radius,
    // fratricide, collateral) reads Scenario / WORLD coordinates, never a mesh
    // transform. See legibilityPass().
    //   1 mil = 1 m at 1000 m. px through binos = mils * innerHeight / 160.
    legibility: true,
    legFloorMil: 1.8,   // -> 12.2 px tall at 1080p through binos, at any range
    legMaxScale: 2.5,   // hard clamp on the silhouette exaggeration
    legOnset: 800,      // m — never touch a figure closer than this
    contactShadow: true,// dark disc at the feet so figures don't merge into terrain
    hillshade: true, aoStrength: 0.35, hillFloor: 0.55, // 13d — see the bake in buildTerrain()
    water: true,        // 13g — shoreline foam + ocean sun glint (fragment-side)
    craters: 40,        // 13h — persistent-crater ring buffer (oldest recycled)
    bloom: false,       // 11d — SUNLAMP only
  },
};

/* --- 13b: ACES filmic, evaluated on the CPU -------------------------------
   three applies ACESFilmicToneMapping in the fragment shader, so anything the
   shader touches (lit materials, fog) gets tone mapped — but scene.background,
   when it is a plain Color, is written as the clear colour and bypasses the
   shader entirely. Enabling ACES without accounting for that opens a visible
   seam where the tone-mapped fog meets the untouched sky.

   This is three's exact r160 pipeline (ACESInputMat -> RRTAndODTFit ->
   ACESOutputMat, with the same exposure/0.6 scale), so a colour pushed through
   here lands where the shader would have put it. Used to pre-tone the
   background so sky and fog still agree at the horizon. */
function acesFit(v) {
  const a = v * (v + 0.0245786) - 0.000090537;
  const b = v * (0.983729 * v + 0.4329510) + 0.238081;
  return a / b;
}
function acesFilmic(hex, exposure) {
  const c = new THREE.Color(hex);
  const s = exposure / 0.6;
  let r = c.r * s, g = c.g * s, bl = c.b * s;
  // ACESInputMat
  let x = 0.59719 * r + 0.35458 * g + 0.04823 * bl;
  let y = 0.07600 * r + 0.90834 * g + 0.01566 * bl;
  let z = 0.02840 * r + 0.13383 * g + 0.83777 * bl;
  x = acesFit(x); y = acesFit(y); z = acesFit(z);
  // ACESOutputMat
  r  =  1.60475 * x - 0.53108 * y - 0.07367 * z;
  g  = -0.10208 * x + 1.10813 * y - 0.00605 * z;
  bl = -0.00327 * x - 0.07276 * y + 1.07602 * z;
  return c.setRGB(Math.min(1, Math.max(0, r)), Math.min(1, Math.max(0, g)), Math.min(1, Math.max(0, bl)));
}

/* Palette saturation trim, one knob for the whole island.

   GRAPHICS.md G0.2 says ACES "desaturates midtones ~10%" and that the jungle
   greens will need a saturation bump to compensate. Measured against this
   palette at exposure 1.15, that is not what happens — ACES *raises* saturation
   on almost all of it, because these are dark-to-mid colours and the RRT fit
   expands saturation below the highlight rolloff:

       jungle A 0x4E7A3D  +13.0%      sand  0xD8C89A  -26.7%
       jungle B 0x3B5E2E  +22.3%      rock  0x5A544E  +31.9%
       ocean    0x1E5A7A  +14.3%

   Only sand — the brightest colour, the one actually in the rolloff — loses
   any. So a >1.0 compensation would push four of five further the wrong way.
   satComp therefore defaults to 1.0 (neutral) and this stays a tuning knob
   rather than a correction: if the island reads oversaturated under ACES, trim
   BELOW 1.0. Turn satComp, not the hex values. */
function gfxPal(hex) {
  const c = new THREE.Color(hex);
  if (!CONFIG.GFX.toneMap || CONFIG.GFX.satComp === 1) return c;
  const hsl = { h: 0, s: 0, l: 0 };
  c.getHSL(hsl);
  return c.setHSL(hsl.h, Math.min(1, hsl.s * CONFIG.GFX.satComp), hsl.l);
}
let DIFFICULTY = 'normal';
let currentType = 'strongpoint';
const SCN_META = {
  strongpoint: { name: 'POSITION UNDER ATTACK' },
  troops:      { name: 'TROOPS IN THE OPEN' },
  bunker:      { name: 'DUG-IN BUNKER' },
  convoy:      { name: 'CONVOY — MOVING TARGET' },
  assault:     { name: 'COMBINED ARMS ASSAULT — DANGER CLOSE' },
  raid:        { name: 'VILLAGE RAID — NO-STRIKE ADJACENT' },
  wreck:       { name: 'DERELICT WRECK (TRAINING)' },
};
const KP_NAMES = ['BREWERY', 'LATRINE', 'SAWMILL', 'CHAPEL', 'DERBY', 'ICEBOX'];
const SCN_TYPES = Object.keys(SCN_META);
const MILS_PER_RAD = 6400 / (Math.PI * 2);

