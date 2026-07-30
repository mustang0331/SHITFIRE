# CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)

A single-file browser trainer for the FORWARD OBSERVER (not the gun crew).

**Start every session by reading [ROADMAP.md](ROADMAP.md).** It is the single authority on what ships next and what is done, it names the one active work order, and its §1 says which file answers which question. Do not infer status from SPEC.md, README.md, or this file — they drift; the board does not.

Scope per stage lives in **SPEC.md** ("BUILD ORDER"). The campaign storyline (volumes, chapters, characters, humor rules) lives in **NARRATIVE.md**. **DOCTRINE.md** (distilled from JFIRE 2019 + the JFO Student Handout) is the authority for CFF formats, prowords, rounding, readback protocol, and strict-mode rules — check it before writing any parser or FDC traffic.

## Golden rules (do not violate)
- **One `SHITFIRE.html`.** (renamed from index.html 2026-07-29; still one self-contained file, no build step, no bundler, no npm, no backend, no API keys.)
- Three.js + addons via **CDN import map** only. No other external deps.
- Only optional external input: a grayscale terrain heightmap/DEM (see SPEC "TERRAIN").
- Voice = browser **Web Speech API** (`SpeechRecognition` + `SpeechSynthesis`), client-side, keyless. **Typed input is a mandatory fallback.**
- FDC is **rule-based JS** (no LLM).
- Target 60 fps. Preallocate; no per-frame allocation in the render loop.

## Keep these interfaces stable (later stages depend on them)
`H(x,z)` · `fireMission(targetLocation)` · `applyCorrection(otFrameDelta)` · `FDC.say(msg)` · `Scenario` · `gradeMission(metrics) → stars` (stage 9+) · **TLOG** (session transcript — logs comms, parse classification, impacts, and AAR outcomes; exportable text/JSON from the mission menu, persisted) used for dialogue review/correction — treat its entry format as stable so exports stay diffable across stages

## Domain facts — get these right
- **Ballistics = direct-impact model.** `impact = aimpoint + error`. NEVER simulate a trajectory, drag, or firing tables.
- **First round** deviates randomly within a range; **follow-up rounds** are significantly tighter.
- **Corrections are in the observer-target (OT) frame.** Convert to a world delta via OT azimuth and move `aimpoint`. **No angle-T / gun-line rotation** — gun assumed to execute perfectly.
- Callsigns: observer **MUSTANG 12**, FDC **HELLHOUND FIRES**. FDC tone: dark-humored, sardonic, sharp on wrong calls — but never break the doctrinal readback.
- Grid precision by asset: **artillery 6-digit/100 m; 60mm mortar 8-digit/10 m** (a design simplification — doctrine ties fine grids to laser-grid missions; 6-digit is always acceptable).
- CFF = **6 elements in 3 transmissions, each read back by the FDC**; corrections in order **deviation → range** ("left/right" nearest 10 m, "add/drop" in 100 m multiples); **danger close = within 600 m of friendlies** (expect creeping fire, ≤100 m corrections); end of mission = **RREMS** with surveillance terms suppressed/neutralized/destroyed. Details: DOCTRINE.md.
- Fratricide (hitting a friendly element) = automatic mission fail (and 0 stars). **Fail ≠ end** (user correction, 2026-07-29): the failure is permanent, but the mission does **not** auto-end — the target must still be suppressed/neutralized/destroyed and the mission closed with RREMS. Do not reintroduce the auto-AAR.
- **Civilian casualties (collateral damage) = automatic mission fail**, identical to fratricide — including fail ≠ end. Civilian villages/figures render distinctly from military elements.
- **Permanent structures and roads must appear on the printed map sheets and the `[M]` map** (symbols + legend) so terrain association is a practicable skill (resect off the airfield, the radio mast, the village). Enemy positions are never plotted.
- **Convoy pit stops:** moving convoys make seeded **1–3 minute stops** at gas stations/ammo depots/airfields along their route — catching a column halted differs from leading it on the move.
- **FDC deviation policy:** doctrinal scripts are guidelines, flexible not rigid. **Dangerous deviation** (danger close w/o proword, corrections walking toward friendlies/civilians, unresolved-unsafe FFE) → FDC **rant**. **Stupid-but-safe deviation** (wrong element order, malformed-but-unambiguous calls, absurd rounding) → **snide remark**, mission proceeds. Strict mode still enforces format for grading — the rant/snide split is tone, not an extra gate.
- **Surface-to-surface fires only.** CAS/9-line is future "Volume V" — a locked menu tease at most; never build it unasked.

