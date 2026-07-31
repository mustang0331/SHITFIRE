# PLAYBOOK.md — running Claude Code on SHITFIRE, effectively

How to keep building this project with Claude Code across multiple chats, written from
the inside after the session that shipped Track G, stages 11–14, and the tooling. This
is strategy, not status — the board (ROADMAP.md) stays the only authority on *what*;
this file is the proven *how*.

---

## 1. The one rule that makes multiple chats safe

**ROADMAP.md is the coordination point, and one row belongs to one chat.** Everything
else follows from that:

- A chat that owns a row owns the `src/` modules that row touches. Two chats must never
  both hold sim rows at the same time unless you are certain their rows touch different
  modules — and "certain" means you checked, not guessed. The safe default for a second
  concurrent chat is **docs, research, or tooling only** (those never collide with sim
  work, and the pre-commit hook plus `build --check` will catch it if someone slips).
- Start every chat by naming the row: *"take 15b"* or *"take the NEXT row."* CLAUDE.md
  already forces the board read; naming the row skips a discovery round-trip.
- If a chat dies mid-row, the next chat can tell: uncommitted `src/` changes + the row
  still marked NEXT. Say *"finish the in-flight row"* and it will reconstruct from the
  diff. The byte-exact build gate means a half-done state can't silently ship.

## 2. The session shape that works

The highest-throughput pattern, proven over ~40 commits in one arc:

1. **One standing directive, then get out of the way.** "Finish X, don't stop to ask,
   use your best judgment" outperforms turn-by-turn steering by a wide margin. Claude
   runs the whole loop per row: read the row → read the code it touches → build → verify
   → commit → dispatch the docs flip → take the next row. Interrupt only to redirect.
2. **One row = one commit = one reviewable diff.** Never let a session bundle three rows
   into one commit. If a row turns out to be two things, split it on the board first.
3. **Docs flips run in the background while code continues.** Sim code stays in the main
   chat (the strongest model, full context); every board flip / doc update goes to a
   background Sonnet subagent with an explicit file allowlist ("edit ONLY ROADMAP.md;
   FORBIDDEN from src/"). This roughly doubles throughput and the allowlist makes
   collisions structurally impossible. Keep this split.
4. **End sessions at commit boundaries.** A fresh chat starting on a clean tree with a
   current board is cheap and reliable. A long session works too (context compaction is
   fine), but per-row or per-cluster sessions waste nothing and lose nothing.

## 3. Verification is the engine — never accept "done" without a gate

The single biggest quality lever this project has. The rule that emerged: **a row ships
with an executable check that would have failed before the change.** The standing stack,
in the order a row runs it:

| Gate | What it proves | When |
|---|---|---|
| `node tools/build.js` + pre-commit byte gate | artifact = exactly what src/ builds | every commit (automatic) |
| `node tools/lint.js` | 0 correctness errors across the concatenated modules | every code row |
| `tools/syntaxgate.ps1` + `parser-order.js` | parses; parser branch precedence intact | every commit (automatic) |
| `node tools/shots.js` | **boots clean, fully offline**, 12 visual states captured | every code row |
| `node tools/replay.js` | parser classifies all recorded play transcripts the same | any parser-adjacent row |
| scratchpad harness (numeric or Playwright e2e) | the row's specific behavior, end to end | written fresh per row |

Two habits worth enforcing when you prompt:

- **"Look at the screenshots"** — three real bugs this session (black trees at noon, a
  bloom-hazed horizon, an over-wide optics fringe) were invisible to every non-visual
  gate and caught only because the frames were actually read. Screenshots exist to be
  looked at, not just generated.
- **"The harness must fail first."** A test written after the fix that has never seen
  the defect proves little. The TDZ boot-kill and the ulp seam-drift were both caught
  because the harness ran before the fix was trusted.

## 4. Feed the machine your play — it's the best backlog generator

The most valuable rows on this board (all of Track G, the mic fix, F5–F7) came from two
sources you control:

- **user_feedback.md** — keep dumping raw notes there after play sessions, in your own
  words, unstructured. Then open a chat: *"triage user_feedback.md into ROADMAP rows."*
  The triage turns complaints into gated, ordered work — and building next to real
  complaints finds the bugs nobody thought to report.
- **TLOG exports** — after a real session, export JSON from the mission menu into
  `Dialogue History/`. Every transcript you drop there permanently joins the
  `replay.js` regression suite: your actual radio traffic becomes the test that future
  parser changes must not break. Transcripts are also the best dialogue-review input
  (the corrSnark repeat was found by reading one).

## 5. What to keep for yourself (Claude's real limits here)

- **Taste.** Palette, density, "does the island feel right" — the ⚠-marked rows and
  GRAPHICS.md's visual QA backlog need your eyes in Chrome. Headless screenshots verify
  function, not feel.
- **Play balance.** Par times and difficulty were audited arithmetically, but only a
  human playing badly on purpose finds where the trainer is unfair or too kind.
- **Doctrine judgment calls.** Claude researches and cites (FM 6-30, FM 7-90, JFIRE),
  but decisions like "is 6-digit acceptable on the mortar net" are training-design
  calls — yours.
- **Parked decisions.** G20, 14c (LIBERTY FIRES), F4, Volume V sit parked precisely
  because they need your intent, not more engineering.

## 6. Prompting patterns that paid off (and one that doesn't)

- **Research before build** for doctrine-heavy rows: one session produces a sourced
  findings section in DOCTRINE.md and flips the row to "RESEARCHED — build ready"; the
  build session then goes fast and argues from citations. (G10/G13/G18 pattern.)
- **Challenge the constraints occasionally.** "What's inhibiting your full potential
  here — let's reexamine" produced the src/ split, the offline vendoring, and the
  ballistics reword — the three changes that made everything after them faster. Worth
  repeating every few weeks as the project evolves.
- **Corrections as principles, not patches.** "Fratricide should fail but not end the
  mission" landed as a permanent rule in CLAUDE.md, so no future chat re-introduces the
  old behavior. When you correct something, ask for the rule to be written down.
- **Anti-pattern: mid-row micro-steering.** Interrupting a row in flight to adjust
  details costs a context round-trip each time. Batch small asks (like the dispersion
  toggle and mic fix) as their own tiny rows between big ones.

## 7. Session-start checklist (any chat, any time)

1. `git status` — clean tree? If not, is it a dead chat's in-flight row?
2. Read ROADMAP.md's Next line. Board closed → play, triage feedback, or unpark something.
3. Name the row (or the feedback item) in your first message.
4. If a second chat is already running sim code: this chat takes docs/research/tooling.

---

*Written 2026-07-30, the day the board first closed. If workflow reality drifts from
this file, fix the file — it's a map, and the ground wins.*
