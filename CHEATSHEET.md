# CHEATSHEET.md — FO Pull-Up Reference Card

A condensed, at-a-glance version of `DOCTRINE.md` for a player mid-mission. **DOCTRINE.md remains the authority** — if anything here ever drifts from it, DOCTRINE.md wins and this file gets corrected to match, not the other way around. Written for scanning in a few seconds, not reading start to finish.

**Status:** reference content only — not yet wired into the sim. Planned as an in-sim pull-up sheet (see README backlog item 26); suggested binding **`[H]`** (unclaimed — current keys are Space/Esc/B/L/M/K/P/V/N/Enter), toggled like the map/library overlays, visible without pausing or losing the mission state underneath.

---

## 1. Call for fire — 3 transmissions

| # | Transmission | Contents |
|---|---|---|
| 1 | **Observer ID + warning order** | "HELLHOUND FIRES, this is MUSTANG 12, **adjust fire**, over." (or **fire for effect**) |
| 2 | **Target location** | Grid / polar / shift — see §2 |
| 3 | **Description + engagement + control** | Target description → method of engagement (optional) → method of fire and control (optional, default "when ready") |

FDC reads back **every transmission**. Misspoke? Say **"CORRECTION"**, resend that transmission.

## 2. Location methods — rounding

| Method | Format | Rounding |
|---|---|---|
| **Grid** | "Grid 123 456, over." | 6-digit standard (100 m) |
| **Polar** | "Direction 1650, distance 2100, down 40, over." | Direction nearest 10 mils · distance nearest 100 m · up/down only if ≥35 m, nearest 5 m |
| **Shift** | "Direction 2340, right 110, add 400, up 55, over." | Direction nearest 10 mils · lateral nearest 10 m · range nearest 100 m · vertical nearest 5 m |

Grid missions: send **OT direction** before or with the first correction.

## 3. Corrections — order: deviation → range → HOB

> "Left 30, add 200, over." → final: "Right 10, drop 50, **fire for effect**, over."

- **Deviation:** nearest **10 m**. Minimum HE correction **30 m** — below that, don't bother.
- **Range:** add/drop in **100 m** multiples (min 100; **50 m OK entering FFE**).
- **HOB:** 5 m multiples (airburst only — rare).
- Enter FFE when a **100 m bracket** splits, or effects are observed.

**Bracketing:** successive (800/400/200 → split) · hasty · one-round · **creeping** (danger close only).

## 4. Danger close

Round predicted within **600 m** of friendlies → announce **"danger close."**
Adjust with **creeping fire**: corrections **≤100 m**, walked in from the safe side.

## 5. End of mission — RREMS

**R**efinement (final shift, nearest 10 m, may be <30 m) → **R**ecord as target (optional) → **E**nd of **M**ission → **S**urveillance.

> "End of mission, target neutralized, estimate 15 casualties, over."

| Term | Meaning |
|---|---|
| **Suppressed** | Effect lasts only during fires |
| **Neutralized** | Out of the fight temporarily — ≥10% casualties/damage |
| **Destroyed** | Must be reconstituted/replaced — ≥30% |

## 6. FDC traffic you'll hear

- **Message to observer (MTO)** — opens with "Message to observer": units to fire, changes, rounds/tube, target number. **Read it back.**
- **"SHOT, OVER"** — at firing.
- **"SPLASH, OVER"** — ~5 s before impact.
- **"ROUNDS COMPLETE, OVER"** — FFE done.

## 7. Prowords

**Over · Out · Say again · Correction · Break · Message to observer · Shot · Splash · Request splash · Rounds complete · Ready · Fire · Repeat** (fire again, same data) **· Check firing · Cease loading · Record as target · Target number AB#### · Grid · Direction · Distance · Left · Right · Add · Drop · Up · Down · Danger close · Fire for effect · End of mission**

## 8. Performance pars (this sim)

- CFF initiated within **2 min** of target ID.
- Initial location within **200 m** of truth.
- FFE within **≤4 adjusting rounds** to pass; **≤2** for the bonus star (real JFO standard is ≤3 — this sim runs slightly looser; see `TLO.md` §4).
- Zero fratricide, zero collateral damage — either is an automatic 0★ fail, full stop.