## Campaign rules (stages 9–11)
Stages 9 (campaign skeleton) and 10 (narrative layer) are complete and committed. **Stage 11 (Epilogue — the three SUNBURN chapters, including SUNLAMP) is deferred by decision** — see ROADMAP.md Track C. Current order is stage 13 (visual overhaul) → stage 12 remainder → stage 11.
- Mission menu = **book series**: Volumes → Chapters, per NARRATIVE.md (Foreword tutorial → Volumes I–IV → Epilogue). Chapters are **fixed-seed** missions; `[N]` random generator becomes **Skirmish** mode.
- **Star grading, original-MW2 style:** 0–5★ per chapter from competency (rounds-to-effect, first-round miss, format, time vs. par), capped by difficulty (Easy 3 / Normal 4 / Hard 5), shown in the mission menu and AAR. Volumes unlock at cumulative star thresholds.
- The Epilogue's directed-energy mission (SUNLAMP) **still uses the direct-impact model** — only pacing, prowords, visuals, and audio differ.
- Humor: follow NARRATIVE.md's humor rules — sprinkle in story chapters, dial to 11 only in the Epilogue, and **never let a joke replace the doctrinal readback**.

## Conventions
- Vanilla JS, ES modules, no framework.
- All constants in a top-level `CONFIG` object, grouped by system.
- Seeded PRNG for reproducible scenarios; dispersion draws from the same stream.

## Testing
- Open `SHITFIRE.html` directly in **Chrome** (voice + print are Chrome/Edge desktop).
- Voice needs mic permission on a real tab. Test print/save via the browser print dialog.
- If ever run as a claude.ai Artifact: use **in-memory state, not `localStorage`** (blocked there).

## Don't
- **Don't ever have two agents editing `SHITFIRE.html` at once.** This has already gone wrong once — GRAPHICS.md was written blind because another agent held the file. One writer, always.
- Don't build a whole stage at once when it has lettered rows — ship one ROADMAP row, commit, then the next.
- Don't add a trajectory sim, angle-T math, a bundler, a server, or npm deps.
- Don't let voice be load-bearing — the typed core must fully work on its own.
- Don't break the stable interfaces above.
- Don't mark anything done from memory. Grep the code or run it.

## Workflow — one work order at a time
1. **Read ROADMAP.md.** Take the single row marked `NEXT`. If none is marked, ask — don't pick one.
2. **One row = one commit**, message prefixed with the row ID: `stage 13c: sky + time of day`, `fix F1: range rounding gate`. Matches existing history (`stage 12e: OT direction and RREMS end of mission`).
3. **Pass the row's gate before committing.** The gate is in the ROADMAP table; graphics rows also run [GRAPHICS.md](GRAPHICS.md) §Verification checklist. Code existing ≠ row done.
4. **Docs land in a separate follow-up commit** — never bundled into the code commit, or the diff stops being reviewable. That follow-up is what flips the row to `DONE`.
5. **Model split.** Sim (`SHITFIRE.html`) code → **Fable**, worked directly rather than dispatched. Doc/planning files (README, SPEC, NARRATIVE, CLAUDE, QUICKSTART, DOCTRINE, TLO, CHEATSHEET, ROADMAP, GRAPHICS, DIALOGUE_REVISIONS) → **Sonnet subagent**.
   - **Do not dispatch to Fable.** As of 2026-07-29 there are no Fable credits on this account, so a Fable dispatch fails outright and wastes the turn. Earlier docs and commit messages reference Fable as the sim model — that is history, not instruction.
6. If a row turns out to be two things, **split the row in ROADMAP.md first**, then build.
- Review diffs before merging. If a row gets messy, `git restore .` and re-prompt narrower — a clean partial beats a broken whole.
