# DETERMINISM_NOTE.md — pre-freeze audit of the browser sim's scored path

Audited 2026-08-28 against source state e895666 (HEAD f330fee; the two commits above
e895666 are docs-only — `git diff --stat e895666..HEAD -- src SHITFIRE.html` is empty).
Sweep: every `Math.random`, `Date.now`, `performance.now`, `crypto.getRandomValues`,
`navigator.*` in `src/js/*.js` (18 files), plus the built `SHITFIRE.html` (identical:
same 17 Math.random lines; `tools/build.js` concatenates src parts per manifest).
`performance.now` and `crypto.getRandomValues`: **zero occurrences anywhere**.

## (a) Nondeterminism sites — complete classified table

| Site | Feeds | Class |
|---|---|---|
| 15-campaign-menu.js:569 | Skirmish scenario TYPE draw from SCN_TYPES (only when no explicit type and not first mission; campaign chapters always pass a fixed type). Mission seed itself stays deterministic (`CONFIG.SEED.mission++`, line 567) | **SIM-PATH** — the one unseeded sim-shaping draw; skirmish only |
| 08-burst-optics.js:523 | Illum flare burn time: `mission.rng()` (seeded) when a mission exists; `Math.random()` fallback only with no mission. ILLUM state is read only by the renderer and the `illumInfo` debug export (17-loop-boot.js:266) — nothing scored reads it. The in-mission draw consumes one mission-stream value, so call order matters; replay reproduces it because illum commands are replayed | **SIM-PATH boundary** — deterministic in-mission; fallback unreachable in any scored context |
| 07-voice-spot-audio.js:293 | `pick()` — quip line selection for every QUIPS call | SANCTIONED FLAVOR (named exemption in PARITY_TESTING.md) |
| 07-voice-spot-audio.js:556 | Caller ack line selection | SANCTIONED FLAVOR |
| 07-voice-spot-audio.js:571 | Audio white-noise buffer fill | SANCTIONED FLAVOR |
| 07-voice-spot-audio.js:627 | Squelch noise start offset | SANCTIONED FLAVOR |
| 08-burst-optics.js:292 | NVG/thermal grain texture pixels | SANCTIONED FLAVOR |
| 08-burst-optics.js:346 | Grain frame cycling | SANCTIONED FLAVOR |
| 08-burst-optics.js:416 | Crater decal size jitter | SANCTIONED FLAVOR |
| 08-burst-optics.js:633–634 | Burst debris velocities (3 calls) | SANCTIONED FLAVOR |
| 10-mission.js:824 | 40% gate: ffeAck tail quip | SANCTIONED FLAVOR (gated — see below) |
| 10-mission.js:902 | 50% gate: completeTail quip | SANCTIONED FLAVOR (gated) |
| 12-cff-handling.js:390 | 45% gate: readbackTail quip | SANCTIONED FLAVOR (gated) |
| 12-cff-handling.js:714 | 55% gate: snideRound quip (correction applied at :707 regardless) | SANCTIONED FLAVOR (gated) |
| 12-cff-handling.js:716 | 50% gate: corrSnark quip | SANCTIONED FLAVOR (gated) |
| 15-campaign-menu.js:635 | 40% gate: career-callback quip | SANCTIONED FLAVOR (gated) |
| 06-clock-comms.js:27 | `wall: Date.now()` on TLOG entries — written, never read (repo-wide grep: no `.wall` consumer) | WALL-CLOCK METADATA |
| 15-campaign-menu.js:495, 499 | `new Date()` — export filename stamps | WALL-CLOCK METADATA |
| 16-input.js:24–26, 239–240 | `navigator` mic + gamepad access — input devices, not entropy | Not a nondeterminism source |

**Gated-quip caveat:** PARITY_TESTING.md exempts quip *selection*; the six gates above
additionally make the *presence* of tail/snark lines nondeterministic, and those lines
land in the TLOG. All doctrinal speech categories (readback, danger-close challenge
10:430, snide-for-sloppy-format 12:389, coach) are deterministically gated — verified
each condition. The replay comparator must therefore diff only parse / impact / FDC
decision points / AAR (which Level 3 already specifies), never raw FDC-line lists.

**Sim clock [verified]:** `sim.now = t` (rAF timestamp / 1000) at 17-loop-boot.js:73 is
the only writer; the scheduler (06-clock-comms.js:4–12) is a sorted queue drained
against `sim.now`. Time enters through the single injection point `animate(tMs)` —
drivable synthetically by a harness. `setTimeout` appears only in persistence debounce
and UI (06:41, 06:84, 06:182). Nothing scored reads wall time.

## (b) Watch-item dispositions for oracle capture

