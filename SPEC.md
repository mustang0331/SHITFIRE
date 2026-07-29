# SHITFIRE — FORWARD OBSERVER (CALL-FOR-FIRE) TRAINER — ONE-SHOT PROMPT

> Feed this to a coding agent (Claude Code / Cursor). Fill in the **PARAMETERS** block first — every `[Q#]` in the spec body pulls from it. Anything you leave blank, the agent should resolve using the stated default.

---

## PARAMETERS TO FILL IN (answer before running)

- **Q1 — LOCATION METHODS (v1):** grid only / grid + polar / all three (grid, polar, shift-from-known-point). → `[FILL IN]` _(default: all three — grid, polar, shift-from-known-point)_
- **Q2 — FIRE SUPPORT ASSET(S) & MUNITIONS:** → **155mm HE, PD fuze only.** _(Grid precision is tied to the asset — see Q4.)_
- **Q3 — ENVIRONMENT / BIOME:** → **WW2 Pacific island chains** — jungle, volcanic rock, coral beaches, surrounding ocean. Terrain is to be driven by **real heightmap/DEM data fed in during development** (e.g. Iwo Jima, Peleliu, Saipan, Tarawa); a procedural island stand-in is used for the first pass. See TERRAIN.
- **Q4 — MAP SIZE & GRID SCHEME:** → **10 km × 10 km.** Grid precision follows the fire-support asset: **artillery = 6-digit (100 m); 60mm mortars = 8-digit (10 m).**
- **Q5 — TARGET SCENARIO(S):** → **Multiple scenario templates** drawn from common FO situations: a position/compound under attack; a moving convoy; a combined-arms assault on an objective **with friendly elements on the field** (danger close / fratricide avoidance); plus simpler ones (troops in the open, dug-in bunker). See SCENARIOS.
- **Q6 — INPUT MODE:** → **voice (Web Speech API) + typed fallback.** _(Voice is Chrome/Edge desktop only.)_
- **Q7 — DOCTRINE STRICTNESS:** → **forgiving keyword parse, with a "strict mode" toggle.**
- **Q8 — DIFFICULTY:** → **The first round deviates randomly within a set range; follow-up rounds have significantly reduced deviance.** Easy/Normal/Hard scale both. See BALLISTICS.
- **Q9 — ADJUSTMENT DOCTRINE:** → **free adjust; bracket hints shown in Easy.**
- **Q10 — CALLSIGNS & FDC VOICE:** → observer **"MUSTANG 12"**, FDC **"HELLHOUND FIRES"**. Tone: **dark-humored, dry, slack-jawed and sardonic — and pointedly so when the call-in is wrong.**
- **Q11 — SCORING / AAR:** → **grade rounds-to-effect, time, format correctness, and first-round accuracy; pass = target neutralized in ≤4 adjusting rounds.** Fratricide = automatic fail.
- **Q12 — PROGRESSION:** → **a fixed scenario list with difficulty settings, PLUS a random mission generator.**
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

If time or context runs short, a fully working stage 1–2 is far more valuable than a broken attempt at all seven.

### VISUAL IDENTITY — "ROBLOX-LEVEL" IS FINE

Bright, clean, low-poly, flat-shaded. Chunky geometry, no textures, vertex-color everything. Readable at a glance over realism. **WW2 Pacific island palette** [Q3]: deep ocean blue `#1E5A7A`/`#2E7DA0` ringing the island, coral/sand beach shelf `#D8C89A`, jungle greens `#4E7A3D`/`#3B5E2E`, volcanic rock `#5A544E`/`#6E665C`, pale tropical sky `#BFE0EA` at zenith fading to humid haze `#E8EDDA` at the horizon. Harsh high midday sun (elevation ~55°), gentle ambient fill, heavy heat-haze fog on the far ocean so the map edge dissolves into sea and sky. Everything crisp; no post-processing required beyond optional mild bloom on the impact flash.

### TERRAIN — HEIGHTMAP-DRIVEN, NO PHYSICS ENGINE

