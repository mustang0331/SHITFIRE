# CONTENT_INVENTORY.md — Port Content Inventory

Everything the engine port must carry over as **content** (data, not code). Extracted directly from `index.html` (~4,500 lines, single-file browser sim). Tight and factual — reference only; strategy/rationale live in the other ENGINE_PORT docs.

> **Snapshot warning.** These values were read from the live source and the sim is still under active development (see [ROADMAP.md](../ROADMAP.md)). Re-extract before relying on them for a port stage — par times and star thresholds in particular are balance decisions still being tuned.

## 1. Campaign structure

### Volumes

| Vol ID | Tab | Name | Star req. to unlock | Terrain seed | Palette |
|---|---|---|---|---|---|
| F | FWD | FOREWORD — THE SCHOOLHOUSE | 0 | 1337 | default (tropical) |
| V1 | I | VOLUME I — GREEN AS GRASS | 0 | 1337 | default |
| V2 | II | VOLUME II — THE RIDGE LINE | 6 | 9021 | default |
| V3 | III | VOLUME III — THUNDER RUN | 14 | 5150 | default |
| V4 | IV | VOLUME IV — BLACK SAND | 24 | 66600 | `black` (black-sand) |
| EP | EPI | EPILOGUE — SUNBURN | 32 | 1337 | default |
| V5 | V | VOLUME V — ON WINGS | Infinity (locked tease) | — | — |

### Chapters

All fields below are the literal values in the `CAMPAIGN` array. `impl: true` unless noted. Chapters unlock in order within a volume (completing the previous, not starring it, is enough).

| Ch. ID | Title | Type | Seed | Par (s) | Flags | Impl |
|---|---|---|---|---|---|---|
| F.1 | EYES BEFORE GUNS | wreck | 51 | — | `tut` step engine, `noMission: true` | yes |
| F.2 | THE MAP IS NOT THE TERRITORY | wreck | 52 | — | `tut` step engine, `noMission: true` | yes |
| F.3 | SAY IT LIKE YOU MEAN IT | wreck | 53 | 420 | `coach` hint array (no tut step engine) | yes |
| 1.1 | TROOPS IN THE OPEN, BRAINS IN THE REAR | troops | 101 | 300 | — | yes |
| 1.2 | KNOCK KNOCK | bunker | 102 | 360 | — | yes |
| 1.3 | HOLD THE LINE | strongpoint | 103 | 360 | — | yes |
| 1.4 | DO IT AGAIN, SLOWER | troops | 104 | 210 | — | yes |
| 2.1 | NUMBERS ON A COMPASS | troops | 201 | 330 | `method: 'polar'`, `noLaser: true` | yes |
| 2.2 | OLD FRIENDS | bunker | 202 | 360 | `method: 'shift'` | yes |
| 2.3 | DEFILADE BLUES | troops | 203 | 420 | `scn: { mask: true }` (crest-masked, no LOS) | yes |
| 2.4 | THE PERIMETER | strongpoint | 204 | 330 | — | yes |
| 3.1 | ROLLING STOCK | convoy | 301 | 420 | — | yes |
| 3.2 | CLOSE ENOUGH TO SMELL IT | assault | 302 | 420 | — | yes |
| 3.3 | UNINVITED GUESTS | raid | 303 | 390 | — | yes |
| 3.4 | EVERYONE'S MOVING | assault | 304 | 390 | — | yes |
| 3.5 | THE WRONG KIND OF FAMOUS | assault | 305 | 390 | `scn: { fClose: true }` (friendlies start inside 520 m) | yes |
| 4.1 | STRICT NET | strongpoint | 401 | 360 | `strict: true` | yes |
| 4.2 | TEN METERS | troops | 402 | 330 | `asset: 'mortar60'`, `scn: { effR: 30 }` | yes |
| 4.3 | NO SECOND CHANCES | bunker | 403 | 240 | `diffs: ['hard']` (hard-only) | yes |
| 4.4 | THE MEAT GRINDER | assault | 404 | 330 | `strict: true`, `diffs: ['hard']` | yes |
| E.1 | THE GREAT CHOW RAID | — | — | — | stage 11 | **no** |
| E.2 | CLAWS OUT | — | — | — | stage 11 | **no** |
| E.3 | SUNLAMP ACTUAL | — | — | — | stage 11 | **no** |
| V5.* | (none — volume has 0 chapters) | — | — | — | locked tease only, "AWAITING AIRCRAFT" | **no** |

