# CONTENT_INVENTORY.md — Port Content Inventory

Everything the engine port must carry over as **content** (data, not code). Extracted directly from `index.html` (~4,500 lines, single-file browser sim). Tight and factual — reference only; strategy/rationale live in the other ENGINE_PORT docs.

> **Snapshot warning.** Re-extracted 2026-08-31 at tag `oracle-v1` (commit `6642f92`) — these are the values the engine port's parity fixtures were captured against. The sim was still under active development up to that commit (see [ROADMAP.md](../ROADMAP.md)); anything ported past this snapshot should re-extract again rather than trust this file.

## 1. Campaign structure

### Volumes

| Vol ID | Tab | Name | Star req. to unlock | Terrain seed | Palette / ToD |
|---|---|---|---|---|---|
| F | FWD | FOREWORD — THE SCHOOLHOUSE | 0 | 1337 | default (tropical) |
| V1 | I | VOLUME I — GREEN AS GRASS | 0 | 1337 | default |
| V2 | II | VOLUME II — THE RIDGE LINE | 4 | 9021 | default |
| V3 | III | VOLUME III — THUNDER RUN | 9 | 5150 | default |
| V4 | IV | VOLUME IV — BLACK SAND | 15 | 66600 | `black` (black-sand); `tod: 'overcast'` volume-wide |
| EP | EPI | EPILOGUE — SUNBURN | 20 | 1337 | default |
| V5 | V | VOLUME V — ON WINGS | Infinity (locked tease) | — | — |

Unlock thresholds were lowered from the original 6/14/24/32 ladder to 4/9/15/20 (code comment `UX11`, author ruling 2026-08-27) — unlocks only, no re-locking of existing saves.

### Chapters

All fields below are the literal values in the `CAMPAIGN` array. `impl: true` unless noted. Chapters unlock in order within a volume (completing the previous, not starring it, is enough).

| Ch. ID | Title | Type | Seed | Par (s) | Flags | Impl |
|---|---|---|---|---|---|---|
| F.1 | EYES BEFORE GUNS | wreck | 51 | — | `tut` step engine, `noMission: true` | yes |
| F.2 | THE MAP IS NOT THE TERRITORY | wreck | 52 | — | `tut` step engine, `noMission: true` | yes |
| F.3 | SAY IT LIKE YOU MEAN IT | wreck | 53 | 420 | `coach` hint array, 3 lines (no tut step engine) | yes |
| 1.1 | TROOPS IN THE OPEN, BRAINS IN THE REAR | troops | 101 | 300 | — | yes |
| 1.2 | KNOCK KNOCK | bunker | 102 | 360 | — | yes |
| 1.3 | HOLD THE LINE | strongpoint | 103 | 360 | — | yes |
| 1.4 | DO IT AGAIN, SLOWER | troops | 104 | 210 | — | yes |
| 2.1 | NUMBERS ON A COMPASS | troops | 201 | 330 | `method: 'polar'`, `noLaser: true` | yes |
| 2.2 | OLD FRIENDS | bunker | 202 | 360 | `method: 'shift'`, `tod: 'night'` | yes |
| 2.3 | DEFILADE BLUES | troops | 203 | 420 | `scn: { mask: true }` (crest-masked, no LOS) | yes |
| 2.4 | THE PERIMETER | strongpoint | 204 | 330 | — | yes |
| 2.5 | FLARES OUT | strongpoint | 205 | 540 | `tod: 'night'`, `reqIllum: true` (coordinated illumination — light, then steel), `coach` hint array, 2 lines | yes |
| 3.1 | ROLLING STOCK | convoy | 301 | 420 | — | yes |
| 3.2 | CLOSE ENOUGH TO SMELL IT | assault | 302 | 420 | — | yes |
| 3.3 | UNINVITED GUESTS | raid | 303 | 390 | `tod: 'dawn'` | yes |
| 3.4 | EVERYONE'S MOVING | assault | 304 | 390 | — | yes |
| 3.5 | THE WRONG KIND OF FAMOUS | assault | 305 | 390 | `scn: { fClose: true }` (friendlies start inside 520 m) | yes |
| 3.6 | SEND IT NOW | strongpoint | 306 | 180 | `reqImmediate: true` (one-transmission immediate-suppression call), `coach` hint array, 2 lines | yes |
| 4.1 | STRICT NET | strongpoint | 401 | 360 | `strict: true` | yes |
| 4.2 | TEN METERS | troops | 402 | 330 | `asset: 'mortar60'` (the old `scn: { effR: 30 }` knob is gone — the mortar asset now carries its own FM 7-90 effect-band set, see §4 `CONFIG.EFFECTS`) | yes |
| 4.3 | NO SECOND CHANCES | bunker | 403 | 240 | `diffs: ['hard']` (hard-only) | yes |
| 4.4 | THE MEAT GRINDER | assault | 404 | 330 | `strict: true`, `diffs: ['hard']` | yes |
| 4.5 | DIG IN | qfp | 405 | 600 | `reqPlanned: true`, `scn: { lanes: true }` (single-axis, 3 phase lines in depth), `coach` hint array, 2 lines | yes |
| E.1 | THE GREAT CHOW RAID | chow | 501 | 300 | `tod: 'dusk'`, `coach` hint array, 2 lines | yes |
| E.2 | CLAWS OUT | kaiju | 502 | 420 | `tod: 'dawn'`, `coach` hint array, 2 lines | yes |
| E.3 | SUNLAMP ACTUAL | bunker | 503 | 300 | `asset: 'sunlamp'`, `tod: 'dusk'`, `coach` hint array, 2 lines | yes |
| V5.* | (none — volume has 0 chapters) | — | — | — | locked tease only, "AWAITING AIRCRAFT" | **no** |