1. **Skirmish type draw (15:569):** capture MUST record the drawn scenario type AND
   mission seed in each transcript's metadata. The TLOG does not already cover this:
   only spot-report entries carry `scn: S.type`, AAR entries do not, and some scenario
   types never send a spot report. Read `window.SHITFIRE.Scenario.type` + `.seed` at
   capture time. Campaign chapters are unaffected (fixed type + seed).
2. **Illum outside a mission (08:523 fallback):** never capture. The Math.random branch
   is reachable only with no active mission, where nothing is scored; any capture
   containing it would embed an unreproducible draw.

## (c) PRNG stream inventory — 11 mulberry32 instantiations (exact seed expressions)

`wSeed = (CONFIG.SEED.terrain ^ (DEM ? strHash(DEM.name) : 0)) >>> 0` (04:357, 04:1003)

| # | Site | Seed expression | Drives |
|---|---|---|---|
| 1 | 04-world.js:358 | `wSeed` | Coast road, facilities, villages, civilians (world stream 1) |
| 2 | 04-world.js:364 | `(Math.imul(wSeed, 2246822519) + 13) >>> 0` | E4+ placements, road-connector bows (world stream 2) |
| 3 | 04-world.js:370 | `(Math.imul(wSeed, 3266489917) + 0x9E3779B9) >>> 0` | E7 rocks/boulders (world stream 3) |
| 4 | 04-world.js:929 | `(wSeed ^ 0x517CC1B7) >>> 0` | Wind (smoke-drift observation only; rounds are not wind-affected) |
| 5 | 04-world.js:1004 | `(wSeed ^ 0x9E3779B9) >>> 0` | Vegetation placement (visual; LOS is terrain-only) |
| 6 | 05-units-scenario.js:225 | `(seed * 2654435761 ^ CONFIG.SEED.terrain) >>> 0` | genScenario → `S.rng`: target, KPs, friendlies, convoy, layout. NOTE: mixes mission seed WITH terrain seed |
| 7 | 05-units-scenario.js:1061 | `(S.seed * 68168169 + 17) >>> 0` | placeUnits — unit mesh layout |
| 8 | 07-voice-spot-audio.js:449 | `(S.seed * 30011 + (reason === 'displace' ? 977 : 7)) >>> 0` | Spot-report content (two sub-streams by reason) |
| 9 | 10-mission.js:184 | `(CONFIG.SEED.mission * 1046527 + 7) >>> 0` | `mission.rng`: first-round error, follow-up dispersion (Box-Muller `gauss()` 10:25–30 draws only from this), FFE stagger, illum burn |
| 10 | 10-mission.js:437 | `(CONFIG.SEED.mission * 2246822519 + 13) >>> 0` | FPF marking/fire stream |
| 11 | 10-mission.js:1255 | `(S.seed * 7919 + 13) >>> 0` | Troop dispersal spread (visual layout; the scoring effect — prone posture — is set deterministically) |

(The earlier survey said "10 sites" but listed these same 11 line numbers; all 11
verified, none missed, none extra. Definition at 01-math-terrain.js:2; exported at
17-loop-boot.js:250.)

## (d) Frame check — tools/test-results, 12 frames, 2026-08-28 12:02–12:03 (build e895666)

| Frame | Verdict |
|---|---|
| 01-day-op | OK — terrain/sea/sky, HUD, radio net, help panel all render |
| 02-binos-7x | OK — circular mask, mil reticle, target cluster on horizon, stadia lines |
| 03-binos-14x | OK — magnified target cluster (troops/barrels), trees, reticle legible |
| 04-binos-4x | OK — wide FOV, reticle rescaled, horizon clean |
| 05-milcard | OK — mil-relation card fully legible over binos view |
| 06-map | OK — tactical map complete: island, grid, all labels, legend, no missing tiles |
| 07-cheatsheet | OK — full CFF reference panel legible, scene intact behind |
| 08-dawn | OK — dim scene consistent with TOD DAWN in HUD; not a render failure |
| 09-night-eye | OK — dark ground is intended (naked eye, TOD NIGHT); sky gradient, surf line, faint lights visible; UI crisp |
| 10-night-nvg | OK — green phosphor + grain, terrain readable, EYES·NVG tag correct |
| 11-night-thermal | OK — dimmest frame but coherent: horizon band, terrain silhouette, grain; consistent with a cool night scene, not corruption |
| 12-library-sheet | OK — 1:50,000 sheet renders: contours, marginalia, legend, scale tabs |

No blank, corrupted, misplaced, or garbled frame.

## (e) Verdict

**No unexplained nondeterminism in the scored path — safe to freeze oracle-v1**, on two
conditions already dispositioned above: capture records skirmish type+seed, and no
capture includes illum fired outside a mission.
