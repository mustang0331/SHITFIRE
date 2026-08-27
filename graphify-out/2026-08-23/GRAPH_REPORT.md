# Graph Report - SHITFIRE  (2026-08-23)

## Corpus Check
- 69 files · ~474,510 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2878 nodes · 5380 edges · 193 communities (99 shown, 94 thin omitted)
- Extraction: 95% EXTRACTED · 5% INFERRED · 0% AMBIGUOUS · INFERRED: 265 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `2cc7380f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- three.module.min.js
- 10-mission.js
- ei
- 08-burst-optics.js
- 12-cff-handling.js
- np
- ui
- kr
- ti
- cs
- ic
- 3. Revised QUIPS pools
- .constructor
- .push
- .copy
- nr
- .applyMatrix4
- 07-voice-spot-audio.js
- .dot
- 04-world.js
- s
- ec
- ii
- 3. THE LIVE BOARD — open work, by domain
- cr
- .dispose
- 01-math-terrain.js
- n
- hm
- Yp
- jp
- 09-camera.js
- .add
- _p
- THE PROMPT
- as
- .setValues
- .fromArray
- .clone
- 02-scene.js
- xr
- .toJSON
- js
- 15-campaign-menu.js
- bd
- vl
- la
- oi
- 14-map-library.js
- vp
- .updateMatrixWorld
- .render
- GRAPHICS.md — Visual Upgrade Plan for SHITFIRE
- ta
- i
- .constructor
- 06-clock-comms.js
- .subVectors
- ld
- zl
- package.json
- .distanceTo
- .parse
- hl
- lm
- oa
- 16-input.js
- .getContext
- vu
- BALLISTICS_RESEARCH.md — real-world accuracy/lethality reference
- 17-loop-boot.js
- TLO.md — Terminal Learning Objective / Training Objectives Crosswalk
- perfsweep.js
- Tl
- xs
- xd
- tr
- PORT_PLAN.md — Staged Migration
- NARRATIVE.md — SHITFIRE Campaign Storyline
- TLO.md — Terminal Learning Objective / Training Objectives Crosswalk
- NARRATIVE.md — SHITFIRE Campaign Storyline
- build.js
- eu
- nh
- DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols
- CONTENT_INVENTORY.md — Port Content Inventory
- perfprobe.js
- EffectComposer
- Pass.js
- cp
- od
- jr
- tu
- CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)
- DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols
- QUICKSTART — Building SHITFIRE with Claude Code
- mastermap_render.py
- mu
- zh
- CHEATSHEET.md — FO Pull-Up Reference Card
- PARITY_TESTING.md — The Browser Sim Is the Oracle
- Appendix A — The Graphics Ceiling
- 03-op-battery.js
- EffectComposer.js
- Pass
- r
- fc
- PLAYBOOK.md — running Claude Code on SHITFIRE, effectively
- shots.js
- ml
- xm
- ARCHITECTURE.md — Target Structure and What Actually Transfers
- Callers & guest voices
- SHITFIRE
- 00-config.js
- lint.js
- am
- .equals
- yh
- ENGINE_PORT — Plan and Strategy for Moving SHITFIRE to a Game Engine
- 11-parser.js
- UnrealBloomPass
- ap
- im
- ENGINE_CHOICE.md — Which Engine, and Why
- perflegibility.js
- dh
- em
- jh
- replay.js
- bind
- jd
- lp
- rp
- docs.md
- research.md
- serve.js
- ShaderPass
- dp
- $l
- rh
- xp
- zd
- mastermap-collect.mjs
- Sky
- UnrealBloomPass.js
- au
- ia
- ju
- lu
- mp
- ql
- qp
- ru
- zc
- zu
- pre-commit
- KOFA_KING_VALLEY_FO_HEIGHTMAP.md
- three/README.md
- ds
- fh
- fs
- gs
- hs
- ih
- ip
- ko
- ms
- nc
- oh
- op
- ph
- pi
- ps
- ri
- sp
- uc
- uh
- us
- vd
- vs
- wd
- wi
- wl
- xh

## God Nodes (most connected - your core abstractions)
1. `ui` - 76 edges
2. `ei` - 68 edges
3. `ti` - 57 edges
4. `nr` - 47 edges
5. `ii()` - 40 edges
6. `cr` - 38 edges
7. `kr` - 38 edges
8. `n()` - 37 edges
9. `oi()` - 35 edges
10. `i()` - 35 edges

## Surprising Connections (you probably didn't know these)
- `fireAdjustRound()` --indirect_call--> `mission()`  [INFERRED]
  src/js/10-mission.js → src/js/17-loop-boot.js
- `fireForEffect()` --indirect_call--> `mission()`  [INFERRED]
  src/js/10-mission.js → src/js/17-loop-boot.js
- `updateScenario()` --indirect_call--> `showAAR()`  [INFERRED]
  src/js/05-units-scenario.js → src/js/13-aar.js
- `handleEOM()` --indirect_call--> `showAAR()`  [INFERRED]
  src/js/12-cff-handling.js → src/js/13-aar.js

## Import Cycles
- None detected.

## Communities (193 total, 94 thin omitted)

### Community 0 - "three.module.min.js"
Cohesion: 0.03
Nodes (74): _a, aa, ac, ar, bh, bp, br, bs (+66 more)

### Community 1 - "10-mission.js"
Cohesion: 0.06
Nodes (56): addRoundEffect(), AMMO, AMMO_WARNED, ammoLeft(), ammoPool(), ammoSpend(), armAtMyCommand(), assessEffect() (+48 more)

### Community 3 - "08-burst-optics.js"
Cohesion: 0.06
Nodes (40): addCrater(), applyVision(), BEAM, bursts, _crM4, _crN, _crQ, _crUp (+32 more)

### Community 4 - "12-cff-handling.js"
Cohesion: 0.07
Nodes (43): convoyHeadD(), enemyVolley(), figScale(), genScenario(), legibilityPass(), _legOne(), pathDir(), pathPoint() (+35 more)

### Community 5 - "np"
Cohesion: 0.05
Nodes (3): hp, np, zp

### Community 7 - "kr"
Cohesion: 0.06
Nodes (5): gi(), kr, qn(), up, Yn()

### Community 9 - "cs"
Cohesion: 0.06
Nodes (6): cs, Kn(), lh(), rs(), _s, ss()

### Community 11 - "3. Revised QUIPS pools"
Cohesion: 0.05
Nodes (40): 10.1 Two parser misses still live, 10.2 `noMission` and `unknown` are thin, and bursts prove it, 10.3 Missing: the in-mission repeat callback, 10.4 Proposed lines (tone: Generation Kill / WARFARE per the user's 2026-07-30 direction), 10. Night-run transcript review (2026-07-31) — the 18-05 / 18-34 / 20-01 exports, against post-14a code, 1. What stays untouched, and why, 2. What the transcript actually showed, 2a. Content boundary — no taking God's name in vain (+32 more)

### Community 12 - ".constructor"
Cohesion: 0.07
Nodes (14): K(), il, bt(), Ft(), Jt(), kt(), n(), Qt() (+6 more)

### Community 13 - ".push"
Cohesion: 0.11
Nodes (6): ed(), gu(), hu, ku(), ym(), l()

### Community 14 - ".copy"
Cohesion: 0.06
Nodes (10): fm, mm, nu, ou, pu, qs, td, ua (+2 more)

### Community 17 - "07-voice-spot-audio.js"
Cohesion: 0.09
Nodes (29): callerKickoff(), callerLine(), callerLocLine(), callerReply(), CALLERS, callerSay(), CARD4_ADJ, CARD4_DIR (+21 more)

### Community 18 - ".dot"
Cohesion: 0.09
Nodes (3): jn(), lr, pm

### Community 19 - "04-world.js"
Cohesion: 0.09
Nodes (30): _boxBatch, _boxUnit, buildVegetation(), buildWorldFeatures(), crossedQuadGeo(), FIG, flushBoxes(), isleName() (+22 more)

### Community 20 - "s"
Cohesion: 0.13
Nodes (22): v(), t(), ba(), c(), bu(), s(), c(), h() (+14 more)

### Community 21 - "ec"
Cohesion: 0.07
Nodes (6): bi, ci(), ec, pa(), Xn(), yi

### Community 23 - "3. THE LIVE BOARD — open work, by domain"
Cohesion: 0.06
Nodes (31): 1. Authority map — which file answers which question, 2. Work-order protocol (the anti-drift rules), 3.1 PERF — frame rate and responsiveness, 3.2 WORLD — what is on the island, and where, 3.3 ENEMY — what the enemy actually does, 3.4 NET — who is calling, and how badly, 3.5 TEMPO — time of day, immediacy, and the clock, 3.6 GFX — rendering beyond the shipped graphics plan (+23 more)

### Community 25 - ".dispose"
Cohesion: 0.09
Nodes (17): Ea(), i(), Hn, ja(), s(), ka(), i(), qa() (+9 more)

### Community 26 - "01-math-terrain.js"
Cohesion: 0.11
Nodes (19): binoFovNow(), binoPower(), clamp(), declMils(), demSample(), fbm(), fdcCall(), fdcShort() (+11 more)

### Community 27 - "n"
Cohesion: 0.13
Nodes (26): ao, bo(), co(), da(), r(), Do(), fo(), n() (+18 more)

### Community 31 - "09-camera.js"
Cohesion: 0.10
Nodes (24): binoEl, cheatEl, crosshairEl, cycleBinoZoom(), _dir, doLase(), drawReticle(), laseEl (+16 more)

### Community 34 - "THE PROMPT"
Cohesion: 0.08
Nodes (26): ADAPTIVE QUALITY & MOBILE, AUDIO (Web Audio, synthesized), BALLISTICS — DIRECT-IMPACT MODEL (NO TRAJECTORY SIM), BUILD ORDER — MVP FIRST (ship each stage working before the next), CALL FOR FIRE — INPUT & PARSER, CAMPAIGN — VOLUMES, CHAPTERS, STARS, COACHING & TRAINING FIDELITY (stage 12), CONTROLS (+18 more)

### Community 36 - ".setValues"
Cohesion: 0.08
Nodes (8): cd, dd, es, fl, hh, mh, rc, ud

### Community 37 - ".fromArray"
Cohesion: 0.09
Nodes (3): _h, jc, kh

### Community 38 - ".clone"
Cohesion: 0.09
Nodes (6): li, nm, qd, tc, Ys(), zs

### Community 39 - "02-scene.js"
Cohesion: 0.12
Nodes (22): applyTOD(), _bakeSun, buildTerrain(), camera, canvas, composerDrop(), hemi, loadDEMFile() (+14 more)

### Community 41 - ".toJSON"
Cohesion: 0.09
Nodes (6): bc, r(), s(), pp, qh, qu

### Community 42 - "js"
Cohesion: 0.09
Nodes (21): js/00-config.js, js/01-math-terrain.js, js/02-scene.js, js/03-op-battery.js, js/04-world.js, js/05-units-scenario.js, js/06-clock-comms.js, js/07-voice-spot-audio.js (+13 more)

### Community 43 - "15-campaign-menu.js"
Cohesion: 0.15
Nodes (20): ALL_CHAPTERS, briefEl, CAMP, CAMPAIGN, chUnlocked(), correctionEfficiency(), gradeMission(), menuEl (+12 more)

### Community 44 - "bd"
Cohesion: 0.13
Nodes (4): bd, md(), pd(), rd

### Community 45 - "vl"
Cohesion: 0.20
Nodes (20): s(), B(), F(), H(), O(), P(), V(), z() (+12 more)

### Community 48 - "14-map-library.js"
Cohesion: 0.15
Nodes (18): drawLegend(), drawMap(), drawWorldLayer(), facSymbol(), LEGEND_ROWS, libEl, mapCv, mapEl (+10 more)

### Community 50 - ".updateMatrixWorld"
Cohesion: 0.12
Nodes (4): el(), na, nl(), zm

### Community 51 - ".render"
Cohesion: 0.12
Nodes (5): kd, ma, A(), ra, S()

### Community 52 - "GRAPHICS.md — Visual Upgrade Plan for SHITFIRE"
Cohesion: 0.10
Nodes (20): 0. Baseline audit, 1. Ordering — do these in this order, Do not do, G0 — Config gate and no-regret renderer settings, G1 — Sky and time of day, G2 — Baked hillshade + ambient occlusion (highest value item), G3 — Near-field terrain LOD patch, G4 — Instanced vegetation (+12 more)

### Community 53 - "ta"
Cohesion: 0.14
Nodes (3): _d(), id, ta

### Community 54 - "i"
Cohesion: 0.31
Nodes (5): i(), si(), a(), c(), e()

### Community 55 - ".constructor"
Cohesion: 0.17
Nodes (3): A(), jl, R()

### Community 56 - "06-clock-comms.js"
Cohesion: 0.13
Nodes (11): applyComms(), clampComms(), commsDefaults(), commsEl, commsHeadEl, events, FDC, loadComms() (+3 more)

### Community 59 - "ld"
Cohesion: 0.14
Nodes (3): fd(), hd, ld

### Community 60 - "zl"
Cohesion: 0.16
Nodes (10): sa, g(), v(), y(), ya(), f(), u(), x() (+2 more)

### Community 61 - "package.json"
Cohesion: 0.12
Nodes (15): eslint, globals, playwright-core, dependencies, playwright-core, description, devDependencies, eslint (+7 more)

### Community 62 - ".distanceTo"
Cohesion: 0.17
Nodes (4): ah, Sc, vc, xc()

### Community 64 - "hl"
Cohesion: 0.16
Nodes (7): hl(), j(), q(), W(), Yt(), wp, yr

### Community 67 - "16-input.js"
Cohesion: 0.17
Nodes (13): endPTT(), micHold(), noteBtn, padPrev, pollGamepad(), pttEl, pttLiveEl, refreshNoteBtn() (+5 more)

### Community 68 - ".getContext"
Cohesion: 0.15
Nodes (7): ai(), fi(), St(), mi, vi, f(), m()

### Community 69 - "vu"
Cohesion: 0.15
Nodes (6): su, _u(), vu(), wu(), xu(), yu()

### Community 70 - "BALLISTICS_RESEARCH.md — real-world accuracy/lethality reference"
Cohesion: 0.14
Nodes (13): 1. What the sim currently does (baseline, from `index.html`), 2. Real dispersion: bias vs. random error (this part the sim already gets right), 3. Range-scaling gap and why it matters here, 4. Mortar-specific: dispersion is right, time of flight is not, 5. Casualty/lethal radius — the sim doesn't model this at all, and arguably shouldn't fully, 6. Danger close: doctrine's flat 600 m is a deliberate simplification, and it's the right one, 7. Smoke & illumination — reference figures for the parked 12h row, 8. Summary of findings, ranked by whether they're worth doing anything about (+5 more)

### Community 71 - "17-loop-boot.js"
Cohesion: 0.21
Nodes (11): applyCorrection(), animate(), applyQuality(), binoHdgEl, headingEl, mission(), msnclockEl, qRatio() (+3 more)

### Community 72 - "TLO.md — Terminal Learning Objective / Training Objectives Crosswalk"
Cohesion: 0.14
Nodes (13): 1. Purpose and Scope, 2. Course Terminal Learning Objective, 3. Enabling Learning Objectives by Volume, 4. Assessment Methodology, 5. Chapter Crosswalk, 6. Future Objectives (not yet built) / Built-but-not-chaptered, Assessment gaps — CLOSED (stage 12), FOREWORD — The Schoolhouse (tutorial, guided, non-punitive) (+5 more)

### Community 73 - "perfsweep.js"
Cohesion: 0.14
Nodes (12): DSF, errors, here, rows, simPath, sx, sxx, sxy (+4 more)

### Community 75 - "xs"
Cohesion: 0.15
Nodes (3): gh, wh, xs

### Community 76 - "xd"
Cohesion: 0.14
Nodes (3): sd, xd, yd

### Community 78 - "PORT_PLAN.md — Staged Migration"
Cohesion: 0.15
Nodes (13): Effort shape (not a schedule), Keeping the browser build alive, P0 — Freeze the oracle *(do this before writing any engine code)*, P1 — Core skeleton and the PRNG proof, P2 — Sim core: ballistics, missions, doctrine, P3 — Engine shell: world and observer, P4 — The loop closes: fire missions end to end, P5 — Cartography (+5 more)

### Community 79 - "NARRATIVE.md — SHITFIRE Campaign Storyline"
Cohesion: 0.15
Nodes (12): Characters, EPILOGUE — SUNBURN *(unlocked by finishing Volume IV · the goofy-ass shit)*, FOREWORD — THE SCHOOLHOUSE *(tutorial — always unlocked)*, Humor rules, NARRATIVE.md — SHITFIRE Campaign Storyline, Premise, Structure, grading, and unlocks, VOLUME I — GREEN AS GRASS *(fundamentals · grid missions · first island)* (+4 more)

### Community 80 - "TLO.md — Terminal Learning Objective / Training Objectives Crosswalk"
Cohesion: 0.15
Nodes (13): 1. Purpose and Scope, 2. Course Terminal Learning Objective, 3. Enabling Learning Objectives by Volume, 4. Assessment Methodology, 5. Chapter Crosswalk, 6. Future Objectives (not yet assessed / not yet built), FOREWORD — The Schoolhouse (tutorial, guided, non-punitive), Known assessment gaps (+5 more)

### Community 81 - "NARRATIVE.md — SHITFIRE Campaign Storyline"
Cohesion: 0.15
Nodes (12): Characters, EPILOGUE — SUNBURN *(unlocked by finishing Volume IV · the goofy-ass shit)*, FOREWORD — THE SCHOOLHOUSE *(tutorial — always unlocked)*, Humor rules, NARRATIVE.md — SHITFIRE Campaign Storyline, Premise, Structure, grading, and unlocks, VOLUME I — GREEN AS GRASS *(fundamentals · grid missions · first island)* (+4 more)

### Community 82 - "build.js"
Cohesion: 0.15
Nodes (9): built, head, here, manifest, outPath, parts, PP_REWRITE, root (+1 more)

### Community 85 - "DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols"
Cohesion: 0.17
Nodes (11): Corrections (adjust phase), Danger close, Doctrinal performance standards (use for star pars), DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols, End of mission — RREMS, FDC → observer traffic, Pre-mission and effects data (researched 2026-07-29 for G10/G13/G15/G16/G18), Prowords & vocabulary (+3 more)

### Community 86 - "CONTENT_INVENTORY.md — Port Content Inventory"
Cohesion: 0.17
Nodes (11): 1. Campaign structure, 2. Scenario templates (`SCN_META`, keyed by `SCN_TYPES`), 3. World feature set (`buildWorldFeatures()`), 4. Named data tables, 5. Assets to author in-engine, Chapters, Commendations (AAR medal strings), `CONFIG` tunables, grouped by system (+3 more)

### Community 87 - "perfprobe.js"
Cohesion: 0.20
Nodes (10): DSF, errors, frameStats(), here, out, simPath, state(), topSelf() (+2 more)

### Community 89 - "Pass.js"
Cohesion: 0.18
Nodes (5): _camera, FullScreenQuad, FullscreenTriangleGeometry, _geometry, TODO: Avoid using autoClear properties, see…

### Community 91 - "od"
Cohesion: 0.17
Nodes (3): gd(), o(), od

### Community 94 - "CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)"
Cohesion: 0.18
Nodes (11): Campaign rules (invariants — the campaign is BUILT, Foreword through Epilogue), CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer), Conventions, Dev tooling (permitted — amended 2026-07-30 by user decision), Domain facts — get these right, Don't, Golden rules (do not violate), Keep these interfaces stable (later stages depend on them) (+3 more)

### Community 95 - "DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols"
Cohesion: 0.18
Nodes (10): Corrections (adjust phase), Danger close, Doctrinal performance standards (use for star pars), DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols, End of mission — RREMS, FDC → observer traffic, Prowords & vocabulary, Scripts are guidelines, not gates (+2 more)

### Community 96 - "QUICKSTART — Building SHITFIRE with Claude Code"
Cohesion: 0.18
Nodes (11): 0. Prereqs, 1. Set up the project (one time), 2. Launch Claude Code, 3. Build stage 1 (the MVP core loop), 4. Test, then commit, 5. Continue stage by stage, 6. Where the build actually is now, If a stage gets messy (+3 more)

### Community 97 - "mastermap_render.py"
Cohesion: 0.22
Nodes (4): elev_color(), MP(), render_island(), w2p()

### Community 100 - "CHEATSHEET.md — FO Pull-Up Reference Card"
Cohesion: 0.20
Nodes (9): 1. Call for fire — 3 transmissions, 2. Location methods — rounding, 3. Corrections — order: deviation → range → HOB, 4. Danger close, 5. End of mission — RREMS, 6. FDC traffic you'll hear, 7. Prowords, 8. Performance pars (this sim) (+1 more)

### Community 101 - "PARITY_TESTING.md — The Browser Sim Is the Oracle"
Cohesion: 0.20
Nodes (10): Exporting fixtures, Level 1 — Pure function parity (fast, run on every commit), Level 2 — Scenario generation parity, Level 3 — Full mission replay, PARITY_TESTING.md — The Browser Sim Is the Oracle, The one thing that must be bit-exact, The standard to hold, Three levels of parity test (+2 more)

### Community 102 - "Appendix A — The Graphics Ceiling"
Cohesion: 0.20
Nodes (10): Appendix A — The Graphics Ceiling, Binding constraint, Ceiling by subsystem, Recommended practical max, The asymmetry, The gift of this app's camera, Training caveat, Two highest-leverage items not in the main plan (+2 more)

### Community 103 - "03-op-battery.js"
Cohesion: 0.24
Nodes (9): BATTERY, batteryMeshes, eye, findBattery(), findOP(), OP, placeBattery(), placeOP() (+1 more)

### Community 104 - "EffectComposer.js"
Cohesion: 0.29
Nodes (3): ClearMaskPass, MaskPass, CopyShader

### Community 105 - "Pass"
Cohesion: 0.20
Nodes (3): Pass, TODO: Avoid using autoClear properties, see…, RenderPass

### Community 106 - "r"
Cohesion: 0.29
Nodes (7): E(), r(), A(), Nd, ol(), b(), E()

### Community 108 - "PLAYBOOK.md — running Claude Code on SHITFIRE, effectively"
Cohesion: 0.22
Nodes (8): 1. The one rule that makes multiple chats safe, 2. The session shape that works, 3. Verification is the engine — never accept "done" without a gate, 4. Feed the machine your play — it's the best backlog generator, 5. What to keep for yourself (Claude's real limits here), 6. Prompting patterns that paid off (and one that doesn't), 7. Session-start checklist (any chat, any time), PLAYBOOK.md — running Claude Code on SHITFIRE, effectively

### Community 109 - "shots.js"
Cohesion: 0.22
Nodes (5): blocked, errors, here, outDir, simPath

### Community 110 - "ml"
Cohesion: 0.22
Nodes (7): cl(), dl(), gl(), ll(), ml(), rl(), ul()

### Community 112 - "ARCHITECTURE.md — Target Structure and What Actually Transfers"
Cohesion: 0.25
Nodes (8): ARCHITECTURE.md — Target Structure and What Actually Transfers, Interfaces to preserve by name, Rebuilds — browser-specific presentation and platform, Target structure, The line-by-line inventory, The rules that keep it honest, Three decisions worth making early, Transfers as logic — port the algorithm, not the code style

### Community 113 - "Callers & guest voices"
Cohesion: 0.25
Nodes (8): Caller archetypes, Callers & guest voices, Guest voices — the umbrella, Humor placement, The concept, The wrongness mechanic, Tone, What the sim implements — the contract

### Community 114 - "SHITFIRE"
Cohesion: 0.25
Nodes (8): Controls, Current state, Files, Roadmap, Run it, Scope: surface-to-surface only (for now), SHITFIRE, The Campaign — Volumes, Chapters, Stars

### Community 115 - "00-config.js"
Cohesion: 0.29
Nodes (6): acesFilmic(), acesFit(), CONFIG, KP_NAMES, SCN_META, SCN_TYPES

### Community 116 - "lint.js"
Cohesion: 0.25
Nodes (6): here, linter, manifest, messages, parts, srcDir

### Community 117 - "am"
Cohesion: 0.32
Nodes (3): am(), rm, sm()

### Community 122 - "ENGINE_PORT — Plan and Strategy for Moving SHITFIRE to a Game Engine"
Cohesion: 0.29
Nodes (7): Before writing a single line of engine code, ENGINE_PORT — Plan and Strategy for Moving SHITFIRE to a Game Engine, Read in this order, Relationship to the browser build, The strategy in six sentences, The three rules that matter most, What is deliberately not in scope

### Community 123 - "11-parser.js"
Cohesion: 0.33
Nodes (5): DIGIT_WORDS, normalize(), parseMessage(), STT_FIX, TENS_WORDS

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
- **557 isolated node(s):** `CONFIG`, `SCN_META`, `KP_NAMES`, `SCN_TYPES`, `canvas` (+552 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **94 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `ui` connect `ui` to `three.module.min.js`, `.add`, `.applyQuaternion`, `xr`, `ic`, `.constructor`, `tr`, `oi`, `.dot`, `ii`, `.subVectors`, `.multiplyScalar`, `jr`, `.distanceTo`?**
  _High betweenness centrality (0.044) - this node is a cross-community bridge._
- **Why does `ti` connect `ti` to `three.module.min.js`, `.clone`, `ic`, `tr`, `.dot`, `ec`, `.constructor`, `.dispose`, `.multiplyScalar`?**
  _High betweenness centrality (0.030) - this node is a cross-community bridge._
- **Why does `ei` connect `ei` to `three.module.min.js`, `.fromArray`, `ui`, `ic`, `.updateMatrixWorld`, `s`, `cr`, `.multiplyScalar`?**
  _High betweenness centrality (0.025) - this node is a cross-community bridge._
- **What connects `CONFIG`, `SCN_META`, `KP_NAMES` to the rest of the system?**
  _557 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `three.module.min.js` be split into smaller, more focused modules?**
  _Cohesion score 0.025 - nodes in this community are weakly interconnected._
- **Should `10-mission.js` be split into smaller, more focused modules?**
  _Cohesion score 0.06308610400682012 - nodes in this community are weakly interconnected._
- **Should `ei` be split into smaller, more focused modules?**
  _Cohesion score 0.03634085213032581 - nodes in this community are weakly interconnected._