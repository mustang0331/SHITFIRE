# CLAUDE.md — SHITFIRE (Forward Observer / Call-for-Fire Trainer)

Single-file browser trainer for the forward observer, not the gun crew. **Start every session
by reading `ROADMAP.md`**: it is the only authority on what ships next; take the one row marked
NEXT, or ask. `PLAYBOOK.md` is how work runs (one row per chat, the gate stack, feedback
triage). **`INVARIANTS.md`** holds the full domain facts, campaign rules, stable interfaces,
gate details, tooling terms and workflow. Read it before touching the parser, FDC traffic, the
campaign, grading or tooling. `DOCTRINE.md` is the authority for CFF formats and prowords.

## Golden rules
- The shipped artifact is one self-contained `SHITFIRE.html` that runs with nothing installed
  and zero network. No runtime dependency, no backend, no keys. Never point the import map at
  a CDN; three.js r160 is vendored in `vendor/` and inlined by the build.
- `SHITFIRE.html` is a **build artifact**. Edit `src/js/*.js` and `src/shell/`, then
  `node tools/build.js`. Never hand-edit it; the pre-commit hook rejects a stale build.
  Manifest order is load-bearing.
- Ballistics = direct-impact model: `impact = aimpoint + error`. Nothing ever computes where a
  round goes from velocity, angle or drag. Corrections are in the OT frame; no angle-T.
- FDC is rule-based JS, no LLM. Voice is never load-bearing; typed input always works.
- Fratricide or civilian casualties = automatic fail, and fail ≠ end: the mission still
  closes with RREMS. Do not reintroduce the auto-AAR.
- Stable interfaces: `H(x,z)` · `fireMission()` · `applyCorrection()` · `FDC.say()` ·
  `Scenario` · `gradeMission()` · the TLOG entry format.
- Surface-to-surface only. Never build CAS/9-line unasked.
- Callsigns MUSTANG 12 / HELLHOUND FIRES. Humor never replaces the doctrinal readback.

## Gate before commit
`node tools/build.js` · `node tools/lint.js` · `tools/syntaxgate.ps1` +
`cscript tools/parser-order.js` · `node tools/shots.js` (look at the frames) ·
`node tools/replay.js` on any parser-adjacent row · a fresh scratchpad harness that fails
before the fix. One ROADMAP row = one commit, message prefixed with the row ID. Docs land in
a separate follow-up commit.

## Don't
- Two writers on one `src/` module. One writer per module; every agent gets a file allowlist.
- Mark anything done from memory. Grep the code or run it, cite `file:line`.
- Add a trajectory sim, angle-T, runtime bundler, server, or runtime npm dependency.
- Auto-format or rewrite `SHITFIRE.html` with a tool. Tools only read it.

## Never read (context hazards)
`SHITFIRE.html` (200 KB build output) · `vendor/` · `tools/node_modules/` ·
`docs/screenshots/` · `*.PDF` / `*.pdf` (hundreds of pages: extract with a script, grep, read
only those pages) · `*.tif` · `scratch/` · `graphify-out/cache/`.

## Orchestration
Rules: `C:\Users\conno\ClaudeVault\permanent\orchestrator-rules.md`. Prebuilt agents in
`.claude/agents/` (`docs`, `research`) carry allowlists and a pinned model; prefer them over
ad-hoc dispatch. Sim code → default model; doc files → sonnet.

## Context, in order
1. `graphify-out/graph.json` for code structure (`graphify update .` after structural changes;
   never every session; never hand-edit `graphify-out/`).
2. The vault, `C:\Users\conno\ClaudeVault\shitfire\`, for decisions and history. The
   architecture note `shitfire\architecture\shitfire-architecture.md` is read on `/resume`,
   not imported here.
3. Raw source only when editing, or when 1 and 2 lack the answer.

@C:/Users/conno/ClaudeVault/shitfire/shitfire-index.md

Open with `/resume`, close with `/save`. Never commit the vault.
