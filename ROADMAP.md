# ROADMAP.md — the board

**This file is the single authority on _what ships next_ and _what is done_.** Nothing else is.
If another document disagrees with this one about order or status, this one wins and the other
document is the bug.

Last updated: 2026-07-30 · Head: `5d3fbe3` · **The file is `SHITFIRE.html`, not `index.html`**

**Shipped off this board:** `13a` bino quality pin · `F1` range rounding · `13b` ACES tone mapping ·
`13c` sky + time of day · **Track E complete** (`E1`–`E7`: legibility, SALUTE, night/NVG/thermal,
asphalt roads, roads on the map, settlement hierarchy, rock outcrops) · **Track G-A complete**
(`G1`–`G6`: watchtower rail, magnetic azimuth, dispersion toggle, bino zoom, mil-card z-order,
movable comms panel) · `F8` mil-card sizes · `F5`–`F7` STT/typo tolerance, danger-clothes fuzzy
match, duplicate-DANGER-CLOSE readback · `G19` CFF audit · **G-B doctrine cluster** (`G7`/`G8`/`G9`/
`G11` OT direction & MTO readback, `G14` immediate suppression/smoke, `G22`–`G27` the CFF protocol
rows).

**Next:** **G10** (FIRE UNIT STATUS exchange, borrowed NGF shape per research). Research is on file in DOCTRINE.md §Pre-mission and effects data, `8304ef6`.

> ⚠ **Nine shipped rows still need a human in Chrome.** Every visual row was verified by harness
> (real arithmetic) and by a headless-Chrome **parse** gate. Neither can see a picture. The list of
> what to look at lives in **[GRAPHICS.md](GRAPHICS.md) §Open visual QA backlog** (15 items, ~15
> minutes); rows marked `DONE ⚠` keep the ⚠ until the matching item is confirmed by eye. Not
> blocking — the active parser work does not touch rendering.

> ⚠ **Concurrency is live in this repo, not hypothetical.** An off-board commit landed on the sim file
> mid-session (`a74cee7`), BALLISTICS_RESEARCH.md and new TLOG transcripts appeared during other work,
> and the `index.html` → `SHITFIRE.html` rename arrived from outside the session while a row was open.
> **Confirm nothing else holds `SHITFIRE.html` before starting a row** — rule 1 below. Four occurrences
> so far.

