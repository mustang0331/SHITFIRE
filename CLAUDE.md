# CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)

A single-file browser trainer for the FORWARD OBSERVER (not the gun crew). Full build spec is in **SPEC.md** — build it **per SPEC.md's "BUILD ORDER" section, one stage at a time.**

## Golden rules (do not violate)
- **One `index.html`.** No build step, no bundler, no npm, no backend, no API keys.
- Three.js + addons via **CDN import map** only. No other external deps.
- Only optional external input: a grayscale terrain heightmap/DEM (see SPEC "TERRAIN").
- Voice = browser **Web Speech API** (`SpeechRecognition` + `SpeechSynthesis`), client-side, keyless. **Typed input is a mandatory fallback.**
- FDC is **rule-based JS** (no LLM).
- Target 60 fps. Preallocate; no per-frame allocation in the render loop.

## Keep these interfaces stable (later stages depend on them)
`H(x,z)` · `fireMission(targetLocation)` · `applyCorrection(otFrameDelta)` · `FDC.say(msg)` · `Scenario`

## Domain facts — get these right
- **Ballistics = direct-impact model.** `impact = aimpoint + error`. NEVER simulate a trajectory, drag, or firing tables.
- **First round** deviates randomly within a range; **follow-up rounds** are significantly tighter.
- **Corrections are in the observer-target (OT) frame.** Convert to a world delta via OT azimuth and move `aimpoint`. **No angle-T / gun-line rotation** — gun assumed to execute perfectly.
- Callsigns: observer **MUSTANG 12**, FDC **HELLHOUND FIRES**. FDC tone: dark-humored, sardonic, sharp on wrong calls — but never break the doctrinal readback.
- Grid precision by asset: **artillery 6-digit/100 m; 60mm mortar 8-digit/10 m.**
- Fratricide (hitting a friendly element) = automatic mission fail.

## Conventions
- Vanilla JS, ES modules, no framework.
- All constants in a top-level `CONFIG` object, grouped by system.
- Seeded PRNG for reproducible scenarios; dispersion draws from the same stream.

## Testing
- Open `index.html` directly in **Chrome** (voice + print are Chrome/Edge desktop).
- Voice needs mic permission on a real tab. Test print/save via the browser print dialog.
- If ever run as a claude.ai Artifact: use **in-memory state, not `localStorage`** (blocked there).

## Don't
- Don't build all 7 stages at once — ship stage N working, commit, then stage N+1.
- Don't add a trajectory sim, angle-T math, a bundler, a server, or npm deps.
- Don't let voice be load-bearing — the typed core must fully work on its own.
- Don't break the stable interfaces above.

## Workflow
- `git init` first. Commit after each working stage. Review diffs before merging.
