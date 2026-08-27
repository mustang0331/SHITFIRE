# Graph Report - SHITFIRE  (2026-08-23)

## Corpus Check
- 58 files · ~356,618 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 921 nodes · 1178 edges · 54 communities (51 shown, 3 thin omitted)
- Extraction: 98% EXTRACTED · 2% INFERRED · 0% AMBIGUOUS · INFERRED: 19 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `2cc7380f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- 12-cff-handling.js
- 10-mission.js
- 3. Enabling Learning Objectives by Volume
- 08-burst-optics.js
- 05-units-scenario.js
- 3. Revised QUIPS pools
- 07-voice-spot-audio.js
- 04-world.js
- 3. THE LIVE BOARD — open work, by domain
- 01-math-terrain.js
- 09-camera.js
- THE PROMPT
- 02-scene.js
- js
- 15-campaign-menu.js
- 14-map-library.js
- GRAPHICS.md — Visual Upgrade Plan for SHITFIRE
- 06-clock-comms.js
- package.json
- 16-input.js
- BALLISTICS_RESEARCH.md — real-world accuracy/lethality reference
- TLO.md — Terminal Learning Objective / Training Objectives Crosswalk
- perfsweep.js
- PORT_PLAN.md — Staged Migration
- NARRATIVE.md — SHITFIRE Campaign Storyline
- TLO.md — Terminal Learning Objective / Training Objectives Crosswalk
- NARRATIVE.md — SHITFIRE Campaign Storyline
- build.js
- DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols
- CONTENT_INVENTORY.md — Port Content Inventory
- perfprobe.js
- CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)
- DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols
- QUICKSTART — Building SHITFIRE with Claude Code
- mastermap_render.py
- PARITY_TESTING.md — The Browser Sim Is the Oracle
- 03-op-battery.js
- shots.js
- ARCHITECTURE.md — Target Structure and What Actually Transfers
- 00-config.js
- lint.js
- ENGINE_PORT — Plan and Strategy for Moving SHITFIRE to a Game Engine
- ENGINE_CHOICE.md — Which Engine, and Why
- perflegibility.js
- replay.js
- docs.md
- research.md
- serve.js
- mastermap-collect.mjs
- pre-commit
- KOFA_KING_VALLEY_FO_HEIGHTMAP.md

## God Nodes (most connected - your core abstractions)
1. `THE PROMPT` - 24 edges
2. `3. Revised QUIPS pools` - 21 edges
3. `js` - 19 edges
4. `fireForEffect()` - 18 edges
5. `GRAPHICS.md — Visual Upgrade Plan for SHITFIRE` - 17 edges
6. `fireAdjustRound()` - 15 edges
7. `buildWorldFeatures()` - 13 edges
8. `CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)` - 13 edges
9. `PORT_PLAN.md — Staged Migration` - 13 edges
10. `setState()` - 12 edges

## Surprising Connections (you probably didn't know these)
- `handleEOM()` --indirect_call--> `showAAR()`  [INFERRED]
  src/js/12-cff-handling.js → src/js/13-aar.js
- `updateScenario()` --indirect_call--> `showAAR()`  [INFERRED]
  src/js/05-units-scenario.js → src/js/13-aar.js
- `fireAdjustRound()` --indirect_call--> `mission()`  [INFERRED]
  src/js/10-mission.js → src/js/17-loop-boot.js
- `fireForEffect()` --indirect_call--> `mission()`  [INFERRED]
  src/js/10-mission.js → src/js/17-loop-boot.js

## Import Cycles
- None detected.

## Communities (54 total, 3 thin omitted)

### Community 0 - "12-cff-handling.js"
Cohesion: 0.20
Nodes (22): addNote(), CFFQ, cffqAdd(), cffqComplete(), cffqLocation(), cffqReset(), cffqStart(), cffqTick() (+14 more)

### Community 1 - "10-mission.js"
Cohesion: 0.06
Nodes (57): addRoundEffect(), AMMO, AMMO_WARNED, ammoLeft(), ammoPool(), ammoSpend(), armAtMyCommand(), assessEffect() (+49 more)

### Community 2 - "3. Enabling Learning Objectives by Volume"
Cohesion: 0.33
Nodes (6): 3. Enabling Learning Objectives by Volume, FOREWORD — The Schoolhouse (tutorial, guided, non-punitive), VOLUME I — Green as Grass (grid fundamentals), VOLUME II — The Ridge Line (location methods, terrain masking), VOLUME III — Thunder Run (dynamic fires, danger close), VOLUME IV — Black Sand (mastery, strict doctrine, 60mm precision)

### Community 3 - "08-burst-optics.js"
Cohesion: 0.06
Nodes (40): addCrater(), applyVision(), BEAM, bursts, _crM4, _crN, _crQ, _crUp (+32 more)

### Community 4 - "05-units-scenario.js"
Cohesion: 0.11
Nodes (21): convoyHeadD(), enemyVolley(), figScale(), genScenario(), legibilityPass(), _legOne(), pathDir(), pathPoint() (+13 more)

### Community 11 - "3. Revised QUIPS pools"
Cohesion: 0.05
Nodes (40): 10.1 Two parser misses still live, 10.2 `noMission` and `unknown` are thin, and bursts prove it, 10.3 Missing: the in-mission repeat callback, 10.4 Proposed lines (tone: Generation Kill / WARFARE per the user's 2026-07-30 direction), 10. Night-run transcript review (2026-07-31) — the 18-05 / 18-34 / 20-01 exports, against post-14a code, 1. What stays untouched, and why, 2. What the transcript actually showed, 2a. Content boundary — no taking God's name in vain (+32 more)

### Community 17 - "07-voice-spot-audio.js"
Cohesion: 0.09
Nodes (29): callerKickoff(), callerLine(), callerLocLine(), callerReply(), CALLERS, callerSay(), CARD4_ADJ, CARD4_DIR (+21 more)

### Community 19 - "04-world.js"
Cohesion: 0.09
Nodes (30): _boxBatch, _boxUnit, buildVegetation(), buildWorldFeatures(), crossedQuadGeo(), FIG, flushBoxes(), isleName() (+22 more)

### Community 23 - "3. THE LIVE BOARD — open work, by domain"
Cohesion: 0.06
Nodes (31): 1. Authority map — which file answers which question, 2. Work-order protocol (the anti-drift rules), 3.1 PERF — frame rate and responsiveness, 3.2 WORLD — what is on the island, and where, 3.3 ENEMY — what the enemy actually does, 3.4 NET — who is calling, and how badly, 3.5 TEMPO — time of day, immediacy, and the clock, 3.6 GFX — rendering beyond the shipped graphics plan (+23 more)

### Community 26 - "01-math-terrain.js"
Cohesion: 0.11
Nodes (19): binoFovNow(), binoPower(), clamp(), declMils(), demSample(), fbm(), fdcCall(), fdcShort() (+11 more)

### Community 31 - "09-camera.js"
Cohesion: 0.10
Nodes (24): binoEl, cheatEl, crosshairEl, cycleBinoZoom(), _dir, doLase(), drawReticle(), laseEl (+16 more)

### Community 34 - "THE PROMPT"
Cohesion: 0.08
Nodes (26): ADAPTIVE QUALITY & MOBILE, AUDIO (Web Audio, synthesized), BALLISTICS — DIRECT-IMPACT MODEL (NO TRAJECTORY SIM), BUILD ORDER — MVP FIRST (ship each stage working before the next), CALL FOR FIRE — INPUT & PARSER, CAMPAIGN — VOLUMES, CHAPTERS, STARS, COACHING & TRAINING FIDELITY (stage 12), CONTROLS (+18 more)

### Community 39 - "02-scene.js"
Cohesion: 0.06
Nodes (37): applyTOD(), _bakeSun, buildTerrain(), camera, canvas, composerDrop(), hemi, loadDEMFile() (+29 more)

### Community 42 - "js"
Cohesion: 0.09
Nodes (21): js/00-config.js, js/01-math-terrain.js, js/02-scene.js, js/03-op-battery.js, js/04-world.js, js/05-units-scenario.js, js/06-clock-comms.js, js/07-voice-spot-audio.js (+13 more)

### Community 43 - "15-campaign-menu.js"
Cohesion: 0.15
Nodes (20): ALL_CHAPTERS, briefEl, CAMP, CAMPAIGN, chUnlocked(), correctionEfficiency(), gradeMission(), menuEl (+12 more)

### Community 48 - "14-map-library.js"
Cohesion: 0.15
Nodes (18): drawLegend(), drawMap(), drawWorldLayer(), facSymbol(), LEGEND_ROWS, libEl, mapCv, mapEl (+10 more)

### Community 52 - "GRAPHICS.md — Visual Upgrade Plan for SHITFIRE"
Cohesion: 0.07
Nodes (30): 0. Baseline audit, 1. Ordering — do these in this order, Appendix A — The Graphics Ceiling, Binding constraint, Ceiling by subsystem, Do not do, G0 — Config gate and no-regret renderer settings, G1 — Sky and time of day (+22 more)

### Community 56 - "06-clock-comms.js"
Cohesion: 0.13
Nodes (11): applyComms(), clampComms(), commsDefaults(), commsEl, commsHeadEl, events, FDC, loadComms() (+3 more)

### Community 61 - "package.json"
Cohesion: 0.12
Nodes (15): eslint, globals, playwright-core, dependencies, playwright-core, description, devDependencies, eslint (+7 more)

### Community 67 - "16-input.js"
Cohesion: 0.17
Nodes (13): endPTT(), micHold(), noteBtn, padPrev, pollGamepad(), pttEl, pttLiveEl, refreshNoteBtn() (+5 more)

### Community 70 - "BALLISTICS_RESEARCH.md — real-world accuracy/lethality reference"
Cohesion: 0.14
Nodes (13): 1. What the sim currently does (baseline, from `index.html`), 2. Real dispersion: bias vs. random error (this part the sim already gets right), 3. Range-scaling gap and why it matters here, 4. Mortar-specific: dispersion is right, time of flight is not, 5. Casualty/lethal radius — the sim doesn't model this at all, and arguably shouldn't fully, 6. Danger close: doctrine's flat 600 m is a deliberate simplification, and it's the right one, 7. Smoke & illumination — reference figures for the parked 12h row, 8. Summary of findings, ranked by whether they're worth doing anything about (+5 more)

### Community 72 - "TLO.md — Terminal Learning Objective / Training Objectives Crosswalk"
Cohesion: 0.14
Nodes (13): 1. Purpose and Scope, 2. Course Terminal Learning Objective, 3. Enabling Learning Objectives by Volume, 4. Assessment Methodology, 5. Chapter Crosswalk, 6. Future Objectives (not yet built) / Built-but-not-chaptered, Assessment gaps — CLOSED (stage 12), FOREWORD — The Schoolhouse (tutorial, guided, non-punitive) (+5 more)

### Community 73 - "perfsweep.js"
Cohesion: 0.14
Nodes (12): DSF, errors, here, rows, simPath, sx, sxx, sxy (+4 more)

### Community 78 - "PORT_PLAN.md — Staged Migration"
Cohesion: 0.15
Nodes (13): Effort shape (not a schedule), Keeping the browser build alive, P0 — Freeze the oracle *(do this before writing any engine code)*, P1 — Core skeleton and the PRNG proof, P2 — Sim core: ballistics, missions, doctrine, P3 — Engine shell: world and observer, P4 — The loop closes: fire missions end to end, P5 — Cartography (+5 more)

### Community 79 - "NARRATIVE.md — SHITFIRE Campaign Storyline"
Cohesion: 0.15
Nodes (12): Characters, EPILOGUE — SUNBURN *(unlocked by finishing Volume IV · the goofy-ass shit)*, FOREWORD — THE SCHOOLHOUSE *(tutorial — always unlocked)*, Humor rules, NARRATIVE.md — SHITFIRE Campaign Storyline, Premise, Structure, grading, and unlocks, VOLUME I — GREEN AS GRASS *(fundamentals · grid missions · first island)* (+4 more)

### Community 80 - "TLO.md — Terminal Learning Objective / Training Objectives Crosswalk"
Cohesion: 0.29
Nodes (7): 1. Purpose and Scope, 2. Course Terminal Learning Objective, 4. Assessment Methodology, 5. Chapter Crosswalk, 6. Future Objectives (not yet assessed / not yet built), Known assessment gaps, TLO.md — Terminal Learning Objective / Training Objectives Crosswalk

### Community 81 - "NARRATIVE.md — SHITFIRE Campaign Storyline"
Cohesion: 0.10
Nodes (20): Caller archetypes, Callers & guest voices, Characters, EPILOGUE — SUNBURN *(unlocked by finishing Volume IV · the goofy-ass shit)*, FOREWORD — THE SCHOOLHOUSE *(tutorial — always unlocked)*, Guest voices — the umbrella, Humor placement, Humor rules (+12 more)

### Community 82 - "build.js"
Cohesion: 0.15
Nodes (9): built, head, here, manifest, outPath, parts, PP_REWRITE, root (+1 more)

### Community 85 - "DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols"
Cohesion: 0.05
Nodes (36): 1. Call for fire — 3 transmissions, 2. Location methods — rounding, 3. Corrections — order: deviation → range → HOB, 4. Danger close, 5. End of mission — RREMS, 6. FDC traffic you'll hear, 7. Prowords, 8. Performance pars (this sim) (+28 more)

### Community 86 - "CONTENT_INVENTORY.md — Port Content Inventory"
Cohesion: 0.17
Nodes (11): 1. Campaign structure, 2. Scenario templates (`SCN_META`, keyed by `SCN_TYPES`), 3. World feature set (`buildWorldFeatures()`), 4. Named data tables, 5. Assets to author in-engine, Chapters, Commendations (AAR medal strings), `CONFIG` tunables, grouped by system (+3 more)

### Community 87 - "perfprobe.js"
Cohesion: 0.20
Nodes (10): DSF, errors, frameStats(), here, out, simPath, state(), topSelf() (+2 more)

### Community 94 - "CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)"
Cohesion: 0.12
Nodes (16): 3-Layer Query Rule, Campaign rules (invariants — the campaign is BUILT, Foreword through Epilogue), CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer), Context Navigation (Graphify), Conventions, Dev tooling (permitted — amended 2026-07-30 by user decision), Domain facts — get these right, Don't (+8 more)

### Community 95 - "DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols"
Cohesion: 0.18
Nodes (10): Corrections (adjust phase), Danger close, Doctrinal performance standards (use for star pars), DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols, End of mission — RREMS, FDC → observer traffic, Prowords & vocabulary, Scripts are guidelines, not gates (+2 more)

### Community 96 - "QUICKSTART — Building SHITFIRE with Claude Code"
Cohesion: 0.18
Nodes (11): 0. Prereqs, 1. Set up the project (one time), 2. Launch Claude Code, 3. Build stage 1 (the MVP core loop), 4. Test, then commit, 5. Continue stage by stage, 6. Where the build actually is now, If a stage gets messy (+3 more)

### Community 97 - "mastermap_render.py"
Cohesion: 0.22
Nodes (4): elev_color(), MP(), render_island(), w2p()

### Community 101 - "PARITY_TESTING.md — The Browser Sim Is the Oracle"
Cohesion: 0.20
Nodes (10): Exporting fixtures, Level 1 — Pure function parity (fast, run on every commit), Level 2 — Scenario generation parity, Level 3 — Full mission replay, PARITY_TESTING.md — The Browser Sim Is the Oracle, The one thing that must be bit-exact, The standard to hold, Three levels of parity test (+2 more)

### Community 103 - "03-op-battery.js"
Cohesion: 0.24
Nodes (9): BATTERY, batteryMeshes, eye, findBattery(), findOP(), OP, placeBattery(), placeOP() (+1 more)

### Community 109 - "shots.js"
Cohesion: 0.22
Nodes (5): blocked, errors, here, outDir, simPath

### Community 112 - "ARCHITECTURE.md — Target Structure and What Actually Transfers"
Cohesion: 0.25
Nodes (8): ARCHITECTURE.md — Target Structure and What Actually Transfers, Interfaces to preserve by name, Rebuilds — browser-specific presentation and platform, Target structure, The line-by-line inventory, The rules that keep it honest, Three decisions worth making early, Transfers as logic — port the algorithm, not the code style

### Community 115 - "00-config.js"
Cohesion: 0.29
Nodes (6): acesFilmic(), acesFit(), CONFIG, KP_NAMES, SCN_META, SCN_TYPES

### Community 116 - "lint.js"
Cohesion: 0.25
Nodes (6): here, linter, manifest, messages, parts, srcDir

### Community 122 - "ENGINE_PORT — Plan and Strategy for Moving SHITFIRE to a Game Engine"
Cohesion: 0.29
Nodes (7): Before writing a single line of engine code, ENGINE_PORT — Plan and Strategy for Moving SHITFIRE to a Game Engine, Read in this order, Relationship to the browser build, The strategy in six sentences, The three rules that matter most, What is deliberately not in scope

### Community 128 - "ENGINE_CHOICE.md — Which Engine, and Why"
Cohesion: 0.33
Nodes (6): ENGINE_CHOICE.md — Which Engine, and Why, Evaluation, Risks that are independent of engine choice, The decision, What is explicitly out of scope for the port, What this product actually demands

### Community 129 - "perflegibility.js"
Cohesion: 0.33
Nodes (4): errors, here, outDir, simPath

### Community 135 - "replay.js"
Cohesion: 0.40
Nodes (4): files, here, root, simPath

### Community 140 - "docs.md"
Cohesion: 0.50
Nodes (3): Hard boundaries — violating these breaks the build for other people, How to write, Reporting back

### Community 141 - "research.md"
Cohesion: 0.50
Nodes (3): Hard boundaries, How to write the findings, Reporting back

### Community 142 - "serve.js"
Cohesion: 0.50
Nodes (3): MIME, root, server

## Knowledge Gaps
- **487 isolated node(s):** `CONFIG`, `SCN_META`, `KP_NAMES`, `SCN_TYPES`, `canvas` (+482 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `DIALOGUE_REVISIONS.md — HELLHOUND / GUNNY punch-up pass` connect `3. Revised QUIPS pools` to `DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols`?**
  _High betweenness centrality (0.026) - this node is a cross-community bridge._
- **Why does `setVision()` connect `08-burst-optics.js` to `02-scene.js`?**
  _High betweenness centrality (0.025) - this node is a cross-community bridge._
- **Why does `ROADMAP.md — the board` connect `3. THE LIVE BOARD — open work, by domain` to `DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols`?**
  _High betweenness centrality (0.020) - this node is a cross-community bridge._
- **What connects `CONFIG`, `SCN_META`, `KP_NAMES` to the rest of the system?**
  _487 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `10-mission.js` be split into smaller, more focused modules?**
  _Cohesion score 0.062111801242236024 - nodes in this community are weakly interconnected._
- **Should `08-burst-optics.js` be split into smaller, more focused modules?**
  _Cohesion score 0.06196078431372549 - nodes in this community are weakly interconnected._
- **Should `05-units-scenario.js` be split into smaller, more focused modules?**
  _Cohesion score 0.10826210826210826 - nodes in this community are weakly interconnected._