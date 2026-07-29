# PARITY_TESTING.md — The Browser Sim Is the Oracle

Most ports are validated by a human playing both versions and saying "feels about the same." This port does not have to settle for that, because SHITFIRE is **fully deterministic**: a seed plus a command sequence produces exactly one outcome. That makes the existing browser build a **golden-master oracle** — an executable specification the port can be tested against automatically, to the meter.

Treat this as the highest-leverage document in the folder. It is what turns "we rewrote it and it seems fine" into "we rewrote it and proved it matches."

## Why determinism holds today

- `mulberry32(seed)` drives everything: terrain wobble, scenario placement, first-round error, follow-up dispersion, FFE stagger, world-feature layout.
- Terrain seed and mission seed are explicit in `CONFIG.SEED`; every campaign chapter declares a fixed mission seed.
- Impacts are computed directly (`impact = aimpoint + error`) — no physics integration, no frame-rate dependence, no floating-point accumulation over time.
- The sim clock is event-scheduled, not frame-driven.

Nothing in the sim path reads wall-clock time, `Math.random()` for anything scored, or engine state. **Preserve this property. It is worth more than any feature.**

## The one thing that must be bit-exact

```js
function mulberry32(a) {
  a |= 0; a = a + 0x6D2B79F5 | 0;
  let t = Math.imul(a ^ a >>> 15, 1 | a);
  t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
  return ((t ^ t >>> 14) >>> 0) / 4294967296;
}
```

This is 32-bit integer arithmetic with unsigned shifts, which ports to C# exactly using `uint`/`int` with `unchecked`. `Math.imul` is `unchecked((int)(a * b))`. `>>>` is `>>` on a `uint`. The final divide is by `4294967296.0` as a **double**.

**Test first, before anything else is ported.** Generate the first 10,000 draws from seeds `1337`, `1`, `101`, `9021`, `5150`, `66600` in the browser, save them, and assert the C# implementation reproduces every one to the bit. If this test does not pass, nothing downstream can be trusted — a one-ULP difference here becomes a 40-metre difference in an impact point, and you will spend a week hunting it as a "ballistics bug."

The same applies to consumption *order*. The PRNG is a stream: if the port draws values in a different sequence — say, evaluating placement candidates in a different order — every subsequent draw diverges even though the algorithm is correct. **Port the call order, not just the calls.**

## Three levels of parity test

### Level 1 — Pure function parity (fast, run on every commit)

Export fixtures from the browser and assert equality in `SHITFIRE.Core.Tests`:

| Fixture | Assertion |
|---|---|
| PRNG streams (6 seeds × 10,000 draws) | Bit-identical |
| `H(x,z)` over a 200×200 grid, procedural seed 1337 | Within 1e-9 |
| `H(x,z)` over the same grid with a known DEM | Within 1e-9 |
| `groundHit` / `hasLOS` for 500 sampled ray pairs | Same hit/miss; hit distance within 0.01 m |
| `gridOf` / `enToWorld` round-trips | Exact |
| `spotBurst` (mils, OT factor, deviation correction) for 200 burst offsets | Exact |
| `otFactor`, correction rounding, 30 m minimum | Exact |

### Level 2 — Scenario generation parity

For every campaign chapter seed (F.1 through 4.4) and every difficulty, generate the scenario in both implementations and compare:

- Target position (within 0.01 m), effect radius, hits required
- Known point positions, ids, and **names**
- Convoy path start/direction/length, pit-stop distance and duration
- Friendly element start positions, advance azimuth, advance cap
- World features: coast-road vertex count and positions, each facility kind/name/position, village names/positions/hut layout, civilian home positions
- OP and battery positions

This is the test that catches PRNG-order drift. A chapter whose target lands 300 m from where the browser puts it is not "close enough" — it is a different chapter, with different terrain masking, and its par time and star thresholds no longer mean anything.

### Level 3 — Full mission replay

The **transcript log (`TLOG`) already records everything needed to replay a mission**: every observer transmission with its parse classification, every FDC line, every impact with distance-to-target, and the AAR outcome — each stamped with sim time, state, chapter, and seed.

The replay harness:

1. Export a transcript from the browser (mission menu → **EXPORT JSON**).
2. Feed the observer transmissions, in order and at their recorded sim times, into Core with the same seed and difficulty.
3. Assert: identical parse classification per message, identical impact coordinates (within 0.01 m), identical FDC decision points (readback / challenge / rant / coach fired or not), identical final AAR — verdict, stars, adjusting rounds, first-round miss, initial location error, correction efficiency, bracket state, wasted rounds.

A transcript library of 20–30 recorded missions covering every scenario type, both fail modes (fratricide, collateral), strict net, the 60mm chapter, the dead-laser chapter, and the tutorials becomes a **regression suite that outlives the port** — it will catch balance changes and rule regressions for the life of the project.

Recording that library is a task for *before* the port begins, while the browser build is the only implementation and is known-good. See [PORT_PLAN.md](PORT_PLAN.md) stage P0.

## Exporting fixtures

No new tooling is needed in the browser build. `window.SHITFIRE` already exposes `CONFIG`, `H`, `fireMission`, `applyCorrection`, `FDC`, `WORLD`, `gradeMission`, `CAMPAIGN`, `CAMP`, `TLOG`, `Scenario`, `mission`, `OP`, `BATTERY`. Fixtures can be generated from the browser console or a headless Chrome run and written as JSON. Keep them in `ENGINE_PORT/fixtures/` under version control — they are the specification.

## Where parity is *not* required

Be explicit, or the team will chase differences that do not matter:

- **Visual output.** Particle counts, smoke drift, mesh detail, colours — no parity expected.
- **Audio.** Different synthesis or recorded lines — no parity expected.
- **Exact FDC wording.** Quip *selection* is `Math.random()` and deliberately not seeded; assert that the FDC *decided to speak in a given category* (readback, danger-close challenge, timid-correction coach), not which line it picked.
- **Timing to the millisecond.** Assert event ordering and sim-time within a small tolerance, not exact frame timing.
- **UI layout.** The port is expected to look better.

## The standard to hold

> The port is not "done" when it runs. It is done when every recorded browser mission replays through the new core and produces the same impacts, the same doctrinal decisions, and the same star grade.

Anything less means the campaign's par times, star thresholds, and training standards — all tuned against the browser build and documented in [reference/TLO.md](reference/TLO.md) — silently stop meaning what they say.
