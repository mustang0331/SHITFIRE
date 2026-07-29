# QUICKSTART — Building SHITFIRE with Claude Code

## 0. Prereqs
Install Claude Code (see the official setup page: https://code.claude.com/docs). Have your Fable 5 / Console credits ready.

## 1. Set up the project (one time)
```bash
mkdir shitfire && cd shitfire
git init
# put these three files in the folder:
#   SPEC.md      <- rename the downloaded fo-trainer-oneshot-prompt.md to SPEC.md
#   CLAUDE.md    <- as provided
#   (nothing else needed — no npm, no package.json)
git add . && git commit -m "spec + project memory"
```

## 2. Launch Claude Code
```bash
claude
```
- Sign in on first launch (use the **Console account** your Fable 5 credits are on).
- Select the model: type `/model` and choose **Fable 5**.

## 3. Build stage 1 (the MVP core loop)
Paste this as your first prompt:

> Read SPEC.md and CLAUDE.md. Build **BUILD ORDER stage 1 only**: procedural stand-in island via `H(x,z)`, first-person OP camera, ONE static target (Position-under-attack, Normal), **typed input only**, rule-based GRID parser, direct-impact ballistics (first-round wide / follow-up tight), OT-frame corrections, shot/splash timing, burst effect, comms log, and AAR. Get the full loop working: observe → locate → transmit → observe impact → correct → fire for effect → assess. Single `index.html`, Three.js via CDN import map, no build step. Stop after stage 1 so I can test.

## 4. Test, then commit
- Open `index.html` in **Chrome**. Run a full mission. Fix issues with Claude.
- `git add . && git commit -m "stage 1: core loop"`

## 5. Continue stage by stage
After each stage tests clean and is committed, prompt:

> Stage 1 works and is committed. Now build **BUILD ORDER stage 2** (voice layer: Web Speech push-to-talk + SpeechSynthesis FDC, typed stays as fallback). Keep the stable interfaces. Stop after stage 2.

Repeat for stages 3–7 (polar+shift → FDC personality+audio → scenarios → map library → real DEMs+polish). **Commit between every stage.**

## 6. Epilogue (stage 11)

Stages 1–10 are complete and committed: real-DEM polish, world detail & population (structures, roads, villages, convoy pit stops, the FDC rant/snide deviation policy), the campaign skeleton (chapter data model, bookshelf volume/chapter menu, star grading, unlocks, persistence, Skirmish mode), and the narrative layer (chapter briefings, the Foreword tutorial with GUNNY BOTTLECAP, per-volume islands, Volume IV strict-net + 60mm mortars, named known points, commendations, and continuity quips). Only stage 11 remains. Prompt pattern:

> Read SPEC.md, CLAUDE.md, and NARRATIVE.md. Build **BUILD ORDER stage 11 only**: the three SUNBURN Epilogue chapters (E.1 THE GREAT CHOW RAID, E.2 CLAWS OUT, E.3 SUNLAMP ACTUAL), including the SUNLAMP directed-energy call for fire — **same direct-impact model**, `impact = aimpoint + error`, only TOF pacing (charging whine), prowords (DISCHARGE / SOLAR EVENT), beam visual, and audio differ; the locked "VOLUME V: ON WINGS" CAS tease stays a locked menu spine, do not build it. Finish with a balance pass on star pars across the campaign. Stop after stage 11 and commit.

**Commit when it tests clean.**

## Testing notes
- **Voice**: needs mic permission on a real Chrome tab (won't work in the Artifact sandbox).
- **Print / map library**: test via the browser print dialog (Save as PDF).
- **DEMs**: for stage 7, drop a grayscale island heightmap in the folder and tell Claude to wire it behind `H(x,z)`.

## If a stage gets messy
Roll back (`git restore .` or checkout the last commit) and re-prompt more narrowly. A clean stage 1–2 beats a broken all-in-one.
