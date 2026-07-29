# SHITFIRE

**A single-file browser trainer for the FORWARD OBSERVER.** You sit on an OP over a low-poly WW2 Pacific island, locate a target with binos, laser, map, and compass, key the net, and talk a (rule-based, deeply unimpressed) fire direction center onto it: *observe → locate → transmit → observe impact → correct → fire for effect → assess*. The gun crew is a black box. You are the eyes.

- **Observer:** MUSTANG 12 (you) · **FDC:** HELLHOUND FIRES (sardonic, doctrinally flawless)
- One `index.html`. No build step, no server, no npm, no API keys. Three.js via CDN import map only.
- Voice via the browser Web Speech API (push-to-talk + spoken FDC); **typed input always works without it**.
- Printable 1:50,000 topo sheets generated from the same heightfield as the 3D world — print one and do real mapwork alongside the sim.
- OP watchtower (+50 ft) raises the observer's eye height, feeding every line-of-sight check. Every mission logs to a **TLOG** transcript (comms, parse classification, impacts, AAR outcome), exportable as text/JSON from the mission menu, for reviewing or correcting FDC dialogue.

## Run it

Open `index.html` in **Chrome or Edge (desktop)**. That's it. Voice needs mic permission; print/save uses the browser print dialog. Gamepad and touch are supported.

## Controls

| Key | Action |
|---|---|
| Mouse / right stick | Look (heading readout in mils) |
| `Space` (hold) / RB | Push-to-talk transmit |
| `B` | Binoculars (mil reticle) |
| `L` | Laser rangefinder (in binos) |
| `M` | Map · `P` map library / print |
| `K` | Mission menu · `N` random mission |
| Typed input box | Full fallback for every call |

## Current state

Build stages 1–10 of [SPEC.md](SPEC.md) are complete: core typed loop, voice layer, all three location methods (grid / polar / shift-from-known-point), FDC personality + radio audio, the five scenario templates with difficulties and danger close, the printable map library, real-DEM ingestion with adaptive quality and mobile/gamepad controls, world detail & population (structures, roads, villages, convoy pit stops, FDC rant/snide deviation policy), the campaign skeleton (volumes/chapters/stars, bookshelf menu, Skirmish mode), and the narrative layer (Foreword tutorial with GUNNY BOTTLECAP, skippable chapter briefings + narrative AAR outros, per-volume islands, Volume IV strict-net + 60mm mortars, named known points, AAR commendations, career-counter continuity quips). Two extras shipped alongside spec: an OP watchtower and a TLOG session transcript (see above). **Stage 11 (Epilogue)** is next, followed by **stage 12**, which addresses training-fidelity gaps a session-transcript review found — the sim grades outcomes but doesn't yet teach target location or adjustment doctrine — see the backlog below.

## Scope: surface-to-surface only (for now)

The trainer covers **surface-to-surface fires** — 155mm artillery (6-digit / 100 m grids) and, in later campaign chapters, 60mm mortars (8-digit / 10 m grids). **Close air support (CAS / 9-line) is planned for a future volume** and is deliberately out of scope until the surface-to-surface campaign is finished. It appears in the mission menu only as a locked tease.

Islands are seeded with permanent structures, roads, and villages (see the World detail item below) so terrain association is a real, teachable skill — the FO can resect and orient off known landmarks like the airfield or the radio mast, plotted on the printed map and the `[M]` map with symbols and a legend. Enemy positions are never plotted. Civilian villages are populated and rendered distinctly from military elements; hitting civilians (**collateral damage**) is an automatic mission fail, exactly like fratricide.

## The Campaign — Volumes, Chapters, Stars

The next phase reworks the flat scenario list into a **story-driven campaign** structured like a book series — see [NARRATIVE.md](NARRATIVE.md) for the full storyline. In brief:

- **Foreword — THE SCHOOLHOUSE**: the tutorial. Guided chapters that teach mils, the map, and your first complete call for fire.
- **Volume I — GREEN AS GRASS**: grid-mission fundamentals on your first island.
- **Volume II — THE RIDGE LINE**: polar and shift-from-known-point, terrain masking.
- **Volume III — THUNDER RUN**: moving targets, danger close, friendlies on the field.
- **Volume IV — BLACK SAND**: mastery — strict doctrine, 60mm precision missions, the final exam.
- **Epilogue — SUNBURN**: the goofy stuff, capped by a call for fire to an intergalactic directed-energy space cannon of mass destruction.
- **Volume V — ON WINGS** *(future)*: CAS. Locked. Coming eventually.

Each chapter is graded **0–5 stars, original-MW2 style**, displayed right on the mission-select menu. Stars come from competency (rounds-to-effect, first-round accuracy, format correctness, time) and are capped by difficulty — Easy caps at 3★, Normal at 4★, only Hard can earn 5★. Cumulative stars unlock the next volume. Fratricide is zero stars and an automatic fail, forever and always.

