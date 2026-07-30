# DOCTRINE.md — Call-for-Fire Format, Terminology, Protocols

Distilled from **ATP 3-09.32 / MCRP 3-31.6 JFIRE (18 Oct 2019)** and the **JFO Student Handout (JFO 0103, 27 Mar 2018)**. This is the authority for the parser, the FDC's traffic, strict mode, and the campaign's skill order. Sim-design deviations are marked ⚠.

## Scripts are guidelines, not gates

The formats below are the *target*, not a hard requirement every call must match to parse. By default the parser accepts loose phrasing (forgiving mode); doctrinal correctness is graded, not gated. Deviation from the script is tiered by how the FDC *reacts*, not by whether the mission can proceed:

- **Clean** — normal net traffic, no comment.
- **Stupid-but-safe** — wrong element order, a malformed-but-unambiguous call, absurd rounding: one snide remark, mission continues.
- **Dangerous** — danger close without the proword, a correction walking fire toward friendlies/civilians, FFE against an unresolved unsafe aimpoint: triggers the FDC's rant system.

**Strict mode** is the one place rigid enforcement lives — a training-mode overlay (Volume IV / Veteran tier) that rejects malformed calls per the element order and rounding standards below, the way a real evaluator would. It is not the sim's default posture.

## The call for fire — 6 elements, 3 transmissions

A CFF is a **request**, not an order. Six elements sent in three transmissions; **the FDC reads back each transmission**. If the observer or FDC errs, the observer announces **"CORRECTION"** and resends the corrected transmission.

**Transmission 1 — Observer ID + Warning order:**
> "HELLHOUND FIRES, this is MUSTANG 12, adjust fire, over."

- Mission types: **adjust fire** (location questionable — expect to correct), **fire for effect** (location accurate first volley), **suppress** (a previously recorded target, by number, + duration — like immediate suppression, sent as ONE transmission: ID + warning order + target number + duration, e.g. "suppress target AK1002, 10 minutes, over"), **immediate suppression / immediate smoke** (friendlies under fire — sent as ONE transmission: ID + warning order + location).
- Method of location is announced here only if NOT grid: "adjust fire **polar**", "adjust fire, **shift AB1002**". Grid is the standard and goes unannounced.

**Transmission 2 — Target location:**
- **Grid:** "Grid 123 456, over." — 6-digit standard. **OT direction must be sent before or with the first correction** ("Direction 5920, over").
- **Polar:** "Direction 1650, distance 2100, down 40, over." — direction to nearest **10 mils**, distance to nearest **100 m**, up/down only if vertical difference ≥ **35 m**, to nearest **5 m**. FDC must know observer's position. **The sim enforces this: a polar CFF is refused until the observer sends a POSITION REPORT ("POSITION GRID …"), and the mission is resolved from the *reported* position.**
- **Shift from known point:** "Direction 2340, right 110, add 400, up 55, over." — direction (OTL) nearest 10 mils, lateral shift nearest **10 m**, range shift nearest **100 m**, vertical nearest 5 m.
- ⚠ Doctrine ties 8/10-digit grids to **laser grid** missions, not to the weapon. The sim's "mortars = 8-digit" rule is a deliberate design simplification — keep it, but strict mode should accept 6-digit as always valid.

**Transmission 3 — Description + method of engagement + method of fire and control:**
- **Target description:** size, type, degree of protection ("platoon of troops dug in").
- **Method of engagement** (optional; standard = area fire, HE/fuze quick): **danger close**, mark, trajectory, ammunition, volume of fire, sheaf.
- **Method of fire and control** (optional; standard = **"when ready"**): **at my command** (FDC: "Ready, over" → observer: "Fire, over"; persists until "Cancel at my command"), do not load, cannot observe, time on target.

## Danger close

Artillery/mortar round predicted within **600 m** of friendlies → observer announces **"danger close."** Adjust with **creeping fire**: corrections of **100 m or less**, walking rounds toward the target from the safe side.

## FDC → observer traffic