> 🔧 **There is a working syntax gate.** `scratchpad/syntaxgate.ps1` extracts the inline module, strips
> the imports, wraps it in `if (false) {}` and loads it as a classic script in headless Chrome, so a
> syntax error surfaces as the early error it is while a clean parse executes nothing. **Its first
> version was worthless** — `new Function(src)` compiles lazily and passed a deliberately broken file.
> Both directions are verified against an injected unbalanced paren, which it catches and locates to
> the exact line. Run it on every code row.

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
| **What still needs looking at in Chrome** | **GRAPHICS.md** §Open visual QA backlog | ROADMAP §5 (pointer only) |
| Dialogue rewrite text | **DIALOGUE_REVISIONS.md** (detail for stage 14) | NARRATIVE |
| Training-objective crosswalk | **TLO.md** | — |
| Real-world dispersion / lethality figures | **BALLISTICS_RESEARCH.md** (reference only — nothing in it is applied, and it does **not** license a trajectory sim) | — |
| How do we move this to a game engine? | **ENGINE_PORT/** (README → CHOICE → ARCHITECTURE → PARITY_TESTING → PORT_PLAN) | SPEC, this board |
| What is this project? (public face) | **README.md** | — |

**README.md no longer carries a backlog.** It had one, it drifted, and it ended up with two
different item #27s and two different #28s. Backlog lives here now, with stable IDs.

---

## 2. Work-order protocol (the anti-drift rules)

1. **One writer on `SHITFIRE.html` at a time. Ever.** This is the rule that was already broken once:
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
6. **Model split** (per CLAUDE.md): sim code in `SHITFIRE.html` → **Opus**, worked directly.
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

### Track A — Stage 13: Visual overhaul

Detail spec: [GRAPHICS.md](GRAPHICS.md). Each row is gated behind one `CONFIG.GFX` flag so any row
can be A/B'd or reverted alone.

| ID | G-ref | What | Owner | Gate | Status |
|---|---|---|---|---|---|
| 13a | G0.4 | **Bino quality pin** — stop adaptive quality dropping pixel ratio while binos are up | Opus | Troop figures countable through binos at 3000 m after a forced quality step-down | **DONE** `af439a5` |
| 13b | G0 rest | `CONFIG.GFX` block, ACES tone mapping, CPU-side background pre-tone | Opus | Map sheets unaffected ✅ · tone confirmed good in Chrome ✅ | **DONE** `d586e54` |
| 13c | G1 | Sky + **time-of-day model** (`Sky.js`, TOD table drives sun/hemi/fog). Sky.js includes `<tonemapping_fragment>`, so no CPU pre-tone is needed on the sky path; it is retained on the `sky:false` fallback. `day` reproduces the pre-13c sun to 0.038°. | Opus | Horizon has no seam; `[M]` map + printed sheets inherit no tint | **DONE** `999e4c6` ⚠ needs Chrome |
| 13d | G2 | **Baked hillshade + AO into terrain vertex colors** — shipped `dd3a37e`. Sun term from the CURRENT TOD via `todDir` (applyTOD re-bakes; 13c's hook had a boot TDZ trap, closed with a hoisted `terrainReady` var). AO = concavity vs a 48 m ring, max 0.35. `hillFloor: 0.55` keeps black sand above 34/255. 7 H()/vertex vs 3, build-time only. Harness 13/13. | Fable | Ridge in 3D matches contours on the sheet ⚠ needs Chrome; black palette doesn't crush ✅ measured | **DONE** `dd3a37e` ⚠ |
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
12e OT direction + RREMS. Remaining, all verified absent from `SHITFIRE.html`:

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| 12-audit | ~~**Verify what 12c/12d actually landed.**~~ **CLOSED — present under different names.** `timeToInit` is **`tInit`** (set in `fireMission`, gated at 120 s in the AAR diagnosis, in the AAR table with the ≤2:00 standard, TLOG'd). Location error is **`aimErr0`** (scalar, gated at 200 m, EAGLE EYE ≤50 m) and the *vector* is drawn on the AAR shot plot as the `SENT (N m off)` marker in the OT frame. Nothing to build. | Fable | **DONE** (audit) |
| ~~12f~~ | ~~**"At my command"** fire-control hold before FFE~~ — **SUPERSEDED by G24**, shipped `bd1de09`. Struck rather than deleted so the ID is not reused. | — | **SUPERSEDED** |
| ~~12g~~ | ~~**Immediate suppression** mission type~~ — **SUPERSEDED by G14/G26**, shipped `fbbc137`, which also covers immediate smoke. Struck, not deleted. | — | **SUPERSEDED** |
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

Chapter stubs already exist in `SHITFIRE.html` with `impl: false` (search `impl: false`).
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
exists and none is scheduled; nothing on this track competes with stage 13/14 rows for `SHITFIRE.html`.

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
| F3 | ~~In-sim cheat-sheet overlay `[H]`~~ **DONE** `e825c0b` — pull-up, nothing pauses, [H]/[ESC]. Content transcribed from CHEATSHEET.md and **updated where today's rows changed the truth** (POS REP first on polar, the OT-direction refusal, MTO readback, G-M angle, new prowords, fail≠end). ⚠ CHEATSHEET.md itself now trails the sim — next docs pass. | Fable | **DONE** `e825c0b` |
| F4 | Multi-phase MEAT GRINDER chaining (4.4) — revisit after the single-mission version is balanced | Opus | PARKED |
| F5 | ~~**STT/typo tolerance in adjust corrections.**~~ **DONE** `1e39359` — transcript evidence: voice recognition renders "right" as "WRITE" and "drop" as "DROPPED"/"DRAW"; the `\b(left\|right)\s+(\d+)` and `\b(add\|drop)\s+(\d+)` regexes (the `left|right` / `add|drop` matches in `parseMessage`) matched none of these, so a mangled deviation word was silently dropped from the correction (no error, no notice) while a mangled range word alone parsed as full `unknown`. A stray-number check now catches the half-correction instead of letting it pass silently. See [DIALOGUE_REVISIONS.md §9.4](DIALOGUE_REVISIONS.md). | Opus | Correction with one STT-plausible word variant (e.g. "write"/"dropped") still parses both fields; nothing is silently dropped | **DONE** `1e39359` |
| F6 | ~~**"Danger clothes" — fuzzy-match the danger-close proword.**~~ **DONE** `21d0dc1` — `p.raw.includes('danger close')` in `handleCFF` was an exact substring match; transcript showed voice recognition twice rendering "DANGER CLOSE" as "DANGER CLOTHES," and the player got rebuked for a proword they actually said. A narrow fuzzy gate now accepts the near-homophone before gating on it. | Opus | A close STT variant of "danger close" (e.g. "danger clothes") still satisfies the gate | **DONE** `21d0dc1` |
| F8 | **Mil card reference sizes did not match the world.** The card teaches `range = size / mils × 1000`, so a size that disagrees with the geometry silently teaches a wrong range. Truck card 5 m vs geometry 4.6 m (+174 m at 2000 m); target hut 3 vs 3.2 (−125 m); **village hut 3 vs 2.6 (+308 m)** — the card says "hut" once but the two hut types differed and are indistinguishable by eye; watchtower 100 m vs 300 m (3× error, and the observer stands on it so it can never be milled at all). Geometry rounded to the card, watchtower replaced by the airfield hangar. | Opus | **DONE** `ef0ef31` |
| F7 | ~~**Readback duplicates DANGER CLOSE.**~~ **DONE** `21d0dc1` — when the observer's own raw text already contained "danger close", `handleCFF` appended `, DANGER CLOSE` to `locStr` unconditionally while the target description could already contain the words, producing "...DANGER CLOSE, DANGER CLOSE TROOPS..." in the one line CLAUDE.md calls sacred. Now deduped. | Opus | Readback shows DANGER CLOSE once regardless of how the observer phrased it | **DONE** `21d0dc1` |

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
| G7 | ~~**OT factor is not transmitted.** It is the *observer's* own correction arithmetic. OT **direction** is the thing that goes to the FDC, and only on **grid** missions. The trainer currently conflates them.~~ **DONE** `313609c` — audit first: the factor was already observer-side everywhere (mil card, laser readout, coach; no FDC line asked for it). The real gap was answering an observer who transmits it anyway — snide teaching line + AAR note instead of the old gibberish mock. Direction required on grid only (G8's gate). Harness 10/10. | Opus | OT factor never appears in observer→FDC traffic; OT direction required on grid missions only; 12b's mil-relation workflow still teaches the factor as an observer tool | **DONE** `313609c` |
| G8 | ~~**No OT direction → the battery cannot compute the correction.** On a grid mission, if OT direction was never sent before the first adjusting round, the FDC must be *unable* to execute the adjustment — not silently accept it.~~ **DONE** `1944cd2` — no OT direction, no correction, on grid missions, in every mode: the old coach-once-then-execute-anyway was wrong physics, not leniency — the battery cannot orient "left 50" or "add 200" without the OT line, so the sim was doing impossible math on the observer's behalf. Refusal repeats until DIRECTION arrives; range-only corrections gated too; bare FFE passes; polar/shift exempt (their calls carry direction). Harness 12/12. | Opus | Grid mission without OT direction blocks at the adjust step with a doctrinally correct refusal, not a generic parse error | **DONE** `1944cd2` |
| G9 | ~~**Polar missions need a POS REP first** — the FDC cannot resolve a polar call without the observer's own location~~ **DONE** `0fc9f6c` — polar missions require a POSITION REPORT ("POSITION GRID 245 523", also POS REP / MY POSITION IS), refused without one. **The mission resolves from the REPORTED position, not the true one** — a self-location error moves every round with it, no hint given (TLOG still records the true error). Parser ordering mattered: "position grid …" would otherwise have parsed as a fire mission at the observer's own position. Expires per mission. Harness 19/19. | Opus | Polar mission requires position report before the call is accepted; DOCTRINE.md updated to match | **DONE** `0fc9f6c` |
| G10 | **Initial firing-element status request** before the first mission — round count, location, munition types, number of guns. Not repeated afterwards. **Consult JFIRE for the correct name and format** — the user is explicit that they are unsure what this initial exchange is called, and that the in-mission MTO must stay untouched. **Finding: no land arty/mortar doctrine names this exchange at all — only NGF has one (FIRE UNIT STATUS, FM 6-30 §8-12); build borrows its shape.** See [DOCTRINE.md](DOCTRINE.md#pre-mission-and-effects-data-researched-2026-07-29-for-g10g13g15g16g18). | Opus | Correct doctrinal name and format sourced from JFIRE and recorded in DOCTRINE.md *before* implementation; in-mission MTO unchanged | RESEARCHED — build ready |
| G11 | ~~**Observer must read the MTO back** to the FDC — word-for-word intent, but accept a correct-gist readback~~ **DONE** `2f66d39` — gist scorer (3+ of the MTO's key elements); **target number is non-negotiable** — wrong/missing number is challenged even on a perfect recitation, since the number alone isn't "the entire MTO." Verbatim vs gist distinguished; strict notes a gist readback. Skipping it mirrors the OT-direction gate: strict blocks once, forgiving coaches once, AAR row either way. MTO now ends in OVER (it hands the net over for the readback). Harness 31/31. | Opus | Readback required; gist-level match accepted; strict mode grades it tighter | **DONE** `2f66d39` |
| G12 | ~~**Fratricide fails the mission but must NOT auto-end it.**~~ **DONE** `27b42cd` — failReason is permanent (0★, verdict never softens) but the mission runs to a real conclusion: effect on target, then RREMS. The rant fires once; repeat hits get one grim line. First cause stands (frat-then-collateral stays FAIL — FRATRICIDE). No early return, so a round near both sides counts for both and the net cannot jam in SHOT. Convoy `escaped` still auto-ends — nothing left to shoot. CLAUDE.md reworded (fail ≠ end). Harness 18/18. | Fable | **DONE** `27b42cd` |
| G13 | ~~**Effects criteria and casualty radii are probably too small.** Needs **destroyed / neutralized / suppressed** as distinct outcomes with distinct criteria, per JFIRE. Give the observer the option to **continue the mission if the target is only suppressed**.~~ **DONE** `e3a35d6` — binary `effectRadius`/`hitsToNeutralize` replaced by the graded model from DOCTRINE.md §Pre-mission and effects data, all constants in `CONFIG.EFFECTS`: per-round casualty/damage % banded by miss distance (full ≤30 m / half ≤50 m / quarter ≤75 m for personnel; 15/25/45 m for point targets — wreck, bunker), posture-scaled (a near-miss flips the target PRONE, FM 7-90 App. B, instead of shrinking a radius), accumulating into SUPPRESSED / NEUTRALIZED ≥10% / DESTROYED ≥30% per FM 6-30 §4-14. Suppression is temporary and visible (enemy fire lapses for a 90 s window; `everSuppressed` remembered for the verdict). Continue-after-suppression: after ROUNDS COMPLETE the mission only closes if accomplished — corrections re-open the shoot, REPEAT re-fires the volley, effect accumulates across volleys. Suppressed-only PASSES a suppress-intent mission; 1★ MARGINAL on a destroy-intent mission. EOM records the observer's claimed surveillance term; AAR compares claim vs. assessed effect and calls out overclaimed BDA. Verified: 19/19 numeric harness against the shipped artifact + 15/15 live scripted mission (suppressed → REPEAT → neutralized → REPEAT refused → RREMS → AAR); replay over 247 recorded transmissions: zero classification regressions; shots rig boots clean offline. | Opus | Three graded outcomes with sourced radii/criteria; "suppressed" offers continue-or-end; `effectRadius`/`hitsToNeutralize` replaced by the graded model | **DONE** `e3a35d6` |
| G14 | **Immediate suppression *and* immediate smoke are one-transmission calls.** Supersedes row **12g**. Live transcript evidence: [DIALOGUE_REVISIONS.md §9.3](DIALOGUE_REVISIONS.md). | Opus | Both parse as a single transmission; FDC skips the MTO | **DONE** `fbbc137` |
| G15 | ~~**Sheaf selection** — needed most for convoys and bunkers. If the observer does not specify, **the FDC chooses from the target description**.~~ **DONE** `8b406e7` — sheaf is a real method-of-engagement element: requested by name in any transmission of the call (raw survives CFFQ concatenation), inferred otherwise (linear for convoy/column, converged for hard point, open for personnel, doctrine default circular 100 m), bare mid-mission "CONVERGED SHEAF, OVER" updates it, "CANCEL ... SHEAF" reverts to the FDC's own choice (ATP 3-09.30 §5-30). PARALLEL/SPECIAL fire as linear (special without attitude gets a format note). Effect scales FFE rounds only: converged ×1.25 point targets / ×0.6 dispersed men; open ×1.2 dispersed personnel / ×0.6 point; on a convoy a LINEAR sheaf is real aimpoint geometry (volley spreads 35 m apart along the column axis, each round still `impact = aimpoint + error`). Choice + source + reason logged at fire time, shown in the AAR, recorded in TLOG. Verified 37/37 numeric + 15/15 e2e + replay unchanged. | Opus | Sheaf accepted when given, inferred when not, and the inference is explainable in the AAR | **DONE** `8b406e7` |
| G16 | ~~**Fuze selection** — airburst for troops in the open, delay for bunkers; FDC infers if unspecified. Follow the doctrine PDFs.~~ **DONE** `5d3fbe3` — fuze requested in the call ("SHELL HE, FUZE VT") or bare mid-adjustment ("FUZE TIME, OVER", legal before FFE per ATP 3-09.30 §5-55); inferred DELAY for hard/overhead-cover, VT for troops under cover, default PD. VICTOR TANGO/VARIABLE TIME/QUICK/POINT DETONATING normalize. The MTO now announces the actual fuze instead of hardcoded FUZE PD. Effect scales every round: VT ×1.5 vs prone (defeats going flat) and ×0.35 vs a bunker roof; DELAY ×1.3 on point targets, ×0.45 vs men in the open; stacks with sheaf and posture. VT/time never fired DANGER CLOSE — FDC overrides to PD and says so, at call time and mid-mission. G23's height-of-burst hook is live: under VT/time an UP/DOWN correction is acknowledged; under PD the refusal stands. Verified 48/48 numeric + 15/15 e2e + replay unchanged. | Opus | Fuze accepted/inferred; choice affects the graded effect, not just the text | **DONE** `5d3fbe3` |
| G17 | ~~**60mm and artillery need different callsigns.**~~ **DONE** `96a2545` — the 60mm section is **HACKSAW FIRES**, renamed centrally at delivery inside `FDC.say` (one point every utterance passes through, so the quip pools are covered too, instead of editing ~35 strings). Strict net bounces the WRONG callsign with a specific challenge in both directions. Phonetically far from HELLHOUND on purpose (STT). NARRATIVE.md roster updated; a distinct personality is stage-14 work. Harness 20/20. | Fable | **DONE** `96a2545` |
| G18 | ~~**60mm and artillery need different effective radii.** A mortar round and a 155 do not do the same thing.~~ **DONE** `13084ef` — `CONFIG.EFFECTS` now carries a band set per firing asset, selected in `effBands()` by the chapter's asset. 155/arty keeps the G13 numbers (personnel 30/50/75 m, 8%/round). 60mm personnel bands are FM 7-90 App. B verbatim (full ≤20 m / half 35 m / little beyond 50 m, 2%/round) so the manual's own check case reproduces exactly: a 10-round standing volley assesses 20% (neutralized), the same volley prone assesses 8% (suppression only). 60mm point-target bands 10/18/35 m at 25%/round — four direct hits to destroy a bunker. Chapter 4.2 (TEN METERS) dropped its legacy `scn:{effR:30}` knob — the mortar band set is the precision constraint that scaled radius was faking; `effScale` survives for future chapters. Harness extended to 25/25 (both assets, FM 7-90 check case, band edges, asset fallback); 15/15 live end-to-end unchanged on the arty path; boots clean offline. | Opus | Per-asset effect radius sourced and recorded; interacts with G13 — **land G13 first** | **DONE** `13084ef` |
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
| G22 | ~~**No 3-transmission CFF state machine**. Transmission 1 alone → `unknown`.~~ **DONE** `4570bbd` — `CFFQ` accumulates partial transmissions and hands the merged call to the *same* `handleCFF`, so one code path still validates and fires. Purely additive: a complete one-shot call never enters the new code, which is how every existing chapter and transcript keeps working. Per-transmission readbacks; a half-sent call lapses after 75 s. Harness 25/25, including a five-case regression guard proving one-shot calls bypass the queue entirely. | §15, and CLAUDE.md |
| G23 | ~~**Height-of-burst corrections are parsed and thrown away.**~~ **DONE** `d81a93e` — vertical captured in all three doctrinal places. HOB is acknowledged but **provably does not move the aimpoint** (fuze quick; no trajectory sim per CLAUDE.md), with a hook comment for G16. Polar/shift verticals are checked against the **35 m rule** using the heightfield, coaching both the omission *and* the spurious inclusion. Fixed two description-swallowing bugs found while wiring it. Harness 21/21. | §54, §27, §28 |
| G24 | ~~**Fire-control prowords all unrecognised.**~~ **DONE** `bd1de09` — all six work. `at my command` **persists** until cancelled (not a one-shot); cancelling releases anything already laid rather than stranding it. Ordering is load-bearing since every one of these strings contains "fire". Harness 34/34, including proof that without the one-shot latch, FIRE could never get a round out. Supersedes **12f**. | §34 |
| G25 | ~~**Safety prowords unrecognised**: `check firing`, `cease loading`.~~ **DONE** `33fe820` — both matched early, always answered (even with no mission), hold enforced twice including at the last gate before rounds leave the tube. **Rounds already in the air are deliberately not recalled** — pretending a check-firing call can do that would teach the observer the call is a bigger safety net than it is. Resuming is implicit. Harness 22/22, including a demonstration that gating before the lift would deadlock the mission permanently. | §67 |
| G26 | ~~**`immediate smoke` / `immediate suppression` parse as an ordinary HE grid mission.**~~ **DONE** `fbbc137` (with G14/G27) — mission type captured, **no MTO** per §42, and the grid-less transcript form (`"immediate suppression 253535"`) plus spoken digits both parse. | §22 |
| G27 | ~~**`suppress target AK1002, 10 minutes`** → `unknown`.~~ **DONE** `fbbc137` — needed a store that did not exist: the target number from `record as target` was announced and discarded. `RECTGT` now holds it with the post-refinement aimpoint. Unknown numbers are refused and the FDC lists what it holds. Duration is graded as format, not simulated. | §22 |
| G28 | ~~**Sheaf and fuze/ammunition terms unrecognised** as standalone transmissions. Currently a documented simplification (§76) that **G15/G16 deliberately overturn** — noted so the two are reconciled rather than half-built.~~ **CLOSED by G15/G16** `8b406e7`/`5d3fbe3` — both now recognised as standalone method-of-engagement terms and mid-mission updates, per those rows above. | §33, §76 |

Working as intended, for the record: grid / polar / shift CFFs, standalone OT direction,
deviation+range corrections, `fire for effect`, RREMS end-of-mission, `say again`, `repeat`.
`"correction, grid …"` works, but by luck — the proword is ignored and the grid re-parsed.

**Consequence for the plan — now resolved.** G22 was the structural prerequisite that G7, G8, G9,
G11, G14, G24, G26 and G27 all sat inside: a state machine tracking which transmission the observer
is on is what makes "read back the MTO" and "POS REP before a polar call" expressible at all.
**G22 shipped `4570bbd`, so all eight are now buildable.** They were correctly *not* built first —
against the old one-shot parser every one would have needed rewriting.

The rule G22 was built under, kept here because the remaining rows must honour it too: DOCTRINE.md
§5 is explicit that scripts are *guidelines, not gates*. The forgiving one-shot call still works
byte-identically and never enters the queue; the doctrinal three-transmission sequence is
*additionally* accepted. Strict mode remains the only place rigid form is enforced. **Any row that
adds a new transmission type must extend `CFFQ`, not fork it** — the whole value of G22 is that one
code path validates and fires a mission.

#### G-C — Structural (decide before coding)

| ID | What | Owner | Gate | Status |
|---|---|---|---|---|
| G20 | **The 10×10 km map may be too small** — an 800 m correction runs out of world. Affects `CONFIG.MAP.size`, terrain, the DEM pipeline, the printed sheet scale and every grid in every fixed-seed chapter. **PARKED at the user's direction 2026-07-29: do not raise, cost, or implement this until the user brings it up.** Left on the board only so the observation is not lost. | — | **PARKED — do not action** |
| G21 | ~~**Do target location cues stay accurate when a new DEM is loaded?**~~ **DONE** `b17acbc` — YES, after one fix. Spot reports, landmarks, KPs, posRep, sheets and minimap all re-derive from `H`+`WORLD`, which both island-change paths rebuild. The one stale survivor: **G27's recorded targets held world coordinates across island changes** — `SUPPRESS TARGET AB7101` would have fired at where a target used to be on a different island. Now wiped on both paths, with a log line; same-island targets survive. Harness 10/10. | Fable | **DONE** `b17acbc` |

**Order:** ~~G1 → G5 → G3 → G2 → G4 → G6~~ ✅ shipped · ~~G19 audit~~ ✅ · ~~G22~~ ✅ `4570bbd`.

~~G23~~ ✅ `d81a93e` · ~~G25~~ ✅ `33fe820` · ~~G24~~ ✅ `bd1de09` · ~~G14/G26/G27~~ ✅ `fbbc137`.

~~G11~~ ✅ `2f66d39` · ~~G8~~ ✅ `1944cd2` · ~~G9~~ ✅ `0fc9f6c` · ~~G7~~ ✅ `313609c`.

**Every row the G19 audit found is now closed, G28 included** (sheaf/fuze, closed by G15/G16), and
the G7/G8/G9/G11 OT-direction/MTO-readback cluster is now closed too.

~~G12~~ ✅ `27b42cd` · ~~G17~~ ✅ `96a2545` · ~~G13~~ ✅ `e3a35d6` · ~~G18~~ ✅ `13084ef`.

~~G15~~ ✅ `8b406e7` · ~~G16~~ ✅ `5d3fbe3`.

**Next: G10** (FIRE UNIT STATUS exchange, borrowed NGF shape). G21 already closed above.
**G20 is PARKED at the user's direction — do not action it until they raise it.**

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

## 5. ⚠ Visual QA — lives in GRAPHICS.md

**The 15-item needs-Chrome list moved to [GRAPHICS.md](GRAPHICS.md) §Open visual QA backlog**
(user direction, 2026-07-29). It is a graphics to-do list, so it belongs with the graphics spec, and
§1 of this file says one question gets one authority.

What stays here: the rows it gates. Every row marked **DONE ⚠** in the tables above has landed and
been verified by harness and by parse, but has **not been seen running**. The ⚠ comes off a row only
when the matching item in GRAPHICS.md is confirmed by eye. A failure there becomes a Track F row, not
a revert.

Currently 15 open items covering 13b/13c, E1, E3–E7 and G1–G6. Not blocking — the active parser
work does not touch rendering.

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
| 2026-07-29 | **G24** `bd1de09` and **G14/G26/G27** `fbbc137` shipped, closing every G19 audit row except G28. G24 absorbs the long-blocked **12f**: `at my command` persists until cancelled, and the harness demonstrates that without a one-shot latch the held round could never be released however many times FIRE was sent. G14/G26/G27 fix a MIS-recognition rather than a gap — `immediate smoke` was parsing as an ordinary HE mission, so the observer got high explosive when he asked for smoke — and G27 required storing the target number that `record as target` had been announcing and discarding. **12f and 12g are now superseded and should be struck from Track B.** |
| 2026-07-29 | **G23** `d81a93e` and **G25** `33fe820` shipped. G23 restores the vertical element in all three doctrinal places and checks polar/shift verticals against the 35 m rule off the heightfield — height of burst is acknowledged but provably does not move the aimpoint, since the model is horizontal-only and faking it would teach a lie. G25 makes the two gun-stopping prowords work; rounds already in the air are deliberately not recalled, and the harness demonstrates that lifting the hold after the state gate (rather than before) would have deadlocked the mission with no way back. |
| 2026-07-29 | **G22 shipped** `4570bbd` — the three-transmission CFF exists. `CFFQ` accumulates partial transmissions and hands the merged call to the same `handleCFF` the one-shot path uses, so there is still exactly one code path that validates and fires a mission. Deliberately purely additive: a complete one-shot call never enters the new code, which is what keeps every existing chapter, tutorial and saved transcript working. Unblocks G7, G8, G9, G11, G14, G24, G26, G27. |
| 2026-07-29 | **G19 CFF protocol audit done, and it reordered the track.** The shipped parser was run against 29 transmissions taken from DOCTRINE.md: **16 classify as `unknown`** and get the FDC's gibberish reply, 3 more mis-classify and are acted on wrongly. Headline: **the 3-transmission CFF does not exist** — there is no multi-transmission state machine, so the doctrinal Transmission 1 gets mocked, contradicting both DOCTRINE.md §15 and CLAUDE.md. Logged as **G22–G28**; G22 is now NEXT because eight other rows sit inside it. |
| 2026-07-29 | **Track G-A shipped whole** — G1 `1c1bfff`, G5 `6b55985`, G3 `6acc97c`, G2 `067b473`, G4 `03beef8`, G6 `1bfe48b`, plus **F8** `ef0ef31` found along the way. Every row carries an executable harness and a headless-Chrome parse; none has been seen running, so §5 grew. Three unreported training-fidelity bugs surfaced during the work (mil-card sizes disagreeing with the world by up to 308 m of taught range error; both declination mistakes landing inside the OT-direction coach's blind spot; the reticle overrunning the screen at 14X), which is the argument for taking user-feedback rows before spec rows — building next to real complaints finds the things nobody thought to report. |
| 2026-07-29 | **Track G added** from [user_feedback.md](user_feedback.md) — 21 rows of feedback from the user actually flying the build, triaged into UI (G1–G6), doctrine (G7–G19) and structural (G20–G21). Inserted ahead of the rest of stage 13. Three rows need doctrine research before code (G10, G13, G18) and one is a decision, not a task (G20). Two supersede existing authority: **G12** overrides CLAUDE.md's "automatic mission fail" wording — fratricide *fails* the mission but must not *end* it — and **G14** supersedes the narrower row 12g. |
| 2026-07-29 | Reviewed the three newer `Dialogue History/` transcripts (08-48, 12-56, 18-05). Added **F5** (STT/typo tolerance in adjust corrections), **F6** ("danger clothes" fuzzy-match), **F7** (readback duplicates DANGER CLOSE) to Track F, all found by reading real play and confirmed against the regexes. Annotated **12g** with live transcript evidence of a player hitting the immediate-suppression gap. Detail in [DIALOGUE_REVISIONS.md §9](DIALOGUE_REVISIONS.md). |
| 2026-07-29 | **F5** `1e39359` and **F6`/`F7** `21d0dc1` shipped, closing all three transcript-evidence STT bugs. F5: a stray-number check now catches a silently half-parsed adjust correction (a mangled "left/right" or "add/drop" word) instead of letting it drop without notice. F6: a narrow fuzzy gate accepts "danger clothes" as the "danger close" proword it was misheard from, instead of rebuking the player for a call they actually made. F7: the readback no longer states DANGER CLOSE twice when the observer's own phrasing and the target description both contain it. |
| 2026-07-29 | **G21** `b17acbc` (DEM audit — recorded targets were the one island-change survivor, wiped), **12-audit** closed (tInit/aimErr0 present under different names), **F3** `e825c0b` ([H] cheat sheet, content updated to today's doctrine), **13d** `dd3a37e` (baked hillshade+AO; closed a boot TDZ trap in 13c's re-bake hook). Two tools now guard the codebase: `tools/syntaxgate.ps1` (parse gate, was scratchpad-only) and `tools/parser-order.js` `f59bb0d` (asserts the 22-branch parseMessage precedence — several branches are only correct because of where they sit). |
| 2026-07-29 | **G12 shipped** `27b42cd` — fratricide and collateral damage now FAIL the mission without ENDING it, per the user's explicit correction. The old cut-to-AAR-in-16-seconds quietly relieved the observer of the worst part of the consequence: the target still shooting at the people he just hit, and a mission still his to finish. All downstream machinery (gradeMission, AAR verdicts, EOM quip gate) already keyed on failReason, so the change confined to resolveImpact. CLAUDE.md's domain-facts bullets now state **fail ≠ end** so the auto-AAR is never reintroduced. |
| 2026-07-29 | **G7/G8/G9/G11 shipped** — the OT cluster and the MTO readback, closing the last un-researched rows in G-B. **G8 reverses a forgiving-mode behaviour on the user's explicit correction**: the old coach-once-then-execute-anyway was wrong physics, not leniency — the battery cannot orient a correction without the observer-target line, so letting it through anyway was the trainer doing impossible math on the observer's behalf; the fix is a refusal that repeats until DIRECTION arrives, on every mode. **G9's reported-position resolution** — the mission fires off the observer's *reported* position, not the true one — makes self-location by resection gradeable by the fall of shot rather than a number nobody checks. G7 was an audit first: the OT factor was already observer-side everywhere, so the fix was teaching an observer who transmits it anyway rather than rewriting anything. G11 makes the target number non-negotiable in the MTO readback even on an otherwise perfect gist recitation. DOCTRINE.md updated with the polar-POS-REP and OT-direction-prerequisite notes; **next is G12**. |
| 2026-07-29 | **JFIRE research landed for G10/G13/G15/G16/G18** — new DOCTRINE.md section "Pre-mission and effects data," sourced primarily from FM 6-30 (1991), the current ATP 3-09.30 Observed Fires (2017), FM 7-90 (mortars), and JFIRE App. F (2007 predecessor edition). Headlines: G10 has no land-doctrine name at all (only NGF's FIRE UNIT STATUS is sourced); G13's 10%/30% figures hold up and nothing bars continue-after-suppression; G18's RED tables are sourced but true lethal-area figures are classified, leaving 155mm effect-radius ⚠ thin. G10, G13, G18 flipped to **RESEARCHED — build ready** (not DONE — no code changed). G15/G16 were not re-gated (already READY) but the same section carries their sourced sheaf/fuze findings for whoever builds them next. |
| 2026-07-30 | **Two dev tools landed** `15fa22d`: `tools/lint.js` concatenates the `src/` modules in manifest order and runs a correctness-only ESLint rule set over the blob, mapping findings back to `module:line` (first pass: 0 errors); `tools/replay.js` boots the artifact headless and re-classifies every recorded observer transmission from the `Dialogue History` TLOG exports through `parseMessage`, diffing `ptype`/`method`/`warno`/`ffe` now-vs-then (first run over 247 recorded transmissions: 14 moves, all `unknown` → correct, zero regressions). `parseMessage` added to the `window.SHITFIRE` QA hooks. Same day, **G13 shipped** `e3a35d6` — graded DESTROYED/NEUTRALIZED/SUPPRESSED outcomes with continue-after-suppression replace the binary `effectRadius`/`hitsToNeutralize` model; see the G13 row above for the full shape. **Board advances to G18** (per-asset radii), then G15/G16, then G10. |
| 2026-07-30 | **G18 shipped** `13084ef` — per-asset effect bands: a 60mm round no longer scores on the 155's numbers. `CONFIG.EFFECTS` carries a band set per firing asset, selected in `effBands()` by the chapter's asset; 155/arty keeps the G13 figures unchanged (personnel 30/50/75 m, 8%/round). 60mm personnel bands are FM 7-90 App. B verbatim (full ≤20 m / half 35 m / little beyond 50 m, 2%/round), tuned so the manual's own check case reproduces exactly: a 10-round section volley on a standing platoon assesses 20% (neutralized), the same volley prone assesses 8% (suppression only). 60mm point-target bands 10/18/35 m at 25%/round — four direct hits to destroy a bunker. Chapter 4.2 (TEN METERS) dropped its legacy `scn:{effR:30}` knob, since the mortar band set is the precision constraint that scaled radius was faking; `effScale` survives for future chapters. Harness extended to 25/25 against the shipped artifact; 15/15 live end-to-end unchanged on the arty path; boots clean offline. **Board advances to G15/G16** (sheaf, then fuze), then G10. |
| 2026-07-30 | **G15 shipped** `8b406e7` — sheaf becomes a real method-of-engagement element instead of accepted-and-ignored text (closing G28's sheaf half). Requested by name in any transmission of the call (survives CFFQ concatenation), inferred otherwise (linear for convoy/column, converged for hard point, open for personnel, doctrine default circular 100 m), a bare mid-mission "CONVERGED SHEAF, OVER" updates it, and "CANCEL ... SHEAF" reverts to the FDC's own inferred choice per ATP 3-09.30 §5-30. PARALLEL/SPECIAL fire as linear (special without attitude gets a format note, not a refusal). Effect scales FFE rounds only: converged ×1.25 on point targets / ×0.6 on dispersed men; open ×1.2 on dispersed personnel / ×0.6 on a point target; on a convoy a LINEAR sheaf is real aimpoint geometry — the volley spreads 35 m apart along the column axis, each round still `impact = aimpoint + error`, so this is the one place sheaf changes *where* rounds land rather than just their effect weighting. Choice, source (requested/inferred/default), and reason are logged at fire time, surfaced in the AAR, and recorded in TLOG. Verified 37/37 numeric harness + 15/15 live scripted e2e + replay over the recorded transcript library unchanged. |
| 2026-07-30 | **G16 shipped** `5d3fbe3` — fuze becomes a real, effect-changing element, closing G28's fuze half and the last open G19-audit finding. Requested in the call ("SHELL HE, FUZE VT") or bare mid-adjustment ("FUZE TIME, OVER" — legal before FFE per ATP 3-09.30 §5-55); inferred DELAY for hard/overhead-cover targets, VT for troops under cover, PD by default. VICTOR TANGO/VARIABLE TIME/QUICK/POINT DETONATING all normalize to the canonical codes, and the MTO now announces the actual fuze chosen instead of a hardcoded FUZE PD. Effect scales every round, not just FFE: VT ×1.5 vs. prone (defeats going flat) and ×0.35 vs. a bunker roof; DELAY ×1.3 on point targets, ×0.45 vs. men in the open; stacks multiplicatively with G15's sheaf scaling and target posture. Doctrine safety carried through: VT/time is never fired DANGER CLOSE — the FDC overrides to PD and states the override, both at call time and mid-mission. G23's height-of-burst hook (parsed-but-inert since that row) is now live under VT/time — an UP/DOWN correction is acknowledged and moves the burst height; under PD the old refusal still stands, since fuze quick has no HOB to adjust. Verified 48/48 numeric harness + 15/15 live scripted e2e + replay unchanged. **Board advances to G10** (FIRE UNIT STATUS exchange, borrowed NGF shape per the existing research). |
