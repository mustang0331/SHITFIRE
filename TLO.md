# TLO.md — Terminal Learning Objective / Training Objectives Crosswalk

## 1. Purpose and Scope

This document states the Terminal Learning Objective (TLO) and supporting Enabling Learning Objectives (ELOs) for the SHITFIRE surface-to-surface call-for-fire curriculum, and crosswalks them against the campaign's chapter structure and 0–5 star assessment model. It is written in the task–conditions–standard format used in service schoolhouse programs of instruction, for anyone extending the campaign (new chapters, new volumes, rebalanced pars) who needs to know *what a chapter is supposed to teach and how "good enough" is measured*, not just what it renders.

**Audience:** the coding agent and any human maintainer adding or revising campaign content; secondarily, a player who wants to know why a chapter exists.

**References:**
- ATP 3-09.32 / MCRP 3-31.6, *JFIRE* (18 Oct 2019) — call-for-fire formats, prowords, danger-close and rounding standards.
- Joint Fires Observer (JFO) Student Handout, JFO 0103 (27 Mar 2018) — evaluation standards (time-to-CFF, location accuracy, adjusting-round budget).
- `DOCTRINE.md` — this repository's in-repo distillation of the above two publications; the authority for parser/FDC behavior and strict mode.
- `NARRATIVE.md` — campaign volume/chapter structure and story framing.
- `SPEC.md` — mechanics, `gradeMission()` contract, and BUILD ORDER.

**This is a training aid built for a browser simulator, not an official publication.** Standards below are drawn from JFIRE/JFO for realism and are adapted to a single-observer, rule-based-FDC, direct-impact ballistics model (`impact = aimpoint + error`, no trajectory, no angle-T). Where the sim deliberately simplifies real doctrine (see `DOCTRINE.md` §"What the sim deliberately ignores"), the ELOs below follow the sim's rules, not the full real-world program of instruction.

## 2. Course Terminal Learning Objective

**Task.** Employ surface-to-surface indirect fires in the forward observer role by initiating and conducting a doctrinally correct call for fire against an assigned target, from initial observation through end-of-mission surveillance.

**Conditions.** Given an elevated observation post (OP watchtower), binoculars with a mil reticle, a laser rangefinder, a lensatic-pattern compass reading in mils, a map sheet correlated to the observed terrain (1:50,000, permanent structures and roads plotted), and a radio net (voice via Web Speech API or typed fallback) to a rule-based fire direction center; daylight, unobstructed visibility, target validated as hostile.

**Standard.** IAW JFO evaluation standards and `DOCTRINE.md`:
- Initiate a complete, correctly formatted call for fire within **2 minutes** of target identification.
- Report an initial target location within **200 m** of the true location.
- Adjust fire onto the target and achieve fire for effect within **4 adjusting rounds**.
- Close the mission with a correctly sequenced RREMS end-of-mission transmission carrying an accurate surveillance term (suppressed / neutralized / destroyed).
- **Zero fratricide and zero civilian collateral damage** — either is an automatic mission failure regardless of all other performance.

## 3. Enabling Learning Objectives by Volume

### FOREWORD — The Schoolhouse (tutorial, guided, non-punitive)

**F.1 — Observation Instruments.** *Task:* acquire and range three marked objects using binoculars and laser rangefinder. *Conditions:* guided OP, hint overlays on, GUNNY BOTTLECAP narration, no time or fail pressure. *Standard:* correctly read heading in mils off the reticle and obtain a laser range for all three objects.

**F.2 — Map Reading and Terrain Association.** *Task:* plot a named landmark (airfield, radio mast, village) from the `[M]` map/print sheet and resect the observer's position against permanent structures and roads. *Conditions:* correlated 1:50,000 sheet with symbol legend, guided overlays. *Standard:* plot the named landmark within 200 m of true position; read the grid right-then-up.

**F.3 — Complete Call-for-Fire Sequence.** *Task:* execute an entire mission end to end — warning order, target location, description, adjustment, and RREMS — against a fixed, stationary target. *Conditions:* beached derelict landing craft, guided, first exposure to FDC readback/SHOT/SPLASH/ROUNDS COMPLETE. *Standard:* complete all six CFF elements across three transmissions with FDC readback acknowledged at each step; reach FFE and close with RREMS.

*Maps to chapters:* F.1, F.2, F.3.

### VOLUME I — Green as Grass (grid fundamentals)

**I.1 — Grid Mission Execution.** *Task:* locate and neutralize troops in the open using a 6-digit grid. *Conditions:* quiet-sector island, 155mm HE, adjust-fire mission type. *Standard:* per Course TLO standard.

**I.2 — Successive Bracketing.** *Task:* neutralize a dug-in point target using the 800/400/200 m bracketing method to FFE. *Conditions:* precise initial grid required. *Standard:* enter FFE by splitting a 100 m bracket; no more than 4 adjusting rounds.

**I.3 — Fire Near a Friendly Perimeter.** *Task:* engage a target near, not on, a defended friendly position with a civilian village beyond it. *Conditions:* position under attack. *Standard:* neutralize the target with zero fratricide and zero collateral damage.

