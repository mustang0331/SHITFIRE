# BALLISTICS_RESEARCH.md — real-world accuracy/lethality reference

Research notes only — **not a spec, not a roadmap row.** Compiled from unclassified,
publicly-available fire-support reference material (JFIRE/ATP 3-09.32, FM 3-09 series,
Ranger Handbook, publicly summarized JMEM-style planning figures) to sanity-check and
inform tuning of `CONFIG.BALLISTICS` / `CONFIG.MISSION` values. Figures are **approximate,
order-of-magnitude, and training-representative** — real dispersion/lethality tables are
weapon-, charge-, fuze-, and edition-specific and much of the precise data is restricted.
Treat everything below the way the project already treats TOF: "believable, not exact."

No conclusions here require touching `index.html`. Read this as input to a future
ROADMAP row (most likely folding into 12i/12h/F2 or a new fix row), not as a row itself.

---

## 1. What the sim currently does (baseline, from `index.html`)

```
BALLISTICS: {
  firstRound: { easy: [40,90], normal: [60,150], hard: [90,220] }   // m, uniform magnitude
  followUp:   { easy: {range:12, defl:7}, normal: {range:18, defl:10}, hard: {range:26, defl:15} } // 1-sigma
  tofDivisor: 300, tofBase: 8, tofMin: 15, tofMax: 40                // same formula for arty AND mortar
}
MISSION: {
  effectRadius: 60 (default; 30 for wreck, 35 for bunker), hitsToNeutralize: 3,
  fratricideRadius: 80, hutCollateralR: 24, civCollateralR: 28
}
assetScale(): 60mm mortar = 0.55x on BOTH first-round and follow-up dispersion, uniformly, at all ranges.
```

Notable: dispersion is a flat per-difficulty constant — it does **not** scale with range
despite SPEC.md §BALLISTICS saying follow-up spread should be "scaling gently with range."
Worth flagging as a gap between spec intent and shipped code (see §7).

---

## 2. Real dispersion: bias vs. random error (this part the sim already gets right)

Real gunnery splits total miss into two independent components:

- **Location/delivery bias** — non-standard conditions the first round eats in full: met
  message age/accuracy, propellant temperature, muzzle-velocity variation from tube wear,
  unregistered piece, Coriolis/rotation-of-earth at long range. This is *systematic*, not
  random — which is exactly why observing and correcting it collapses the error almost
  entirely on the next round.
- **Random dispersion (Probable Error)** — round-to-round variation that survives even
  after the bias is corrected: propellant burn variability, tube wear, minor met
  fluctuation between rounds. Doctrine expresses this as **Range PE** and **Deflection PE**
  (the radius containing 50% of rounds along each axis), always with **Range PE > Deflection
  PE**, typically by a factor of roughly 2–3:1 for tube artillery at service ranges.

**The sim's two-tier model (large first-round miss, small elliptical follow-up spread with
range > deflection) is structurally correct** — it mirrors the doctrinal split rather than
being an arbitrary game mechanic. This is the single most important "we already did this
right" finding; no change needed to the shape of the model.

Rough PE-to-sigma conversion for reference: `PE ≈ 0.6745σ` (PE is the 50%-radius, same
relationship as a probable-error/CEP family). The sim's `followUp` values are 1-sigma, so
e.g. Normal's `range:18` corresponds to a Range PE ≈ 12 m — plausible for short-range fires
(see §3 on why the sim's engagement ranges read as short for a 155 battery).

---

## 3. Range-scaling gap and why it matters here

Real Range PE grows with range (more time in flight, more charge-related variance, more
met sensitivity) — a rough unclassified rule of thumb for a 155mm battery is Range PE
climbing from ~15–20 m at short/medium range to 40+ m out past 15–18 km, with Deflection PE
growing much more slowly. `CONFIG.MISSION.targetRange: [1500, 3200]` sits at the very short
end of a 155's engagement envelope (a 155 typically isn't employed at 1.5–3 km — that's
inside minimum safe/minimum charge range for many gun-target geometries), so the current
flat, tight follow-up dispersion is *not unreasonable for the ranges the sim actually uses*.
The gap is real but low-stakes: if `targetRange` is ever widened (e.g., a "long shoot" or
counterfire scenario further from the OP), dispersion should widen with it, or the far end
of the range band will read as unrealistically tight. Cheap to add later: scale `followUp`
sigmas by something like `1 + (range - 2000) / 12000` — gentle, matches SPEC's own language,
doesn't need a new CONFIG shape.