- **Message to Observer (MTO)** after the CFF readback, opening with the proword **"Message to observer"**. Required: **units to fire, changes to the CFF (if any), number of rounds per tube, target number** (e.g. "Message to observer, battery, 4 rounds, target AB2001, over"). Optional: time of flight, maximum ordinate altitude, other info. **The observer reads back the entire MTO.** No MTO is sent for immediate suppression.
- **"SHOT, OVER"** from FDC at firing. **"SPLASH, OVER"** ~5 s before impact (on request or given by SOP).
- FFE completion: **"ROUNDS COMPLETE, OVER."**

## Corrections (adjust phase)

Sent in order **deviation → range → height of burst**, opposite the spotting, relative to the **OT line**:

> "Left 30, add 200, over." · final: "Right 10, drop 50, **fire for effect**, over."

- **Deviation:** mils spotted × **OT factor** (OT distance ÷ 1000, whole number) → meters, to nearest **10 m**. Minimum HE correction **30 m** (below that, ignore deviation).
- **Range:** add/drop in multiples of **100 m** (minimum 100; **50 m allowed when entering FFE**). Methods: **successive bracketing** (800/400/200 → split until FFE), **hasty bracketing**, **one-round adjustment**, **creeping fire** (danger close).
- **HOB:** up/down in 5 m multiples (airburst/steep slopes only — rare with fuze quick).
- Enter FFE when splitting a **100 m bracket** (point target) or effects are observed.
- **OT direction is a hard prerequisite on grid missions** — the FDC refuses any correction until it holds the observer-target line (left/right and add/drop are meaningless without it). Polar and shift carry direction in the call.

## End of mission — RREMS

Final transmission: **R**efinement (final aimpoint shift, nearest 10 m, may be <30 m) → **R**ecord as target (optional) → **E**nd of **M**ission → **S**urveillance:

> "End of mission, target neutralized, estimate 15 casualties, over."

Surveillance terms are precise: **suppressed** (effect lasts only during fires), **neutralized** (out of the fight temporarily, ≥10% casualties/damage), **destroyed** (must be reconstituted/replaced, ≥30%).

## Prowords & vocabulary

