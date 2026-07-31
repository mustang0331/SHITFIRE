# CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)

A single-file browser trainer for the FORWARD OBSERVER (not the gun crew).

**Start every session by reading [ROADMAP.md](ROADMAP.md).** It is the single authority on what ships next and what is done, it names the one active work order, and its §1 says which file answers which question. Do not infer status from SPEC.md, README.md, or this file — they drift; the board does not. **[PLAYBOOK.md](PLAYBOOK.md)** is the how: the proven multi-chat strategy for running Claude Code on this project (one row per chat, docs to background agents, the verification stack, feedback triage).

**SPEC.md** ("BUILD ORDER") is a historical design record — the build it ordered is complete. The campaign storyline (volumes, chapters, characters, humor rules) lives in **NARRATIVE.md**. **DOCTRINE.md** (distilled from JFIRE 2019 + the JFO Student Handout) is the authority for CFF formats, prowords, rounding, readback protocol, and strict-mode rules — check it before writing any parser or FDC traffic.

## Golden rules (do not violate)
- **The shipped artifact is one self-contained `SHITFIRE.html` that runs with nothing installed.** No runtime npm dependency, no backend, no API keys. **Opening it in Chrome must always Just Work on a machine with nothing but a browser** — that is the invariant, and it is the *only* thing "one file" was ever protecting.
- **Source lives in `src/`; `SHITFIRE.html` is a BUILD ARTIFACT** (source split 2026-07-30, user-approved). Edit `src/js/*.js` and the shell in `src/shell/`, then `node tools/build.js` regenerates `SHITFIRE.html` by **deterministic concatenation** in `src/manifest.json` order — a paste, not a bundler; no minify, no transform, no dependency. **Never hand-edit `SHITFIRE.html`** — the next build overwrites it, and the pre-commit hook rejects a commit whose `SHITFIRE.html` is not the current build of `src/`. Module ORDER in the manifest is load-bearing (hoisting, top-level `const`/`let`, parser branch precedence). The build adds nothing to the artifact and needs nothing installed to *run* it — only to *rebuild* it.
- Three.js r160 is **vendored in `vendor/` and INLINED into the artifact** by the build as `data:` URLs in the import map — the shipped file runs with **zero network** (field/classroom/USB use, verified by `tools/shots.js`, which blocks all http(s) and fails on any request). Chrome forbids ES-module imports over `file://`, so a CDN or a sibling vendor file could never satisfy the double-click invariant — inlining is the only shape that is one-file, offline, and double-clickable at once. Never point the import map back at a CDN. No other external deps.
- Only optional external input: a grayscale terrain heightmap/DEM (see SPEC "TERRAIN").
- Voice = browser **Web Speech API** (`SpeechRecognition` + `SpeechSynthesis`), client-side, keyless. **Typed input is a mandatory fallback.**
- FDC is **rule-based JS** (no LLM).
- Target 60 fps. Preallocate; no per-frame allocation in the render loop.

## Keep these interfaces stable (later stages depend on them)
`H(x,z)` · `fireMission(targetLocation)` · `applyCorrection(otFrameDelta)` · `FDC.say(msg)` · `Scenario` · `gradeMission(metrics) → stars` (stage 9+) · **TLOG** (session transcript — logs comms, parse classification, impacts, and AAR outcomes; exportable text/JSON from the mission menu, persisted) used for dialogue review/correction — treat its entry format as stable so exports stay diffable across stages

