# ARCHITECTURE.md — Target Structure and What Actually Transfers

The central claim of this port: **about half of SHITFIRE is engine-agnostic domain logic that should be moved verbatim, and the other half is browser presentation that must be rebuilt.** Everything below follows from separating those two halves cleanly and never letting them mix again.

## The line-by-line inventory

Measured from `index.html` (4,516 lines total, ~3,900 of executable script). Sections are the `/* ===== NAME ===== */` banners in the file.

### Transfers as logic — port the algorithm, not the code style

| Section | Lines | Notes |
|---|---:|---|
| MISSION STATE MACHINE | 316 | **The crown jewel.** Direct-impact ballistics, first-round/follow-up error model, OT-frame corrections, TOF, impact resolution, fratricide/collateral checks, adjustment coaching, target dispersion. Port with the most care and the most tests. |
| SCENARIO | 355 | Seeded generation for all seven scenario types, placement constraints, LOS requirements, convoy pathing and pit stops, friendly/civilian positioning. |
| MESSAGE HANDLING | 311 | FDC rule engine: readback, MTO, danger-close gating, strict-net enforcement, unsafe-correction refusal, deviation tiers, RREMS. |
| CAMPAIGN | 205 | Volume/chapter data, `gradeMission()`, star caps, unlock rules, persistence shape. |
| AAR | 160 | Verdicts, metrics, diagnosis logic (the HTML rendering does not transfer, the analysis does). |
| PARSER | 137 | Number-word normalization, grid/polar/shift extraction, corrections, prowords, named KPs. |
| CONFIG | 123 | Pure data. Becomes a resource/config asset. |
| TERRAIN `H(x,z)` | 72 | Procedural island, DEM sampling, `groundHit` ray-march, `hasLOS`. |
| TRANSCRIPT LOG | 62 | Structured session log — also the parity fixture format. |
| OP + FIRING BATTERY | 102 | Placement scans (pure math over `H`). |
| PRNG + NOISE | 35 | `mulberry32`, value noise, fBm. **Must be bit-identical** — see [PARITY_TESTING.md](PARITY_TESTING.md). |
| GRID HELPERS | 18 | Grid ↔ world, azimuth, mils. |
| CLOCK + SCHEDULER | 14 | Replaced by an engine-independent sim clock; the concept transfers, the code is trivial. |
| **Subtotal** | **~1,910** | |

Plus the placement half of WORLD FEATURES (~150 of 258 lines): the coast-road ring scan, facility siting scores, village and path layout are pure math over `H`; only mesh construction is engine-specific.

### Rebuilds — browser-specific presentation and platform

| Section | Lines | Replaced by |
|---|---:|---|
| MAP LIBRARY `[P]` | 329 | In-engine 2D drawing (marching squares transfers as logic; canvas calls do not) + a print/PDF path |
| WORLD FEATURES (mesh half) | ~110 | Engine meshes / MultiMesh instancing |
| FDC VOICE | 201 | Platform TTS or recorded lines (see ENGINE_CHOICE risks) |
| AAR SHOT PLOT | 197 | Engine viewport render + 2D overlay (projection math transfers) |
| MAP `[M]` | 163 | Engine 2D drawing |
| SCENE | 155 | Engine scene, lights, fog, ocean, terrain mesh build |
| CAMERA CONTROL + TOOLS | 131 | Engine camera, FOV switching, reticle drawing |
| MISSION MENU + BRIEFINGS | 186 | Engine UI |
| INPUT (wiring, gamepad, touch, PTT) | 186 | Engine input map |
| BURST EFFECTS | 94 | Engine particles |
| MAIN LOOP | 85 | Engine frame loop |
| AUDIO | 72 | Engine audio (synthesis or samples) |
| UNITS | 65 | Engine scenes/prefabs |
| DEM INGESTION | 54 | Engine image loading |
| BOOT | 11 | Engine startup |
| **Subtotal** | **~1,940** | |

**Read this as reassurance, not as a warning.** The half that is hard to get right — ballistics, doctrine, grading, seeded generation — is the half that transfers. The half being thrown away is the half an engine gives you better versions of.

## Target structure

```
SHITFIRE.Core/                 ← plain C# class library. NO engine references. Ever.
  Math/        Prng, Noise, GridMath, MilRelation
  Terrain/     IHeightField, ProceduralIsland, DemHeightField, RayMarch, LineOfSight
  Sim/         Ballistics, Mission, Corrections, ImpactResolver, TargetBehavior
  Scenario/    ScenarioGenerator, ScenarioTypes, WorldFeatureLayout
  Doctrine/    CallParser, FdcRules, StrictNet, DeviationTiers, Rrems
  Campaign/    CampaignData, Grading, Unlocks, Progress
  Review/      TranscriptLog, AarAnalysis, AdjustmentDiagnosis
  Config/      SimConfig (mirrors CONFIG)

SHITFIRE.Core.Tests/           ← headless. Runs in CI with no engine, no GPU, no window.

SHITFIRE.Game/                 ← the Godot (or Unity) project
  world/       terrain mesh, ocean, world-feature meshes, units, bursts
  observer/    camera rig, binoculars, reticle, laser, mil card
  ui/          comms log, status strip, mission menu, briefing, AAR, shot plot
  cartography/ screen map, printable sheet renderer, print/PDF export
  audio/       radio net, impact, ambience
  speech/      STT adapter, TTS/voice-line adapter   ← platform-swappable
  bridge/      SimHost: drives Core, translates events to presentation
```

## The rules that keep it honest