---

## 4. Mortar-specific: dispersion is right, time of flight is not

`assetScale()` tightens the 60mm's dispersion to 0.55x, which is directionally correct —
mortars have shorter, lower-velocity, higher-drag rounds and are used at much shorter range,
so tighter absolute dispersion (in meters) checks out, and it's consistent with
CLAUDE.md's stated 60mm precision (8-digit/10 m vs. artillery's 6-digit/100 m).

**What's missing: mortars fly a high-angle trajectory, so time-of-flight at a given range
is longer than a flat-trajectory gun round, not shorter.** A 60mm mortar at 1–2 km range
typically has a TOF in the 15–30+ second range depending on charge — comparable to or
*longer* than a 155 gun's TOF at a much longer range, because the mortar round spends more
time climbing and falling rather than flying flat. The sim's `tofFor()` uses one formula
(`range/300 + 8`, clamped 15–40) for both assets, so a mortar shooting a 1200 m target gets
a TOF near the 15 s floor — same shape as a gun would at that range, when in reality the
mortar's arc would keep it airborne longer. This is the one clearly wrong physical fact in
the current model (everything else is a defensible simplification). A minimal fix would be
a `tofBase`/`tofDivisor` pair keyed by asset the same way `assetScale()` already is, e.g. a
higher `tofBase` for mortar so short-range mortar TOF doesn't undershoot a gun round's.

---

## 5. Casualty/lethal radius — the sim doesn't model this at all, and arguably shouldn't fully

The sim's `effectRadius` (60 m default, 30–35 m for point targets) is a **gameplay
"rounds must land within this circle to count" radius**, not a lethality/casualty-radius
simulation, and per DOCTRINE.md the RREMS surveillance terms (suppressed/neutralized/
destroyed) are narrated outcomes, not computed from a casualty model. Real unclassified,
order-of-magnitude figures for context (these vary by source, fuze, and target posture —
prone/dug-in vs. standing/exposed drops the effective radius sharply):

| Munition | Commonly-cited casualty radius (exposed personnel) | Notes |
|---|---|---|
| 60mm mortar HE (M720/M888) | ~15–20 m | shaped for close-range infantry support |
| 81mm mortar HE | ~25–35 m | |
| 105mm HE | ~35–50 m | |
| 155mm HE (M795) | ~50–100 m | wide range across sources; fuze quick biases frag low/forward, VT/proximity airburst spreads frag over a wider footprint incl. into defilade |
| M67 fragmentation grenade | ~15 m casualty / ~5 m near-certain | for scale comparison only |

Real lethality isn't a hard circle at all — it's a probability-of-incapacitation surface
that falls off with distance from burst (steepest near the point of detonation, a long tail
beyond the "casualty radius" headline number), further modified by posture (standing >>
prone >> dug-in) and by fuze/height-of-burst (VT/proximity fuzes detonate a few meters
above ground and shape a markedly different frag pattern than fuze-quick ground bursts).
**Recommendation: don't chase this precision.** The project's own ballistics model
deliberately ignores fragmentation pattern and height-of-burst effects on lethality (per
DOCTRINE.md "what the sim deliberately ignores"), and a probability-falloff casualty model
would be real complexity for no training-value payoff — the trainee is being graded on
CFF/adjustment skill, not munitions-effects modeling. The one place this is worth a glance:
the current `effectRadius` values (60/30/35 m) sit inside the plausible 155mm HE casualty-
radius band above, which is a reasonable place for them to sit as a hard "in effect" gate —
no change indicated.

---

## 6. Danger close: doctrine's flat 600 m is a deliberate simplification, and it's the right one