All 26 implemented chapters (F.1–F.3, 1.1–1.4, 2.1–2.5, 3.1–3.6, 4.1–4.5, E.1–E.3) carry `impl: true` — the three Epilogue chapters (E.1–E.3) were `impl: false` placeholders in an earlier snapshot of this doc and are now fully implemented, with their own `type`/`seed`/`par`. Every chapter carries `story` (briefing text, optionally TTS'd) and `outro` (one-line AAR narrative beat). F.1/F.2 additionally carry `tut` (step-by-step tutorial engine, suppresses live missions); F.3, 2.5, 3.6, 4.5, E.1, E.2 and E.3 carry `coach` (a short array of hint lines shown alongside the normal live-fire mission and coaching system, not instead of it).

## 2. Scenario templates (`SCN_META`, keyed by `SCN_TYPES`)

`SCN_META`/`SCN_TYPES` now define **15** scenario types (00-config.js:323–346), up from 7. The old fixed `hitsNeed`/`effectRadius` win model is gone entirely (zero matches anywhere in `src/js` — not just moved out of `CONFIG.MISSION`). It is replaced by a graded **% terminal-effect** model (`CONFIG.EFFECTS`, detailed in §4): every HE round adds a distance- and posture-weighted casualty/damage contribution to the target's accumulated effect, using a band set (`rFull`/`rHalf`/`rSupp`, m) keyed by target class (`personnel` vs `point`) and firing asset (`arty` = 155 battery, `mortar60`). A mission passes at `neutralizePct` (SUPPRESSED only counts as a pass when the mission's own intent is `suppress`); `destroyPct` for a DESTROYED verdict. Where a scenario populates `S.friendlies`, zero rounds may land inside that unit's radius or the mission auto-fails to fratricide (`CONFIG.MISSION.fratricideRadius`, 80 m default — some types override it); civilian/hut collateral (`hutCollateralR`/`civCollateralR`, §4) is a separate, universal auto-fail check on every scenario type. **`convoy` is the one exception**, still graded by a literal vehicle-hit count.