Every chapter carries `story` (briefing text, optionally TTS'd) and `outro` (one-line AAR narrative beat); F.1–F.3 additionally carry `tut`/`coach` guided-hint data instead of relying on the live-fire coaching system.

## 2. Scenario templates (`SCN_META`, keyed by `SCN_TYPES`)

| Type | Display name | Setup | Win condition |
|---|---|---|---|
| `strongpoint` | POSITION UNDER ATTACK | Friendly compound (huts, flag, 2 vehicles) placed 1500–3200 m out; enemy cluster (8 troops) offset 150–250 m from it, LOS required from OP. | `hitsNeed` (default 3) rounds land within `effectRadius` (default 60 m) of enemy; zero rounds within `fratricideRadius` (80 m) of the compound. |
| `troops` | TROOPS IN THE OPEN | 8-troop cluster in the open, 1500–3200 m out, LOS required (or crest-masked/no-LOS variant when chapter sets `scn.mask`). | Same hits/effect-radius threshold; masked variant scores identically but without a visible burst. |
| `bunker` | DUG-IN BUNKER | Single bunker mesh + small troop cluster on high ground (22–140 m elev band, falls back to 8–999 m), `effectRadius: 35`, `hitsNeed: 2`. | 2 rounds within 35 m of the bunker. |
| `convoy` | CONVOY — MOVING TARGET | 4-vehicle column moving along a scanned coast-road path (1800 m), piecewise time→distance with a seeded 1–3 minute pit stop at the nearest fuel/ammo/airfield facility (falls back to a plain crew halt). | 3+ vehicle hits before the column reaches the end of the road (escape = fail). |
| `assault` | COMBINED ARMS ASSAULT — DANGER CLOSE | 8-troop enemy cluster; friendly squad (6) advances from 900 m out (or 520 m if chapter sets `scn.fClose`) toward the objective over the mission. | Neutralize enemy; zero rounds within fratricide radius of the advancing friendly line at time of impact. |
| `raid` | VILLAGE RAID — NO-STRIKE ADJACENT | Enemy cluster placed 130–200 m from a real generated village (LOS preferred, falls back to masked variant), huts flagged no-strike. | Neutralize raiders; zero rounds within `hutCollateralR`/`civCollateralR` of any hut or civilian. |
| `wreck` | DERELICT WRECK (TRAINING) | Single derelict landing-craft mesh, 1100–2100 m out, `effectRadius: 30`, `hitsNeed: 2`. Used for Foreword tutorial chapters. | 2 rounds within 30 m; no fail state in tutorial (`noMission` steps) or normal grading in F.3. |

**Scenario options consumed by chapters (`activeChapter.scn`):**
- `mask: true` — target must have **no** line of sight from the OP (crest-masked; 2.3).
- `fClose: true` — friendlies start inside 520 m instead of 900 m and hold closer (3.5).
- `effR: <meters>` — overrides `effectRadius` (4.2 sets 30 m for the 60mm mortar).

Every scenario also seeds up to 2 known points (`S.kps`), named from `KP_NAMES`, placed 900–2100 m out with LOS, for shift-from-known-point missions.

## 3. World feature set (`buildWorldFeatures()`)

Deterministic per-terrain-seed (`wSeed = terrainSeed ^ hash(DEM name)`), rebuilt on volume change or DEM load.

- **Coast road** — 128-point radial scan for the outermost "low shelf" elevation band (2–9 m) per bearing, gap-filled from neighbors, 2-pass smoothed, closed loop. Basis for facility/village placement (`roadAt(fraction)`).
- **Airfield** — flattest low ground (elev 2–10 m) with a heading where the ground deviates ≤4.5 m over a 600 m run in one of 4 cardinal-ish directions; places a runway ribbon (620 m) + 2 mil buildings + a mast.
- **Radio mast** — highest ground (elev ≥35 m) that is ≥900 m from the OP (kept off the OP's own hill so it reads as a resection landmark); mast pole + small building.
- **Coastal gun** — headland, elev 8–30 m, with open water ~240 m seaward; gun casemate + barrel oriented out to sea.
- **Fuel point** — on the coast road at a seeded fraction (10–40% along), inland 30 m; building + 3 fuel drums.
- **Ammo depot** — on the coast road at a second seeded fraction (kept ≥40% away from the fuel point, wrapped), inland 34 m; 3 angled storage buildings.
- **Civilian villages** — `WORLD.villageCount` (2) placed on the coast road, fractions kept apart from each other and from facilities; 5–7 huts scattered in a 12–42 m radius, `civsPerVillage` (6) wandering civilian figures per village with a home-radius wander behavior.
- **Dirt paths** — connect villages and mast/gun facilities in pairs, wavy interior routes toward the first ridge shoulder; a path is discarded if it would cross water.
- **OP watchtower** (`placeOP`) — the observer's elevated post; not part of `buildWorldFeatures` but a fixed per-terrain placement.
- **Firing battery** (`placeBattery`) — the guns' world position; also placed separately, per terrain.

All facility/village positions are recorded in `WORLD.facilities` / `WORLD.villages` for reuse by scenario generation (no-target-near-village rule, convoy pit-stop targeting, `tutLandmark()` for F.2).

## 4. Named data tables

- **`KP_NAMES`** — `['BREWERY', 'LATRINE', 'SAWMILL', 'CHAPEL', 'DERBY', 'ICEBOX']`. Registered known-point names for shift-from-known-point missions (2.2 and any scenario with `S.kps`).

### QUIPS pools (HELLHOUND FIRES dialogue)

| Pool | Purpose | Lines |
|---|---|---|
| `greet` | Opening net-check line when a session/mission starts | 5 |
| `readbackTail` | Sardonic tag appended after a clean CFF readback | 5 |
| `corrSnark` | Remark on a large/odd correction | 4 |
| `ffeAck` | Acknowledges FIRE FOR EFFECT | 4 |
| `completeTail` | Tag after ROUNDS COMPLETE | 4 |
| `eomGood` | End-of-mission, target neutralized/destroyed | 4 |
| `eomBad` | End-of-mission, target survived / mission abandoned | 4 |
| `badGrid` | Grid doesn't parse / off the map | 4 |
| `water` | Target grid lands in the ocean | 3 |
| `noMission` | Observer sent a correction with no mission active | 3 |
| `inFlight` | Observer keyed up while rounds are still in the air | 3 |
| `unknown` | Unparseable transmission | 4 |
| `complete` | Prompts the observer that the mission is ready to close | 1 |
| `dangerClose` | Danger-close proword omitted near friendlies | 2 |
| `wrongWay` | Corrections moving the burst away from the target | 2 |
| `rantLost` | Observer transmission has no recognizable CFF content | 2 |
| `coachTimid` | Coaching: correction too small relative to observed miss | 2 |
| `coachBracket` | Coaching: all rounds on the same side, no bracket established | 2 |
| `coachBracketGood` | Coaching: bracket achieved, prompts halving | 2 |
| `coachStagnant` | Coaching: correction didn't move the burst | 2 |
| `coachLoc` | Coaching: first-round location error exceeds 200 m standard | 1 |
| `careerFrat` | Pre-mission callback referencing the player's fratricide history | 2 |
| `careerCollat` | Pre-mission callback referencing collateral-damage history | 2 |
| `careerVet` | Pre-mission callback for an experienced/returning player | 2 |
| `snide` | Generic one-liner for a stupid-but-safe malformed call | 4 |
| `snideRound` | One-liner when the FDC auto-rounds sloppy correction numbers | 3 |
| `nearCiv` | Advisory: target grid sits close to a village | 2 |
| `unsafeCorr` | Rejects a correction that would walk fire onto friendlies/civilians | 2 |
| `unsafeInsist` | Fires anyway after the observer insists on an unsafe correction | 2 |
| `rantCiv` | Full rant sequence (2 variants, 3 lines each) for civilian collateral damage — mission-ending | 2 sequences × 3 lines |
| `rantFrat` | Full rant sequence (3 variants, 3 lines each) for fratricide — mission-ending | 3 sequences × 3 lines |

### Commendations (AAR medal strings)

- `GODDAMN HOLE IN ONE` — effect after a single adjusting round.
- `EAGLE EYE` — initial location within 50 m.
- `ACTUALLY READ THE MANUAL` — clean call format, zero coaching notes.
- `FAST MOVER` — finished in ≤60% of chapter par.
- `IRON NET` — passed a strict-mode (Volume IV) chapter.
- `BY THE BOOK` — closed with refinement and/or record-as-target (full RREMS).
- `ORIENTED` — sent OT direction to the FDC.
- `TEN-METER MAN` — passed a `mortar60`-asset (4.2) chapter.

### `CONFIG` tunables, grouped by system

- **`SEED`** — `terrain: 1337` (default), `mission: 1` (overridden per-chapter/skirmish).
- **`MAP`** — `size: 10000`, `originE: 20000`, `originN: 50000` (SW-corner easting/northing, meters).
- **`TERRAIN`** — `meshSegments: 300`, `islandRadius: 3900`, `radiusWobble: 800`, `ridgeHeight: 95`, `massifHeight: 165`, `massifRadius: 950`, `massifX: -520`, `massifZ: -360`, `oceanFloor: -30`, `demRes: 512`, `demMaxElev: 240`, `demSeaLum: 10`.
- **`CAMERA`** — `fov: 60`, `binoFov: 9`, `eyeHeight: 2.2`, `towerHeight: 300` (50 ft OP), `mouseSens: 0.0022`, `pitchClamp: 1.35`.
- **`BALLISTICS`** — `tofDivisor: 300`, `tofBase: 8`, `tofMin: 15`, `tofMax: 40`, `splashLead: 5`; `firstRound` dispersion (uniform, m) `easy [40,90] / normal [60,150] / hard [90,220]`; `followUp` dispersion (1-sigma, m) `easy {range:12, defl:7} / normal {18,10} / hard {26,15}`; `ffeRounds: 6`, `ffeStagger: [0.25, 0.9]`.
- **`MISSION`** — `effectRadius: 60`, `hitsToNeutralize: 3`, `fratricideRadius: 80`, `passMaxAdjustRounds: 4`, `targetRange: [1500, 3200]`, `enemyOffset: [150, 250]`.
- **`FDC`** — `obs: 'MUSTANG 12'`, `fdc: 'HELLHOUND FIRES'`, `readbackDelay: 1.3`, `shotDelay: 4.0`.
- **`VOICE`** — `lang: 'en-US'`, `ttsLang: 'en-IN'`, `rate: 1.05`, `pitch: 0.8`, `volume: 1.0`, `ttsEnabled: true`, `preferredVoices` (ordered match list for FDC TTS voice selection).
- **`GAMEPAD`** — `lookSpeed: 2.8`, `deadzone: 0.16`.
- **`UI`** — `laseHold: 6` (seconds).
- **`WORLD`** — `roadWidth: 7`, `pathWidth: 3.2`, `villageCount: 2`, `civsPerVillage: 6`, `convoyStopDur: [60, 180]`, `hutCollateralR: 24`, `civCollateralR: 28`.
- **`GFX`** — visual-overhaul flags (stage 13 / GRAPHICS.md), each independently toggleable: `toneMap`, `exposure: 1.15`, `satComp: 1.0`, `sky`, `hillshade`, `aoStrength: 0.35`, `nearLOD`, `nearLODSize: 2400`, `nearLODSeg: 300`, `veg`, `vegBudget: 4000`, `vegRadius: 2600`, `water`, `craters: 0`, `bloom` (SUNLAMP-only, Epilogue).

## 5. Assets to author in-engine

Everything below has **no authored asset today** — it is procedurally generated (low-poly `BoxGeometry` primitives, noise-driven terrain, or synthesized audio/voice) and must be authored or substituted when porting:

- **Terrain** — procedural heightfield (`H(x,z)`, ridged fBm + volcanic massif + coral beach shelf) or an optionally-loaded grayscale DEM image; no terrain meshes/textures exist as assets.
- **Units** — troops, vehicles (4-vehicle convoy, strongpoint pair, derelict landing craft), the friendly assault squad: all single untextured boxes (`BoxGeometry`) with flat materials — no character/vehicle models, rigs, or animation.
- **Structures** — huts, bunker, airfield buildings, radio mast, coastal gun casemate, fuel/ammo depot buildings, OP watchtower, firing battery emplacement: all boxes/simple primitives, no modeled structures.
- **Civilians** — wandering villager figures: single box mesh (`_civGeo`) per civilian, no character model or walk animation (position-lerped wander only).
- **Burst/impact effects** — flashes, smoke puffs, flames, debris cubes: primitive meshes and particle-less sprite/box effects, no VFX assets (no particle systems, no impact decals beyond code-driven color/scale changes).
- **Roads/paths** — procedural ribbon meshes (`ribbonMesh`) following a scanned/smoothed coastline or waypoint chain; no road/path textures or meshes.
- **Audio** — 100% synthesized via Web Audio API at runtime (noise-buffer booms, biquad-filtered squelch, oscillator-based low end); zero audio asset files (no SFX libraries, no music).
- **Voice** — both the FDC (HELLHOUND FIRES) and player transmissions run through the browser's Web Speech API (`SpeechSynthesis`/`SpeechRecognition`), keyless and client-side; no voice-actor recordings exist for any line in `QUIPS`, chapter `story`/`outro` text, or tutorial dialogue.
- **UI chrome** — map sheets, reticle overlay, menu/bookshelf theme, AAR screen: all DOM/CSS/canvas-drawn, no UI art assets.

These are the items an engine team must budget for as new production (models, textures, VFX, SFX, VO) rather than data migration — everything in §1–§4 above is a straightforward data port.