## Suggestions / design backlog

Everything below is drawn from the existing spec, code, and project files. Items marked **(shipped)** landed in the stage noted; items marked **(planned)** are folded into remaining build-order stages; the rest are candidates.

**World detail & population (shipped — stage 8)**
1. **Structures, roads & landmarks** — seeded, terrain-aware placement of military (ammo depot, fuel point/gas station, airfield strip, radio mast, coastal-gun emplacement, watchtower) and civilian (huts, sheds, a dock) structures plus a road/dirt-path network, all low-poly/flat-shaded/vertex-colored and instanced or merged so the 60 fps rule holds.
2. **Map symbols + legend for terrain association** — permanent structures and roads drawn on the printable map sheets and the `[M]` map with a small symbol set and legend, so the FO can resect/orient off known landmarks. Enemy positions still never plotted.
3. **Civilian villages** — hut clusters with wandering civilian figures, rendered distinctly from military/enemy models. Civilian casualties (collateral damage) are an automatic mission fail, same as fratricide.
4. **Convoy pit stops** — moving convoys make seeded 1–3 minute stops at gas stations, ammo depots, or airfields along their route; teaches catching a column halted versus leading it on the move.
5. **FDC deviation policy (rant vs. snide)** — the doctrinal scripts are guidelines, not gates: dangerous deviations (danger close w/o proword, corrections walking toward friendlies/civilians, unresolved-unsafe FFE) trigger the FDC's rant system; stupid-but-safe deviations (wrong element order, malformed-but-unambiguous calls, absurd rounding) get a snide remark and the mission proceeds. Strict mode still enforces format for grading.

**Campaign & progression (shipped — stages 9–10)**
6. **5-star grading engine** — derive stars from the metrics the AAR already tracks (adjusting rounds, first-round miss, format grade, time vs. par); difficulty caps (3/4/5★) plus star-gated volume unlocks, MW2 Spec-Ops style.
7. **Volume/chapter mission menu** — replace the flat scenario grid with a book-shelf menu: volumes as tabs/spines, chapters listed with title, blurb, best stars per difficulty, and lock state. Keep `[N]` random missions as a separate **Skirmish** mode.
8. **Fixed per-chapter seeds** — every chapter is a reproducible seeded mission (the seeded PRNG already guarantees this), so star runs are comparable and shareable.
9. **Campaign persistence** — extend the existing best-score store to stars + unlock state (`localStorage`, in-memory fallback per the artifact rule).
10. **Foreword tutorial chapters** — scripted, hint-overlay-guided missions using the existing Easy-mode hint plumbing: one on optics/mils, one on map/grid plotting (pairs with a printed sheet), one full walked-through call for fire.
11. **Radio-drama briefings** — each chapter opens with a short text brief (skippable, also read by `SpeechSynthesis`) that carries the storyline; AAR gets a one-line narrative outro.
12. **Strict mode = Veteran** — Volume IV forces the existing strict-doctrine toggle on, making it the "Veteran" difficulty tier rather than a buried option.
13. **One island per volume** — lean on stage-7 DEM ingestion: each volume moves to a new island (procedural stand-in → Peleliu-like ridges → Saipan-like → Iwo-like black sand), so terrain difficulty escalates with the story.

**Fires content (shipped — stage 10)**
14. **60mm mortar chapters** — the spec already defines 8-digit/10 m precision for mortars; Volume IV introduces them as a second surface-to-surface asset, teaching precision grids on a tighter dispersion model.
15. **Named registration points with story flavor** — known points for shift missions get campaign names (KP BREWERY, KP LATRINE) that the narrative references.
16. **Commendations** — funny AAR medals for feats: one-adjustment neutralization, sub-50 m first round, full-format call in strict mode ("ACTUALLY READ THE MANUAL").

**Humor (shipped — stage 10) & Epilogue (planned — stage 11)**
17. **Sprinkled mission humor** *(shipped)* — chapter-specific FDC quip pools, continuity call-outs to career counters (fratricides/collateral/missions), and AAR commendations (GODDAMN HOLE IN ONE, EAGLE EYE, ACTUALLY READ THE MANUAL, FAST MOVER, IRON NET, TEN-METER MAN). Never at the cost of the doctrinal readback.
18. **Epilogue: directed-energy call for fire** — orbital weapon **SUNLAMP** (an intergalactic space cannon satellite of mass destruction). Same doctrine, same direct-impact model — `impact = aimpoint + error` — but TOF becomes a charging whine, SPLASH becomes "SOLAR EVENT," the beam is a sky-to-ground column, and the FDC has completely run out of patience with the 10-digit grid you think you need.
19. **Epilogue side chapters** — a fire mission on the seagull flock raiding the general's barbecue (no-fire line: the cooks), and a B-movie kaiju crab assaulting the beach. Full doctrinal traffic throughout, which is the joke.