CLAUDE.md/DOCTRINE.md fix danger close at 600 m for the sim's 155 battery. In reality,
danger-close (and its underlying formal concept, Risk Estimate Distance / minimum safe
distance) is **weapon-specific**, not a single number — JFIRE's real reference table has
mortars carrying a much shorter danger-close distance than 155mm/naval gunfire, on the
order of a third to half of the artillery figure, scaling up through 105mm, 155mm, naval 5",
and air-delivered ordnance (which varies far more widely still). DOCTRINE.md already flags
its 600 m figure as an artillery-specific ⚠ simplification, which is the correct call for a
single-file trainer — but it means **the 60mm mortar chapters (Volume IV) are currently
using an artillery danger-close distance for a mortar**, which is backwards from real
doctrine (mortars should trigger danger-close *closer* to friendlies, not at the same 600 m
as the battery). This is a candidate finding for **F2** (tiered danger-close bands), which
is already on the board and unowned by a specific number scheme: if F2 is implemented,
consider keying the band thresholds off `assetScale()`'s existing mortar/battery
distinction rather than introducing a new CONFIG axis.

---

## 7. Smoke & illumination — reference figures for the parked 12h row

Relevant since 12h (smoke/illum missions) is blocked on 13c and will need *some* physical
anchor when it's picked up:

- **Smoke (WP or base-eject smoke round):** an individual round's cloud is on the order of
  100–150 m across at first bloom, builds over ~30–60 s as multiple rounds/canisters
  function, and a sustained screen usually needs continuous fire or multiple rounds — a
  single round's smoke thins out in a few minutes without wind. Wind speed/direction
  dominates how the screen drifts and how long it holds a line, which is exactly what the
  parked **12i wind model** row is for — smoke is the one effect where wind is not just
  visual flavor but the actual mechanic being taught (reading drift to keep a screen
  between friendlies and the objective).
