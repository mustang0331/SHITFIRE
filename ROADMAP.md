# ROADMAP.md — the board

**This file is the single authority on _what ships next_ and _what is done_.** Nothing else is.
If another document disagrees with this one about order or status, this one wins and the other
document is the bug.

Last updated: 2026-07-29 · Baseline commit: `a74cee7` (AAR shot plot legibility pass)

> ⚠ **A Fable agent committed to `index.html` at 03:36 while this board was being written**
> (`a74cee7`). Confirm no agent still holds the file before dispatching row 13a. This is rule 1
> below, and it is the second time in this project it has come up.

---

## 1. Authority map — which file answers which question

Confusion in this project comes from asking the wrong file. One question, one authority:

| Question | Authority | Not |
|---|---|---|
| What ships next? What's done? | **ROADMAP.md** (this file) | SPEC, README |
| What is stage N supposed to contain? | **SPEC.md** | README |
| How do agents work here? Rules? | **CLAUDE.md** | QUICKSTART |
| What is doctrinally correct? | **DOCTRINE.md** | SPEC, vibes |
| Story, chapters, characters, humor | **NARRATIVE.md** | — |
| Graphics implementation detail | **GRAPHICS.md** (detail for stage 13) | SPEC |
| Dialogue rewrite text | **DIALOGUE_REVISIONS.md** (detail for stage 14) | NARRATIVE |
| Training-objective crosswalk | **TLO.md** | — |
| What is this project? (public face) | **README.md** | — |

**README.md no longer carries a backlog.** It had one, it drifted, and it ended up with two
different item #27s and two different #28s. Backlog lives here now, with stable IDs.

---

## 2. Work-order protocol (the anti-drift rules)

1. **One writer on `index.html` at a time. Ever.** This is the rule that was already broken once:
   GRAPHICS.md opens with "no code has been changed by this plan yet — it was written while another
   agent held the file." Two agents editing a 200 KB single file is how this project dies. Doc agents
   may run in parallel with a sim agent **only** on files the sim agent is not touching.
2. **One row here = one work order = one commit.** No compound commits. If a row turns out to be two
   things, split the row first, then do them.
3. **Commit message = the row ID.** `stage 13c: sky + time of day`, `fix F1: range rounding gate`.
   Matches the existing history (`stage 12e: OT direction and RREMS end of mission`).
4. **Gate before commit.** Every row names its gate in the table below. A row is not done because the
   code exists; it is done when the gate passes. Graphics rows additionally run
   [GRAPHICS.md §Verification checklist](GRAPHICS.md).
5. **Docs land after the sim commit, in a separate commit.** Never bundle a doc rewrite into a code
   commit — it makes the diff unreviewable.
6. **Model split** (per CLAUDE.md): sim code in `index.html` → **Fable**. Doc/planning files →
   **Sonnet subagent**.
7. **Status values:** `DONE` · `NEXT` (exactly one row at a time) · `READY` · `BLOCKED` · `PARKED` ·
   `AUDIT` (believed done, needs verification before being marked DONE).
8. **Never mark a row DONE from memory.** Grep the code or run it.

---

## 3. Shipping order

Decided 2026-07-29: **graphics overhaul → stage 12 remainder → stage 11 (Epilogue).**

