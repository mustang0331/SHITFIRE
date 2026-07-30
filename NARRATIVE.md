# NARRATIVE.md — SHITFIRE Campaign Storyline

The authority for the campaign's story, structure, tone, and grading presentation. SPEC.md owns the mechanics, DOCTRINE.md owns the call-for-fire formats and protocols each chapter teaches; this file owns what the player is told and why each chapter exists. **Training logic drives the story, never the reverse** — every chapter teaches exactly one new thing (or examines everything taught so far), and the fiction is wrapped around that.

## Premise

**The Pacific, 1944-ish, fictionalized.** You are **MUSTANG 12** — until yesterday a wire-laying lance corporal, today the forward observer for an island-hopping campaign, because the actual FO "fell off the boat" (nobody elaborates). Your only companion on the net is **HELLHOUND FIRES**, a fire direction center voiced like it has been awake since Guadalcanal and personally disappointed by every observer since. HELLHOUND will always give you flawless doctrinal traffic — readback, SHOT, SPLASH, ROUNDS COMPLETE — and will make you pay in contempt for every malformed call it has to drag out of you.

The campaign is presented as a **book series: Volumes with Chapters**, opened by a Foreword (the tutorial) and closed by an Epilogue (the goofy stuff). The through-line: a green observer becomes, chapter by chapter, someone the guns can trust — and then the war gets weird.

### Characters

- **MUSTANG 12** — the player. Never voiced except by the player's own transmissions.
- **HELLHOUND FIRES** — the FDC of the 155 battery. Sardonic, profane, doctrinally perfect. Personality already implemented; the campaign adds *continuity*: quips reference earlier chapters and the player's track record ("MUSTANG 12, HELLHOUND — last time you said grid you hit a reef. Proceed.").
- **HACKSAW FIRES** (Volume IV mortar chapters) — the 60mm mortar section's fire direction. *Implemented (G17, 2026-07-29):* on mortar chapters every FDC line self-identifies as HACKSAW (swapped centrally at delivery in `FDC.say`), the strict net bounces a call addressed to HELLHOUND with "HELLHOUND is the one-five-five battery and they are not on this net," and the chapter intro announces the callsign. Same voice pipeline and, for now, the same personality as HELLHOUND — a distinct voice (a company mortar section is *closer* to the fight: younger, terser, more harried) is stage-14 dialogue work, not a mechanical gap. Name chosen to be phonetically far from HELLHOUND so speech recognition cannot confuse the two.
- **GUNNY BOTTLECAP** (Foreword only) — schoolhouse instructor voice for the tutorial chapters. Patient the way a man holding a coffee he hates is patient. Delivered through the same briefing/hint text pipeline; no new voice tech. *Implemented (stage 10) as an event-driven step engine, guns cold for F.1/F.2.*
- **SUNLAMP ACTUAL** (Epilogue only) — the fire direction "center" of an intergalactic directed-energy space cannon satellite of mass destruction. Speaks in cheerful corporate-orbital jargon. Still demands a proper call for fire.

## Structure, grading, and unlocks

- **Menu theme:** a bookshelf. Volumes are spines/tabs; chapters list title, one-line blurb, and best stars per difficulty. Locked volumes show star requirement. `[N]` random missions live in a separate **SKIRMISH** mode outside the campaign.
- **Stars (original-MW2 style):** each chapter is graded **0–5★** on competency — rounds-to-effect, first-round accuracy, format correctness, time vs. par — with difficulty caps: **Easy ≤ 3★, Normal ≤ 4★, Hard = 5★ possible**. Stars display on the chapter row in the menu. Fratricide = mission fail = 0★.
- **Unlocks:** chapters within a volume unlock in order (completing, not starring); the next volume unlocks at a cumulative star threshold, so replaying for stars matters.
- **Briefings:** each chapter opens with a short brief (text + optional `SpeechSynthesis` read, skippable) and closes with a one-line narrative beat in the AAR. *Implemented (stage 10) as a skippable chapter briefing overlay + narrative AAR outro on pass.*
- Every chapter is a **fixed-seed mission** — reproducible, comparable star runs.

---

## FOREWORD — THE SCHOOLHOUSE *(tutorial — always unlocked)*

Stateside, a training beach, plywood targets. GUNNY BOTTLECAP teaches; guided hint overlays are on; failure is impossible, only slower. *Implemented (stage 10): all three chapters playable end to end.*