Terrain height `H(x,z)` is driven by a **grayscale heightmap/DEM of a real WW2 Pacific island**, supplied during development [Q3] (e.g. Iwo Jima with Suribachi, Peleliu's Umurbrogol ridges, Saipan, Tarawa). Sample the DEM to build a low-poly flat-shaded mesh across the [Q4] **10 km × 10 km** area (~8–15 m triangles near the OP, coarsening with distance / LOD rings), ringed by ocean. **For the ZERO-ASSET first pass (no DEM yet), synthesize a procedural stand-in island** from seeded value-noise — a central volcanic massif, jungle-covered ridge fingers, and a coral beach shelf dropping into surrounding water — so the one-shot runs immediately and a real DEM can be swapped in later behind the same `H(x,z)` interface. Either way, expose `H(x,z)` plus a ray-vs-heightfield lookup (march the ray, sample `H`) used for: impact resolution, laser rangefinder returns, and line-of-sight / crest-masking checks (can the observer even SEE a given point). The OP sits on high ground (a ridge or Suribachi-like feature) overlooking the beaches and inland valley. No rigid-body physics anywhere — rounds are never simulated in flight (see BALLISTICS).

### PRINTABLE MAP LIBRARY & MAPWORK

Every terrain in the library has a matching **printable topographic map sheet generated from the SAME `H(x,z)`** used by the 3D world, so the paper map and the sim world are guaranteed to correlate — a grid the user reads off the printout plots to the same point the sim's rangefinder/grid readout reports.

- **Render the sheet** as a top-down 2D map (SVG or canvas): **contour lines** derived by marching `H` at the [Q14] interval (10 m, index contour every 50 m, index lines labeled with elevation), water/coastline fill, spot elevations on peaks/ridges, a labeled **grid** matching [Q4] (10 km × 10 km, numbered grid lines, grid-square references at the precision set by the asset), the **OP** marked, and any **known / registration points** for shift missions.
- **Marginal information like a real map sheet:** title (island name + scenario), **scale bar** and representative fraction per [Q14] (default 1:50,000), a **north arrow with grid-magnetic declination diagram**, a grid-reference guide, and a symbol legend. Style it monochrome/line-art so it **prints cleanly on a black-and-white printer**.
- **NO enemy positions plotted** — the sheet is a clean map the user plots on by hand (that IS the mapwork). Provide an optional **"answer-key" toggle** that prints a second version with the target/known solution for self-check.
- **Map Library screen:** lists every available map with a thumbnail; selecting one shows the full sheet with **Print** (browser `window.print()` driving a dedicated `@media print` stylesheet sized to [Q14] paper — the sheet fills the page, marginal data in the margins, everything else in the app hidden) and **Save** (export the sheet as PNG via canvas, and/or let the user "Save as PDF" through the browser print dialog). Offer the **1:25,000 quadrant** option [Q14] for a larger-scale print of one quarter of the island.
- New island DEMs added to the terrain library automatically get a correlated sheet, since both come from the same heightfield — the library grows with the maps.

Reachable via **[P]** or a menu button; opens the sheet for the currently loaded map.

### THE OBSERVATION POST & CAMERA

First-person from the OP. Mouse-look (yaw/pitch), clamped pitch. The observer does not walk far — small reposition at most. Show a subtle horizon-referenced heading readout in **mils (0–6400)**, since the observer thinks in mils. Default FOV ~60°; narrows dramatically in optics (see TOOLS). Head-bob off; this is a stable observation platform.

### OBSERVER TOOLS (this matters more than graphics)

- **BINOCULARS [B]:** overlay optic. Narrow FOV (~7× → ~9° FOV). Draw a proper **mil reticle**: horizontal and vertical scales graduated every 5 mils with 10-mil numbered ticks, so the observer can measure angular deviation of a burst from the target. Vignette the edges. Heading in mils shown on the reticle.
- **LASER RANGEFINDER [L]:** while in binos, lasing returns the range in meters to whatever the center reticle is on (ray-march `H`), plus that point's grid per [Q4]. Brief "LASING…" then a range/grid readout.
- **COMPASS / DIRECTION:** current look azimuth in mils always available; this is the observer's measured direction to target/burst.
- **MAP [M]:** simple top-down schematic — OP position, known points (for shift missions if [Q1] includes them), grid lines, north arrow. No enemy positions revealed; the player must locate targets themselves. The full, print-ready version of this map is the sheet in the **Map Library** (see PRINTABLE MAP LIBRARY & MAPWORK).

Together these are how the observer LOCATES the target (grid/polar/shift per [Q1]) and SPOTS corrections. Make them accurate and legible.

### CALL FOR FIRE — INPUT & PARSER

Push-to-talk: hold **[Space]** to transmit, release to send [Q6]. Show a live transcript; always provide a typed input box as fallback/override (Web Speech is imperfect and Chrome-only). Parse the transmission into the doctrinal elements, tolerant of loose phrasing per [Q7]:

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

1. **Readback / Message to Observer (MTO):** echo the mission and target location for confirmation. In strict mode require the observer to confirm.
2. **"SHOT, OVER"** at the moment of firing.
3. **"SPLASH, OVER"** ~5 seconds before impact.
4. After the observer's correction: brief acknowledgement, then repeat 2–3.
5. On "fire for effect": fire the effect volley, then **"ROUNDS COMPLETE, OVER"**, then end-of-mission surveillance and the observer's battle damage assessment.

Callsigns per [Q10]: observer **MUSTANG 12**, FDC **HELLHOUND FIRES**. The FDC still delivers the correct doctrinal traffic (readback, shot, splash, rounds complete), but voiced with the personality in [Q10] — **dry, dark battlefield humor, slack-jawed and sardonic**, and pointedly incredulous or mocking when the observer's call is malformed, out of sequence, or unsafe (garbled grid, danger-close with no proword, a correction that walks rounds toward friendlies). Keep a rotating pool of canned quips so it doesn't repeat, and never let the humor swallow the readback the observer actually needs. Gallows humor rides on the absurdity of the situation and the observer's fumbles — not on slurs or cruelty; keep it roughly PG-13.

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

The observer sees the burst relative to the target and calls corrections **in the observer-target (OT) frame**: "LEFT/RIGHT n" (perpendicular to their line of sight, meters) and "ADD/DROP n" (along their line of sight, meters). Because impacts are placed directly in world space, **no angle-T / gun-line rotation is needed** — convert the OT-frame correction to a world delta using the current OT azimuth and move the `aimpoint`. The next round lands exactly where the observer shifted it (plus new dispersion): clean cause-and-effect, ideal for training. Support successive bracketing; when the observer calls "fire for effect", fire the effect volley. Adjustment doctrine per [Q9].

### IMPACT EFFECTS (procedural, low-poly)

On burst: a bright flash (brief, optional bloom), an expanding low-poly dust/smoke hemisphere, a rising smoke column that drifts and dissipates, a fast expanding ground shock ring, and thrown debris bits. Scale by munition [Q2] (VT/airburst = above-ground flash + wider frag pattern; smoke = billowing screen; illum = descending flare with swaying shadow if included). Delay the boom audio by `distance/343` seconds for realism.

### SCORING & AFTER-ACTION REVIEW

Track per mission: number of adjusting rounds to effect, mission time, whether the call format was correct (elements present, correct sequence), and first-round miss distance. On "end of mission", show an **AAR panel**: rounds-to-effect, time, format grade, first-round accuracy, and a PASS/FAIL per [Q11] (default pass = neutralized in ≤4 adjusting rounds). List any format errors as coaching notes ("target location before description", etc.).

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
- **Convoy** — a moving vehicle column on a coast road/trail. A **moving target**: the observer must account for movement and timing (lead the column, time the fire for effect to catch it).
- **Combined-arms assault (with friendlies)** — friendly maneuver elements advancing on an objective; **danger close**, the target shifts as the fight develops, and **friendly elements on the field must not be hit**.
- **Troops in the open / dug-in bunker** — simpler engagements for skill-building.

Friendly and no-fire elements appear in the relevant scenarios and are rendered distinctly (recognizable markers/colors). Hitting a friendly element (**fratricide**) is an **automatic mission fail** and triggers a sharp reaction from the FDC [Q10]. Danger-close missions require the danger-close proword or the FDC challenges the call [Q7].

### PROGRESSION [Q12]

Provide **both**: a **fixed scenario list** (see SCENARIOS), each with Easy/Normal/Hard, selectable from a menu; **and** a seeded **random mission generator** that recombines scenario type / target / range / bearing via [N]. Persist best scores per scenario + difficulty in `localStorage` (note: if this runs as a claude.ai Artifact, use in-memory state instead of `localStorage`).

### ADAPTIVE QUALITY & MOBILE

Track a frame-time EMA; if it drops, reduce terrain LOD ring distance, dust particle counts, and smoke column segments before dropping resolution. On touch devices, replace push-to-talk with a tap-and-hold transmit button and default to typed input (Web Speech is unreliable on mobile) [Q6]. Preallocate; avoid per-frame allocation in the render loop; reuse vectors.

### DELIVERABLE

Make it feel incredible before it feels realistic: the first time a player SPEAKS "HELLHOUND FIRES, this is MUSTANG 12, adjust fire, over" and a dry, slack-jawed voice drawls back "MUSTANG 12, HELLHOUND FIRES — send it, and try to hit the right island this time, over," then watches a round land 120 meters off and calls "right 50, add 100" and walks it onto the target — that is the moment. One `index.html`, no keys, runs on a barracks laptop in a browser; the only optional external input is a supplied island heightmap, everything else hand-written except Three.js. If a feature is missing, the trainer should still teach the loop: observe → locate → transmit → observe impact → correct → fire for effect → assess.