## Domain facts — get these right
- **Ballistics = direct-impact model.** The IMPACT POINT is always `impact = aimpoint + error` — never simulate a trajectory, drag, or firing tables to *place* a round. **Terminal EFFECTS at that point may be modeled statistically** — fuze (airburst/delay/quick), sheaf shape, target posture (standing/prone/dug-in) and per-asset lethal radius may scale casualties/probability of effect, per DOCTRINE.md's researched figures (§Pre-mission and effects data). That is `effect = f(where it landed, how it was fused, what it hit)`, not a flight model. The line that must never be crossed: nothing computes *where a round goes* from muzzle velocity, angle, or drag.
- **First round** deviates randomly within a range; **follow-up rounds** are significantly tighter.
- **Corrections are in the observer-target (OT) frame.** Convert to a world delta via OT azimuth and move `aimpoint`. **No angle-T / gun-line rotation** — gun assumed to execute perfectly.
- Callsigns: observer **MUSTANG 12**, FDC **HELLHOUND FIRES**. FDC tone: dark-humored, sardonic, sharp on wrong calls — but never break the doctrinal readback.
- Grid precision by asset: **artillery 6-digit/100 m; 60mm mortar 8-digit/10 m; SUNLAMP (Epilogue) accepts 10-digit/1 m** (a design simplification — doctrine ties fine grids to laser-grid missions; 6-digit is always acceptable).
- CFF = **6 elements in 3 transmissions, each read back by the FDC**; corrections in order **deviation → range** ("left/right" nearest 10 m, "add/drop" in 100 m multiples); **danger close is per firing asset — within 600 m of friendlies for the 155 battery, 250 m for the 60mm section** (`dangerCloseRadius()`, F2; expect creeping fire, ≤100 m corrections); end of mission = **RREMS** with surveillance terms suppressed/neutralized/destroyed. Details: DOCTRINE.md.
- Fratricide (hitting a friendly element) = automatic mission fail (and 0 stars). **Fail ≠ end** (user correction, 2026-07-29): the failure is permanent, but the mission does **not** auto-end — the target must still be suppressed/neutralized/destroyed and the mission closed with RREMS. Do not reintroduce the auto-AAR.
- **Civilian casualties (collateral damage) = automatic mission fail**, identical to fratricide — including fail ≠ end. Civilian villages/figures render distinctly from military elements.
- **Permanent structures and roads must appear on the printed map sheets and the `[M]` map** (symbols + legend) so terrain association is a practicable skill (resect off the airfield, the radio mast, the village). Enemy positions are never plotted.
- **Convoy pit stops:** moving convoys make seeded **1–3 minute stops** at gas stations/ammo depots/airfields along their route — catching a column halted differs from leading it on the move.
- **FDC deviation policy:** doctrinal scripts are guidelines, flexible not rigid. **Dangerous deviation** (danger close w/o proword, corrections walking toward friendlies/civilians, unresolved-unsafe FFE) → FDC **rant**. **Stupid-but-safe deviation** (wrong element order, malformed-but-unambiguous calls, absurd rounding) → **snide remark**, mission proceeds. Strict mode still enforces format for grading — the rant/snide split is tone, not an extra gate.
- **Surface-to-surface fires only.** CAS/9-line is future "Volume V" — a locked menu tease at most; never build it unasked.

## Campaign rules (invariants — the campaign is BUILT, Foreword through Epilogue)
- Mission menu = **book series**: Volumes → Chapters, per NARRATIVE.md (Foreword tutorial → Volumes I–IV → Epilogue). Chapters are **fixed-seed** missions; `[N]` is **Skirmish** mode. Epilogue scenario types (`chow`, `kaiju`) are chapter-only — never in the skirmish rotation.
- **Star grading, original-MW2 style:** 0–5★ per chapter from competency (rounds-to-effect, first-round miss, format, time vs. par), capped by difficulty (Easy 3 / Normal 4 / Hard 5), shown in the mission menu and AAR. Volumes unlock at cumulative star thresholds.
- The Epilogue's directed-energy mission (SUNLAMP) **uses the direct-impact model** — only pacing, prowords, visuals, and audio differ. The beam is presentation standing on a point the ordinary machinery already resolved.
- Humor: follow NARRATIVE.md's humor rules — sprinkle in story chapters, dial to 11 only in the Epilogue, and **never let a joke replace the doctrinal readback**. Content boundary (user direction, 2026-07-30): HELLHOUND's profanity never takes God's name in vain.

## Conventions
- Vanilla JS, ES modules, no framework.
- All constants in a top-level `CONFIG` object, grouped by system.
- Seeded PRNG for reproducible scenarios; dispersion draws from the same stream.

## Testing
- Open `SHITFIRE.html` directly in **Chrome** (voice + print are Chrome/Edge desktop). Or run
  `tools/serve.cmd` for a localhost origin where mic permission persists across sessions.
- Voice needs mic permission on a real tab (asked once per session on `file://`). Test print/save
  via the browser print dialog.