- **Illumination:** a single round under parachute typically gives on the order of 500,000–
  1,000,000 candela, useful light over roughly a 1 km diameter footprint, for a hang time
  in the ballpark of 30–60 seconds before it burns out and starts to descend/swing on the
  chute (the swinging shadow the sim's IMPACT EFFECTS section already calls out). Sustained
  illumination of a target area means re-firing on a timed cadence before the last round
  burns out — a natural timing mechanic for a night mission once 13c's time-of-day model
  lands.

Neither of these needs precision, same reasoning as §5 — they exist to make the mechanic
(read wind drift; time illum rounds) legible, not to model pyrotechnics accurately.

---

## 8. Summary of findings, ranked by whether they're worth doing anything about

1. **Mortar TOF sharing the gun's formula is the one outright wrong fact** (§4) — mortars
   should read *slower*, not just tighter, at short range. Cheapest, highest-confidence fix
   if this ever gets picked up.
2. **Danger-close is artillery-shaped for mortar missions too** (§6) — feeds directly into
   the already-parked F2 row; worth folding this finding in when F2 is scoped rather than
   re-deriving it then.
3. **Follow-up dispersion doesn't scale with range** (§3) — real but currently low-stakes
   because `targetRange` keeps engagements short; only matters if the range band widens.
4. **Casualty-radius modeling** (§5) — deliberately out of scope, current `effectRadius`
   values already sit in a defensible real-world band; recommend leaving as-is.
5. **Smoke/illum reference figures** (§7) — no action now, just groundwork for 12h/12i
   whenever they come off BLOCKED.

Nothing here implies a code change today. If any of #1–#3 get scheduled, they're small,
single-function edits (`tofFor`, danger-close threshold table, `followUpError`) that fit
the existing CONFIG-driven, one-row-one-commit pattern without new architecture. Illustrative
snippets for each are in §9 — sketches for whoever scopes the row, not a diff to apply as-is,
and per CLAUDE.md, actual edits to `index.html` are Fable's file, one writer at a time.

---

## 9. Suggested code changes (illustrative — not applied)

All three follow the same shape as the existing `assetScale()` helper
(`index.html:2022-2024`): key a small table off `activeChapter.asset === 'mortar60'` instead
of inventing a new CONFIG axis. None of these change the stable interfaces
(`fireMission`/`applyCorrection`/`FDC.say`/`Scenario`/`gradeMission`/`TLOG`) — they're
internal to the ballistics helpers.

### 9.1 Mortar TOF (§4) — `tofFor()`, `index.html:2038-2042`

Today, one formula for both assets:

```js
function tofFor(aim) {
  const B = CONFIG.BALLISTICS;
  const range = dist2(BATTERY.x, BATTERY.z, aim.x, aim.z);
  return clamp(range / B.tofDivisor + B.tofBase, B.tofMin, B.tofMax);
}
```

Sketch — asset-keyed base/floor so a short-range mortar round doesn't undershoot a gun
round's TOF, mirroring how `assetScale()` already branches:

```js
// CONFIG.BALLISTICS addition:
//   tof: { battery: { divisor: 300, base: 8,  min: 15, max: 40 },
//          mortar60: { divisor: 220, base: 14, min: 18, max: 30 } }  // higher base = high-angle arc
function tofFor(aim) {
  const key = activeChapter && activeChapter.asset === 'mortar60' ? 'mortar60' : 'battery';
  const t = CONFIG.BALLISTICS.tof[key];
  const range = dist2(BATTERY.x, BATTERY.z, aim.x, aim.z);
  return clamp(range / t.divisor + t.base, t.min, t.max);
}
```

Numbers above are illustrative, not sourced to a specific firing table — tune by feel
against §4's "mortar TOF at short range should read comparable to or longer than a gun's,
not shorter" rule, then check against the existing `SHOT`/`SPLASH` pacing so splash-lead
(`B.splashLead`) still reads naturally for both assets.

### 9.2 Asset-keyed danger-close (§6) — around `index.html:2505-2511` and the mirrored check at `:2592`

Today, one flat threshold used in two places:

```js
// :2505
let minF = Infinity;
// ... minF = Math.min(minF, dist2(cx, cz, f.x, f.z));
const dcSent = p.raw.includes('danger close');
if (minF < 600 && !dcSent) { FDC.say(pick(QUIPS.dangerClose), { delay: 1.2 }); /* ... */ }

// :2592
if (dist2(nx, nz, f.x, f.z) < (f.r || CONFIG.MISSION.fratricideRadius) + 50) { /* ... */ }
```

Sketch — a `dangerCloseRadius()` helper next to `assetScale()`, used at both sites so the
600 m figure and the mortar figure never drift apart:

```js
// CONFIG.MISSION addition: dangerClose: { battery: 600, mortar60: 250 }
function dangerCloseRadius() {
  const key = activeChapter && activeChapter.asset === 'mortar60' ? 'mortar60' : 'battery';
  return CONFIG.MISSION.dangerClose[key];
}
// :2505
if (minF < dangerCloseRadius() && !dcSent) { /* unchanged */ }
```

This also happens to be the natural place to fold in **F1b** (the `<` vs `<=` inclusivity
fix DOCTRINE.md flags) — change the comparison to `<=` in the same edit rather than a
separate pass over the same lines.

### 9.3 Range-scaled follow-up dispersion (§3) — `followUpError()`, `index.html:2030-2037`

Today, flat 1-sigma regardless of range:

```js
function followUpError(rng, otAz) {
  const s = CONFIG.BALLISTICS.followUp[DIFFICULTY];
  const k = assetScale();
  const along = gauss(rng) * s.range * k, across = gauss(rng) * s.defl * k;
  // ...
}
```

Sketch — a gentle range multiplier applied only to the range-axis sigma (deflection PE
grows much more slowly in reality, per §2, so leave `across` alone):

```js
function followUpError(rng, otAz, otRange) {
  const s = CONFIG.BALLISTICS.followUp[DIFFICULTY];
  const k = assetScale();
  const rangeK = 1 + Math.max(0, otRange - 2000) / 12000;   // gentle growth past 2 km
  const along = gauss(rng) * s.range * k * rangeK, across = gauss(rng) * s.defl * k;
  // ...
}
```

Needs `otRange` threaded in from the two call sites (`fireAdjustRound`/`fireForEffect`,
which already compute `otAz` next to a distance they could pass through). Low priority per
§3 — only matters if `CONFIG.MISSION.targetRange` ever widens past the current short band.
