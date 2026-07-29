# PORT_PLAN.md — Staged Migration

Same discipline as the browser build's BUILD ORDER: **ship stage N working and committed before starting stage N+1.** Stages are numbered `P0`–`P8` so they never collide with the sim's own stage numbers.

Each stage lists its work, its **exit criteria** (objective, checkable — not "feels done"), and what it deliberately leaves undone.

---

## P0 — Freeze the oracle *(do this before writing any engine code)*

Nothing is ported yet. The goal is to capture what "correct" means while the browser build is the only implementation and is known-good.

- Record a **transcript library** of 20–30 complete missions via the mission menu's EXPORT JSON: every scenario type, both fail modes (fratricide, collateral), a strict-net mission, the 60mm chapter, the dead-laser chapter, all three tutorials, and at least one 5★ run and one disastrous run.
- Export **fixtures**: PRNG streams for all campaign seeds, `H(x,z)` grids (procedural and DEM), ray-march/LOS samples, `spotBurst` cases, per-chapter scenario dumps. Store in `ENGINE_PORT/fixtures/`.
- Tag the browser build in git as the parity baseline (e.g. `oracle-v1`).
- Write down every tuned constant that has no derivation — par times, star thresholds, effect radii, dispersion ranges. These are balance decisions, not physics; losing them loses the campaign.

**Exit criteria:** fixtures and transcripts committed; the baseline tagged; a one-page note listing any behaviour discovered to be non-deterministic (there should be none in the sim path).

---

## P1 — Core skeleton and the PRNG proof

