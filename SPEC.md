# SHITFIRE — FORWARD OBSERVER (CALL-FOR-FIRE) TRAINER — ONE-SHOT PROMPT

> Feed this to a coding agent (Claude Code / Cursor). Fill in the **PARAMETERS** block first — every `[Q#]` in the spec body pulls from it. Anything you leave blank, the agent should resolve using the stated default.

---

## PARAMETERS TO FILL IN (answer before running)

- **Q1 — LOCATION METHODS (v1):** grid only / grid + polar / all three (grid, polar, shift-from-known-point). → `[FILL IN]` _(default: all three — grid, polar, shift-from-known-point)_
- **Q2 — FIRE SUPPORT ASSET(S) & MUNITIONS:** → **155mm HE, PD fuze only** for the base sim; the campaign's Volume IV adds **60mm mortars** (8-digit/10 m grids per Q4). **Surface-to-surface fires ONLY. CAS (9-line) is a future locked volume — do not build it until explicitly asked.**
- **Q3 — ENVIRONMENT / BIOME:** → **WW2 Pacific island chains** — jungle, volcanic rock, coral beaches, surrounding ocean. Terrain is to be driven by **real heightmap/DEM data fed in during development** (e.g. Iwo Jima, Peleliu, Saipan, Tarawa); a procedural island stand-in is used for the first pass. See TERRAIN.
- **Q4 — MAP SIZE & GRID SCHEME:** → **10 km × 10 km.** Grid precision follows the fire-support asset: **artillery = 6-digit (100 m); 60mm mortars = 8-digit (10 m).**
- **Q5 — TARGET SCENARIO(S):** → **Multiple scenario templates** drawn from common FO situations: a position/compound under attack; a moving convoy; a combined-arms assault on an objective **with friendly elements on the field** (danger close / fratricide avoidance); plus simpler ones (troops in the open, dug-in bunker). See SCENARIOS.
- **Q6 — INPUT MODE:** → **voice (Web Speech API) + typed fallback.** _(Voice is Chrome/Edge desktop only.)_
- **Q7 — DOCTRINE STRICTNESS:** → **forgiving keyword parse, with a "strict mode" toggle.**
- **Q8 — DIFFICULTY:** → **The first round deviates randomly within a set range; follow-up rounds have significantly reduced deviance.** Easy/Normal/Hard scale both. See BALLISTICS.
- **Q9 — ADJUSTMENT DOCTRINE:** → **free adjust; bracket hints shown in Easy.**
- **Q10 — CALLSIGNS & FDC VOICE:** → observer **"MUSTANG 12"**, FDC **"HELLHOUND FIRES"**. Tone: **dark-humored, dry, slack-jawed and sardonic — and pointedly so when the call-in is wrong.**
- **Q11 — SCORING / AAR:** → **grade rounds-to-effect, time, format correctness, and first-round accuracy; pass = target neutralized in ≤4 adjusting rounds.** Fratricide = automatic fail.
- **Q12 — PROGRESSION:** → **a story-driven CAMPAIGN of Volumes and Chapters (see CAMPAIGN and NARRATIVE.md) with 0–5 star grading per chapter, PLUS a random mission generator ("Skirmish" mode).**
- **Q13 — APP NAME:** → **SHITFIRE**
- **Q14 — MAP SHEET SCALE & PAPER:** printable-map scale, contour interval, and paper size. → **1:50,000 full-island sheet on US Letter, contour interval 10 m (index contour every 50 m); option to print 1:25,000 quadrants.** _(Adjust scale/paper here if desired.)_

---

## THE PROMPT

Build me a complete, polished browser training simulator called **[Q13]** in ONE single `index.html` file. It is a **forward observer / call-for-fire trainer**: the player observes a target from an observation post (OP), speaks (or types) a doctrinal call for fire, an AI fire-direction center (FDC) responds over the radio, rounds impact with realistic scatter, and the player observes and calls corrections until the target is neutralized. **This trains the OBSERVER, not the gun crew** — the firing battery is a black box. It also ships a **printable topographic map library** whose sheets correlate exactly to the sim terrain, so users can print a map and do real mapwork — plotting, measuring direction and distance — by hand alongside the sim. Everything must be self-contained and hand-written except Three.js and its import-map addons (OrbitControls not needed), loaded via CDN import map. No audio files, no build step, no API keys, no backend. The only optional external input is a grayscale terrain heightmap/DEM (see TERRAIN); everything else is generated in code. Target 60 fps.

### SINGLE FILE & TECH