- If ever run as a claude.ai Artifact: use **in-memory state, not `localStorage`** (blocked there).
- **The gate stack, in the order a code row runs it** (details + per-gate purpose in PLAYBOOK.md §3):
  `node tools/build.js` (pre-commit enforces the byte-exact artifact) · `node tools/lint.js`
  (0 correctness errors) · `tools/syntaxgate.ps1` + `cscript tools/parser-order.js` (parse + branch
  precedence, automatic on commit) · `node tools/shots.js` (boots clean, fully offline, 12 screenshot
  states — **look at the frames**) · `node tools/replay.js` (parser vs every recorded transcript, on
  any parser-adjacent row) · plus a fresh scratchpad harness per row that fails before the fix.

## Dev tooling (permitted — amended 2026-07-30 by user decision)
Test, lint, and QA tooling **is allowed**. The golden rule was never about keeping the repo austere —
it is about the *shipped artifact* needing no install. Three conditions, all non-negotiable:

1. **Tooling lives in `tools/`** with its own `package.json`; `node_modules/` is gitignored. Nothing
   the tooling needs may leak into the repo root or into `SHITFIRE.html`.
2. **Tools only ever READ `SHITFIRE.html`.** No tool may rewrite, preprocess, generate, or reformat
   the sim file. **This bans auto-formatters outright** — reflowing a 200 KB single file destroys
   review and `git blame` for zero functional gain.
3. **The invariant holds:** `SHITFIRE.html` opens in Chrome and runs with nothing installed. If a
   change ever makes tooling *required* to run the sim, the change is wrong no matter how useful the
   tool is.

Tooling is never a gate on the sim's design. If a tool can't cope with the single-file structure, the
tool loses, not the file.

## Don't
- **Don't have two writers on the same `src/` module at once, and never hand-edit `SHITFIRE.html`.** The source split means two agents *can* now safely work different modules in parallel — that is the point of it — but two on one module still collides, and editing the built artifact directly is lost on the next build. One writer per module; rebuild, don't hand-patch.
- Don't build a whole stage at once when it has lettered rows — ship one ROADMAP row, commit, then the next.
- Don't add a trajectory sim, angle-T math, a runtime bundler, a server, or a **runtime** npm dependency. (The dev-only concat build and test tooling are allowed — see *Dev tooling*. "No bundler" means the shipped artifact is never webpack/rollup output; `tools/build.js` is a deterministic paste.)
- Don't let voice be load-bearing — the typed core must fully work on its own.
- Don't break the stable interfaces above.
- Don't mark anything done from memory. Grep the code or run it.

## Workflow — one work order at a time
The build-out phase is complete (2026-07-30); the project is in its **play → feedback → triage →
small rows** era. New rows are born from `user_feedback.md` triage and TLOG transcript review, not
from a spec. The rules below still govern every row that gets born.
1. **Read ROADMAP.md.** Take the single row marked `NEXT`. If none is marked, ask — don't pick one.
2. **One row = one commit**, message prefixed with the row ID: `stage 13c: sky + time of day`, `fix F1: range rounding gate`. Matches existing history (`stage 12e: OT direction and RREMS end of mission`).
3. **Pass the row's gate before committing.** The gate is in the ROADMAP table; graphics rows also run [GRAPHICS.md](GRAPHICS.md) §Verification checklist. Code existing ≠ row done.
4. **Docs land in a separate follow-up commit** — never bundled into the code commit, or the diff stops being reviewable. That follow-up is what flips the row to `DONE`.
5. **Model split (updated 2026-07-30 — Fable has free rein, per user).** Sim (`src/`) code →
   **Fable**, in the main thread or dispatched as a subagent; both are allowed. Doc/planning files
   (README, SPEC, NARRATIVE, CLAUDE, QUICKSTART, DOCTRINE, TLO, CHEATSHEET, ROADMAP, GRAPHICS,
   DIALOGUE_REVISIONS) → **Sonnet subagent** by default — that's cost-efficiency guidance, not a
   prohibition. The one hard rule regardless of model: an agent gets an explicit file allowlist,
   and one writer per `src/` module.
6. If a row turns out to be two things, **split the row in ROADMAP.md first**, then build.
- Review diffs before merging. If a row gets messy, `git restore .` and re-prompt narrower — a clean partial beats a broken whole.