- Create `SHITFIRE.Core` (plain C# class library, zero engine references) and `SHITFIRE.Core.Tests`.
- Port, in this order: `Prng` → `Noise` → `GridMath` → `MilRelation` → `IHeightField` + `ProceduralIsland` + `DemHeightField` → `RayMarch` / `LineOfSight`.
- Wire CI to run tests headlessly.

**Exit criteria:** Level 1 parity tests from [PARITY_TESTING.md](PARITY_TESTING.md) all green, including bit-identical PRNG streams. **Do not proceed past a failing PRNG test** — everything downstream inherits the error.

**Not yet:** no engine project exists. No rendering. This stage produces a library and a test report, nothing visible.

---

## P2 — Sim core: ballistics, missions, doctrine

The heart of the port. Still headless.

- `Ballistics` (first-round error, follow-up elliptical dispersion, asset scaling, TOF), `Mission` lifecycle, `Corrections` (OT-frame → world), `ImpactResolver` (terrain resolution, effect assessment, fratricide/collateral), `TargetBehavior` (alert/disperse).
- `CallParser` (number words, grid/polar/shift, named KPs, corrections, prowords, RREMS) and `FdcRules` (readback, MTO, danger-close gate, strict net, deviation tiers, unsafe-correction refusal, adjustment coaching).
- `ScenarioGenerator` for all seven types plus world-feature layout.
- `Grading`, `TranscriptLog`, `AarAnalysis`.

**Exit criteria:** Level 2 (scenario generation) and Level 3 (full mission replay) parity green across the entire P0 transcript library. At this point the port is *correct* before it is *visible* — which is the whole strategy.

**Not yet:** no UI, no 3D, no audio, no speech.

---

## P3 — Engine shell: world and observer

First engine code. Getting a recognisable OP view.

- Engine project, terrain mesh built from `IHeightField`, ocean, lighting/fog, low-poly palette (including the Volume IV black-sand variant).
- OP watchtower placement and the elevated first-person camera; mouse/gamepad look with mils heading readout.
- Binoculars (FOV switch + mil reticle), laser rangefinder with OT-factor readout, mil-relation card.
- World features and units rendered (instanced where sensible).
- `SimHost` bridge: engine drives Core with an injected delta; Core emits events.

**Exit criteria:** stand on the tower, look around, glass a target, lase it, and read a grid that matches what Core computes. Frame budget holds 60 fps on the target machine.

**Not yet:** no call for fire — you can observe but not shoot.

---

## P4 — The loop closes: fire missions end to end

- Typed input first (the browser build's own lesson: typed must work standalone).
- Comms log, status strip, SHOT/SPLASH timing, burst effects, distance-delayed audio.
- Adjustment, fire for effect, end of mission, AAR panel with metrics and diagnosis.

**Exit criteria:** a full mission — observe → locate → transmit → observe impact → correct → FFE → assess — playable in the engine, with impacts matching the browser for the same seed and inputs.

**Not yet:** no campaign, no maps, no voice.

---

## P5 — Cartography

The heaviest UI stage; scheduled deliberately rather than discovered late.

- Screen map `[M]` with world features, KPs, OP, battery.
- Printable sheet: contours via marching squares over `H`, grid and marginal data, scale bar, declination diagram, legend, answer-key toggle, quadrant sheets.
- **Print/PDF export path** — the browser's free print dialog has no engine equivalent (see ENGINE_CHOICE risks).

**Exit criteria:** a printed sheet correlates to the 3D world — a grid read off paper plots to the same point the in-engine rangefinder reports. This is ELO F.2 in [reference/TLO.md](reference/TLO.md) and is graded, so it is not optional.

---

## P6 — Campaign, briefings, tutorials

- Campaign data as an engine resource; bookshelf menu with volumes, chapters, stars, locks; skirmish mode; persistence; dev-unlock toggle.
- Chapter briefings and AAR outros; the Foreword tutorial step engine; commendations; career continuity.
- AAR shot plot (bird's-eye render with SENT / numbered adjust / FFE markers).

**Exit criteria:** every implemented chapter (F.1 → 4.4) playable and grading identically to the browser for the same seed, difficulty, and inputs.

---

## P7 — Speech

Deliberately late: the browser build proved the typed core must stand alone, and speech is the least portable subsystem.

- STT adapter behind an interface (recommended: offline Vosk with a constrained doctrinal grammar — numbers, directions, ~40 prowords).
- FDC voice: platform TTS, a bundled engine, or **recorded voice lines** with spliced digits. Recorded is the quality play; the quip pools are already discrete authored lines.
- Radio filtering, squelch, hiss.

**Exit criteria:** push-to-talk drives a full mission; **disabling speech entirely leaves the trainer fully usable.** If that is not true, the dependency has become load-bearing and must be broken.

---

## P8 — Parity sign-off, polish, and the features that were waiting

- Full regression run of the transcript library against the finished build.
- Adaptive quality, accessibility pass, packaging and distribution.
- Only now: the deferred content — Epilogue (E.1–E.3), then whatever comes next.

**Exit criteria:** the standard from PARITY_TESTING — every recorded browser mission replays to the same impacts, doctrinal decisions, and star grade.

---

## Sequencing rationale

Two choices drive everything else:

**Correctness before visibility (P1–P2 headless).** It is tempting to get a camera on a hill in week one because it feels like progress. Resist it. Debugging ballistics through a rendering layer is dramatically harder than debugging it in a test runner, and every hour spent on visuals before the core is proven is an hour spent on a foundation that may still shift.

**Speech last (P7).** It is the only subsystem with no engine-native answer, the most platform-specific, and the least load-bearing. Putting it early risks the port's schedule on the least essential feature.

## Keeping the browser build alive

Do not delete or freeze `index.html` when the port ships. It remains valuable as:

- **The parity oracle** — the arbiter when the port's behaviour is questioned.
- **The zero-install distribution channel** — one file, any machine with a browser, no permission from IT. That property is genuinely hard to replicate with an engine build, and for a training tool it may matter more than the graphics.

Plan to maintain both for at least one release cycle. Where they diverge, the doctrine and campaign documents in [reference/](reference/) — not either implementation — are the source of truth.

## Effort shape (not a schedule)

Relative weight, for planning conversations only — no calendar estimate is given because it depends entirely on who is doing the work:

| Stage | Relative effort | Risk |
|---|---|---|
| P0 freeze oracle | Small | Low — but skipping it is the single biggest risk in the whole plan |
| P1 core skeleton | Small | Low (PRNG parity is fiddly but bounded) |
| P2 sim core | **Large** | Medium — the volume is here, but it is well-specified and testable |
| P3 engine shell | Medium | Low |
| P4 loop closes | Medium | Low |
| P5 cartography | **Large** | **High** — print/PDF has no free answer |
| P6 campaign | Medium | Low — mostly data |
| P7 speech | Medium | **High** — platform-dependent, quality-sensitive |
| P8 sign-off | Small | Low |
