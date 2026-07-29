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
- **Polar:** "Direction 1650, distance 2100, down 40, over." — direction to nearest **10 mils**, distance to nearest **100 m**, up/down only if vertical difference ≥ **35 m**, to nearest **5 m**. FDC must know observer's position.
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

## End of mission — RREMS

Final transmission: **R**efinement (final aimpoint shift, nearest 10 m, may be <30 m) → **R**ecord as target (optional) → **E**nd of **M**ission → **S**urveillance:

> "End of mission, target neutralized, estimate 15 casualties, over."

Surveillance terms are precise: **suppressed** (effect lasts only during fires), **neutralized** (out of the fight temporarily, ≥10% casualties/damage), **destroyed** (must be reconstituted/replaced, ≥30%).

## Prowords & vocabulary

**Over / Out / Say again / Correction / Break** · **Message to observer** (FDC's lead-in to the MTO) · **Shot / Splash / Request splash / Rounds complete / Ready / Fire / Repeat** (= fire again, last data — never "repeat" as "say again" on a fire net) · **Check firing / Cease loading** (safety stops) · **Record as target / Target number AB####** · **Grid / Direction / Distance / Left / Right / Add / Drop / Up / Down / Danger close / Fire for effect / End of mission**.

## Doctrinal performance standards (use for star pars)

From JFO evaluation standards (JFO-SSUP-2001, adjust-fire mission): initiate the CFF within **2 minutes** of target identification; initial target location within **200 m** of the true location; announce subsequent corrections within **15 seconds** of the burst; enter fire for effect within **±50 m** of the target using **no more than 3 adjusting rounds**. These map directly onto the star-grading metrics (time-to-CFF par, correction-timing par, first-round accuracy, rounds-to-effect, format correctness) — the sim's own applied pars (`passMaxAdjustRounds: 4`, bonus star at ≤2 rounds; see TLO.md §4) are a deliberately looser design adaptation of this raw 3-round figure, not a literal restatement of it.

## What the sim deliberately ignores ⚠

- **Angle T / gun-target line** effects — corrections apply perfectly in the OT frame (per SPEC ballistics).
- Sheaf choice, trajectory, volume-of-fire math — accepted in a call, but they don't change the model.
- Authentication challenges after readback — could be a Volume IV strict-mode flavor beat, not a mechanic.