| Ch. | Title | Teaches |
|---|---|---|
| F.1 | **Eyes Before Guns** | Look controls, heading in mils, binos `[B]`, reticle, laser `[L]`. Lase three marked objects. |
| F.2 | **The Map Is Not the Territory (But It's Damn Close)** | Map `[M]`, grid reading, plotting, and terrain association — resect and orient off the island's permanent structures and roads (the airfield, the radio mast, the village) plotted with their map symbols; pairs with a printed sheet from the map library `[P]`. |
| F.3 | **Say It Like You Mean It** | The full call for fire — six elements, three transmissions, FDC readback — on a beached derelict landing craft. First MTO, first SHOT/SPLASH, first correction, first FFE, first RREMS end of mission. |

Narrative beat: Gunny signs your qualification sheet without looking up. "Congratulations. You're somebody else's problem now."

## VOLUME I — GREEN AS GRASS *(fundamentals · grid missions · first island)*

A quiet-sector island, mostly secured, "a good place to be bad at your job." Everything is 155mm HE, 6-digit grids.

| Ch. | Title | Scenario / skill |
|---|---|---|
| 1.1 | **Troops in the Open, Brains in the Rear** | Troops in the open; basic grid mission, adjust, FFE. |
| 1.2 | **Knock Knock** | Dug-in bunker; precise initial grid matters. Teaches **successive bracketing** (800/400/200, split to FFE). |
| 1.3 | **Hold the Line** | Position under attack; fire near (not on) a friendly perimeter — a civilian village sits just behind it, an early taste of why precision matters beyond just not hitting your own guys. |
| 1.4 | **Do It Again, Slower** | Timed grid re-engagement; format discipline under a par time. Humor beat: HELLHOUND reads your Foreword scores aloud. |

## VOLUME II — THE RIDGE LINE *(location methods · terrain masking · ridge island)*

Coral ridges and defilade — an island that hides things. Polar and shift-from-known-point enter.

| Ch. | Title | Scenario / skill |
|---|---|---|
| 2.1 | **Numbers on a Compass** | Polar mission: direction + distance off your own position. |
| 2.2 | **Old Friends** | Shift from known point; registered KPs with campaign names (KP BREWERY, KP LATRINE). *Named KPs shipped (BREWERY, LATRINE, SAWMILL, CHAPEL, DERBY, ICEBOX); shown on maps/logs, and the parser accepts "shift BREWERY" as well as "shift known point 1001."* |
| 2.3 | **Defilade Blues** | Crest-masked target — burst heard, not seen; adjust off the map and sound. *Implemented (stage 10): crest-masked from the OP, no LOS, fought off the map.* |
| 2.4 | **The Perimeter** | Position under attack, harder: pick the right location method under pressure. |

*A future chapter teaching mil-relation range estimation and the OT-factor correction workflow (see SPEC.md's COACHING & TRAINING FIDELITY) would belong here, alongside the volume's other measurement-off-the-reticle skills.*

## VOLUME III — THUNDER RUN *(dynamic fires · danger close · friendlies everywhere)*

The big push. Everything on the island is moving, and half of it is friendly.

| Ch. | Title | Scenario / skill |
|---|---|---|
| 3.1 | **Rolling Stock** | Convoy: lead a moving column, time the FFE — or catch it during its seeded 1–3 minute pit stop at a fuel point/ammo depot/airfield instead of chasing it down the coast road. |
| 3.2 | **Close Enough to Smell It** | Danger close (inside 600 m of friendlies): proword required or HELLHOUND challenges the call. Teaches **creeping fire** — corrections of 100 m or less, walked in from the safe side. |
| 3.3 | **Uninvited Guests** | Raiders hitting a civilian village at first light; fire on the raiders without putting a round in the huts. Collateral damage is a fail, same as fratricide, no matter whose side of the fence it lands on. Applies 3.2's creeping fire against a discrimination problem instead of a friendly perimeter. *Implemented (stage 10): raiders placed 130–200 m off a real civilian village, huts no-strike; falls back to a masked-village variant if the OP has no line of sight.* |
| 3.4 | **Everyone's Moving** | Combined-arms assault; shifting target, friendlies advancing. |
| 3.5 | **The Wrong Kind of Famous** | Fratricide-avoidance stress mission — friendlies interleaved with targets. The chapter title is the threat. *Implemented (stage 10): friendlies start inside 520 m and advance through the mission.* |

## VOLUME IV — BLACK SAND *(mastery · strict mode · 60mm precision · final exam)*

A black-sand fortress island. Strict doctrine mode is forced on — this is the Veteran tier. *Per-volume islands are implemented (stage 10): each volume runs its own procedural terrain seed behind `H(x,z)` (Foreword/Vol I: 1337, II: 9021, III: 5150, IV: 66600 with a black-sand palette); a user-loaded DEM always wins, and the world rebuilds on volume change.*

| Ch. | Title | Scenario / skill |
|---|---|---|
| 4.1 | **Strict Net** | Any prior mission type, but doctrine per DOCTRINE.md is enforced: three-transmission structure, MTO readback, OT direction with the first correction, rounding standards, "CORRECTION" proword. Malformed calls get rejected like a real FDC would. *Implemented (stage 10) as STRICT NET: callsigns, warning order, target description, and OVER required or the call is rejected with a doctrinal challenge; corrections must be rounded (deviation tens, range fifties/hundreds); end of mission must carry a surveillance term, one challenge then it passes.* |
| 4.2 | **Ten Meters** | 60mm mortars: 8-digit / 10 m grids, tighter dispersion, precision targets. *Implemented (stage 10): 8-digit grid demanded (6-digit gets a challenge), 0.55x dispersion, section MTO wording, 30 m effect radius.* |
| 4.3 | **No Second Chances** | Hard, timed, one adjusting round budgeted above par. |
| 4.4 | **The Meat Grinder** | Final exam: multi-phase mission chaining grid, polar, shift, danger close, and a moving element. Pass this and HELLHOUND says something almost kind. Almost. *Shipped (stage 10) as a single mission — strict + hard + assault/danger close — not the multi-phase chain described above. True multi-phase chaining is tracked as future polish (see README backlog).* |

## EPILOGUE — SUNBURN *(unlocked by finishing Volume IV · the goofy-ass shit)*

The war is won. The paperwork isn't. Doctrine remains in full effect, which is the joke — every absurd mission gets the complete, deadpan readback.

| Ch. | Title | Bit |
|---|---|---|
| E.1 | **The Great Chow Raid** | Fire mission on the seagull flock assaulting the general's barbecue. No-fire line: the cooks. Fratricide rules apply to Private Dombrowski and his potato salad. |
| E.2 | **CLAWS OUT** | A B-movie kaiju crab wades ashore. It's a moving target. It is also a crab the size of a church. HELLHOUND refuses to acknowledge that anything is unusual. |
| E.3 | **SUNLAMP ACTUAL** | The finale: a call for fire to **SUNLAMP** — an intergalactic space cannon satellite of mass destruction, on loan "from a partner force." Same doctrine, same direct-impact model. TOF is a charging whine from everywhere at once; "SHOT" becomes "DISCHARGE"; "SPLASH" becomes "SOLAR EVENT, OVER"; the beam is a column of noon-colored light. SUNLAMP ACTUAL cheerfully requests a 10-digit grid; HELLHOUND, patched into the net, has never sounded more tired. |

## VOLUME V — ON WINGS *(FUTURE — locked tease, do not build yet)*

Close air support: 9-line briefs, talk-ons, attack geometry. Appears on the shelf as a locked spine — "VOLUME V: ON WINGS — awaiting aircraft." Built only after the surface-to-surface campaign is complete.

---

## Humor rules

1. **The readback is sacred.** Jokes ride alongside doctrinal traffic, never replace it. (Existing CLAUDE.md rule; the campaign inherits it.)
2. **Sprinkle, don't soak.** Story chapters get at most one scripted gag plus the existing quip pools; the Epilogue is where the dial goes to 11.
3. **Continuity is the punchline.** The best jokes are HELLHOUND remembering the player's history. Track a few counters (fratricides, worst first-round miss, say-agains) and let quips reference them.
4. **The absurd is treated as routine.** Nobody in the fiction ever remarks that the crab is impossible. The paperwork is filed correctly.
5. **The player is the butt of the joke only when they've earned it** — wrong calls, unsafe calls, slow calls. Competence gets grudging respect.
6. **Rant vs. snide is a dial, not a switch.** HELLHOUND's script is a guideline, flexible not rigid. A **dangerous** deviation (danger close skipped, a correction walking fire toward friendlies or the village) earns the full rant. A **stupid-but-safe** deviation (wrong element order, absurd rounding, a malformed-but-unambiguous call) earns one snide line and the mission moves on. The doctrinal readback happens either way.
