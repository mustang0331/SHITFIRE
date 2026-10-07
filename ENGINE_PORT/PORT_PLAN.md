# PORT_PLAN.md — Staged Migration

Same discipline as the browser build's BUILD ORDER: **ship stage N working and committed before starting stage N+1.** Stages are numbered `P0`–`P8` so they never collide with the sim's own stage numbers.

Each stage lists its work, its **exit criteria** (objective, checkable — not "feels done"), and what it deliberately leaves undone.

> **2026-08-28:** the port is underway as a **P0–P4 slice targeting Unreal Engine 5.8** on branch `unreal-port` (author's override — see [ENGINE_CHOICE.md](ENGINE_CHOICE.md) *Decision override*). P5–P8 are unscheduled. P5's "print/PDF has no free answer" is since answered in [UNREAL_RESEARCH.md](UNREAL_RESEARCH.md) §5: render the sheet to a high-res render target → PNG now, wrap in a PDF via libharu later.
>
> **2026-10-01 status:** P0 done (fixtures + 33 transcripts at tag `oracle-v1`). P1–P2 done — the C++ core replays all 33 transcripts exactly, adversarial sign-off in `port/ADVERSARY_SIGNOFF.md`. P3 done (world, observer, optics, FX, lighting; the author's visual review is still open). P4: typed loop closed end to end in the engine — net HUD, units, audio — with the in-engine gates green in Development and the Shipping-config gate runnable as `-ShitfireParityGate` (see `port/README.md`). The P4 exit still owes the author's by-hand mission and visual review. The live row board is `ROADMAP.md` §3.8.

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

## P4.5 — Night and optics *(scoped 2026-10-06; follows the P4 by-hand exit)*

Named by ENGINE_CHOICE.md (2026-09-02 amendment) as "night + optics follow the slice".
The port renders one hard-coded day rig (`ShitfireWorldActor.cpp:39-42`, "Day only per the
P3 ruling"); the browser has five times of day and three vision modes. Night chapters 2.2
and 2.5 (`reqIllum`) are unplayable until this lands. Browser sources are the oracle;
reference frames are fixtures 09/10/11 (night-eye, NVG, thermal, `DETERMINISM_NOTE.md:92-94`).

| Row | Scope | Browser source | Exit criterion |
|---|---|---|---|
| P4.5a | TOD table + apply: five rows (day/dawn/dusk/overcast/night) incl. the `light` ambient scalar | `02-scene.js:59-76`, `:100-125` | mission `tod` selects sun dir/colour/intensity, sky, fog; all five rows reproduce; `light` readable by optics code |
| P4.5b | Night base look: moonlight, dark ground, no starfield | `02-scene.js:44-58` | night chapter is dark but legible naked-eye, matching fixture 09 |
| P4.5c | Vision mode state, `[O]` cycle, status-strip text | `08-burst-optics.js:84-93`, `:357-380`; `16-input.js:155,276`; `10-mission.js:15-20` | `[O]` cycles day → nvg → thermal → day; HUD shows `OPTIC <mode> \| TOD <tod>` |
| P4.5d | NVG look: P43 green, AGC, bloom-out in daylight (`nvgBloomAt` 0.35) | `08-burst-optics.js:102-110, 163-180, 201-221`; `00-config.js:191` | NVG readable at night; collapses to flat pale green when ambient > 0.35 |
| P4.5e | Thermal look: white-hot, day wash (`thermalWashAt` 0.30), relief kept | `08-burst-optics.js:176-181, 222-235`; `00-config.js:192` | figures separate from cold terrain at night; terrain washes out when ambient > 0.30 |
| P4.5f | Per-material optic signatures (`vis` NVG luminance + thermal RGB) on figures, huts, asphalt, path, rock, canopy, burst FX | `04-world.js:93-159, 1063-1068`; `08-burst-optics.js:36-42, 399-403, 434` | enemy / friendly / civilian discrimination holds in both optics (friendly cap bright NVG, cyan thermal; civ hat cold) |
| P4.5g | Optic overlay: vignette, tube tint, grain, emitter halos (24 Hz, 320×180) | `08-burst-optics.js:261-312, 324-355`; `00-config.js:181-192` | vignette NVG 0.90 / thermal 0.62, grain 0.16 / ×0.45, halos on flare and bursts |
| P4.5h | Illumination at night: flare as a real shadowing light, wind drift, per-flare burn from `mission.rng`, `usedIllum`/`reqIllum` grading | `08-burst-optics.js:451-543`; `10-mission.js:1122-1131`; `00-config.js:117` | flare lights terrain and figures in all three modes; burn seeded; chapter 2.5 grades. Partial today: `ShitfireFlareActor.cpp:16-100` (no drift, no shadows, glow tuned to the day exposure) |

**Decision before P4.5b/d:** the port pins exposure at EV100 ≈ 13 with auto-exposure off
(`ShitfireWorldActor.cpp:370`, `ObserverPawn.cpp:61-68`). Night and NVG gain need an
exposure story (per-TOD manual EV, or a different rig) — `[YOUR CALL]`, and ENGINE_CHOICE.md
asks for the Unreal override to be re-examined before P5 anyway.

**Determinism:** grain and halo randomness are sanctioned flavour; the illum burn is SIM-PATH,
one `mission.rng` draw (`DETERMINISM_NOTE.md:15,20-21`). Parity gates stay green throughout.

**Exit criteria:** chapters 2.2 and 2.5 playable by hand at night in all three optics, with
impacts and AAR still matching the oracle; fixtures 09/10/11 reproduced to the author's eye.

---

## ART — milsim art pass *(row PORT3, opened 2026-10-07 at the author's direction)*

The 2026-09-02 ruling (ENGINE_CHOICE.md) set the bar at a full milsim look, but no stage owned
the art. P3 built the pipeline (`asset_manifest.json` → `download_assets.py` → `import_assets.py`,
LFS, `uasset-allowlist.txt`, `CREDITS.md`) and shipped free placeholders. Inventory 2026-10-07,
by class (evidence in the 2026-10-07 vault log):

| Class | Today | Gap |
|---|---|---|
| Terrain | one 300×300-quad mesh, four 2K CC0 layers blended by slope/height, no AO/roughness, no near-field detail | near-field detail layer, macro variation, AO/height blend |
| Vegetation | Quaternius low-poly palm + desert bush, two species, no wind, no LOD, no clutter | realistic palm/scrub/grass, clutter scatter, LOD/billboard tier |
| Rocks | grey engine cubes | boulder/outcrop set |
| Roads | one 2K dirt tile ribbon, hard edge | edge blend, normal map, asphalt variant for the town |
| Sea | translucent plane, sum-of-sines normal, screen-space reflection | real displacement, shore foam, sea-state table |
| Sky/fog | SkyAtmosphere + height fog, no clouds, one day TOD | cloud layer (overcast row), tuned haze; TOD rows are P4.5a |
| Infantry | Quaternius soldier, three clips, no kit, no prone/kneel clips | realistic rigged soldier, friendly uniform variant, fuller clip set |
| Civilians | same soldier mesh re-tinted | distinct civilian body and hat silhouette |
| Vehicles | Poly Pizza Humvee + M939 (CC-BY), Quaternius tank, fit-scaled to the 5 m box | realistic HMMWV/truck/armour with wreck states |
| Tower | engine cubes | modelled timber/steel tower, deck at exactly `towerHeight` |
| Battery | Quaternius cannon, cube tent, sphere flashes | real tube/howitzer, crew, emplacement dressing |
| KP posts | white cubes | stake/panel mesh legible at optic ranges |
| Buildings | cubes for huts, town, bunker, mast, flag | modular arid village kit, town block, mast |
| Burst FX | sphere flash, cylinder smoke columns, no Niagara | Niagara flash/dust/debris/smoke, scorch decal |
| Illum flare | sphere + point light, no drift | P4.5h owns it |

**Rows.** Each lands as its own commit through the manifest pipeline, with a viewed probe frame
and a plugged-in perf capture before and after.

| Row | Scope | Gate |
|---|---|---|
| ART-a | Master material convention: one parent material carrying the `vis` optic-signature parameters (NVG luminance, thermal colour) per browser table (`04-world.js:93-159`); every imported pack material becomes an instance of it. Prerequisite for every other row, and for P4.5f | an untagged material fails a test, not a silent luminance fallback |
| ART-b | Infantry: rigged soldier body (enemy, friendly variant), distinct civilian body; Quaternius Universal Animation Library retargeted (prone, crawl, death, rifle). Feet-origin pivot check added to `HumanoidImportGate`; `SoldierNativeHeightCm` re-measured per body; casualty state distinct from prone (closes browser `GFX3`) | figures countable at 3000 m through 7X (GRAPHICS.md check 2); enemy/friendly/civilian unambiguous at 2000 m |
| ART-c | Vehicles: HMMWV, light truck, one armour type with wreck variants. Manifest gains an authored mil-dimension field; a vehicle import gate like the humanoid one | truck reads 5 / 2.5 / 1.67 mil at 1 / 2 / 3 km against the reticle stadia |
| ART-d | Structures: village/compound kit, town block, bunker, mast, tower, KP post, battery dressing. Hut footprints stay on the mil card (7 × 3 × 5 m, 5.2 × 3 × 4.2 m); tower deck at `towerHeight`, eye 2.2 m above it | map/printed-sheet oracle unchanged; reticle readings on the tower match the oracle |
| ART-e | Environment: rocks, realistic palm/scrub, ground clutter. Clutter seed and placement obey the browser's exclusion discs, road/beach/slope rules and the 6 m canopy cap (`04-world.js:1026-1050`); up-facing normals / two-sided shading so canopy reads from 300 m down; LOD step in `import_assets.py`; 2K texture cap | 4000 placements inside budget; KP posts and burst plumes never masked |
| ART-f | Burst FX in Niagara with the browser's envelope pinned first (flash 0.18 s, ring 4→70 in 1.3 s, dust 6→42 m in 3 s, column to ~107 m over 11 s leaning with island wind, dead at 14 s; `08-burst-optics.js:616-685`) | timeline fixture passes before and after |
| ART-g | Sea and sky: water with shore foam and a sea-state table; cloud layer for the overcast TOD. Fog envelope (linear 4500–15000 m) and the ACES horizon stay as the oracle has them | fixtures 09/10/11 still match; 3200 m legibility floor holds |
| ART-h | Terrain and roads: near-field detail, AO/height blend, road edge blend, asphalt variant; black-sand palette variant for Volume IV | no change to `H(x,z)`; mil-card reference objects unchanged |

**Decisions before ART-b (the first visible row):**
- **[YOUR CALL] Milsim vs legibility.** The browser's figure palette is an inverted two-tone
  rule (enemy dark body / bright cap, friendly dark body / ice cap, civilian pale body / dark
  hat) chosen so figures are countable at 3000 m and families distinguishable at 2000 m.
  Realistic camouflage breaks it. The row needs a ruling on which wins when they conflict.
  Recommendation: legibility wins; realism goes into kit, posture, animation and materials, not
  the silhouette palette.
- **[YOUR CALL] Nanite stance.** `DefaultEngine.ini` says nothing ships Nanite, the manifest sets
  it on five vegetation meshes, and the import enables it in a try/except that only logs. Settle
  it and check in-editor before any Nanite rock pack lands.
- **Order against P4.5.** Perf headroom is 2.9 ms avg / 2.2 ms p95 at 1080p plugged in, Lumen
  on, before any art (`port/README.md:230-236`); the six-burst FFE baseline was never measured.
  P4.5h's shadowing flare light and the night rig also cost frame time. Recommendation: ART-a
  and ART-b first (every frame shows figures), then P4.5, then ART-c onward with the budget
  re-measured.

**Verified sources (fetched 2026-10-07, licence page read):**
- Quaternius Universal Animation Library, CC0, free. Animations only, no body mesh.
- HMMWV M998A1 by 42manako, Sketchfab, CC BY, free; attribution in CREDITS.md.
- M1A1 Abrams by MirzaArrafiERV_45, Sketchfab, CC BY, free; attribution. Not on the mil card.
- Poly Haven Namaqualand collection, CC0, free; rocks/scrub/ground, 8K textures (cap to 2K), the
  UE scene is a 5.4 project (migrate, never open in place; the allowlist will reject strays).
- Sixteen further candidates (Fab character, mortar, village, watchtower, palm, desert, Niagara
  and smoke packs) were found by search but **not verified**: licence, price and 5.8 support
  unchecked. Treat as leads only. Trademark on vehicle names and shapes is outside CC BY; the
  Fab EULA on redistribution inside a free binary has not been read.

**Pipeline work the rows depend on:** new manifest source types (Sketchfab, Poly Haven) with
hash pins so a fresh clone regenerates `Content/`; allowlist directories for Animations,
Characters, Buildings, Water, Niagara, IK retargeters, each with a why and a regenerate-how;
an animation-only import and retarget step; a LOD step; a per-class perf line.

**Exit criteria:** GRAPHICS.md's own gates on the engine build: figures countable at 3000 m
through binos, village vs military element unambiguous at 2000 m, first round at max deviation
spottable and measurable off the reticle, 60 fps at 1080p on the RTX 3060 laptop with six
bursts live; parity gates untouched; every asset in the manifest, credited, regenerable.

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