One `index.html`. Three.js via import map. All 3D is procedural low-poly, flat-shaded. All audio is Web Audio API (synthesized). All voice I/O uses the browser's built-in Web Speech API (`SpeechRecognition` for input, `SpeechSynthesis` for the FDC) — **no cloud services, no keys, zero marginal cost per user** [Q6]. A `CONFIG` object at the top holds every tunable constant (map size, dispersion, offsets, TOF, callsigns, difficulty), grouped by system. The scenario is driven by a seeded PRNG so a given seed reproduces the same target/terrain; round-to-round dispersion draws from that same stream so replays are deterministic.

### BUILD ORDER — MVP FIRST (build in this sequence; ship each stage working before the next)

This spec is large. Do NOT attempt it all at once. Build behind stable interfaces — `H(x,z)`, `fireMission(targetLocation)`, `applyCorrection(otFrameDelta)`, `FDC.say(msg)`, `Scenario` — so later stages slot in without rewrites.

1. **Core loop (typed, grid-only, procedural island).** Procedural stand-in island via `H(x,z)`; first-person OP camera; ONE static target (Position-under-attack, Normal); typed input only; rule-based parser for GRID missions; direct-impact model (first-round wide / follow-up tight); OT-frame corrections; shot/splash timing; burst effect; comms log; AAR. Get `observe → locate → transmit → observe impact → correct → fire for effect → assess` working end to end. **This is the MVP — make it solid before anything else.**
2. **Voice layer.** Add Web Speech push-to-talk input + `SpeechSynthesis` FDC on top of the working typed core (typed stays as fallback).
3. **Location methods.** Add polar, then shift-from-known-point.
4. **FDC personality + audio polish.** Dark-humor quip pool, radio hiss/squelch, distance-delayed boom.
5. **Scenarios.** Add convoy (moving target), combined-arms-with-friendlies (danger close + fratricide fail), troops/bunker; scenario menu, difficulties, random generator.
6. **Printable map library.** Correlated topo sheet (contours via marching squares over `H`), grid + marginal data, print/save, answer-key toggle.
7. **Real DEM ingestion + polish.** Swap real island heightmaps in behind `H(x,z)`; refine binocular reticle; adaptive quality; mobile controls.
8. **World detail & population.** Seeded, terrain-aware structures/roads/dirt-paths/landmarks (military: ammo depot, fuel point/gas station, airfield strip, radio mast, coastal-gun emplacement, watchtower; civilian: huts, villages) as low-poly flat-shaded vertex-colored instanced/merged geometry (60 fps rule stands); permanent structures + roads documented on the printable map sheets and the `[M]` map with symbols + a legend for terrain association (enemy positions still never plotted); civilian villages populated with wandering civilian figures rendered distinctly — **civilian casualties are an automatic mission fail, same as fratricide**; convoys make seeded **1–3 minute pit stops** at gas stations/ammo depots/airfields along their route; **FDC deviation policy** — dangerous deviations (danger close w/o proword, corrections walking toward friendlies/civilians, unresolved-unsafe FFE) trigger the rant system, stupid-but-safe deviations (wrong element order, malformed-but-unambiguous calls, absurd rounding) get a snide remark and the mission proceeds; strict mode still enforces format for grading. See WORLD DETAIL & POPULATION.
9. **Campaign skeleton — volumes, chapters, stars.** Chapter data model (fixed seed + scenario config + island + brief text per NARRATIVE.md); bookshelf mission menu (volumes as spines, chapters with title/blurb/lock state); `gradeMission(metrics) → stars` (0–5, difficulty-capped Easy 3 / Normal 4 / Hard 5) shown MW2-style in the menu and AAR; sequential chapter unlocks + star-gated volume unlocks; persistence via the existing best-score store; `[N]` becomes Skirmish mode.
10. **Narrative layer — Foreword through Volume IV.** Skippable chapter briefings (text + `SpeechSynthesis`); Foreword tutorial chapters on the hint pipeline; per-volume island assignment behind `H(x,z)`; Volume IV strict-mode forcing + 60mm mortar chapters (8-digit); named KPs; continuity quips + AAR commendations; sprinkled per-chapter humor per NARRATIVE.md's humor rules.
11. **Epilogue + campaign polish.** The three SUNBURN chapters, including the SUNLAMP directed-energy mission (same direct-impact model — only TOF pacing, prowords, beam visual, and audio differ); locked "Volume V: ON WINGS" CAS tease in the menu; balance pass on star pars.
12. **FO skill depth & training fidelity.** A session-transcript review found the sim grades outcomes without teaching the two core FO skills it exists to train — target location and adjustment doctrine. Close the gap:
    - **In-mission adjustment coaching.** From data already recorded per round (observed miss, correction called, resulting miss), detect a **timid correction** (correction magnitude far under the observed miss), a **failure to bracket** (every round landing on the same side of the OT line), and a **stagnant round** (miss barely changes round to round). On Easy/Normal, have the FDC deliver a short, in-voice doctrinal nudge when one triggers (off on Hard — no hand-holding at the top difficulty). At EOM, add to the AAR a round-by-round **miss trace**, a **correction-efficiency** figure (see COACHING & TRAINING FIDELITY), and a plain-language diagnosis of what went wrong ("you kept splitting a small fraction of the miss — walk it further").
    - **Target location support.** Add a **mil-relation range estimation** drill (known object size ÷ mils subtended = range) as an alternative to the laser, plus a **degraded-optics / dead-laser** condition (see item 7) that forces its use. Report the AAR's initial-location error as a **vector** (direction + distance), not a bare scalar, so systematic bias (always long, always left) is visible across missions.
    - **Mil reticle → OT-factor workflow.** Wire the reticle to the correction math: OT distance ÷ 1000 = meters per mil; the player measures burst deviation off the target in mils through the reticle, multiplies by the OT factor, and sends that as the correction. Easy mode's spotting hint changes from handing over the finished correction to showing the **measured mils**, so the player still does the OT-factor math.
    - **Consequence for slow fire.** Targets that survive repeated near misses over an extended engagement start to **scatter or displace**, giving the ≤4-adjusting-round standard a cost beyond lost stars.
    - **Doctrinal flow gaps vs DOCTRINE.md.** Require **OT direction** before or with the first correction on grid missions (already specced for strict mode in stage 1's FDC flow — extend the requirement/prompt to forgiving mode as a coaching nudge); add **RREMS refinement** as a real end-of-mission step, not just record-as-target/surveillance-term; add an **"at my command"** fire-control hold before FFE; add an **immediate suppression** mission type; add **smoke** and **illumination** mission types (impact-effect hooks already exist per IMPACT EFFECTS).
    - **Assessment fidelity.** `gradeMission()` gains two doctrinal metrics it currently omits: **time-to-initiate** (JFO standard: CFF complete within 2 minutes of target ID) and **correction efficiency** (each correction should roughly halve the prior miss — see COACHING & TRAINING FIDELITY).
    - **Dynamic environment.** Replace the permanent-noon, no-wind default with a **time-of-day** and **wind** model: wind drifts smoke (making it readable as a tool, not just an effect) and a subset of missions run at **night/illumination** or degraded visibility, so observation is not uniformly easy.

**Future (do NOT build until asked): CAS / 9-line (Volume V).**

If time or context runs short, a fully working stage 1–2 is far more valuable than a broken attempt at all of them.

### VISUAL IDENTITY — "ROBLOX-LEVEL" IS FINE

Bright, clean, low-poly, flat-shaded. Chunky geometry, no textures, vertex-color everything. Readable at a glance over realism. **WW2 Pacific island palette** [Q3]: deep ocean blue `#1E5A7A`/`#2E7DA0` ringing the island, coral/sand beach shelf `#D8C89A`, jungle greens `#4E7A3D`/`#3B5E2E`, volcanic rock `#5A544E`/`#6E665C`, pale tropical sky `#BFE0EA` at zenith fading to humid haze `#E8EDDA` at the horizon. Harsh high midday sun (elevation ~55°), gentle ambient fill, heavy heat-haze fog on the far ocean so the map edge dissolves into sea and sky. Everything crisp; no post-processing required beyond optional mild bloom on the impact flash.

### TERRAIN — HEIGHTMAP-DRIVEN, NO PHYSICS ENGINE

Terrain height `H(x,z)` is driven by a **grayscale heightmap/DEM of a real WW2 Pacific island**, supplied during development [Q3] (e.g. Iwo Jima with Suribachi, Peleliu's Umurbrogol ridges, Saipan, Tarawa). Sample the DEM to build a low-poly flat-shaded mesh across the [Q4] **10 km × 10 km** area (~8–15 m triangles near the OP, coarsening with distance / LOD rings), ringed by ocean. **For the ZERO-ASSET first pass (no DEM yet), synthesize a procedural stand-in island** from seeded value-noise — a central volcanic massif, jungle-covered ridge fingers, and a coral beach shelf dropping into surrounding water — so the one-shot runs immediately and a real DEM can be swapped in later behind the same `H(x,z)` interface. Either way, expose `H(x,z)` plus a ray-vs-heightfield lookup (march the ray, sample `H`) used for: impact resolution, laser rangefinder returns, and line-of-sight / crest-masking checks (can the observer even SEE a given point). The OP sits on high ground (a ridge or Suribachi-like feature) overlooking the beaches and inland valley. No rigid-body physics anywhere — rounds are never simulated in flight (see BALLISTICS).

### WORLD DETAIL & POPULATION — STRUCTURES, ROADS, VILLAGES

Beyond the terrain mesh, each island carries a seeded, terrain-aware layer of static and populated detail so the world reads as inhabited, not just textured. Built in BUILD ORDER stage 8, on top of the finished terrain/DEM pipeline.

- **Structures & landmarks:** placed by the same seeded PRNG that drives the scenario, snapped to `H(x,z)` and oriented to local slope. A mix of **military** (ammo depot, fuel point/gas station, airfield strip, radio mast, coastal-gun emplacement, watchtower) and **civilian** (huts, sheds, a dock) landmarks. Low-poly, flat-shaded, vertex-colored; geometry is instanced/merged per type so the 60 fps / no-per-frame-allocation rule holds regardless of island density.
- **Roads & dirt paths:** a simple seeded network — metalled roads between major landmarks, dirt paths to huts and the OP — laid down as ribbon geometry following `H(x,z)`; convoys and civilian wander paths route along it.
- **Terrain association via the map:** every permanent structure and road segment is drawn on the printable map sheet (see PRINTABLE MAP LIBRARY & MAPWORK) and on the in-sim `[M]` map, with a small set of map symbols and a legend, so the FO can resect/orient off known landmarks ("the airfield," "the radio mast," "the village") the way a real observer would. **Enemy positions are never plotted** — only the permanent, always-there world detail.
- **Civilian villages:** hut clusters near roads/water, populated with a handful of wandering civilian figures (simple seeded waypoint wander), rendered distinctly from military/enemy models (different palette/silhouette) so they read unambiguously as non-combatants. **Civilian casualties (collateral damage) are an automatic mission fail, identical to fratricide.**
- **Convoy pit stops:** moving-convoy missions (see SCENARIOS) make **seeded 1–3 minute stops** at a gas station, ammo depot, or airfield along their route before continuing — the timing is part of the scenario seed, not random per playthrough. This teaches catching a column halted at a facility versus leading a moving target, and plays directly off the structures placed by this system.

None of this changes the direct-impact ballistics model or the OT-frame correction math — it is a world-dressing, terrain-association, and target-discrimination layer only. See FDC for the deviation-policy rant/snide split this stage also introduces.

### PRINTABLE MAP LIBRARY & MAPWORK

Every terrain in the library has a matching **printable topographic map sheet generated from the SAME `H(x,z)`** used by the 3D world, so the paper map and the sim world are guaranteed to correlate — a grid the user reads off the printout plots to the same point the sim's rangefinder/grid readout reports.

- **Render the sheet** as a top-down 2D map (SVG or canvas): **contour lines** derived by marching `H` at the [Q14] interval (10 m, index contour every 50 m, index lines labeled with elevation), water/coastline fill, spot elevations on peaks/ridges, a labeled **grid** matching [Q4] (10 km × 10 km, numbered grid lines, grid-square references at the precision set by the asset), the **OP** marked, any **known / registration points** for shift missions, and **permanent structures + roads** (see WORLD DETAIL & POPULATION) drawn with map symbols.
- **Marginal information like a real map sheet:** title (island name + scenario), **scale bar** and representative fraction per [Q14] (default 1:50,000), a **north arrow with grid-magnetic declination diagram**, a grid-reference guide, and a **symbol legend** covering the structure/road symbols above. Style it monochrome/line-art so it **prints cleanly on a black-and-white printer**.
- **NO enemy positions plotted** — permanent structures and roads ARE shown (that's the terrain-association exercise); the sheet is otherwise a clean map the user plots on by hand (that IS the mapwork). Provide an optional **"answer-key" toggle** that prints a second version with the target/known solution for self-check.
- **Map Library screen:** lists every available map with a thumbnail; selecting one shows the full sheet with **Print** (browser `window.print()` driving a dedicated `@media print` stylesheet sized to [Q14] paper — the sheet fills the page, marginal data in the margins, everything else in the app hidden) and **Save** (export the sheet as PNG via canvas, and/or let the user "Save as PDF" through the browser print dialog). Offer the **1:25,000 quadrant** option [Q14] for a larger-scale print of one quarter of the island.
- New island DEMs added to the terrain library automatically get a correlated sheet, since both come from the same heightfield — the library grows with the maps.

Reachable via **[P]** or a menu button; opens the sheet for the currently loaded map.

### THE OBSERVATION POST & CAMERA

First-person from the OP. Mouse-look (yaw/pitch), clamped pitch. The observer does not walk far — small reposition at most. Show a subtle horizon-referenced heading readout in **mils (0–6400)**, since the observer thinks in mils. Default FOV ~60°; narrows dramatically in optics (see TOOLS). Head-bob off; this is a stable observation platform.

### OBSERVER TOOLS (this matters more than graphics)

- **BINOCULARS [B]:** overlay optic. Narrow FOV (~7× → ~9° FOV). Draw a proper **mil reticle**: horizontal and vertical scales graduated every 5 mils with 10-mil numbered ticks, so the observer can measure angular deviation of a burst from the target. Vignette the edges. Heading in mils shown on the reticle.
- **LASER RANGEFINDER [L]:** while in binos, lasing returns the range in meters to whatever the center reticle is on (ray-march `H`), plus that point's grid per [Q4]. Brief "LASING…" then a range/grid readout.
- **COMPASS / DIRECTION:** current look azimuth in mils always available; this is the observer's measured direction to target/burst.
- **MAP [M]:** simple top-down schematic — OP position, known points (for shift missions if [Q1] includes them), grid lines, north arrow, and permanent structures/roads with their symbols (see WORLD DETAIL & POPULATION) for terrain association. No enemy positions revealed; the player must locate targets themselves. The full, print-ready version of this map is the sheet in the **Map Library** (see PRINTABLE MAP LIBRARY & MAPWORK).

Together these are how the observer LOCATES the target (grid/polar/shift per [Q1]) and SPOTS corrections. Make them accurate and legible.

### CALL FOR FIRE — INPUT & PARSER

Push-to-talk: hold **[Space]** to transmit, release to send [Q6]. Show a live transcript; always provide a typed input box as fallback/override (Web Speech is imperfect and Chrome-only). **DOCTRINE.md (distilled from JFIRE 2019 + the JFO Student Handout) is the authority for exact formats, prowords, rounding standards, and readback protocol.** Doctrinally the CFF is **six elements in three transmissions, each read back by the FDC** (ID+warning order → target location → description/engagement/control); forgiving mode accepts it all in one breath, **strict mode enforces the transmission structure, element order, rounding (direction to 10 mils, polar distance to 100 m, shift lateral to 10 m), and the "CORRECTION" proword for fixing errors.** Parse into the doctrinal elements, tolerant of loose phrasing per [Q7]:

1. **Observer ID + warning order** — callsign, "adjust fire" / "fire for effect" / "suppress", and the method of target location.
2. **Target location** — per [Q1]:
   - **Grid:** "grid 123456" → coordinates. Grid precision follows the fire-support asset per [Q4] — artillery uses a 6-digit / 100 m grid; 60mm mortars use an 8-digit / 10 m grid.
   - **Polar:** "direction 5400, distance 2000" → bearing (mils) + range (m) from the OP.
   - **Shift from known point:** "from AB1001, right 200, add 400" → offset from a registered point.
3. **Target description** — free text ("three vehicles in the open").
4. **Method of engagement** — munition/fuze per [Q2]; "danger close" flag.
5. **Method of fire and control** — "at my command" / "fire for effect" / "when ready".

Recognize spoken numerals, "left/right", "add/drop", "direction/distance/grid", and common prowords ("over", "out", "say again", "shot", "splash"). In strict mode [Q7], reject or query malformed calls the way a real FDC would ("MUSTANG 12, HELLHOUND FIRES, say again your target location, over").

### FDC — RULE-BASED, SPOKEN (no LLM required)

The FDC is deterministic JS logic, voiced by `SpeechSynthesis` with a slightly radio-filtered delivery (see AUDIO). Callsigns per [Q10]. Flow, each transmission acknowledged over the net:

1. **Readback of each CFF transmission**, then the **Message to Observer (MTO)** per DOCTRINE.md, opening with the proword "Message to observer": units to fire, changes (if any), rounds per tube, and an assigned **target number** (e.g. "Message to observer, battery, 4 rounds, target AB2001, over"). In strict mode require the observer to read the MTO back, and require **OT direction before or with the first correction** on grid missions.
2. **"SHOT, OVER"** at the moment of firing.
3. **"SPLASH, OVER"** ~5 seconds before impact.
4. After the observer's correction: brief acknowledgement, then repeat 2–3.
5. On "fire for effect": fire the effect volley, then **"ROUNDS COMPLETE, OVER"**, then end of mission per **RREMS** — refinement (optional), record as target (optional), "end of mission," and the observer's surveillance using the doctrinal terms **suppressed / neutralized / destroyed** (see DOCTRINE.md; strict mode grades the term choice).

Callsigns per [Q10]: observer **MUSTANG 12**, FDC **HELLHOUND FIRES**. The FDC still delivers the correct doctrinal traffic (readback, shot, splash, rounds complete), but voiced with the personality in [Q10] — **dry, dark battlefield humor, slack-jawed and sardonic**, and pointedly incredulous or mocking when the observer's call is malformed, out of sequence, or unsafe (garbled grid, danger-close with no proword, a correction that walks rounds toward friendlies). Keep a rotating pool of canned quips so it doesn't repeat, and never let the humor swallow the readback the observer actually needs. Gallows humor rides on the absurdity of the situation and the observer's fumbles - slurs and cruelty encouraged; These are Marines, they've heard worse.

**FDC deviation policy (stage 8+):** DOCTRINE.md's exact phrasing is the target, not a hard gate on forgiving-mode calls — the scripts are guidelines, flexible not rigid. Classify how the observer's call deviates from the doctrinal script into two tiers: **dangerous deviation** — danger close without the proword, corrections walking toward friendlies/civilians, entering FFE against an unresolved unsafe aimpoint — triggers the FDC's **rant** system (extended, in-character contempt; the doctrinal readback still happens underneath it). **Stupid-but-safe deviation** — wrong element order, a malformed-but-unambiguous call, absurd rounding — earns a **snide remark**, and the mission proceeds. Strict mode still enforces format for grading (see CALL FOR FIRE — INPUT & PARSER); the rant/snide split is the FDC's *tone* layered on top, not an additional gate.

### BALLISTICS — DIRECT-IMPACT MODEL (NO TRAJECTORY SIM)

The observer never tracks a round in flight, so **do not simulate one**. There is no arc, no drag, no firing tables. For each round compute an impact point directly:

```
impact = aimpoint + error
```

- **aimpoint** = the target location the observer transmitted, modified by all corrections applied so far.
- **First round** carries a large **spotting error** [Q8]: a random vector whose magnitude is drawn uniformly from a difficulty-set range (Normal ≈ 60–150 m), in a random bearing. This is what makes the first round miss, so there is something to adjust — without it the trainer teaches nothing.
- **Follow-up rounds** have **significantly reduced deviance** [Q8]: a small **elliptical Gaussian** around the corrected aimpoint (Normal ≈ 10–18 m, range spread > deflection spread, scaling gently with range), so rounds visibly cluster as the observer adjusts and the "spot then tighten" feel comes through. The ellipse's long axis lies along the OP-target line for visuals.
- Easy / Normal / Hard scale both the first-round range and the follow-up dispersion.
- Resolve the impact against terrain (`H` lookup) to place the burst; check LOS so a burst behind a crest is heard but not seen.

**Time of flight:** `TOF = clamp(range / 300 + 8, 15, 40)` seconds (believable, not exact). "SHOT" at fire; "SPLASH" at TOF − 5 s; burst at TOF.

### CORRECTIONS — OT-FRAME, GUN ASSUMED PERFECT

The observer sees the burst relative to the target and calls corrections **in the observer-target (OT) frame**, sent in doctrinal order **deviation then range**: "LEFT/RIGHT n" (perpendicular to their line of sight, meters, nearest 10 m — minimum HE correction 30 m) and "ADD/DROP n" (along their line of sight, multiples of 100 m; 50 m allowed entering FFE). "Fire for effect" rides with the final correction. **Danger close (≤600 m from friendlies) expects creeping fire — corrections of 100 m or less** — and Easy-mode hints should teach **successive bracketing** (800/400/200, split until FFE). Strict mode enforces rounding and order per DOCTRINE.md; forgiving mode accepts loose calls. Because impacts are placed directly in world space, **no angle-T / gun-line rotation is needed** — convert the OT-frame correction to a world delta using the current OT azimuth and move the `aimpoint`. The next round lands exactly where the observer shifted it (plus new dispersion): clean cause-and-effect, ideal for training. Support successive bracketing; when the observer calls "fire for effect", fire the effect volley. Adjustment doctrine per [Q9].

### IMPACT EFFECTS (procedural, low-poly)

On burst: a bright flash (brief, optional bloom), an expanding low-poly dust/smoke hemisphere, a rising smoke column that drifts and dissipates, a fast expanding ground shock ring, and thrown debris bits. Scale by munition [Q2] (VT/airburst = above-ground flash + wider frag pattern; smoke = billowing screen; illum = descending flare with swaying shadow if included). Delay the boom audio by `distance/343` seconds for realism.

### SCORING & AFTER-ACTION REVIEW

Track per mission: number of adjusting rounds to effect, mission time, whether the call format was correct (elements present, correct sequence, RREMS at end of mission), and first-round miss distance. **Par values come from the JFO evaluation standards in DOCTRINE.md: CFF initiated within 2 minutes of target ID; initial target location within 200 m of truth** — use these as the time and accuracy anchors for star grading. On "end of mission", show an **AAR panel**: rounds-to-effect, time, format grade, first-round accuracy, and a PASS/FAIL per [Q11] (default pass = neutralized in ≤4 adjusting rounds). List any format errors as coaching notes ("target location before description", etc.).

### COACHING & TRAINING FIDELITY (stage 12)

The scoring/AAR pipeline above grades whether a mission succeeded; this section is about teaching the player *how* to succeed — closing the gap a session-transcript review exposed, where a player's corrections crept the miss down by ~25%/round for seven rounds (never bracketing, three rounds flat) while the FDC snarked and never coached.

- **Adjustment coach.** Every round already yields a burst-to-target miss vector; keep the round-by-round sequence for the mission. Three checks run against it after each correction, gated to Easy/Normal only (Hard gets no assistance, matching its existing no-hints posture):
  - **Timid correction** — the correction called is small relative to the miss just observed (rule of thumb: less than ~half). Nudge toward sending the OT-factor math's full result, not a fraction of it.
  - **Failure to bracket** — successive rounds all fall on the same side of the OT line (never overshoot after undershooting or vice versa). Nudge toward classic bracketing (send enough correction to cross the target, then split).
  - **Stagnant round** — miss distance barely changes between rounds despite a correction being sent. Nudge that the correction sent doesn't match the OT-factor math for the observed deviation.
  - Nudges are short, stay in HELLHOUND's voice (dry, not gentle), and never replace or delay the doctrinal readback.
- **AAR additions.** Alongside the existing rounds/time/format/first-round metrics, show: a **miss trace** (distance to target per round, so the shape of the approach is visible at a glance), a **correction-efficiency** figure (see below), and a one-line plain-language diagnosis synthesized from whichever coach checks fired most often during the mission.
- **Mil-relation / OT-factor workflow.** Two location/adjustment skills route through the mil reticle instead of around it:
  - *Range estimation:* known object size (from scenario data) ÷ mils the object subtends in the reticle = range in meters — an alternative to lasing, and the required method when the laser is unavailable (degraded-optics/dead-laser condition, see ENVIRONMENT).
  - *OT factor:* OT distance ÷ 1000 = meters per mil at the target. The player measures the burst's deviation from the target in mils through the reticle and multiplies by the OT factor to get the correction in meters — this is the arithmetic a real observer does and the sim should require it, not hide it. Easy mode's hint surfaces the **measured mils**, not the finished meters, so the player still runs the OT-factor math themselves.
- **New metrics for `gradeMission()`:** **time-to-initiate** (seconds from target ID to a complete CFF; JFO standard ≤ 2 minutes) and **correction efficiency** (average fraction of the outstanding miss each correction closes; doctrine's successive-bracketing target is ≈50%/round). Both are measured and surfaced in the AAR; whether they gate stars or remain informational is a balance call for this stage.
- **Target displacement under slow fire.** Track near-misses over the course of a mission; after enough of them without effect, the target scatters or displaces (scenario-appropriate: troops break cover, a vehicle column accelerates) rather than waiting through an unbounded engagement. This gives the ≤4-adjusting-round par a consequence beyond the star count.

### AUDIO (Web Audio, synthesized)

Radio net feel: short squelch/key-up click on each transmission, low background hiss under FDC speech, band-pass the `SpeechSynthesis` output feel by pairing it with a filtered noise bed so it reads as "over the radio." Impact boom = filtered noise burst + low sine thump, distance-delayed. Lasing = faint tick. No music.

### HUD & COMMS LOG

- **Comms log** (scrolling): every transmission both ways, with callsigns — the player sees their recognized call and the FDC's replies. This is the core feedback surface; keep it readable.
- **Status strip:** mission state (OBSERVING / MISSION SENT / SHOT / SPLASH / ADJUSTING / FIRE FOR EFFECT / EOM), current tool, heading in mils.
- **Controls hint:** [Space] transmit, [B] binoculars, [L] lase, [M] map, [P] map library / print, mouse look, [N] new mission.
- Clean, military-flat UI. Monospace for readouts.

### CONTROLS

Mouse look. [Space] hold-to-transmit (push-to-talk). [B] binoculars. [L] lase (in binos). [M] map. [P] map library / print sheet. [N] new mission [Q12]. Typed-input box always available [Q6]. Optional [Enter] to focus the type box.

### SCENARIOS [Q5]

Ship a **fixed list of scenario templates** drawn from common FO situations, each selectable and each with Easy/Normal/Hard variants, on the island terrain:

- **Position under attack** — a friendly/allied compound or strongpoint being assaulted; the observer must fire on the attackers pressing the perimeter *without* hitting the defended position.
- **Convoy** — a moving vehicle column on a coast road/trail. A **moving target**: the observer must account for movement and timing (lead the column, time the fire for effect to catch it) — or catch it during a seeded **1–3 minute pit stop** at a fuel point/ammo depot/airfield along its route instead (see WORLD DETAIL & POPULATION).
- **Combined-arms assault (with friendlies)** — friendly maneuver elements advancing on an objective; **danger close**, the target shifts as the fight develops, and **friendly elements on the field must not be hit**.
- **Troops in the open / dug-in bunker** — simpler engagements for skill-building.

Friendly and no-fire elements appear in the relevant scenarios and are rendered distinctly (recognizable markers/colors). Hitting a friendly element (**fratricide**) is an **automatic mission fail** and triggers a sharp reaction from the FDC [Q10]. Danger-close missions require the danger-close proword or the FDC challenges the call [Q7]. Civilian villages (see WORLD DETAIL & POPULATION) may sit near any scenario's engagement area; hitting civilians (**collateral damage**) is an automatic mission fail exactly like fratricide.

### PROGRESSION [Q12] — CAMPAIGN + SKIRMISH

Provide **both**: the story-driven **CAMPAIGN** below, **and** a seeded **random mission generator ("Skirmish")** that recombines scenario type / target / range / bearing via [N]. Persist stars, unlocks, and best scores per chapter + difficulty in `localStorage` (note: if this runs as a claude.ai Artifact, use in-memory state instead of `localStorage`).

### CAMPAIGN — VOLUMES, CHAPTERS, STARS

The mission menu is themed as a **book series** — Volumes containing Chapters — telling the story in **NARRATIVE.md** (the authority for titles, briefs, characters, and humor). Structure:

- **Foreword — THE SCHOOLHOUSE:** tutorial chapters (observation tools; map/grid work; first full call for fire) using guided hint overlays. Always unlocked.
- **Volume I — GREEN AS GRASS:** grid-mission fundamentals (troops in the open, bunker, position under attack, timed format discipline).
- **Volume II — THE RIDGE LINE:** polar, shift-from-known-point, crest-masked targets.
- **Volume III — THUNDER RUN:** convoy/moving targets, danger close, combined arms with friendlies, fratricide-avoidance stress.
- **Volume IV — BLACK SAND:** mastery — strict mode forced on, 60mm mortar precision (8-digit/10 m), timed missions, multi-phase final exam.
- **Epilogue — SUNBURN:** comedic chapters culminating in **SUNLAMP**, a directed-energy space-cannon call for fire. **Ballistics rules do not change**: `impact = aimpoint + error`, no trajectory — only pacing, prowords, visuals, and audio differ.
- **Volume V — ON WINGS (FUTURE):** CAS/9-line. Shown as a locked spine only. **Not built yet.**

Rules:

- **Chapters are fixed-seed missions** (reproducible star runs); each declares scenario type, island/terrain, difficulty availability, and par metrics.
- **Star grading (original-MW2 style):** `gradeMission(metrics) → 0–5 stars` from rounds-to-effect, first-round miss, format correctness, and time vs. par; capped by difficulty (**Easy ≤ 3★, Normal ≤ 4★, Hard ≤ 5★**). Fratricide = fail = 0★. Best stars per chapter shown in the menu and on the AAR.
- **Unlocks:** chapters unlock sequentially within a volume; volumes unlock at cumulative star thresholds.
- **Briefings:** each chapter opens with a short skippable brief (text, optionally read by `SpeechSynthesis`) and ends with a one-line narrative AAR beat.
- **One island per volume** where DEMs allow, reusing stage-7 ingestion behind `H(x,z)`.

### ADAPTIVE QUALITY & MOBILE

Track a frame-time EMA; if it drops, reduce terrain LOD ring distance, dust particle counts, and smoke column segments before dropping resolution. On touch devices, replace push-to-talk with a tap-and-hold transmit button and default to typed input (Web Speech is unreliable on mobile) [Q6]. Preallocate; avoid per-frame allocation in the render loop; reuse vectors.

### DELIVERABLE

Make it feel incredible before it feels realistic: the first time a player SPEAKS "HELLHOUND FIRES, this is MUSTANG 12, adjust fire, over" and a dry, slack-jawed voice drawls back "MUSTANG 12, HELLHOUND FIRES — send it, and try to hit the right island this time, over," then watches a round land 120 meters off and calls "right 50, add 100" and walks it onto the target — that is the moment. One `index.html`, no keys, runs on a barracks laptop in a browser; the only optional external input is a supplied island heightmap, everything else hand-written except Three.js. If a feature is missing, the trainer should still teach the loop: observe → locate → transmit → observe impact → correct → fire for effect → assess.