1. **`SHITFIRE.Core` never references the engine.** No `Vector3` from Godot/Unity, no `Node`, no engine time, no engine random. It uses its own small value types and its own clock. If Core cannot compile in a bare console app, the boundary has been violated.
2. **Presentation never computes doctrine.** The UI may not decide whether a call is valid, what a correction means, or how many stars a run earns. It renders what Core reports. Every rule that leaks upward becomes a rule that cannot be tested.
3. **One direction of data flow.** Input → Core commands → Core state change → events → presentation. The current sim already works this way in spirit (`fireMission`, `applyCorrection`, `FDC.say`); make it explicit.
4. **The sim clock is not the frame clock.** Core advances by an injected delta. This makes the sim testable at 1000× speed and immune to frame-rate variance — and it is what allows a whole mission to be replayed in milliseconds during parity testing.
5. **Determinism is a contract, not a nicety.** Same seed + same command sequence = same impacts, everywhere, forever. See [PARITY_TESTING.md](PARITY_TESTING.md).

## Interfaces to preserve by name

`CLAUDE.md` in the repo root names the stable interfaces of the browser sim. They carry over as the Core public surface — keeping the names makes the two implementations legible to each other during the port:

| Browser | Core |
|---|---|
| `H(x,z)` | `IHeightField.Height(float x, float z)` |
| `fireMission(targetLocation, warno, meta)` | `Mission.Fire(TargetLocation loc, WarningOrder wo, CallMeta meta)` |
| `applyCorrection(otFrameDelta)` | `Mission.ApplyCorrection(OtDelta d)` |
| `FDC.say(msg)` | `IFdcChannel.Say(FdcLine line)` — an event, not a UI call |
| `Scenario` | `ScenarioState` |
| `gradeMission(metrics)` | `Grading.Score(MissionMetrics m) → Stars` |
| `TLOG` | `TranscriptLog` |

## Addendum — 2026-08-28: the module reality, and the C++ target

The inventory above was measured against the pre-split single `index.html` (4,516 lines). The source is now **18 modules under `src/js/`, ~11,200 lines** (measured at commit `e895666`), and `SCN_META` defines **15 scenario types**, not seven. The transfers-vs-rebuilds classification above still holds; the line counts are historical. The port targets **Unreal Engine 5.8** (see [ENGINE_CHOICE.md](ENGINE_CHOICE.md) *Decision override*), so the core is a **plain C++ static library built `/fp:precise` outside the engine** ([UNREAL_RESEARCH.md](UNREAL_RESEARCH.md) §3a) — the C# structure above maps to it 1:1, namespace `shitfire::`.

Module → what transfers → target namespace:

| Module | Transfers | Namespace |
|---|---|---|
| `00-config.js` | `CONFIG` data (THREE imports and GFX render keys do not) | `config` |
| `01-math-terrain.js` | whole module — PRNG, noise, `H`, rays, grid helpers | `math` + `terrain` |
| `02-scene.js` | DEM parse/ingestion only (~:471) | `terrain` |
| `03-op-battery.js` | OP/battery placement scans | `scenario` |
| `04-world.js` | placement half — roads, facilities, villages, civilians | `scenario` |
| `05-units-scenario.js` | `genScenario` + placement constraints | `scenario` |
| `06-clock-comms.js` | sim clock/scheduler, `TLOG`, FDC say-queue + delivery pacing | `sim` + `review` |
| `07-voice-spot-audio.js` | SALUTE generation, scripted callers | `doctrine` |
| `08-burst-optics.js` | nothing (its dispersion draw consumes `mission.rng` — stream-order note) | — |
| `09-camera.js` | nothing | — |
| `10-mission.js` | whole module — ballistics, mission machine, corrections, impacts | `sim` |
| `11-parser.js` | whole module | `doctrine` |
| `12-cff-handling.js` | whole module — FDC rule engine | `doctrine` |
| `13-aar.js` | verdict/diagnosis assembly (~:364); the plot render does not | `review` |
| `14-map-library.js` | marching-squares logic only (P5 — out of the current slice) | — |
| `15-campaign-menu.js` | `CAMPAIGN` data, `gradeMission`, `CAMP` persistence shape | `campaign` |
| `16-input.js` | nothing | — |
| `17-loop-boot.js` | nothing (its `window.SHITFIRE` export documents the Core API) | — |

Interfaces to preserve by name, C++ form (the C# table above still governs the shapes):

| Browser | C++ core |
|---|---|
| `H(x,z)` | `terrain::HeightField::height(double x, double z)` |
| `fireMission(...)` | `sim::Mission::fire(...)` |
| `applyCorrection(otFrameDelta)` | `sim::Mission::applyCorrection(OtDelta)` |
| `FDC.say(msg)` | `FdcLine` event via `SimCore::drainEvents()` |
| `Scenario` | `ScenarioState` |
| `gradeMission(metrics)` | `campaign::grade(MissionMetrics) → Stars` |
| `TLOG` | `review::TranscriptLog` |

## Three decisions worth making early

**Fixed-point or double?** JavaScript numbers are IEEE-754 doubles. Use `double` in Core for anything feeding the PRNG stream or impact math. Using `float` will silently break bit-parity with the browser oracle and cost days of confusion.

**How is the campaign authored?** Today it is a JS array literal. In-engine it should be data (JSON or an engine resource) so chapters can be tuned without a rebuild. Keep the schema close to the current object shape so the transfer is mechanical.

**Does terrain stay procedural?** The procedural island and the DEM path share one interface, and the port should preserve that. It is what lets real islands drop in later without touching the sim.
