# QUICKSTART — Building SHITFIRE with Claude Code

## 0. Prereqs
Install Claude Code (see the official setup page: https://code.claude.com/docs).

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
- Sign in on first launch.
- Select the model: type `/model` and choose **Fable**. Sim (`SHITFIRE.html`) work is done directly by Fable unless it decides to use subagents; doc-only passes can be handed to a Sonnet subagent.

## 3. Build stage 1 (the MVP core loop)
Paste this as your first prompt:

> Read SPEC.md and CLAUDE.md. Build **BUILD ORDER stage 1 only**: procedural stand-in island via `H(x,z)`, first-person OP camera, ONE static target (Position-under-attack, Normal), **typed input only**, rule-based GRID parser, direct-impact ballistics (first-round wide / follow-up tight), OT-frame corrections, shot/splash timing, burst effect, comms log, and AAR. Get the full loop working: observe → locate → transmit → observe impact → correct → fire for effect → assess. Single `SHITFIRE.html`, Three.js via CDN import map, no build step. Stop after stage 1 so I can test.

## 4. Test, then commit
- Open `SHITFIRE.html` in **Chrome**. Run a full mission. Fix issues with Claude.
- `git add . && git commit -m "stage 1: core loop"`

## 5. Continue stage by stage
After each stage tests clean and is committed, prompt:

> Stage 1 works and is committed. Now build **BUILD ORDER stage 2** (voice layer: Web Speech push-to-talk + SpeechSynthesis FDC, typed stays as fallback). Keep the stable interfaces. Stop after stage 2.

Repeat for stages 3–7 (polar+shift → FDC personality+audio → scenarios → map library → real DEMs+polish). **Commit between every stage.**

## 6. Where the build actually is now

Stages 1–10 are complete and committed, and most of stage 12 landed after them (12a–12e). **[ROADMAP.md](ROADMAP.md) is the board** — it names the one row marked `NEXT`, and the order is no longer simply "stage N then N+1." Current order: **stage 13 (visual overhaul) → stage 12 remainder → stage 11 (Epilogue)**. Stage numbers are stable IDs, not sequence.

### The prompt pattern, from here on

One ROADMAP row per prompt. One row per commit. Never two.

> Read ROADMAP.md, CLAUDE.md, and SPEC.md. Build **ROADMAP row 13a only** (the bino quality pin — stop adaptive quality dropping pixel ratio while binos are up; detail in GRAPHICS.md §G0.4). Pass the row's gate before committing: troop figures must stay countable through binos at 3000 m after a forced quality step-down. Keep the stable interfaces. Commit as `stage 13a: bino quality pin`. Stop after this row.

Then flip the row to `DONE` in ROADMAP.md as a **separate doc commit** — never bundled into the code commit.



## Testing notes
- **Voice**: needs mic permission on a real Chrome tab (won't work in the Artifact sandbox).
- **Microphone permission**: double-clicked `SHITFIRE.html` is a `file://` page, so Chrome has no
  origin to remember a mic grant against — expect **one prompt per session**, not one per
  transmission (the first push-to-talk holds the mic stream open for the rest of the tab's life; the
  stream is never recorded or read). To grant once and be done forever, double-click
  `tools/serve.cmd` instead: it launches a stdlib-only Node static server at `http://localhost:8137`
  and opens the same sim there, and a real origin means Chrome remembers the grant permanently after
  the first click. Needs Node (already on the dev machine); strictly optional — double-clicking
  `SHITFIRE.html` directly still works with nothing installed.
- **Print / map library**: test via the browser print dialog (Save as PDF).
- **DEMs**: for stage 7, drop a grayscale island heightmap in the folder and tell Claude to wire it behind `H(x,z)`.
- **DEV UNLOCK**: a toggle in the mission menu's DEVELOPER row opens every volume/chapter regardless of star progress or sequence, without touching saved progress — flip it on to jump straight to a late-campaign chapter for testing instead of grinding unlocks.

## If a stage gets messy
Roll back (`git restore .` or checkout the last commit) and re-prompt more narrowly. A clean stage 1–2 beats a broken all-in-one.