**Over / Out / Say again / Correction / Break** · **Message to observer** (FDC's lead-in to the MTO) · **Shot / Splash / Request splash / Rounds complete / Ready / Fire / Repeat** (= fire again, last data — never "repeat" as "say again" on a fire net) · **Check firing / Cease loading** (safety stops) · **Record as target / Target number AB####** · **Grid / Direction / Distance / Left / Right / Add / Drop / Up / Down / Danger close / Fire for effect / End of mission**.

## Doctrinal performance standards (use for star pars)

From JFO evaluation standards (JFO-SSUP-2001, adjust-fire mission): initiate the CFF within **2 minutes** of target identification; initial target location within **200 m** of the true location; announce subsequent corrections within **15 seconds** of the burst; enter fire for effect within **±50 m** of the target using **no more than 3 adjusting rounds**. These map directly onto the star-grading metrics (time-to-CFF par, correction-timing par, first-round accuracy, rounds-to-effect, format correctness) — the sim's own applied pars (`passMaxAdjustRounds: 4`, bonus star at ≤2 rounds; see TLO.md §4) are a deliberately looser design adaptation of this raw 3-round figure, not a literal restatement of it.

## Pre-mission and effects data (researched 2026-07-29 for G10/G13/G15/G16/G18)

**G10 — initial firing-element status.** No land field-artillery/mortar doctrine names a scripted pre-mission status exchange — checked FM 6-30 (1991) Ch.4 and current **ATP 3-09.30 Observed Fires (Sep 2017)** end to end; neither has one. The only doctrinally-named version is naval: **FIRE UNIT STATUS** (FM 6-30 §8-12) — the ship, on arriving on station, reports **"ON STATION AND READY FOR CALL FOR FIRE"**; a status report "may be sent" covering ammunition types/quantities available, and "may be requested by the observer." No fixed element order or readback — it isn't scripted like the CFF. For land units this is handled off-net (FIST/FSO liaison, unit SOP) before the observer ever keys up. ⚠ Recommendation: borrow the NGF shape (one-time, observer-requested, one FDC reply, no readback — round count/location/tubes/munitions) since it's the only sourced analog; there's no land-arty script to be more "correct" to.

**G13 — effects criteria and volume of fire.** DOCTRINE.md's suppressed/neutralized(≥10%)/destroyed(≥30%) figures match **FM 6-30 (1991) §4-14 and App. E-2** word for word ("10 percent or more casualties will neutralize a unit," "30 percent... normally renders a unit ineffective"); current **ATP 3-09.30 (2017)** doesn't restate the percentages but doesn't contradict them — no source found gives different numbers. Volume of fire: **FM 7-90 (Tactical Employment of Mortars) App. B** is the one unclassified source that tabulates rounds-to-effect — a 60mm section firing **5 rounds/tube** at a **standing** platoon gets ~20% casualties (clears neutralize); the same volley at the same target **prone** gets <10% (does not) — posture alone swings the verdict. No equivalent table exists for 155mm; the real JMEM/GMET tables are classified (stated explicitly in FM 6-30 §4-14e and FM 7-90 App. B). Continuing after suppression-only: no text explicitly grants or forbids it — suppression is written as a legitimate, deliberately-chosen low-cost intent, not a failure state, and RREMS's surveillance term is the observer's own post-fires call, not a gate — so nothing in doctrine blocks the sim from offering continue-or-end.

**Implemented `e3a35d6`.** Binary `effectRadius`/`hitsToNeutralize` replaced by a graded model (`CONFIG.EFFECTS`): per-round casualty/damage % banded by miss distance — full ≤30 m / half ≤50 m / quarter ≤75 m for personnel, 15/25/45 m for point targets (wreck, bunker) — posture-scaled per the FM 7-90 App. B finding above (a near-miss now flips the target PRONE instead of shrinking a radius), accumulating into SUPPRESSED / NEUTRALIZED ≥10% / DESTROYED ≥30% per FM 6-30 §4-14. Continue-after-suppression is built exactly as scoped: after ROUNDS COMPLETE the mission only closes if accomplished, else corrections re-open the shoot and REPEAT re-fires the volley, with effect accumulating across volleys. Suppressed-only PASSES a suppress-intent mission (immediate suppression, suppress-target); it is a 1★ MARGINAL result on a destroy-intent mission. Verified 19/19 numeric harness + 15/15 live scripted mission; replay over 247 recorded transmissions shows zero parser-classification regressions from the change.

**G15 — sheaf.** Verbatim-consistent across **FM 6-30 §4-6.f (1991)** and **ATP 3-09.30 §4-45 (2017)**, 26 years apart:
- **Converged** — every piece lands on the same point; small hard targets (bunker, gun pit).
- **Open** — bursts one effective-burst-width apart, perpendicular to the gun-target line; personnel targets, or TLE too poor for a tight sheaf.
- **Parallel** — every tube fires identical data; the **mortar computer's default** for a linear target.
- **Special** (linear/rectangular/circular/irregular) — any length/width; requires **attitude** (long-axis azimuth) when length is given.
- **Default if unrequested**: circular, 100 m radius (both editions).
Requested by name in the method-of-engagement segment; cancelled with **"CANCEL CONVERGED [or OPEN] SHEAF"** (ATP 3-09.30 §5-30). Convoy → linear/special along the road; bunker → converged.

**G16 — fuze.** Current codified list (**ATP 3-09.30 §4-43**): **PD** (point detonating — the old "fuze quick"), **VT** (variable time/proximity), **MT**/**ET** (mechanical/electronic time), **MTSQ**, **DELAY**, **MOF** (multi-option, mortars only). Target guidance, sourced from **FM 6-30 §4-15**, corroborated for mortars by **FM 7-90 App. B**:
- **Quick/PD** — standing or prone personnel in the open, unarmored vehicles, light materiel; loses effect vs. trenches, dug-in troops, earthworks.
- **Delay** — dense woods, light earthworks/frame buildings, unarmored vehicles (ricochet/penetration).
- **Time/VT** — troops in the open, in trenches, in deep foxholes, soft-skin vehicles (airburst); VT preferred over time — self-adjusts height of burst, works high-angle and aerial-observer.
- **Concrete-piercing/heavy delay** — bunkers, hardened overhead cover.
Requested by name in method-of-engagement ("SHELL HE, FUZE VT, OVER"); FUZE TIME may also be called mid-adjustment, after range/deviation but before FIRE FOR EFFECT (ATP 3-09.30 §5-55).

**G18 — per-asset effects.** Two different numbers — don't conflate:
- **Risk-estimate distances (RED, safety, not effect)** — **JFIRE Dec 2007 App. F, Tables 29–30** (predecessor to the 2019 edition DOCTRINE.md cites; ⚠ not re-verified against the 2019 text, but its "danger close is usually 600 m for cannon and mortars" line matches this file exactly, so the tables are likely stable): **60mm mortar (M224)** 0.1% PI = 100/150/175 m, 10% PI = 60/65/65 m (1/3, 2/3, max range); **155mm Howitzer HE (M109/M198/M777)** 0.1% PI = 200/280/450 m, 10% PI = 100/100/125 m.
- **Effect-on-target radii** — the real JMEM tables are classified (FM 6-30 §4-14e, FM 7-90 App. B). Best unclassified proxies: 60mm mortar **suppression** radius (FM 7-90 App. B) — within 20 m, target "probably suppressed, if not hit"; within 35 m, 50% chance; beyond 50 m, little effect (neutralize/destroy for 60mm: see G13's rounds/posture figures above — no separate radius given). ⚠ For 155mm, no primary-sourced lethal radius was found; a non-primary forum compilation (citing the classified JMEM/FM 6-141-2, unverifiable) claims ~971–1240 m² lethal area for M107 (≈18–20 m radius) and ~2136 m² for M795 (≈26 m radius) against troops in the open — record only as a rough placeholder, not doctrine. A sourced-if-indirect alternative: **ATP 3-09.30 Table 1-1** FPF beaten-zone widths — 60mm (2 tubes) 60×30 m, 155mm (2 tubes) 100×50 m — built from doctrine's own working definition of "lethal" (§1-32): bursting diameter = twice the distance at which a round reliably places one lethal fragment per m².

**Implemented `13084ef`.** `CONFIG.EFFECTS` now carries a band set per firing asset (arty vs. mortar60), selected in `effBands()` by the chapter's asset — a 60mm round is no longer scored on the 155's numbers. **155/arty** keeps the G13 figures unchanged: personnel full/half/quarter at 30/50/75 m, 8%/round. **60mm mortar** personnel bands are FM 7-90 App. B verbatim — full effect ≤20 m, half to 35 m, little effect beyond 50 m — with perRound set to 2% specifically so the manual's own check case reproduces exactly: a section volley of 10 rounds on a **standing** platoon assesses 20% (clears neutralize), the same volley **prone** assesses 8% (suppression only, per §4-14's 10% threshold). 60mm point-target (bunker/wreck) bands are 10/18/35 m at 25%/round — four direct hits to destroy. Chapter 4.2 (TEN METERS) dropped its legacy `scn:{effR:30}` knob — the mortar band set is the precision constraint that a scaled radius was faking; `effScale` survives for future chapters. Verified: numeric harness extended to 25/25 against the shipped artifact (both assets, the FM 7-90 check case, band edges, asset fallback); 15/15 live end-to-end scripted mission unchanged on the arty path; boots clean offline.

## What the sim deliberately ignores ⚠

- **Angle T / gun-target line** effects — corrections apply perfectly in the OT frame (per SPEC ballistics).
- Sheaf choice, trajectory, volume-of-fire math — accepted in a call, but they don't change the model.
- Authentication challenges after readback — could be a Volume IV strict-mode flavor beat, not a mechanic.