**I.4 — Format Discipline Under Time.** *Task:* re-engage a grid target inside a par time. *Conditions:* timed re-engagement, prior-score callback. *Standard:* complete the mission under chapter par time with clean CFF format.

*Maps to chapters:* 1.1–1.4.

### VOLUME II — The Ridge Line (location methods, terrain masking)

**II.1 — Polar Plot.** *Task:* locate a target by direction and distance from the observer's own position. *Conditions:* ridge-line island. *Standard:* direction reported to nearest 10 mils, distance to nearest 100 m.

**II.2 — Shift from a Known Point.** *Task:* locate a target by shift from a registered, named known point (e.g., KP BREWERY, KP LATRINE). *Conditions:* KPs plotted on map/log. *Standard:* direction (OTL) to nearest 10 mils, lateral shift to nearest 10 m, range shift to nearest 100 m.

**II.3 — Unobserved (Crest-Masked) Adjustment.** *Task:* adjust fire onto a target masked from direct line of sight, using burst sound/map data only. *Conditions:* no LOS from the OP. *Standard:* achieve FFE within the adjusting-round budget despite lacking a direct spot.

**II.4 — Method Selection Under Pressure.** *Task:* choose the correct location method (grid, polar, or shift) for a position-under-attack scenario without being told which to use. *Conditions:* harder variant of I.3. *Standard:* select and execute an appropriate method; using the wrong required method caps chapter performance (see §4).

*Maps to chapters:* 2.1–2.4.

### VOLUME III — Thunder Run (dynamic fires, danger close)

**III.1 — Moving-Target / Time-on-Target Reasoning.** *Task:* engage a convoy either on the move or during a seeded 1–3 minute halt at a fuel point, ammo depot, or airfield. *Conditions:* moving column, uncertain halt timing. *Standard:* achieve required hits (convoy kill threshold) within the mission window.

**III.2 — Danger Close / Creeping Fire.** *Task:* engage a target inside 600 m of friendlies. *Conditions:* danger-close geometry. *Standard:* announce "danger close"; walk corrections in 100 m or less from the safe side toward the target; no fratricide.

**III.3 — Target Discrimination Near Civilians.** *Task:* engage raiders near a civilian village without striking the huts. *Conditions:* raiders 130–200 m off a no-strike village (or masked variant if no LOS). *Standard:* creeping-fire discipline as in III.2; zero collateral damage.

**III.4 — Fires with Maneuvering Friendlies.** *Task:* engage a shifting target while friendly forces are advancing on the same ground. *Conditions:* combined-arms assault. *Standard:* neutralize target, zero fratricide, corrections track the shifting target.

**III.5 — Fratricide-Avoidance Stress.** *Task:* engage interleaved hostile and friendly elements, friendlies starting inside 520 m and closing throughout the mission. *Conditions:* highest fratricide-risk geometry in the campaign. *Standard:* zero fratricide under continuously tightening safety margin.

*Maps to chapters:* 3.1–3.5.

### VOLUME IV — Black Sand (mastery, strict doctrine, 60mm precision)

**IV.1 — Strict Format Compliance.** *Task:* execute any prior mission type under enforced doctrine. *Conditions:* Strict Net (Veteran tier) — three-transmission structure, MTO readback, OT direction with first correction, "CORRECTION" proword. *Standard:* malformed calls are rejected with a doctrinal challenge, not silently accepted; corrections rounded to standard (deviation tens, range fifties/hundreds); RREMS carries a valid surveillance term.

**IV.2 — 60mm Mortar Precision.** *Task:* execute a strict-format mission with an 8-digit/10 m grid asset. *Conditions:* 60mm mortar section, tighter dispersion (0.55×), 30 m effect radius. *Standard:* 8-digit grid required — a 6-digit grid draws a challenge; otherwise Course TLO standard.

**IV.3 — Time-Constrained Precision Engagement.** *Task:* neutralize a hard target with a reduced adjusting-round budget above par. *Conditions:* timed, strict, one round of margin above par. *Standard:* FFE within the tightened round budget under strict format.

**IV.4 — Integrated Exam.** *Task:* execute a strict-format, hard-difficulty, danger-close/assault mission as a capstone assessment across the location and correction skills taught in Volumes I–III. *Conditions:* strict net, hard difficulty. *Standard:* full Course TLO standard under strict enforcement; zero tolerance for fratricide or collateral damage.

*Maps to chapters:* 4.1–4.4.

## 4. Assessment Methodology

Each chapter is graded 0–5★ by `gradeMission()`, which converts mission metrics into a doctrinally-grounded score:

- **0★** — mission failed: target not neutralized, or the mission ended in fratricide/collateral damage (both are treated as an automatic fail, independent of every other metric).
- **1★** (base) — mission accomplished: target neutralized/destroyed per the RREMS surveillance term.
- **+1★** — clean format: zero FDC coaching notes logged against the call (no format deviations flagged during the mission).
- **+1★** — efficient adjustment: FFE reached in 2 or fewer adjusting rounds.
- **+1★** — initial-location accuracy: first reported target location within 200 m of true location — the JFO evaluation standard.
- **+1★** — beats chapter par time: mission completed at or under the chapter's par duration.
- **Difficulty caps:** Easy caps at 3★, Normal at 4★, Hard permits the full 5★.
- **Method-lock penalty:** if a chapter requires a specific location method (grid, polar, or shift) and the observer used a different one, the star total is capped at 2★ regardless of other bonuses earned.

The per-mission **TLOG** (session transcript: radio traffic, parse classification, impacts, AAR outcome) is the instructor-review artifact — it is exportable as text/JSON from the mission menu and is the mechanism by which a maintainer (human or agent) audits *why* a chapter scored the way it did, independent of the star number alone.

## 5. Chapter Crosswalk

| Chapter ID | Title | Primary ELO | Assessment Focus | Pass Evidence |
|---|---|---|---|---|
| F.1 | Eyes Before Guns | F.1 — Observation Instruments | Reticle/mil reading, laser range | 3 objects lased and ranged |
| F.2 | The Map Is Not the Territory (But It's Damn Close) | F.2 — Map Reading & Terrain Association | Grid plot accuracy, resection | Landmark plotted within 200 m |
| F.3 | Say It Like You Mean It | F.3 — Complete CFF Sequence | End-to-end format walkthrough | FFE + RREMS completed, all elements sent |
| 1.1 | Troops in the Open, Brains in the Rear | I.1 — Grid Mission Execution | Basic grid, adjust, FFE | Target neutralized, ≤4 adjusting rounds |
| 1.2 | Knock Knock | I.2 — Successive Bracketing | Bracket-to-FFE method, initial grid precision | FFE entered on 100 m bracket split |
| 1.3 | Hold the Line | I.3 — Fire Near Friendly Perimeter | Fratricide/collateral avoidance near a perimeter+village | Neutralized, zero fratricide/collateral |
| 1.4 | Do It Again, Slower | I.4 — Format Discipline Under Time | Par-time completion | Mission closed under chapter par |
| 2.1 | Numbers on a Compass | II.1 — Polar Plot | Direction/distance rounding | Direction to 10 mils, distance to 100 m |
| 2.2 | Old Friends | II.2 — Shift from Known Point | Named-KP shift accuracy | Lateral to 10 m, range to 100 m |
| 2.3 | Defilade Blues | II.3 — Unobserved Adjustment | Crest-masked fire correction | FFE achieved without direct LOS |
| 2.4 | The Perimeter | II.4 — Method Selection Under Pressure | Correct method choice | Neutralized without method-lock penalty |
| 3.1 | Rolling Stock | III.1 — Moving-Target Reasoning | Convoy timing / halt exploitation | Convoy hit threshold met in window |
| 3.2 | Close Enough to Smell It | III.2 — Danger Close / Creeping Fire | Proword use, ≤100 m corrections | Neutralized, zero fratricide, danger close announced |
| 3.3 | Uninvited Guests | III.3 — Target Discrimination | Creeping fire near no-strike civilians | Neutralized, zero collateral damage |
| 3.4 | Everyone's Moving | III.4 — Fires with Maneuvering Friendlies | Tracking a shifting target amid advance | Neutralized, zero fratricide |
| 3.5 | The Wrong Kind of Famous | III.5 — Fratricide-Avoidance Stress | Sustained safety margin under closing friendlies | Mission complete, zero fratricide throughout |
| 4.1 | Strict Net | IV.1 — Strict Format Compliance | Rejected/challenged malformed calls | Valid RREMS under strict enforcement |
| 4.2 | Ten Meters | IV.2 — 60mm Mortar Precision | 8-digit grid requirement | 8-digit grid accepted, FFE achieved |
| 4.3 | No Second Chances | IV.3 — Time-Constrained Precision | Tightened adjusting-round budget | FFE within reduced round budget |
| 4.4 | The Meat Grinder | IV.4 — Integrated Exam | Composite of prior ELOs under strict+hard | Full Course TLO standard met |

## 6. Future Objectives (not yet assessed / not yet built)

- **Epilogue (E.1–E.3, SUNBURN)** exercises the same doctrine and direct-impact model against deliberately absurd targets (seagulls, a kaiju crab, an orbital directed-energy platform). It is unlocked by finishing Volume IV but is **not graded as a serious TLO** — full doctrinal traffic is played for comedic contrast, not competency measurement. Consider it culture, not curriculum.
- **Volume V — On Wings (future).** A prospective TLO for close air support: employ fixed/rotary-wing CAS via a 9-line brief, talk-on, and attack-geometry deconfliction. Locked menu tease only; do not build ahead of the surface-to-surface campaign per `NARRATIVE.md`.
- **Smoke and illumination missions (future ELOs).** Would extend the Volume II–III ELOs with marking, screening, and night-shoot skills using the existing impact-effect hooks; not yet scoped as chapters.
