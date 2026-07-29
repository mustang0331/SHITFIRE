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

## 6. World detail & campaign stages (8–11)

Stages 1–7 are complete. Stage 8 populates the islands themselves — structures, roads, villages — before the campaign wraps a story around them. Prompt pattern:

> Read SPEC.md and CLAUDE.md. Build **BUILD ORDER stage 8 only**: seeded, terrain-aware structures/roads/dirt-paths/landmarks (military: ammo depot, fuel point, airfield strip, radio mast, coastal-gun emplacement, watchtower; civilian: huts, villages), low-poly/instanced/merged geometry (60 fps rule stands); permanent structures + roads on the printable map sheets and the [M] map with symbols + a legend for terrain association (never enemy positions); civilian villages with wandering civilians, rendered distinctly — civilian casualties are an automatic mission fail, same as fratricide; convoy pit stops (seeded 1–3 minutes at fuel/ammo/airfield facilities); the FDC deviation policy (scripts are guidelines — dangerous deviation triggers the rant system, stupid-but-safe deviation gets a snide remark and the mission proceeds; strict mode still enforces format for grading). Stop after stage 8.

Then the campaign phase adds the volume/chapter story mode on top of the now-populated islands — **read NARRATIVE.md alongside SPEC.md** for these. Prompt pattern:

> Read SPEC.md, CLAUDE.md, and NARRATIVE.md. Build **BUILD ORDER stage 9 only**: chapter data model (fixed seeds), bookshelf volume/chapter mission menu with MW2-style 0–5 star display, `gradeMission(metrics)` with difficulty caps, sequential + star-gated unlocks, persistence, and [N] as Skirmish mode. Surface-to-surface only — Volume V (CAS) is a locked tease, do not build it. Stop after stage 9.

Then stage 10 (narrative layer: briefings, Foreword tutorial, per-volume islands, strict-mode Volume IV, 60mm chapters, humor per NARRATIVE.md) and stage 11 (Epilogue incl. the SUNLAMP directed-energy mission — same direct-impact model — plus balance pass). **Commit between every stage.**

## Testing notes
- **Voice**: needs mic permission on a real Chrome tab (won't work in the Artifact sandbox).
- **Print / map library**: test via the browser print dialog (Save as PDF).
- **DEMs**: for stage 7, drop a grayscale island heightmap in the folder and tell Claude to wire it behind `H(x,z)`.

## If a stage gets messy
Roll back (`git restore .` or checkout the last commit) and re-prompt more narrowly. A clean stage 1–2 beats a broken all-in-one.