**Future / unscheduled**
20. **CAS Volume V** — 9-line brief, talk-on, attack geometry; a genuinely different observer skill set. Locked menu entry now, built only when surface-to-surface is done.
21. **Smoke and illumination missions** — still surface-to-surface; strong training value (marking, screening, night shoots) and the impact-effect hooks already exist in the spec.
22. **Printable campaign log** — a training-record sheet (chapters, stars, dates) rendered through the same print pipeline as the map library.
23. **Night/dawn chapters** — cheap palette + fog changes for atmosphere; pairs with illumination if #21 lands.
24. **Multi-phase Meat Grinder chaining** — 4.4 THE MEAT GRINDER currently ships as a single hard mission (strict + hard + assault/danger close) rather than a chained grid → polar → shift → danger-close → moving-element sequence. Revisit as a true multi-phase mission once the single-mission version has been played and balanced.
25. **Suppress / immediate suppression mission types** — DOCTRINE.md documents both (planned target by number + duration; friendlies-under-fire single-transmission call) as CFF mission types, but the parser currently only recognizes "adjust fire" and "fire for effect." Add parser + FDC handling, or move them to DOCTRINE.md's "what the sim deliberately ignores" list if they're staying out of scope.
26. **In-sim FO cheat sheet** — [CHEATSHEET.md](CHEATSHEET.md) is written and ready; wire it up as an in-sim pull-up reference card (suggested key `[H]`, toggled like the map/library overlays) so a player can glance at CFF format/rounding/prowords mid-mission without alt-tabbing out.

**FO skill depth (planned — stage 12)**

A review of a real player session transcript (chapter 1.2 KNOCK KNOCK, Normal) found the sim grades outcomes but doesn't teach the two core FO skills: the player's corrections crept the miss down ~25%/round for seven rounds instead of bracketing toward doctrine's ~50%/round, three rounds in a row accomplished nothing, and the FDC snarked seven times without ever coaching. These seven items close that gap:
27. **In-mission adjustment coaching** — detect timid corrections (correction far smaller than the observed miss), failure to bracket (every round on the same side of the OT line), and stagnant rounds (miss barely changes) from data already recorded per round; have the FDC deliver live doctrinal nudges on Easy/Normal (off on Hard), plus an AAR round-by-round miss trace, a correction-efficiency figure, and a plain-language diagnosis.
28. **Target location under-support** — add mil-relation range estimation (known object size ÷ mils subtended = range) as a laser alternative, a degraded-optics/dead-laser drill that forces its use, and report the AAR's initial-location error as a vector (direction + distance) instead of a bare scalar so systematic bias is visible.
29. **Mil reticle ↔ correction workflow** — wire the reticle to an OT-factor workflow (OT distance ÷ 1000 = meters per mil; measure burst deviation in mils, multiply, send that as the correction); Easy mode's spotting hint should surface the measured mils, not the finished correction, so it doesn't short-circuit the skill.
30. **Consequence for slow fire** — targets currently wait through an unbounded engagement; have them scatter or displace after repeated near misses, giving the ≤4-adjusting-round standard a cost beyond star loss.
31. **Doctrinal flow gaps vs DOCTRINE.md** — OT direction is never required before/with the first correction on grid missions outside strict mode; RREMS refinement is absent at end of mission; there's no "at my command" fire control, no immediate suppression mission type, and no smoke or illumination missions.
32. **Assessment fidelity** — `gradeMission()` doesn't yet measure time-to-initiate (JFO standard: CFF within 2 minutes of target ID) or correction efficiency (each correction should roughly halve the miss).
33. **Static environment** — permanent noon, no wind (so no smoke drift to read), no night/illumination conditions, no visibility variation; all observation is currently easier than it should be.

## Files

| File | Purpose |
|---|---|
| [index.html](index.html) | The entire trainer |
| [SPEC.md](SPEC.md) | Full build spec + BUILD ORDER (the authority) |
| [NARRATIVE.md](NARRATIVE.md) | Campaign storyline: volumes, chapters, characters, humor rules |
| [DOCTRINE.md](DOCTRINE.md) | CFF formats, prowords, protocols — distilled from JFIRE 2019 + the JFO Student Handout |
| [CHEATSHEET.md](CHEATSHEET.md) | Condensed FO pull-up reference card — planned in-sim overlay (backlog #26) |
| [TLO.md](TLO.md) | Terminal/Enabling Learning Objectives and star-grading crosswalk for the campaign |
| [CLAUDE.md](CLAUDE.md) | Project rules for the coding agent |
| [QUICKSTART.md](QUICKSTART.md) | Stage-by-stage build workflow |
