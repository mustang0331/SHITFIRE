# ROADMAP.md — the board

**This file is the single authority on _what ships next_ and _what is done_.** Nothing else is.
If another document disagrees with this one about order or status, this one wins and the other
document is the bug.

Last updated: 2026-07-29 · Baseline commit: `d586e54` (stage 13b)

**Shipped off this board:** `13a` bino quality pin · `F1` range-rounding fix · `13b` ACES tone mapping
· `13c` sky + time of day · `E1` target legibility · `E2` SALUTE spot report.
**In progress: `E3`** (night + NVG/thermal).

> 📋 **Build-out in progress.** The user directed a full build-out on 2026-07-29 ("do not stop until
> completed"), adding Track E from side notes. Rows are being worked by sequential Opus subagents —
> **one writer in `index.html` at a time, no exceptions.** Several rows are committed but carry
> ⚠ *needs Chrome*: their gates are visual and no agent can close them. Expect a visual QA pass at the
> end covering everything so marked.

> ⚠ **Visual rows need a human in Chrome.** 13c–13i have gates ("no horizon seam", "figures countable
> at 3000 m", "civilian/military discrimination survives") that cannot be closed by an agent that
> cannot see the render. Opus writes the code and verifies the math with a harness; **someone has to
> look at it before the row flips to DONE.** 13b's visual gate was closed this way — confirmed good
> by the user on 2026-07-29, with `satComp` left neutral at 1.0.

> ⚠ **Concurrency is live in this repo, not hypothetical.** Another agent committed to `index.html`
> at 03:36 while this board was being drafted (`a74cee7`), and BALLISTICS_RESEARCH.md plus new TLOG
> transcripts appeared at ~08:48 during the 13a/F1 work. **Confirm nothing else holds `index.html`
> before starting a row** — rule 1 below. This has now come up three times.

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
| Real-world dispersion / lethality figures | **BALLISTICS_RESEARCH.md** (reference only — nothing in it is applied, and it does **not** license a trajectory sim) | — |
| How do we move this to a game engine? | **ENGINE_PORT/** (README → CHOICE → ARCHITECTURE → PARITY_TESTING → PORT_PLAN) | SPEC, this board |
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
6. **Model split** (per CLAUDE.md): sim code in `index.html` → **Opus**, worked directly.
   Doc/planning files → **Sonnet subagent**. **Do not dispatch to Fable** — no credits on this
   account as of 2026-07-29, so the dispatch fails and wastes the turn. The `Fable` seen in older
   commit messages is history, not instruction.
7. **Status values:** `DONE` · `NEXT` (exactly one row at a time) · `READY` · `BLOCKED` · `PARKED` ·
   `AUDIT` (believed done, needs verification before being marked DONE).
8. **Never mark a row DONE from memory.** Grep the code or run it.

---

## 3. Shipping order

Decided 2026-07-29: **graphics overhaul → stage 12 remainder → stage 11 (Epilogue).**

**Revised 2026-07-29 (user directive, "finish the entire build out"):** Track E — observation and
target acquisition — is inserted **ahead of the rest of stage 13**. Rationale: an observer who cannot
see or find the target is a broken trainer, so legibility outranks beautification. **Track E is now
closed** (E1–E7 all shipped).

**Revised again 2026-07-29, same reasoning applied to real play:** Track G (the user's feedback
notebook) is inserted ahead of the rest of stage 13. Order is now

**~~E1–E7~~ ✅ → Track G → 13d–13i → stage 12 remainder → stage 11 → stage 14 + Track F**

Track G outranks beautification because it came from someone actually flying the trainer, and because
several of its rows are doctrine correctness rather than polish. Where a Track G row overlaps an
existing one (G14 ⊃ 12g, G18 → 12k, G12 vs CLAUDE.md), **Track G wins** — see that track's notes.

Stage *numbers* are stable identifiers — they map to commit messages and, in one case, to a string
inside the code (`SHITFIRE.html` carries `blurb: 'stage 11'`). They are **not** the shipping order.
Stage 13 ships before stage 12; that is intentional and this table is why.

**⚠ The file is now `SHITFIRE.html`, not `index.html`** (renamed by the user 2026-07-29, recorded as a
rename in `f4e7ad8`). CLAUDE.md's "One `index.html`" rule is unchanged in *intent* — one self-contained
file, no build step — but its wording and the references in README/SPEC/QUICKSTART/GRAPHICS are stale
until the docs sweep runs. Line-number references anywhere on this board predate the rename and have
drifted by the E4–E7 insertions; **locate by symbol, not by line**.

### Track A — Stage 13: Visual overhaul ← **ACTIVE**

Detail spec: [GRAPHICS.md](GRAPHICS.md). Each row is gated behind one `CONFIG.GFX` flag so any row
can be A/B'd or reverted alone.

| ID | G-ref | What | Owner | Gate | Status |
|---|---|---|---|---|---|
| 13a | G0.4 | **Bino quality pin** — stop adaptive quality dropping pixel ratio while binos are up | Opus | Troop figures countable through binos at 3000 m after a forced quality step-down | **DONE** `af439a5` |
| 13b | G0 rest | `CONFIG.GFX` block, ACES tone mapping, CPU-side background pre-tone | Opus | Map sheets unaffected ✅ · tone confirmed good in Chrome ✅ | **DONE** `d586e54` |
| 13c | G1 | Sky + **time-of-day model** (`Sky.js`, TOD table drives sun/hemi/fog). Sky.js includes `<tonemapping_fragment>`, so no CPU pre-tone is needed on the sky path; it is retained on the `sky:false` fallback. `day` reproduces the pre-13c sun to 0.038°. | Opus | Horizon has no seam; `[M]` map + printed sheets inherit no tint | **DONE** `999e4c6` ⚠ needs Chrome |
| 13d | G2 | **Baked hillshade + AO into terrain vertex colors** — highest-value row | Opus | Ridge in 3D matches contours on the sheet; black-sand palette doesn't crush; `rebuildWorld()` under ~250 ms | READY |
| 13e | G3 | Near-field terrain LOD patch (fixes 33 m facets > 60 m effect radius) | Opus | Burst deviation judgable against micro-relief; no seam z-fight; `groundHit`/`hasLOS` untouched | READY |
| 13f | G4 | Instanced vegetation + scatter | Opus | **Civilian/military discrimination at 2000 m survives**; no veg on structures/roads; canopy ≤6 m | READY |
| 13g | G5 | Shoreline foam + ocean sun glint | Opus | 60 fps held; one preallocated `uTime` uniform, no per-frame alloc | READY |
| 13h | G6 | Persistent craters + lingering marker smoke + water-splash column | Opus | A walked bracket stays readable as a shot group; pool stays preallocated | READY |
| 13i | G7 | Optics presentation — vignette, sway (2D canvas/CSS only) | Opus | Mil graduations at full contrast, unencroached | READY |
| — | G8 | Bloom — **deferred to stage 11**, SUNLAMP only | — | — | PARKED → 11 |

**13a first and alone**, per GRAPHICS.md: it is a two-line *training-fidelity correctness fix*, not a
graphics change, and it should not be buried in a rebalance commit.

### Track E — Observation & target acquisition (user-directed, 2026-07-29)

Direct requests from the user, not from SPEC or GRAPHICS. **Track E outranks the rest of stage 13**:
these fix the trainer's most-felt defect — *"it is very difficult to see people right now"* — which is
a training failure, not a cosmetic one. An observer who cannot resolve or find the target cannot
practise the skill the app exists to teach.

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| E1 | **Target legibility** — angular-size floor so a figure never falls under a minimum subtense; contrast against jungle/sand; contact disc so figures don't merge into terrain | Opus | Figures countable at 3200 m · **civ/mil discrimination improved, never degraded** | **DONE** `ffc2076` ⚠ needs Chrome |
| E2 | **SALUTE / activity spot report** — cues the observer onto the target *area* without handing over the grid; difficulty-scaled vagueness; landmark- or sector-referenced | Opus | Orients the observer; never gives the 6/8-digit answer; doesn't tread on the readback | **DONE** `ec4d343` ⚠ needs Chrome |
| E3 | **Night TOD + NVG / thermal optics** — `night` in TOD_TABLE; DAY/NVG/THERMAL cycle on `[O]`; thermal white-hot so figures read against cold terrain | Opus | Usable at night; **no EffectComposer**; civ/mil discrimination survives in every mode; doesn't trivialise day | **DONE** `ef9d473` ⚠ needs Chrome |
| E4 | **Asphalt roads** distinct from dirt tracks, in-world | Opus | Metalled routes visually distinct from dirt paths | **DONE** `830d3cc` ⚠ needs Chrome |
| E5 | **Roads on the `[M]` map and the printed sheets** — currently absent from the map despite being a terrain-association anchor | Opus | Road network legible on both, with legend symbols | **DONE** `cee195f` ⚠ needs Chrome |
| E6 | **Settlement hierarchy** — small villages, larger towns, and the airfield, all plotted on map + sheets | Opus | Three distinct settlement scales readable in-world and on paper | **DONE** `0d2eaed` ⚠ needs Chrome |
| E7 | **Rock formations / boulder piles** for terrain association | Opus | Distinctive, resectable off the map; placed by the seeded PRNG | **DONE** `ddd22f9` ⚠ needs Chrome |

**Track E is closed.** All seven rows shipped 2026-07-29. Every one carries a ⚠ — the maths is
harnessed but nothing in this track has been seen in Chrome, and the whole track is *about* what things
look like. See §6 for the consolidated visual-QA list; that list is the gate on calling Track E verified
rather than merely built.

E4–E7 all serve the same end: **terrain association**. CLAUDE.md already requires permanent structures
and roads on both the printed sheets and `[M]` with symbols and a legend so the observer can resect off
the airfield, the mast, the village — E5 exists because roads are in the world but were never drawn on
the map, which breaks that requirement.

### Track B — Stage 12 remainder: FO skill depth

Shipped: 12a coach · 12b mil-relation/OT-factor · 12c doctrinal metrics · 12d slow-fire reaction ·
12e OT direction + RREMS. Remaining, all verified absent from `index.html`:

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| 12-audit | **Verify what 12c/12d actually landed** — `timeToInit` and location-error-as-vector do not appear under any obvious identifier. Confirm present or demote to a row. | Opus | Named finding per SPEC stage-12 bullet | AUDIT |
| 12f | **"At my command"** fire-control hold before FFE | Opus | FDC holds; observer's "fire" releases; strict mode grades it | BLOCKED by 13 |
| 12g | **Immediate suppression** mission type (single transmission, no MTO per DOCTRINE.md). Confirmed live: a transcript shows a player typing `IMMEDIATE SUPPRESSION 253535 OUT` in a real friendlies-under-fire chapter and getting told to "say again... in English this time" — see [DIALOGUE_REVISIONS.md §9.3](DIALOGUE_REVISIONS.md). | Opus | Parser accepts; FDC skips MTO; DOCTRINE.md §mission-types satisfied | BLOCKED by 13 |
| 12h | **Smoke + illumination** mission types | Opus | Impact-effect hooks fire; illum needs 13c TOD for a night mission to mean anything | BLOCKED by 13c |
| 12i | **Wind model** — drifts smoke so it's readable as a tool | Opus | Smoke drift visible and directionally consistent | BLOCKED by 13c |
| 12j | **Degraded optics / dead laser** condition forcing mil-relation ranging | Opus | Laser unavailable; 12b's mil-relation path is the only solution | BLOCKED by 13 |
| 12k | Stage 12 balance pass | Opus | Star pars sane across affected chapters | BLOCKED |

> **Overlap resolved.** SPEC stage 12's "dynamic environment" bullet and GRAPHICS G1 were
> independently specifying the same time-of-day feature. **13c owns time-of-day** (it is the lighting
> authority — sun, hemisphere, and fog all derive from it). Stage 12 keeps only what consumes it:
> wind (12i), night/illumination *missions* (12h), and degraded visibility (12j). Do not implement
> TOD twice.

### Track C — Stage 11: Epilogue (deferred, by decision)

| ID | What | Owner | Status |
|---|---|---|---|
| 11a | E.1 THE GREAT CHOW RAID | Opus | PARKED |
| 11b | E.2 CLAWS OUT | Opus | PARKED |
| 11c | E.3 SUNLAMP ACTUAL — **still `impact = aimpoint + error`**; only pacing, prowords, beam visual, audio differ | Opus | PARKED |
| 11d | G8 bloom, gated to SUNLAMP + quality tier 0 only | Opus | PARKED |
| 11e | Campaign-wide star par balance pass | Opus | PARKED |

Chapter stubs already exist at [index.html:3688-3691](index.html#L3688-L3691) with `impl: false`.
Volume V "ON WINGS" stays a locked spine. **Do not build CAS.**

### Track D — Stage 14: Dialogue punch-up

Detail spec: [DIALOGUE_REVISIONS.md](DIALOGUE_REVISIONS.md) — a complete rewrite of the quip pools,
already written and unapplied.

| ID | What | Owner | Status |
|---|---|---|---|
| 14a | Apply revised QUIPS pools (`corrSnark` 4→8 is the repeat the transcript caught) | Opus | READY |
| 14b | Chapter narrative punch-ups + mission briefs | Opus | READY |
| 14c | LIBERTY FIRES guest FDC, one chapter | Opus | PARKED — new character, needs a NARRATIVE.md home first |

**Zero conflict surface with stage 13** (string pools vs. renderer), so 14a/14b can be pulled forward
between any two graphics rows on request. It still serializes — rule 1 has no exceptions.

### Track E — Engine port (planning complete; execution not scheduled)

Plan and strategy live in **[ENGINE_PORT/](ENGINE_PORT/)** — engine choice (Godot 4 + C#), the
transfers-vs-rebuilds inventory, the parity-testing strategy, and a staged plan P0–P8. No engine code
exists and none is scheduled; nothing on this track competes with stage 13/14 rows for `index.html`.

| ID | What | Owner | Status |
|---|---|---|---|
| E0 | **P0 freeze the oracle** — record the transcript library + export fixtures while the browser build is the only implementation. Cheap, and the plan's biggest risk if skipped. | — | READY |
| E1 | P1–P2 headless sim core (`SHITFIRE.Core` + parity tests) | — | PARKED |
| E2 | P3–P8 engine shell through parity sign-off | — | PARKED |

**The browser build is not retired by the port.** It stays as the parity oracle and the zero-install
distribution channel. Every stage-12/13/14 row landed here makes the oracle *better*, not obsolete —
so continuing on this board is not wasted work if the port later goes ahead.

### Track F — Fixes (not stages; schedulable anytime)

| ID | What | Owner | Status |
|---|---|---|---|
| F1 | **Doctrine bug: range rounding accepted 50 m outside FFE.** Both rounding checks validated against `% 50` unconditionally; per DOCTRINE.md 50 m is legal only on the correction entering FFE. Gated on `p.ffe`; the STRICT NET reply and the strict-chapter intro line both stated the loose rule as always true and were corrected. | Opus | **DONE** `e7ccfdb` |
| F1b | Danger-close check `minF < 600` is exclusive; doctrine's "within 600 m" is inclusive → `<=`. Fold into F2 if F2 runs first (F2 replaces the flat threshold). | — | READY |
| F2 | Tiered danger-close radio tension (≤800/≤700/≤600 bands) + FDC map-awareness gating — unmarked friendlies mean the FDC can't know, so no friction; marked friendlies mean a visible pause. **Read [BALLISTICS_RESEARCH.md](BALLISTICS_RESEARCH.md) §6 first** — it argues the flat 600 m gate is a deliberate and correct simplification, which bears directly on how far this row should go. | — | READY |
| F3 | In-sim cheat-sheet overlay `[H]` from [CHEATSHEET.md](CHEATSHEET.md) | Opus | READY |
| F4 | Multi-phase MEAT GRINDER chaining (4.4) — revisit after the single-mission version is balanced | Opus | PARKED |
| F5 | **STT/typo tolerance in adjust corrections.** Transcript evidence: voice recognition renders "right" as "WRITE" and "drop" as "DROPPED"/"DRAW"; the `\b(left\|right)\s+(\d+)` and `\b(add\|drop)\s+(\d+)` regexes (`index.html:3457,3459,3497,3499,3531,3533`) match none of these, so a mangled deviation word is silently dropped from the correction (no error, no notice) while a mangled range word alone parses as full `unknown`. See [DIALOGUE_REVISIONS.md §9.4](DIALOGUE_REVISIONS.md). | Opus | Correction with one STT-plausible word variant (e.g. "write"/"dropped") still parses both fields; nothing is silently dropped | READY |
| F6 | **"Danger clothes" — fuzzy-match the danger-close proword.** `p.raw.includes('danger close')` (`index.html:3635`) is an exact substring match; transcript shows voice recognition twice rendering "DANGER CLOSE" as "DANGER CLOTHES," and the player gets rebuked for a proword they actually said. Needs tolerance for that near-homophone before gating on it. | Opus | A close STT variant of "danger close" (e.g. "danger clothes") still satisfies the gate | READY |
| F8 | **Mil card reference sizes did not match the world.** The card teaches `range = size / mils × 1000`, so a size that disagrees with the geometry silently teaches a wrong range. Truck card 5 m vs geometry 4.6 m (+174 m at 2000 m); target hut 3 vs 3.2 (−125 m); **village hut 3 vs 2.6 (+308 m)** — the card says "hut" once but the two hut types differed and are indistinguishable by eye; watchtower 100 m vs 300 m (3× error, and the observer stands on it so it can never be milled at all). Geometry rounded to the card, watchtower replaced by the airfield hangar. | Opus | **DONE** `ef0ef31` |
| F7 | **Readback duplicates DANGER CLOSE.** When the observer's own raw text already contains "danger close", the readback-generation code appends `, DANGER CLOSE` unconditionally (`index.html:3640`), producing "...DANGER CLOSE, DANGER CLOSE TROOPS..." in the one line CLAUDE.md calls sacred. Dedupe. | Opus | Readback shows DANGER CLOSE once regardless of how the observer phrased it | READY |

**F1 is a correctness bug in the doctrine the app exists to teach.** Recommend landing it alongside
13a as the other small, high-value, low-risk fix.

### Track G — User feedback notebook (source: [user_feedback.md](user_feedback.md), 2026-07-29)

Raw feedback from the user playing the build. **[user_feedback.md](user_feedback.md) is the user's
notebook and stays theirs** — this track is my triage of it, not a replacement for it. Nothing is
deleted from the notebook when a row ships; the row records it here.

This is the highest-value input on the board. Track A/B rows were written from a spec; these were written
by someone flying the trainer and hitting the edges. **Track G outranks the rest of stage 13** for the
same reason Track E did: several of these are doctrine correctness, and one (G7) means the FO can
currently practise a procedure the real fire net would reject.

Grouped by kind, because the risk profiles are completely different. **G-A** are self-contained UI/QoL
fixes. **G-B** are doctrine corrections and need DOCTRINE.md / JFIRE consulted first — several change
what the parser accepts and what the FDC says, which is the app's whole reason to exist. **G-C** are
structural and want a decision before code.

#### G-A — UI, optics and instrumentation (self-contained, ship first)

**G-A is COMPLETE.** All six shipped 2026-07-29, each with an executable harness and a
headless-Chrome parse. None has been seen running — see §5.

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| G1 | **Remove the watchtower rail/fence** — it obstructs the observer's view from the OP | Opus | View from the OP unobstructed at every heading; tower still reads as a structure | **DONE** `1c1bfff` ⚠ |
| G2 | **Magnetic vs true azimuth.** Map is true mil; everything the *observer* reads — compass, laser, HUD heading — should be **magnetic**, declination **+7°**. One conversion at the display boundary, not scattered. | Opus | Grid azimuth on the sheet and magnetic on the HUD differ by exactly 7° (124.4 mils); CFF traffic uses the correct one per DOCTRINE.md; no double-application | **DONE** `067b473` ⚠ |
| G3 | **Dispersion toggle** — turn off round error variance for testing. User-requested explicitly as a test aid. | Opus | Toggle makes `impact === aimpoint`; **flagged in the AAR and TLOG so a no-dispersion run can never be mistaken for a graded one** | **DONE** `6acc97c` |
| G4 | **Binocular FOV + two more zoom levels**, with the mil reticle staying **true at every level** | Opus | Mil graduations measure correctly at all zoom levels (a mil is a mil, or the reticle is a lie); FOV/zoom relationship stated in one place | **DONE** `03beef8` ⚠ |
| G5 | **Mil card renders behind the chat terminal** — z-order bug | Opus | Card fully visible with the terminal open | **DONE** `6b55985` ⚠ |
| G6 | **Chat terminal draggable + resizable** — it takes up too much space | Opus | Draggable, resizable, position persisted; never covers the reticle centre by default | **DONE** `1bfe48b` ⚠ |

Three bugs were **found while building these**, none of them reported, all in the class the
project cares most about — the trainer teaching something false:

- **F8** (fixed, `ef0ef31`): five of the six mil-card reference sizes disagreed with the geometry.
  Worst case the card said "hut" once while villages and enemy positions used different hut
  heights, so the same word milled to two different ranges — a 308 m error at 2000 m.
- **G2's real payload**: the two declination mistakes (sending the magnetic reading raw; applying
  the G-M angle backwards) both land 124 mils out, and the OT-direction coach only spoke above
  200 mils. The trainer accepted a rotated OT frame *in silence*.
- **G4's reticle**: graduated arms were hard-coded to ±42 mils, which overran the screen at 14X
  (595 px against a 540 px half-screen). Extent is now derived from the visible field.

The reticle's stadia bars were also sized to a 2 m **man**, contradicting the mil card's own
DO NOT MIL A MAN warning two panels away. Re-referenced to the 5 m truck.

#### G-B — Doctrine corrections (read DOCTRINE.md first; several need JFIRE)

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| G7 | **OT factor is not transmitted.** It is the *observer's* own correction arithmetic. OT **direction** is the thing that goes to the FDC, and only on **grid** missions. The trainer currently conflates them. | Opus | OT factor never appears in observer→FDC traffic; OT direction required on grid missions only; 12b's mil-relation workflow still teaches the factor as an observer tool | READY |
| G8 | **No OT direction → the battery cannot compute the correction.** On a grid mission, if OT direction was never sent before the first adjusting round, the FDC must be *unable* to execute the adjustment — not silently accept it. | Opus | Grid mission without OT direction blocks at the adjust step with a doctrinally correct refusal, not a generic parse error | READY |
| G9 | **Polar missions need a POS REP first** — the FDC cannot resolve a polar call without the observer's own location | Opus | Polar mission requires position report before the call is accepted; DOCTRINE.md updated to match | READY |
| G10 | **Initial firing-element status request** before the first mission — round count, location, munition types, number of guns. Not repeated afterwards. **Consult JFIRE for the correct name and format** — the user is explicit that they are unsure what this initial exchange is called, and that the in-mission MTO must stay untouched. | Opus | Correct doctrinal name and format sourced from JFIRE and recorded in DOCTRINE.md *before* implementation; in-mission MTO unchanged | RESEARCH FIRST |
| G11 | **Observer must read the MTO back** to the FDC — word-for-word intent, but accept a correct-gist readback | Opus | Readback required; gist-level match accepted; strict mode grades it tighter | READY |
| G12 | **Fratricide fails the mission but must NOT auto-end it.** Currently ends immediately. The mission still has to be *finished* — target destroyed / neutralized / suppressed — it is simply a failure when it ends. Same question applies to collateral damage. | Opus | Friendly hit = recorded failure + 0★, mission continues to a real conclusion; CLAUDE.md's auto-fail rule reworded from "auto-end" to "auto-fail" | READY |
| G13 | **Effects criteria and casualty radii are probably too small.** Needs **destroyed / neutralized / suppressed** as distinct outcomes with distinct criteria, per JFIRE. Give the observer the option to **continue the mission if the target is only suppressed**. | Opus | Three graded outcomes with sourced radii/criteria; "suppressed" offers continue-or-end; `effectRadius`/`hitsToNeutralize` replaced by the graded model | RESEARCH FIRST |
| G14 | **Immediate suppression *and* immediate smoke are one-transmission calls.** Supersedes and widens row **12g**, which covered suppression only. Live transcript evidence of a player hitting this: [DIALOGUE_REVISIONS.md §9.3](DIALOGUE_REVISIONS.md). | Opus | Both parse as a single transmission; FDC skips the MTO per DOCTRINE.md | READY |
| G15 | **Sheaf selection** — needed most for convoys and bunkers. If the observer does not specify, **the FDC chooses from the target description**. | Opus | Sheaf accepted when given, inferred when not, and the inference is explainable in the AAR | READY |
| G16 | **Fuze selection** — airburst for troops in the open, delay for bunkers; FDC infers if unspecified. Follow the doctrine PDFs. | Opus | Fuze accepted/inferred; choice affects the graded effect, not just the text | READY |
| G17 | **60mm and artillery need different callsigns.** Currently both talk to HELLHOUND FIRES. | Opus | Distinct callsign per asset; NARRATIVE.md updated so the name is story-consistent | READY |
| G18 | **60mm and artillery need different effective radii.** A mortar round and a 155 do not do the same thing. Research required. | Opus | Per-asset effect radius sourced and recorded; interacts with G13 — **land G13 first** | RESEARCH FIRST |
| G19 | **Full CFF protocol audit** — the user reports "inconsistencies" without enumerating them. Read DOCTRINE.md against the parser and the FDC script end to end and produce a findings list before changing anything. | Opus | Written findings list, each item either fixed or logged as its own row | **DONE** — findings below |

#### G19 findings — measured, 2026-07-29

Method: the shipped `normalize()` + `parseMessage()` were transcribed into a JScript harness and
run against 29 transmissions taken from DOCTRINE.md, citing the line that mandates each. Result:

> **16 of 29 correct doctrinal transmissions classify as `unknown`** — the branch whose FDC reply is
> *"Say again, over. Slower, and in English this time,"* the mockery reserved for gibberish. Three
> more mis-classify as something else and are silently acted on wrongly.

The user's word was "inconsistencies". It is worse than that, and this is the headline:

**G22 — the 3-transmission CFF does not exist.** DOCTRINE.md §15 and CLAUDE.md both state a CFF is
*six elements in three transmissions, each read back by the FDC*. There is no multi-transmission
state machine. `parseMessage` extracts warning order, location and description from **one** string,
so the doctrinal Transmission 1 — `"HELLHOUND FIRES, this is MUSTANG 12, adjust fire, over"` —
parses as `unknown` and gets mocked. An observer who has learned the real format literally cannot
use it; the trainer only accepts the whole call as a single run-on transmission. This contradicts
the app's own stated authority in two places and is almost certainly what the user hit.

Everything else, grouped:

| ID | Finding | Doctrine |
|---|---|---|
| G22 | **No 3-transmission CFF state machine** (above). Transmission 1 alone → `unknown`. | §15, and CLAUDE.md |
| G23 | **Height-of-burst corrections are parsed and thrown away.** `corr` has exactly two fields, `right` and `add`. Every `up`/`down` — in a correction, in a polar call's vertical, in a shift's vertical — is discarded. `"up 20, over"` alone → `unknown`. | §54, §27, §28 |
| G24 | **Fire-control prowords all unrecognised**: `at my command`, `fire`, `cancel at my command`, `do not load`, `cannot observe`, `time on target`. Worse, `"at my command, grid …"` parses as an ordinary grid CFF and the hold is swallowed into the target description. Supersedes/absorbs **12f**. | §34 |
| G25 | **Safety prowords unrecognised**: `check firing`, `cease loading`. These are the two calls that stop guns. Being mocked for them is the worst possible response in the worst possible moment. | §67 |
| G26 | **`immediate smoke` / `immediate suppression` with a grid parse as an ordinary HE grid mission** — mission type silently lost. Without a grid (`"immediate suppression 253535"`, from a real transcript) → `unknown`. Folds into **G14**. | §22 |
| G27 | **`suppress target AK1002, 10 minutes`** (a recorded target by number + duration) → `unknown`. | §22 |
| G28 | **Sheaf and fuze/ammunition terms unrecognised** as standalone transmissions. Currently a documented simplification (§76) that **G15/G16 deliberately overturn** — noted so the two are reconciled rather than half-built. | §33, §76 |

Working as intended, for the record: grid / polar / shift CFFs, standalone OT direction,
deviation+range corrections, `fire for effect`, RREMS end-of-mission, `say again`, `repeat`.
`"correction, grid …"` works, but by luck — the proword is ignored and the grid re-parsed.

**Consequence for the plan.** G22 is a structural change to the parser that G7, G8, G9, G11, G14,
G24, G26 and G27 all sit inside — a state machine that tracks which transmission the observer is on
is the thing that makes "read back the MTO" and "POS REP before a polar call" expressible at all.
**Do G22 first**, then the rest against it. Building them against the current one-shot parser would
mean rewriting all of them.

**One rule to carry into G22:** DOCTRINE.md §5 is explicit that scripts are *guidelines, not gates*,
and the forgiving default must survive. The state machine has to accept a complete one-shot call
exactly as it does today — that is what every existing chapter and transcript uses — while *also*
accepting the doctrinal three-transmission sequence. Strict mode is the only place the three-part
form becomes mandatory.

#### G-C — Structural (decide before coding)

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| G20 | **The 10×10 km map may be too small** — an 800 m correction runs out of world. Affects `CONFIG.MAP.size`, terrain, the DEM pipeline, the printed sheet scale and every grid in every fixed-seed chapter. **PARKED at the user's direction 2026-07-29: do not raise, cost, or implement this until the user brings it up.** Left on the board only so the observation is not lost. | — | **PARKED — do not action** |
| G21 | **Do target location cues stay accurate when a new DEM is loaded?** User's open question. Verify — do not assume. Covers the E2 spot report, `nearestLandmark`, known points and the printed sheet. | Opus | Answered with evidence against a real loaded DEM (`KOFA_KING_VALLEY_FO_HEIGHTMAP.png` is in the tree); any drift fixed or logged | AUDIT |

**Order:** ~~G1 → G5 → G3 → G2 → G4 → G6~~ ✅ shipped. ~~G19 audit~~ ✅ done — findings below, and
they changed the plan: **G22 (the 3-transmission CFF state machine) is now NEXT**, because G7, G8,
G9, G11, G14, G24, G26 and G27 all sit inside it. Then the G10/G13 doctrine research, then the rest
of G-B. **G20 is PARKED at the user's direction — do not action it until they raise it.** G21 last.

**Two G-A rows added keybinds** that the docs sweep must pick up: `[Z]` / mouse wheel cycles
binocular power (4X/7X/14X), and `SHIFT+D` toggles dispersion. Both are in the in-app hint line
already; README's control table is not yet updated.

**Two rows contradict documents that currently outrank them, and the documents lose:**
- **G12 vs CLAUDE.md.** CLAUDE.md says fratricide is an "automatic mission fail". The user's correction
  is that *fail* and *end* are different things. The rule becomes: fratricide and collateral damage
  auto-**fail** (0★, permanently), but the mission runs to a real conclusion. CLAUDE.md needs the
  wording change as part of G12's doc commit.
- **G14 vs row 12g.** 12g is narrower than the truth (suppression only, and marked `BLOCKED by 13`).
  G14 supersedes it; 12g should be struck when G14 lands rather than both being carried.

---

## 4. Definition of done (every row)

1. Gate in the table passes.
2. 60 fps held; no per-frame allocation added.
3. Stable interfaces unchanged in shape: `H(x,z)` · `fireMission` · `applyCorrection` · `FDC.say` ·
   `Scenario` · `gradeMission` · `TLOG` (in `SHITFIRE.html`; locate by symbol).
4. Ballistics untouched — `impact = aimpoint + error`. No trajectory, no angle-T, no gun-line rotation.
5. Fratricide and collateral damage still auto-fail.
6. Committed with the row ID as the message prefix.
7. This file updated to `DONE` **in the follow-up doc commit**, not the code commit.

---

## 5. ⚠ Needs-Chrome visual QA — the open gate on Tracks A and E

Nine shipped rows are gated on something no harness can check. Everything below was verified by
measurement or by parse; **none of it has been seen running.** This is one pass in Chrome, ~15 minutes,
and it closes all of it. Take a mission on the default island (terrain seed 1337) and check, in order:

| # | Row | What to look at | What would be a regression |
|---|---|---|---|
| 1 | 13c | The horizon | A visible band or seam where the sky meets the fog |
| 2 | 13b/13c | Open `[M]`, then `[P]` and print-preview a sheet | Any colour tint bleeding onto the map or the sheet — they are separate 2D canvases and should be untouched |
| 3 | E1 | Troops at 1500 m and at 3200 m through binos | Cannot count figures; **or civilians and soldiers becoming harder to tell apart** — that one is a hard fail, not a nit |
| 4 | E3 | `[O]` through DAY → NVG → THERMAL, at night and in daylight | Civ/mil discrimination lost in any mode; either device useless at night; either device *better* than the naked eye in full day |
| 5 | E4 | An asphalt route beside a dirt track, from the OP | The two classes not obviously different at range |
| 6 | E4/E3 | Asphalt in THERMAL after dark | Sealed road not reading warm against cool ground — it is meant to be a teachable cue |
| 7 | E5 | Compare a road bend on `[M]` against the same bend in the world | The paper and the world disagreeing about where a road goes |
| 8 | E6 | The town from ~2–3 km | Not unmistakably a *town* rather than a big village; civilian areas not obvious |
| 9 | E7 | A boulder field at 1500–3200 m, and its symbol on both surfaces | Piles reading as noise instead of one identifiable feature; symbol illegible at `[M]` scale; symbol confusable with the fuel-point circle |
| 10 | E7 | A named outcrop in a SALUTE report vs. its label on the sheet | Net name and sheet label disagreeing |
| 11 | G1 | Look down over the deck edge from the OP | Near ground still hidden; or the tower no longer reads as a structure |
| 12 | G4 | `[Z]` / wheel through 4X → 7X → 14X, glassing a truck | Reticle sparse or clipped at any power; mil graduations not visibly rescaling |
| 13 | G2 | `MAG` in the topbar, and a printed sheet's new declination diagram | Diagram illegible in black and white; MAG mistaken for the old AZ |
| 14 | G5 | Open the mil card `[R]` with the comms panel up | Card still overlapped, or fighting the touch bar on a small window |
| 15 | G6 | Drag the comms header, resize from the corner, double-click to reset | Resize corner invisible; transcript clipped instead of resized; scroll not pinned to bottom |

Anything that fails here becomes a Track F row, not a silent revert. Anything that passes flips its ⚠
off in the tables above.

---

## 6. Change log

| Date | Change |
|---|---|
| 2026-07-29 | Board created. Order set: stage 13 → stage 12 remainder → stage 11. Backlog moved out of README.md (duplicate IDs #27/#28 retired). Time-of-day assigned to 13c, removed from stage 12. G8 bloom reassigned to stage 11 as 11d. |
| 2026-07-29 | Baseline moved `a847731` → `a74cee7`: an off-board Fable commit (AAR shot plot auto-fit + legibility) landed mid-session. Not attached to any stage — the AAR shot plot is an extra shipped alongside spec, like the OP watchtower and TLOG. Logged here so the board matches history. |
| 2026-07-29 | **13a** `af439a5` and **F1** `e7ccfdb` shipped. Both implemented by Opus, not Fable — Fable 5 ran out of usage credits mid-dispatch. Deviation from CLAUDE.md's model split, accepted for two small well-gated rows; the split still stands as the default. |
| 2026-07-29 | BALLISTICS_RESEARCH.md added to the authority map (reference only). Its §6 is flagged as required reading for row F2. |
| 2026-07-29 | **13b** `d586e54` code landed; visual gate left open pending Chrome. GRAPHICS.md G0.2 corrected — its "ACES desaturates midtones, bump the greens" guidance was measured false against this palette, so `satComp` ships neutral instead of 1.12. The CPU background pre-tone (`acesFilmic()`) was not in the original plan; it is now a stated requirement for 13c. |
| 2026-07-29 | **13b visual gate closed** — user confirmed the render reads good in Chrome. `satComp` stays neutral at 1.0. Row is DONE. |
| 2026-07-29 | **Model split changed: sim code → Opus, worked directly. Fable is out of credits on this account, so dispatching to it fails outright.** CLAUDE.md, this file's rule 6, and QUICKSTART updated; owner column swept. `Fable` in commit messages before this date is history, not instruction. |
| 2026-07-29 | **Track E closed** — E3 `ef9d473`, E4 `830d3cc`, E5 `cee195f`, E6 `0d2eaed`, E7 `ddd22f9`. All seven rows shipped; all carry ⚠, since the track is entirely about appearance and none of it has been seen in Chrome. New §5 collects that QA into one 15-minute pass. |
| 2026-07-29 | **`index.html` → `SHITFIRE.html`**, renamed by the user, recorded as a git rename in `f4e7ad8` (staged from HEAD's exact blob so the diff is a pure path change and `--follow` still reaches all history). CLAUDE.md's first golden rule and the references in README/SPEC/QUICKSTART/GRAPHICS are stale until the docs sweep. |
| 2026-07-29 | **A working JS syntax gate exists now** (`scratchpad/syntaxgate.ps1`): extracts the inline module, strips the imports, wraps it in `if (false) {}` and loads it as a classic script in headless Chrome, so a syntax error is reported as the early error it is while a clean parse executes nothing. Its first version used `new Function(src)` and was **worthless** — that compiles lazily, so it reported OK on a deliberately broken file. Both directions are now verified against an injected unbalanced paren, which it caught and located to the exact line. Every code row from E7 on should run it. |
| 2026-07-29 | **G19 CFF protocol audit done, and it reordered the track.** The shipped parser was run against 29 transmissions taken from DOCTRINE.md: **16 classify as `unknown`** and get the FDC's gibberish reply, 3 more mis-classify and are acted on wrongly. Headline: **the 3-transmission CFF does not exist** — there is no multi-transmission state machine, so the doctrinal Transmission 1 gets mocked, contradicting both DOCTRINE.md §15 and CLAUDE.md. Logged as **G22–G28**; G22 is now NEXT because eight other rows sit inside it. |
| 2026-07-29 | **Track G-A shipped whole** — G1 `1c1bfff`, G5 `6b55985`, G3 `6acc97c`, G2 `067b473`, G4 `03beef8`, G6 `1bfe48b`, plus **F8** `ef0ef31` found along the way. Every row carries an executable harness and a headless-Chrome parse; none has been seen running, so §5 grew. Three unreported training-fidelity bugs surfaced during the work (mil-card sizes disagreeing with the world by up to 308 m of taught range error; both declination mistakes landing inside the OT-direction coach's blind spot; the reticle overrunning the screen at 14X), which is the argument for taking user-feedback rows before spec rows — building next to real complaints finds the things nobody thought to report. |
| 2026-07-29 | **Track G added** from [user_feedback.md](user_feedback.md) — 21 rows of feedback from the user actually flying the build, triaged into UI (G1–G6), doctrine (G7–G19) and structural (G20–G21). Inserted ahead of the rest of stage 13. Three rows need doctrine research before code (G10, G13, G18) and one is a decision, not a task (G20). Two supersede existing authority: **G12** overrides CLAUDE.md's "automatic mission fail" wording — fratricide *fails* the mission but must not *end* it — and **G14** supersedes the narrower row 12g. |
| 2026-07-29 | Reviewed the three newer `Dialogue History/` transcripts (08-48, 12-56, 18-05). Added **F5** (STT/typo tolerance in adjust corrections), **F6** ("danger clothes" fuzzy-match), **F7** (readback duplicates DANGER CLOSE) to Track F, all found by reading real play and confirmed against the regexes. Annotated **12g** with live transcript evidence of a player hitting the immediate-suppression gap. Detail in [DIALOGUE_REVISIONS.md §9](DIALOGUE_REVISIONS.md). |
