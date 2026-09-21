# CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer) — `unreal-port` worktree

Single-file browser trainer for the forward observer, not the gun crew. **Start every session
by reading `ROADMAP.md`**: it is the only authority on what ships next; take the one row marked
NEXT, or ask. `PLAYBOOK.md` is how work runs (one row per chat, the gate stack, feedback
triage). **`INVARIANTS.md`** holds the full domain facts, campaign rules, stable interfaces,
gate details, tooling terms and workflow. Read it before touching the parser, FDC traffic, the
campaign, grading or tooling. `DOCTRINE.md` is the authority for CFF formats and prowords.

## This worktree: the Unreal port
- Branch `unreal-port`, a git worktree of `prj\SHITFIRE`. **The stash is shared**: never bare
  `git stash`; use a WIP commit or a tagged `git stash push -u -m <tag>`.
- Engine-free C++20 sim core in `port/core/` (`/fp:precise`), parity tests in
  `port/core-tests/` against the browser-oracle fixtures (`ENGINE_PORT/fixtures/`, tag
  `oracle-v1`). UE 5.8 project in `port/unreal/`. Build and version pins: `port/README.md`.
  P2 verdict and the adversarial standard: `port/ADVERSARY_SIGNOFF.md`.
- Parallel agents each build into their own `port/build-<name>/` (gitignored). No shared
  build dir.
- The browser sim rules below still bind the port: parity with the oracle is the gate.

## Golden rules
- The shipped browser artifact is one self-contained `SHITFIRE.html` that runs with nothing
  installed and zero network. Never point the import map at a CDN.
- `SHITFIRE.html` is a **build artifact**. Edit `src/js/*.js` and `src/shell/`, then
  `node tools/build.js`. Never hand-edit it; the pre-commit hook rejects a stale build.
- Ballistics = direct-impact model: `impact = aimpoint + error`. Nothing ever computes where a
  round goes from velocity, angle or drag. Corrections are in the OT frame; no angle-T.
- FDC is rule-based, no LLM. Voice is never load-bearing; typed input always works.
- Fratricide or civilian casualties = automatic fail, and fail ≠ end: the mission still
  closes with RREMS.
- Stable interfaces: `H(x,z)` · `fireMission()` · `applyCorrection()` · `FDC.say()` ·
  `Scenario` · `gradeMission()` · the TLOG entry format.
- Surface-to-surface only. Never build CAS/9-line unasked.

## Gate before commit
Browser rows: `node tools/build.js` · `node tools/lint.js` · `tools/syntaxgate.ps1` +
`cscript tools/parser-order.js` · `node tools/shots.js` · `node tools/replay.js`.
Port rows: clean configure into your own `port/build-<name>/`, Level 1/2/3 green, 33/33
transcript replays exact (`port/README.md`). One ROADMAP row = one commit, row ID prefix.

## Don't
- Two writers on one module. One writer per module; every agent gets a file allowlist.
- Mark anything done from memory. Grep or run it, cite `file:line`.
- Take a fleet's reported numbers as verification. Rebuild and rerun yourself.

## Never read (context hazards)
`port/unreal/` (1 234 files; open only a file you were told to edit) · `port/build*/` (CMake
output) · `port/vendor/` · `SHITFIRE.html` · `vendor/` · `tools/node_modules/` ·
`docs/screenshots/` · `*.PDF` / `*.pdf` · `*.tif` · `graphify-out/cache/`.

## Orchestration
Rules: `C:\Users\conno\ClaudeVault\permanent\orchestrator-rules.md`. Prebuilt agents in
`.claude/agents/` (`docs`, `research`) carry allowlists and a pinned model. Sim and port code
→ default model; doc files → sonnet.

## Context, in order
1. `graphify-out/graph.json` for browser code structure. 2. The vault,
`C:\Users\conno\ClaudeVault\shitfire\`, for decisions and history (architecture note on
`/resume`, not imported). 3. Raw source only when editing or when 1 and 2 lack the answer.

@C:/Users/conno/ClaudeVault/shitfire/shitfire-index.md

Open with `/resume`, close with `/save`. Never commit the vault.