| Type | Display name | Setup | Win condition |
|---|---|---|---|
| `strongpoint` | POSITION UNDER ATTACK | Friendly compound (huts, flag, 2 vehicles) placed 1500–3200 m out; enemy cluster (8 troops) offset 150–250 m from it (re-checked ≥800 m from OP), LOS required from OP. | Personnel bands; fratricide zone = compound, r 80 m. |
| `troops` | TROOPS IN THE OPEN | 8-troop cluster in the open, 1500–3200 m out, LOS required (or crest-masked/no-LOS variant, 1200–2600 m, when chapter sets `scn.mask`). | Personnel bands; no fratricide zone. |
| `bunker` | DUG-IN BUNKER | Single bunker mesh + 3-troop cluster on high ground (22–140 m elev band, falls back to 8–999 m), LOS required. | Point bands (neutralize ≥40%, destroy 100%); no fratricide zone. |
| `convoy` | CONVOY — MOVING TARGET | 4-vehicle column, 40 m interval, moving along a scanned coast-road path (1800 m), piecewise time→distance with a seeded 60–180 s pit stop at the nearest fuel/ammo/airfield facility (falls back to a seeded point 35–60% along the route). | Literal vehicle-hit count (not % graded): 3+ of 4 vehicles destroyed before the column reaches the end of the road; column reaches the end with 2+ alive = fail (`escaped`). |
| `assault` | COMBINED ARMS ASSAULT — DANGER CLOSE | 8-troop enemy cluster; friendly squad (6) advances from 900 m out / holds 180 m (or 520 m out / holds 120 m if chapter sets `scn.fClose`) toward the objective over the mission. | Personnel bands; fratricide zone = 6 dynamic points along the advancing friendly line, r 40 m each (not the flat 80 m default). |
| `raid` | VILLAGE RAID — NO-STRIKE ADJACENT | Enemy cluster placed 130–200 m from a real generated village, ≥90 m clear of huts (LOS preferred, falls back to masked variant). | Personnel bands; no fratricide zone — hut/civilian collateral radius is the no-strike enforcement. |
| `wreck` | DERELICT WRECK (TRAINING) | Single derelict landing-craft mesh, 1100–2100 m out, elev 1–6 m, LOS required. Used for Foreword tutorial chapters. | Point bands; no fratricide zone; the placement's own 120 m village-exclusion makes collateral fail effectively unreachable — the "no fail state in tutorial" behavior is structural, not a special case. |
| `battery` | COUNTERBATTERY — GUNS IN ACTION | Friendly firebase 1500–3200 m out, LOS required; enemy gun 1500–3000 m from the firebase (LOS not required), dug-in posture. Enemy fires back: first volley 12–20 s in, then 3 rounds every 24–38 s around the firebase — costs score (`S.btyCas`) but is never an auto-fail. | Point bands; fratricide zone = firebase, r 80 m. Deadline HUD label `VOLLEY`. |
| `defense` | DEFEND THE OP — DANGER CLOSE | Enemy 1000–1600 m from the OP, LOS required; advances on the OP over the mission, pace scaling with suppression/casualties taken. | Personnel bands; fratricide zone = OP, r 80 m. Auto-fail (`overrun`) if the enemy closes to ≤90 m of the OP. Deadline HUD label `WIRE`. |
| `callin` | CALL FOR SUPPORT — ROCK EATERS | Enemy at standard range/LOS; a pinned friendly team 180–350 m from the enemy calls the mission in — their spot report is corrupted (40% clean / 30% flipped cardinal / 30% wrong distance) but the real target grid is unaffected. | Personnel bands; fratricide zone = friendly team, r 80 m. Caller reacts in voice lines on kill/suppress. |
| `occupied` | OCCUPIED SETTLEMENT — NO-STRIKE ADJACENT | 30% of the time: enemy ring 20–40 m around a random facility; otherwise a 3-point ring on a village perimeter arc, each point (and the ring centroid) ≥42 m from every hut. Dug-in posture. | Personnel bands; no fratricide zone — hut/civilian collateral is the no-strike enforcement (village variant only). |
| `qfp` | QUICK FIRE PLAN — H-HOUR | Friendly firebase 1400–2600 m out, LOS required; default: 3 named avenues (ALPHA/BRAVO/CHARLIE) 500–800 m out, ≥300 m apart, enemy assaults one at random; `scn.lanes: true` variant (ch. 4.5): single axis with 3 phase lines at 850/550/250 m. 4–6 min planning window before step-off. | Fratricide zone = firebase, r 80 m. Same overrun-at-≤90 m auto-fail and suppression-scaled pace as `defense`. Deadline HUD label `H-HOUR` pre-step-off, `WIRE` after. |
| `twin` | TWO TARGETS — TRAFFIC BY NUMBER | Two independent 4-troop elements: the first at standard range/LOS, the second 700–1400 m from the first (own LOS/range/village checks). | Personnel bands, graded independently per element — whichever the transmitted grid is nearer to; no fratricide zone. |
| `chow` | THE GREAT CHOW RAID — DANGER CLOSE | BBQ site 1200–2200 m out, elev 2–9 m, LOS required; 8-figure seagull flock offset 150–210 m from it; 3 cooks placed around the site. Chapter-only (E.1). | Personnel bands; fratricide zone = BBQ site, r 60 m (not the flat 80 m default). |
| `kaiju` | CLAWS OUT — MOVING HARD TARGET | Giant crab on a path from just offshore of the nearest village straight out to sea, speed 2.1 m/s; point target class at 2× effect scale (`effScale: 2`, easier to hit) but 1/3 damage contribution (`armor: 3`, harder to hurt). Chapter-only (E.2). | Destroy-only — neutralized does not pass. No fratricide zone. Auto-fail (`escaped`) on landfall. Deadline HUD label `LANDFALL`. |