Stage *numbers* are stable identifiers — they map to commit messages and, in one case, to a string
inside the code ([index.html:3691](index.html#L3691) carries `blurb: 'stage 11'`). They are **not**
the shipping order. Stage 13 ships before stage 12; that is intentional and this table is why.

### Track A — Stage 13: Visual overhaul ← **ACTIVE**

Detail spec: [GRAPHICS.md](GRAPHICS.md). Each row is gated behind one `CONFIG.GFX` flag so any row
can be A/B'd or reverted alone.

| ID | G-ref | What | Owner | Gate | Status |
|---|---|---|---|---|---|
| 13a | G0.4 | **Bino quality pin** — stop adaptive quality dropping pixel ratio while binos are up | Fable | Troop figures countable through binos at 3000 m after a forced quality step-down | READY |
| 13b | G0 rest | `CONFIG.GFX` block, ACES tone mapping + **palette rebalance in the same commit** | Fable | Jungle/sand/ocean read unchanged in tone; map sheets unaffected | READY |
| 13c | G1 | Sky + **time-of-day model** (`Sky.js`, TOD table drives sun/hemi/fog) | Fable | Horizon has no seam; `[M]` map + printed sheets inherit no tint | READY |
| 13d | G2 | **Baked hillshade + AO into terrain vertex colors** — highest-value row | Fable | Ridge in 3D matches contours on the sheet; black-sand palette doesn't crush; `rebuildWorld()` under ~250 ms | READY |
| 13e | G3 | Near-field terrain LOD patch (fixes 33 m facets > 60 m effect radius) | Fable | Burst deviation judgable against micro-relief; no seam z-fight; `groundHit`/`hasLOS` untouched | READY |
| 13f | G4 | Instanced vegetation + scatter | Fable | **Civilian/military discrimination at 2000 m survives**; no veg on structures/roads; canopy ≤6 m | READY |
| 13g | G5 | Shoreline foam + ocean sun glint | Fable | 60 fps held; one preallocated `uTime` uniform, no per-frame alloc | READY |
| 13h | G6 | Persistent craters + lingering marker smoke + water-splash column | Fable | A walked bracket stays readable as a shot group; pool stays preallocated | READY |
| 13i | G7 | Optics presentation — vignette, sway (2D canvas/CSS only) | Fable | Mil graduations at full contrast, unencroached | READY |
| — | G8 | Bloom — **deferred to stage 11**, SUNLAMP only | — | — | PARKED → 11 |

**13a first and alone**, per GRAPHICS.md: it is a two-line *training-fidelity correctness fix*, not a
graphics change, and it should not be buried in a rebalance commit.

### Track B — Stage 12 remainder: FO skill depth

Shipped: 12a coach · 12b mil-relation/OT-factor · 12c doctrinal metrics · 12d slow-fire reaction ·
12e OT direction + RREMS. Remaining, all verified absent from `index.html`:

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| 12-audit | **Verify what 12c/12d actually landed** — `timeToInit` and location-error-as-vector do not appear under any obvious identifier. Confirm present or demote to a row. | Fable | Named finding per SPEC stage-12 bullet | AUDIT |
| 12f | **"At my command"** fire-control hold before FFE | Fable | FDC holds; observer's "fire" releases; strict mode grades it | BLOCKED by 13 |
| 12g | **Immediate suppression** mission type (single transmission, no MTO per DOCTRINE.md) | Fable | Parser accepts; FDC skips MTO; DOCTRINE.md §mission-types satisfied | BLOCKED by 13 |
| 12h | **Smoke + illumination** mission types | Fable | Impact-effect hooks fire; illum needs 13c TOD for a night mission to mean anything | BLOCKED by 13c |
| 12i | **Wind model** — drifts smoke so it's readable as a tool | Fable | Smoke drift visible and directionally consistent | BLOCKED by 13c |
| 12j | **Degraded optics / dead laser** condition forcing mil-relation ranging | Fable | Laser unavailable; 12b's mil-relation path is the only solution | BLOCKED by 13 |
| 12k | Stage 12 balance pass | Fable | Star pars sane across affected chapters | BLOCKED |

> **Overlap resolved.** SPEC stage 12's "dynamic environment" bullet and GRAPHICS G1 were
> independently specifying the same time-of-day feature. **13c owns time-of-day** (it is the lighting
> authority — sun, hemisphere, and fog all derive from it). Stage 12 keeps only what consumes it:
> wind (12i), night/illumination *missions* (12h), and degraded visibility (12j). Do not implement
> TOD twice.

### Track C — Stage 11: Epilogue (deferred, by decision)

| ID | What | Owner | Status |
|---|---|---|---|
| 11a | E.1 THE GREAT CHOW RAID | Fable | PARKED |
| 11b | E.2 CLAWS OUT | Fable | PARKED |
| 11c | E.3 SUNLAMP ACTUAL — **still `impact = aimpoint + error`**; only pacing, prowords, beam visual, audio differ | Fable | PARKED |
| 11d | G8 bloom, gated to SUNLAMP + quality tier 0 only | Fable | PARKED |
| 11e | Campaign-wide star par balance pass | Fable | PARKED |

Chapter stubs already exist at [index.html:3688-3691](index.html#L3688-L3691) with `impl: false`.
Volume V "ON WINGS" stays a locked spine. **Do not build CAS.**

### Track D — Stage 14: Dialogue punch-up

Detail spec: [DIALOGUE_REVISIONS.md](DIALOGUE_REVISIONS.md) — a complete rewrite of the quip pools,
already written and unapplied.

| ID | What | Owner | Status |
|---|---|---|---|
| 14a | Apply revised QUIPS pools (`corrSnark` 4→8 is the repeat the transcript caught) | Fable | READY |
| 14b | Chapter narrative punch-ups + mission briefs | Fable | READY |
| 14c | LIBERTY FIRES guest FDC, one chapter | Fable | PARKED — new character, needs a NARRATIVE.md home first |

**Zero conflict surface with stage 13** (string pools vs. renderer), so 14a/14b can be pulled forward
between any two graphics rows on request. It still serializes — rule 1 has no exceptions.

### Track F — Fixes (not stages; schedulable anytime)

| ID | What | Owner | Status |
|---|---|---|---|
| F1 | **Doctrine bug: range rounding accepts 50 m outside FFE.** `index.html:2570` and `:2623` both check `% 50` unconditionally; per DOCTRINE.md 50 m is legal only on the correction entering FFE. Gate on `p.ffe`, and fix the STRICT NET line that states the loose rule as always true. Volume IV's "doctrine, verbatim" chapter is currently looser than doctrine. | Fable | READY |
| F1b | Danger-close check `minF < 600` is exclusive; doctrine's "within 600 m" is inclusive → `<=`. Fold into F2 if F2 runs first (F2 replaces the flat threshold). | Fable | READY |
| F2 | Tiered danger-close radio tension (≤800/≤700/≤600 bands) + FDC map-awareness gating — unmarked friendlies mean the FDC can't know, so no friction; marked friendlies mean a visible pause | Fable | READY |
| F3 | In-sim cheat-sheet overlay `[H]` from [CHEATSHEET.md](CHEATSHEET.md) | Fable | READY |
| F4 | Multi-phase MEAT GRINDER chaining (4.4) — revisit after the single-mission version is balanced | Fable | PARKED |

**F1 is a correctness bug in the doctrine the app exists to teach.** Recommend landing it alongside
13a as the other small, high-value, low-risk fix.

---

## 4. Definition of done (every row)

1. Gate in the table passes.
2. 60 fps held; no per-frame allocation added.
3. Stable interfaces unchanged in shape: `H(x,z)` · `fireMission` · `applyCorrection` · `FDC.say` ·
   `Scenario` · `gradeMission` · `TLOG` ([index.html:3916](index.html#L3916)).
4. Ballistics untouched — `impact = aimpoint + error`. No trajectory, no angle-T, no gun-line rotation.
5. Fratricide and collateral damage still auto-fail.
6. Committed with the row ID as the message prefix.
7. This file updated to `DONE` **in the follow-up doc commit**, not the code commit.

---

## 5. Change log

| Date | Change |
|---|---|
| 2026-07-29 | Board created. Order set: stage 13 → stage 12 remainder → stage 11. Backlog moved out of README.md (duplicate IDs #27/#28 retired). Time-of-day assigned to 13c, removed from stage 12. G8 bloom reassigned to stage 11 as 11d. |
| 2026-07-29 | Baseline moved `a847731` → `a74cee7`: an off-board Fable commit (AAR shot plot auto-fit + legibility) landed mid-session. Not attached to any stage — the AAR shot plot is an extra shipped alongside spec, like the OP watchtower and TLOG. Logged here so the board matches history. |