**Scenario options consumed by chapters (`activeChapter.scn`):**
- `mask: true` — target must have **no** line of sight from the OP (crest-masked; troops only; 2.3).
- `fClose: true` — friendlies start inside 520 m instead of 900 m and hold closer (assault only; 3.5).
- `lanes: true` — `qfp` switches from 3 named avenues to a single axis with 3 phase lines in depth (4.5).
- `effR: <meters>` — scales the effect bands (`S.effScale = effR / 60`); still live in the engine but no current chapter sets it — 4.2's old `effR: 30` was replaced by `asset: 'mortar60'`, which selects the mortar's own band set instead of scaling the arty one.

Every scenario also seeds up to 2 known points (`S.kps`), named from `KP_NAMES`, placed 900–3000 m out with LOS (the second ≥800 m from the first, both ≥400 m from the enemy target), for shift-from-known-point missions.

## 3. World feature set (`buildWorldFeatures()`)

Deterministic per-terrain-seed (`wSeed = terrainSeed ^ hash(DEM name)`), rebuilt on volume change or DEM load.

- **Coast road** — 128-point radial scan for the outermost "low shelf" elevation band (2–9 m) per bearing, gap-filled from neighbors, 2-pass smoothed, closed loop. Basis for facility/village/town placement (`roadAt(fraction)`). A post-smooth "de-water" pass then pulls any centerline point still underwater (<1.0 m elev) inland in 20 m steps, up to 260 m, until dry.
- **Two-tier road system** — asphalt (metalled route between major landmarks — airfield/fuel/ammo/towns — `roadWidth: 8` m carriageway + `shoulderW: 3.2` m pale gravel shoulder each side) vs. dirt (unimproved track to huts/villages/gun/mast/the OP, `pathWidth: 2.8` m, no shoulder). Built as three merged ribbon meshes (shoulder, carriageway, dirt track).
- **Airfield** — flattest low ground (elev 2–14 m); adaptive runway length tried at 620 / 480 / 340 m; heading chosen from 12 candidate headings (30° steps) sampling the full 40 m-wide footprint, requiring ≤7.0 m deviation over the run; places a 40 m-wide runway ribbon + metalled apron/taxiway + 2 hangars + 1 tower/ops building + a windsock mast + 3 fuel drums.
- **Radio mast** — highest ground (elev ≥35 m) that is ≥900 m from the OP (kept off the OP's own hill so it reads as a resection landmark); mast pole + small building.
- **Coastal gun** — headland, elev 8–30 m, with open water ~240 m seaward; gun casemate + barrel oriented out to sea.
- **Fuel point** — on the coast road at a seeded fraction (10–40% along), inland 30 m; building + 3 fuel drums.
- **Ammo depot** — on the coast road at a second seeded fraction (kept ≥40% away from the fuel point, wrapped), inland 34 m; 3 angled storage buildings.
- **Civilian villages** — `WORLD.villageCount` (2) placed on the coast road (center 55 m inland), fractions kept apart from each other and from facilities; 5–7 huts scattered in a 12–54 m radius (a 12 m floor plus up to 42 m span — not a 42 m ceiling), `civsPerVillage` (6) wandering civilian figures per village, wander radius 5–14 m.
- **Towns** *(new settlement tier)* — `WORLD.townCount` (1); site search on elev 3–45 m, 70–340 m from the coast road, ≤24 m deviation over a 6-point sample, ≥900 m from any village and ≥700 m from any facility. A main street runs to the nearest coast-road point (so the spur lands at one end, not through the middle) with a perpendicular cross street; 14–20 buildings (`townBuildings: [14, 21]`) line both streets, 6.5–14 m wide, 30% chance two-storey, masonry material with tiled roofs vs. single-storey thatch for the rest, setback 21–26 m (`townSetback: 21` + jitter) off the street centerline; the whole town is dropped if fewer than 6 buildings actually place. `civsPerTown` (10) wander within a 189 m half-width. Connects to the coast road by a **metalled (asphalt)** spur, unlike a plain village's dirt track. Stored in `WORLD.villages` tagged `tier: 'town'`.
- **Rock outcrops / boulder piles** *(new landmark feature)* — `WORLD.rock` (`count: 14, sep: 460, rMin: 11, rMax: 27, nMin: 4, nMax: 10, named: 3`); candidates scored by slope (≥0.045) × convexity (≥0.4, standing proud of neighbors) on a 100 m grid, elev 18–150 m; kept ≥300 m from the OP, ≥460 m apart, ≥200 m from facilities, ≥220 m from villages, with an 8-sector-around-OP placement quota (guarantees spread) before an unrestricted fill pass to `count`. The 3 highest-scoring rocks are named from a fixed pool (`ROCK_NAMES`, see §4) and join the map's landmark labels. Each rock gets 4–10 boulders (`nMin`–`nMax`) — one dominant centered block plus a ring — vertex-colored from a 5-tone palette. Stored in `WORLD.rocks`.
- **Dirt paths** — connect villages (non-town) and mast/gun facilities in pairs, wavy interior routes toward the first ridge shoulder; a path is discarded if it would cross water.
- **OP watchtower** (`placeOP`) — plain highest-point scan (coarse 120 m grid, then ±120 m refinement at 10 m steps), no elevation floor/ceiling; the observer's elevated post; not part of `buildWorldFeatures` but a fixed per-terrain placement.
- **Firing battery** (`placeBattery`) — 150 m grid scan out to ±3400 m, elev 4–30 m, 1200–2800 m from the OP, flatness ≤3 m over a 40 m step on both axes, scored toward a ~1800 m standoff (falls back to OP+{800,800} if nothing qualifies); the guns' world position; also placed separately, per terrain.

All facility/village/town positions are recorded in `WORLD.facilities` / `WORLD.villages` (towns tagged `tier: 'town'`) and rock outcrops in `WORLD.rocks`, for reuse by scenario generation (no-target-near-village rule, convoy pit-stop targeting, `tutLandmark()` for F.2).

## 4. Named data tables

- **`KP_NAMES`** — `['BREWERY', 'LATRINE', 'SAWMILL', 'CHAPEL', 'DERBY', 'ICEBOX']`. Registered known-point names for shift-from-known-point missions (2.2 and any scenario with `S.kps`).
- **`ROCK_NAMES`** — `['THE TEETH', 'ANVIL RK', 'HOGBACK', 'THE KNUCKLES', 'SPLIT RK', 'WIDOW RK', 'THE MOLARS', 'GRAVEYARD']` (04-world.js). Landmark names spliced at random (no replacement) onto the 3 highest-scoring rock outcrops per terrain (`WORLD.rock.named`, §3).

### QUIPS pools (HELLHOUND FIRES dialogue)

`QUIPS` (07-voice-spot-audio.js:36–290) now has **36** pools, up from 31 — every pool that existed at the last extraction grew (code comment `14a`: "every pool expanded ... to survive a seven-correction mission without repeating"), except the 6 marked unchanged below, and 5 pools are new.

| Pool | Purpose | Lines |
|---|---|---|
| `greet` | Opening net-check line when a session/mission starts | 8 |
| `readbackTail` | Sardonic tag appended after a clean CFF readback | 8 |
| `corrSnark` | Remark on a large/odd correction | 8 |
| `ffeAck` | Acknowledges FIRE FOR EFFECT | 6 |
| `completeTail` | Tag after ROUNDS COMPLETE | 6 |
| `eomGood` | End-of-mission, target neutralized/destroyed | 6 |
| `eomBad` | End-of-mission, target survived / mission abandoned | 6 |
| `badGrid` | Grid doesn't parse / off the map | 6 |
| `water` | Target grid lands in the ocean | 4 |
| `noMission` | Observer sent a correction with no mission active | 7 |
| `inFlight` | Observer keyed up while rounds are still in the air | 4 |
| `unknown` | Unparseable transmission | 7 |
| `careerFrat` | Pre-mission callback referencing the player's fratricide history | 3 |
| `careerCollat` | Pre-mission callback referencing collateral-damage history | 3 |
| `careerVet` | Pre-mission callback for an experienced/returning player | 3 |
| `complete` | Prompts the observer that the mission is ready to close | 1 (unchanged) |
| `suppressedOnly` | **New.** Post-FFE result: target suppressed but not neutralized/destroyed — prompts continue or end | 2 |
| `noEffect` | **New.** Post-FFE result: no appreciable effect observed | 2 |
| `dangerClose` | Danger-close proword omitted near friendlies | 3 |
| `wrongWay` | Corrections moving the burst away from the target | 3 |
| `rantLost` | Observer transmission has no recognizable CFF content | 5 |
| `coachTimid` | Coaching: correction too small relative to observed miss | 2 (unchanged) |
| `coachBracket` | Coaching: all rounds on the same side, no bracket established | 2 (unchanged) |
| `coachBracketGood` | Coaching: bracket achieved, prompts halving | 2 (unchanged) |
| `coachStagnant` | Coaching: correction didn't move the burst | 2 (unchanged) |
| `coachLoc` | Coaching: first-round location error exceeds 200 m standard | 1 (unchanged) |
| `snide` | Generic one-liner for a stupid-but-safe malformed call | 5 |
| `snideRound` | One-liner when the FDC auto-rounds sloppy correction numbers | 4 |
| `nearCiv` | Advisory: target grid sits close to a village | 3 |
| `unsafeCorr` | Rejects a correction that would walk fire onto friendlies/civilians | 3 |
| `unsafeInsist` | Fires anyway after the observer insists on an unsafe correction | 3 |
| `spotSrc` | **New.** Source-attribution line for a spot-report cue (who relayed it) | 6 |
| `spotTail` | **New.** Closing flavor line appended after a spot report | 6 |
| `spotNone` | **New.** Mission-start line when no spot-report cue is available | 2 |
| `rantCiv` | Full rant sequence for civilian collateral damage — mission-ending | 3 sequences × 3 lines |
| `rantFrat` | Full rant sequence for fratricide — mission-ending | 4 sequences × 3 lines |

### Commendations (AAR medal strings)

Still exactly 8 (13-aar.js:354–362), all gated behind a passed mission (`if (pass)`). Only one has changed since the last extraction — `GODDAMN HOLE IN ONE` is now **`HELL OF A HOLE IN ONE`** (trigger condition unchanged). None of the newer mission mechanics (`reqIllum`, `reqImmediate`, `reqPlanned`, the HACKSAW 60mm section beyond 4.2, SUNLAMP, or the night/dawn/dusk/overcast `tod` chapters) earned their own commendation.

- `HELL OF A HOLE IN ONE` — effect after a single adjusting round (`m.adjustRounds <= 1`).
- `EAGLE EYE` — initial location within 50 m (`m.aimErr0 <= 50`).
- `ACTUALLY READ THE MANUAL` — clean call format, zero coaching notes (`m.notes.length === 0`).
- `FAST MOVER` — finished in ≤60% of chapter par, or 300 s outside a chapter.
- `IRON NET` — chapter sets `strict: true` (currently 4.1, 4.4 — both Volume IV, but the check itself is generic on the flag).
- `BY THE BOOK` — closed with refinement and/or record-as-target (`m.refined || m.recorded`, full RREMS).
- `ORIENTED` — sent OT direction to the FDC (`m.otDirSent`).
- `TEN-METER MAN` — chapter sets `asset: 'mortar60'` (currently only 4.2, but the check is generic on the asset field).

### `CONFIG` tunables, grouped by system

Listed in source declaration order (00-config.js:9–248, plus `CONFIG.AMMO` assigned separately at :311–314). Six systems are entirely new since the last extraction (`UAS`, `EFFECTS`, `NAV`, `SPOT`, `OPTICS`, `AMMO`); `MISSION` lost its old flat hit-count keys to `EFFECTS`; `CAMERA`, `TERRAIN`, `FDC`, `WORLD`, `GFX` each gained or changed keys — see inline notes.

- **`SEED`** — `terrain: 1337` (default), `mission: 1` (overridden per-chapter/skirmish). Unchanged.
- **`MAP`** — `size: 10000`, `originE: 20000`, `originN: 50000` (SW-corner easting/northing, meters). Unchanged.
- **`TERRAIN`** — `meshSegments: 300`, `islandRadius: 3900`, `radiusWobble: 800`, `ridgeHeight: 95`, `massifHeight: 165`, `massifRadius: 950`, `massifX: -520`, `massifZ: -360`, `oceanFloor: -30`, `demRes: 512`, `demMaxElev: 240`, `demSeaLum: 10`, plus **new** `patchSize: 1200`, `patchDiv: 4` (near-field terrain LOD patch re-sampled over the mission target area — replaces the old `GFX.nearLOD`/`nearLODSize`/`nearLODSeg` keys, which no longer exist).
- **`CAMERA`** — `fov: 60`, `eyeHeight: 2.2`, `towerHeight: 300` (300 m OP watchtower — the old doc's "50 ft" annotation was wrong even at the time; the code comment has always said 300 m), `mouseSens: 0.0022`, `pitchClamp: 1.35`. The old single `binoFov: 9` key is **gone**, replaced by a 3-power zoom system: `binoZooms: [4, 7, 14]` (magnification steps cycled with [Z]/mouse wheel — FOV is derived per-power as naked-eye FOV / power, not authored directly), `binoZoom: 1` (index into `binoZooms`, default 7×), and **new** `swayMil: 0.65` (hand-held sway amplitude in mils; 0 disables).
- **`UAS`** *(new)* — small-UAS picture-in-picture feed camera: `alt: 520`, `radius: 420`, `angSpeed: 0.045`, `slew: 70`, `fov: 34`, `pipW: 0.30`, `pipH: 0.34`.
- **`BALLISTICS`** — `tofDivisor: 300`, `tofBase: 8`, `tofMin: 15`, `tofMax: 40`, `splashLead: 5`; `firstRound` dispersion (uniform, m) `easy [40,90] / normal [60,150] / hard [90,220]`; `followUp` dispersion (1-sigma, m) `easy {range:12, defl:7} / normal {18,10} / hard {26,15}`; `ffeRounds: 6`, `ffeStagger: [0.25, 0.9]`; plus **new** `dispersion: true` (SHIFT+D test-aid toggle — `false` forces every round onto the aimpoint; such a mission is explicitly ungraded, no stars, AAR/TLOG flag it).
- **`MISSION`** — `fratricideRadius: 80`, `passMaxAdjustRounds: 4`, `targetRange: [1500, 3200]`, `enemyOffset: [150, 250]`, plus **new** `skirmishRange: [800, 4200]` (skirmish-only OT band, area-uniform island draw; chapters keep the fixed-seed `targetRange` annulus) and `dangerClose: { battery: 600, mortar60: 250 }` (per-asset danger-close distance). The old flat `effectRadius: 60` / `hitsToNeutralize: 3` keys are **gone**, superseded by `EFFECTS` below.
- **`EFFECTS`** *(new)* — graded terminal-effect model, replacing the old fixed hits-needed/effect-radius pair: `suppressSec: 90` (suppression outlives the last round by this); `screen: { radius: 120, sec: 90 }` (smoke-obscuration suppression); `illumBurn: [50, 130]` (per-flare burn time, s, seeded per flare); `posture: { standing: 1.0, prone: 0.4 }`. Per-asset band sets, each split `personnel`/`point` with `rFull`/`rHalf`/`rSupp` (m, full/half/quarter-credit effect radius), `perRound` (% contribution at full effect), `neutralizePct`, `destroyPct`: `arty.personnel { rFull:30, rHalf:50, rSupp:75, perRound:8, neutralizePct:10, destroyPct:30 }`, `arty.point { rFull:15, rHalf:25, rSupp:45, perRound:55, neutralizePct:40, destroyPct:100 }`, `mortar60.personnel { rFull:20, rHalf:35, rSupp:50, perRound:2, neutralizePct:10, destroyPct:30 }`, `mortar60.point { rFull:10, rHalf:18, rSupp:35, perRound:25, neutralizePct:40, destroyPct:100 }`.
- **`NAV`** *(new)* — `declEastDeg: 7` (magnetic declination — instruments read magnetic, the grid/net is true; `magnetic = grid - declination`).
- **`FDC`** — `obs: 'MUSTANG 12'`, `fdc: 'HELLHOUND FIRES'`, `readbackDelay: 1.3`, `shotDelay: 4.0`, plus **new** `fdc60: 'HACKSAW FIRES'` (the 60mm mortar section's own callsign, Volume IV precision shoots) and `fdcSun: 'SUNLAMP ACTUAL'` (Epilogue E.3's orbital fire-direction center).
- **`VOICE`** — `lang: 'en-US'`, `ttsLang: 'en-IN'`, `rate: 1.05`, `pitch: 0.8`, `volume: 1.0`, `ttsEnabled: true`, `preferredVoices` (ordered match list for FDC TTS voice selection). Unchanged.
- **`GAMEPAD`** — `lookSpeed: 2.8`, `deadzone: 0.16`. Unchanged.
- **`SPOT`** *(new)* — spot-report cue system (tells the observer where to look, not where the target is): `enabled: true`, `lead: 2.4`, `gap: 2.9`, `saluteChance: 0.7`, `landmarkMax: 2600`.
- **`OPTICS`** *(new)* — DAY/NVG/THERMAL overlay, cycled with [O]: `enabled: true`, `hz: 24` (overlay redraw rate, not the frame rate), `cvW: 320`, `cvH: 180` (overlay backing-store resolution), `grain: 0.16` (NVG scintillation amplitude), `haloR: 26`, `nvgBloomAt: 0.35`, `thermalWashAt: 0.30` (ambient thresholds where each device starts degrading — both stay available at every time of day).
- **`UI`** — `laseHold: 6` (seconds). Unchanged.
- **`WORLD`** — `roadWidth: 8` (was 7), `shoulderW: 3.2` (**new** — asphalt shoulder width each side), `pathWidth: 2.8` (was 3.2), `villageCount: 2`, `civsPerVillage: 6`, plus **new** town-tier keys `townCount: 1`, `townBuildings: [14, 21]`, `townR: 135`, `civsPerTown: 10`, `townStreetHalf: 135`, `townCrossHalf: 88`, `townSetback: 21`, and **new** `rock: { count: 14, sep: 460, rMin: 11, rMax: 27, nMin: 4, nMax: 10, named: 3 }` (rock-outcrop feature, §3); `convoyStopDur: [60, 180]`, `hutCollateralR: 24`, `civCollateralR: 28` unchanged.
- **`GFX`** — visual-overhaul flags (stage 13 / GRAPHICS.md), each independently toggleable: `toneMap`, `exposure: 1.15`, `satComp: 1.0`, `sky`, `veg`, `vegBudget: 4000`, `vegRadius: 2600`, plus **new** `vegMaxH: 6` (canopy height cap, so foliage can never hide a burst plume); **new** legibility system `legibility`, `legFloorMil: 1.8` (minimum angular height a distant figure is scaled up to hold), `legMaxScale: 2.5`, `legOnset: 800` (m — never rescale a figure closer than this), `contactShadow`; `hillshade`, `aoStrength: 0.35`, plus **new** `hillFloor: 0.55`; `water`; `craters: 40` (was `0` — now a live persistent-crater ring buffer, not a placeholder); `bloom` (SUNLAMP-only, Epilogue). The old `nearLOD`/`nearLODSize: 2400`/`nearLODSeg: 300` keys are **gone** — see `TERRAIN.patchSize`/`patchDiv` above.
- **`AMMO`** *(new, assigned after the main `CONFIG` object)* — per-session ammunition stock, decremented per round actually fired, replenished on chapter launch (skirmish carries the running stock): `b155: { he: 240, smoke: 48, illum: 36 }`, `m60: { he: 96, smoke: 24, illum: 24 }`.

## 5. Assets to author in-engine

Everything below has **no authored asset today** — it is procedurally generated (low-poly `BoxGeometry` primitives, noise-driven terrain, or synthesized audio/voice) and must be authored or substituted when porting:

- **Terrain** — procedural heightfield (`H(x,z)`, ridged fBm + volcanic massif + coral beach shelf) or an optionally-loaded grayscale DEM image; no terrain meshes/textures exist as assets.
- **Units** — troops, vehicles (4-vehicle convoy, strongpoint pair, derelict landing craft), the friendly assault squad: all single untextured boxes (`BoxGeometry`) with flat materials — no character/vehicle models, rigs, or animation.
- **Structures** — huts, bunker, airfield buildings, radio mast, coastal gun casemate, fuel/ammo depot buildings, town buildings, rock-outcrop boulder piles, OP watchtower, firing battery emplacement: all boxes/simple primitives, no modeled structures.
- **Civilians** — wandering villager/townsfolk figures: single box mesh (`_civGeo`) per civilian, no character model or walk animation (position-lerped wander only).
- **Burst/impact effects** — flashes, smoke puffs, flames, debris cubes: primitive meshes and particle-less sprite/box effects, no VFX assets (no particle systems, no impact decals beyond code-driven color/scale changes).
- **Roads/paths** — procedural ribbon meshes (`ribbonMesh`/merged ribbons) following a scanned/smoothed coastline or waypoint chain, now in two classes (asphalt-with-shoulder vs. dirt, §3); no road/path textures or meshes.
- **Audio** — 100% synthesized via Web Audio API at runtime (noise-buffer booms, biquad-filtered squelch, oscillator-based low end); zero audio asset files (no SFX libraries, no music).
- **Voice** — both the FDC (HELLHOUND FIRES) and player transmissions run through the browser's Web Speech API (`SpeechSynthesis`/`SpeechRecognition`), keyless and client-side; no voice-actor recordings exist for any line in `QUIPS`, chapter `story`/`outro` text, or tutorial dialogue.
- **UI chrome** — map sheets, reticle overlay, menu/bookshelf theme, AAR screen: all DOM/CSS/canvas-drawn, no UI art assets.

These are the items an engine team must budget for as new production (models, textures, VFX, SFX, VO) rather than data migration — everything in §1–§4 above is a straightforward data port.
