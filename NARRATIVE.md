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
| 2.5 | **Flares Out** | Night defense — **coordinated illumination** (JFIRE Table 14): get an illumination mission up, walk the light over the attackers in coarse steps, then "SHELL HE, OVER" and adjust steel under your own flare, re-illuminating as it dies. The first chapter deliberately built around a *sequence* of fire missions. Fighting it dark on thermal completes but caps at 2★ — the skill under test is the light. *Implemented 2026-07-31 (`TEMPO4`).* |

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
| 3.6 | **Send It Now** | Immediate suppression — the one-transmission call ("IMMEDIATE SUPPRESSION, grid, DANGER CLOSE, OUT"): a strongpoint minutes from being overrun, no time for the three-transmission minuet. A deliberate call still completes but caps at 2★ — speed is the skill under test. *Implemented 2026-07-31 (`TEMPO2`).* |

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

*E.1 is built as specified above — chapter-only `chow` scenario, COOKS as a no-fire line, the flock
inherently danger close, doctrine enforced in full. Shipped `57532a5`; see ROADMAP.md `11a`.*

*E.2 is built as specified above — chapter-only `kaiju` scenario, the crab wading ashore on a
convoy-style path at 2.1 m/s, landfall a fail-and-end verdict ("FAIL — LANDFALL"), and a
DESTROYED-only kill rule (armor divides every round's contribution, and no "combat-ineffective" out
exists for it) so the fight can't resolve as merely stopping a still-standing crab. Battalion rules it
"a surface target and therefore yours"; HELLHOUND declines to react to any of it. Shipped `1ed1dcd`;
see ROADMAP.md `11b`.*

*E.3 is built as specified above — the call for fire goes to SUNLAMP ACTUAL, an orbital
directed-energy cannon, with every doctrinal element intact: six elements, three transmissions,
readback, OT-frame corrections, `impact = aimpoint + error`. The beam is presentation on a point the
ordinary machinery already resolved, which is the whole joke of the Epilogue applied to its own
finale. `'sunlamp'` joins G17's delivery-time callsign swap, so SUNLAMP ACTUAL owns all FDC traffic on
the chapter, quip pools included; prowords swap SHOT→DISCHARGE and SPLASH→SOLAR EVENT; TOF plays as a
WebAudio charging whine ramping for exactly the time of flight rather than a silent wait; impact is one
pooled additive beam column of noon-colored light. Dispersion tightens to `assetScale` 0.22 —
orbital-precision follow-up rounds. SUNLAMP ACTUAL cheerfully requests a ten-digit grid and gets one
honored to the meter, with 8- and 6-digit grids still accepted, just with corporate grace instead of a
challenge — the corporate-orbital MTO wrapper (ONE APERTURE IN ADJUST, FULL ARRAY IN EFFECT, DIRECTED
ENERGY, AA7003) sits on the same non-negotiable target-number check and readback scorer as any other
MTO. Fought on a bunker complex, with HELLHOUND patched into the net one last time and audibly tired.
Shipped `98d5f16`; see ROADMAP.md `11c`. **All three SUNBURN chapters are now live — the Epilogue's
content is complete.***

## VOLUME V — ON WINGS *(FUTURE — locked tease, do not build yet)*

Close air support: 9-line briefs, talk-ons, attack geometry. Appears on the shelf as a locked spine — "VOLUME V: ON WINGS — awaiting aircraft." Built only after the surface-to-surface campaign is complete.

---

## Callers & guest voices

This is the character and voice authority for **ROADMAP.md `NET1`** — friendly ground units who call MUSTANG 12 direct, asking for fire. Everything below is new infrastructure for future chapters. It does not touch a single line of the Foreword-through-Epilogue campaign above, which stays exactly as built.

### The concept

Every mission so far hands the observer a clean brief: a target already identified, a location already known. `NET1` breaks that. A friendly unit in a fight — a **rock eater**, the user's word for the grunts on the ground, and the one this doc keeps — keys up on a second net and asks for help the way a scared or bored soldier actually would. No grid. No warning order. No prowords. Something like:

> "There's a machine gun in the treeline messing us up, can you drop something on it."

That's the whole call. No six elements, no three transmissions, nothing MUSTANG 12 could forward to the FDC as-is. The observer's job is the real skill this trainer has been building toward all along: take that plain-language report, work it against the map and the terrain in front of them — which treeline, which direction, how far — and *build* a proper call for fire from it. Only then does the observer key up to HELLHOUND (or HACKSAW, or whichever FDC owns the net that chapter) and send the doctrinal call. **The caller leg is never doctrinal. The FDC leg always is. The readback stays sacred either way** — this doesn't loosen anything downstream of the observer's own transmission.

Two nets, two registers. The rock eater talks like a person with a problem. The FDC talks like a fire direction center. The observer is the translator standing between them, and translating badly is exactly the failure this teaches.

### The wrongness mechanic

A caller can be wrong, and the sim should let them be. Fire support has always run on secondhand reports from scared, moving, imperfectly-oriented people, and catching a bad report is as much the job as reading a map. Three failure shapes to build toward:

- **Misidentified landmark.** The caller says "the barn" and means the shed, or calls a treeline a wood line when it's a hedgerow, or names a terrain feature that doesn't match anything on the observer's own sheet.
- **Bad cardinal direction.** "They're firing from the east side" when the caller is turned around and it's actually west — panic and unfamiliar ground do this to real people.
- **Distance wildly off.** "Fifty meters out" when the OP's own optics make it closer to four hundred. Under fire, distance estimation is often the first thing to go.

None of this is the caller lying — it's the caller being a scared or rushed human on a radio, which is the entire point. The observer is expected to **weigh the caller's report against what their own eyes, binos, and map actually show**, not transcribe it uncritically into a grid. A caller's report is a lead, not a target location.

### Caller archetypes

Four voices, distinct enough to be recognizable a line in. None of them know CFF format. None of them are trying to sound military-manual — they sound like people who have a radio and a problem.

**RAZOR 3 — team leader, in contact.** Young, scared, talking fast because slowing down means thinking about what's happening. Fixates on some small stupid detail mid-firefight, the way people actually do when their brain needs somewhere else to go.
- "RAZOR 3, we are getting FUCKED UP by a machine gun in the treeline, I need you to do something about that right now."
- "It's — okay it's in the trees, the trees past the field, I don't know how far, it's far, just make it stop."
- "Doc's working on Guerrero, he's gonna be pissed he missed this, he already called dibs on the good rack back at the FOB —"
- "Did it move? I don't — wait, yeah, it's still firing, same spot, same spot, GO."
- "I don't know what a grid is, man, I just need the trees to stop shooting at us."
- "Copy — wait, copy what? Just shoot it, over, or whatever you say, OVER."

**GATOR 6 — convoy escort NCO.** Staff sergeant, bored out of his skull most days, unbothered even when it's not boring. Talks like he's ordering a sandwich. Dry, deadpan, gallows humor as a resting state rather than a reaction.
- "GATOR 6. We got a truck full of very angry people shooting at us from that ridge on our nine o'clock. Whenever's convenient."
- "Nobody's dead yet, so no rush, but I'd appreciate it before that changes."
- "Ridge with the dead tree on it. Not the other ridge. The one with the tree."
- "Funny thing, I was just telling the driver this route never has any trouble. Universe has a sense of humor."
- "We can keep driving through it if your guys are busy. Just checking in. Would prefer not to, is all."
- "Copy whatever you're about to say. Just make the ridge quiet, I've got a schedule to keep."

**Second Lieutenant PURDY, callsign IRON 2 — over-formal, clueless, trying so hard.** Fresh out of the schoolhouse, uses the biggest words he half-remembers, gets the vocabulary confidently wrong. Eager in a way that's almost worse than useless.
- "IRON 2, be advised, we are experiencing significant enemy contact from a, ah, fortified emplacement, over."
- "Requesting immediate — is it immediate? I want it to be immediate — requesting fire support on the, uh, the structure. The one with the roof."
- "Copy your last, I think. Say again what a grid is, over. No, belay that, don't say again, just proceed."
- "This is IRON 2, danger close, I mean — is it danger close? My guys are close to it. Use your judgment, over."
- "The enemy element is approximately, uh — a significant distance. I'll get back to you on the exact figure."
- "Solid copy, HELLHOUND — sorry, MUSTANG — solid copy either way, out. Out. Over and out."

**Sergeant TRAN, callsign SPADE 4 — laconic, answers in fragments.** Says the minimum, means all of it. No wasted words, no wasted feeling, and somehow the calmest voice on either net.
- "SPADE 4. Gun. Treeline. Ours are pinned."
- "Negative on grid. Don't have one. You got eyes, I don't."
- "Two hundred out. Maybe less."
- "Wrong tree line. Try the next one over."
- "Good hit."
- "Still there. Do it again."

### Tone

Reference points: **WARFARE (2025, Mendoza/Garland)** and **Generation Kill** — a movie and a miniseries both about ground troops on the radio, not war movies with quips. Crude, unpolished, and funny the way real grunts are funny: dark, deadpan, a guy noting the weirdest detail in the room three seconds after almost dying. **Not sitcom banter, not one-liners traded back and forth** — the humor is a coping reflex, not a script.

Profanity is expected and welcome inside the standing content boundary: **nobody on this net takes God's name in vain** ("goddamn," "God help," and the like are out, same rule that governs HELLHOUND — see CLAUDE.md and DIALOGUE_REVISIONS.md §2a). Ordinary profanity — hell, damn, fuck, shit — is fine and should sound native to these characters, not bolted on.

### Humor placement

The existing rules (see **Humor rules** below) govern the callers exactly as they govern everyone else on the net: sprinkle, don't soak — a caller earns one dark or absurd beat per exchange, not a routine. **A joke never costs the caller's tactical information.** RAZOR 3 can rant about Guerrero's rack while still delivering "machine gun, treeline, far" somewhere in the transmission — the humor rides alongside the report, the same way HELLHOUND's quips ride alongside the readback, never in place of the thing the observer actually needs to extract.

### Guest voices — the umbrella

"Callers" are one category under a wider umbrella: any voice on the net that isn't MUSTANG 12 or that chapter's FDC. The rock eaters above are the first and only guest voices this section defines. The umbrella exists so the *next* one has a named home instead of landing as an orphan character with nowhere documented to live — which is exactly the problem that parked `14c`.

**Reserved slot: LIBERTY FIRES**, the guest FDC character proposed in DIALOGUE_REVISIONS.md §8 (a rear-echelon volunteer battery covering the net for one Epilogue chapter while HELLHOUND's crew is elsewhere). That character is **not** written here and stays parked exactly as ROADMAP.md has it — this section only names the slot so that when the user unparks `14c`, it has a place to go without another blocked row.

### What the sim implements — the contract

For whoever picks up `NET1` in `src/`:

1. **Caller traffic is a distinct message class, never fed to the CFF parser.** A rock eater's transmission never produces an MTO, a readback, or a strict-mode challenge — it's a plain-language report that triggers a target-location task for the observer, full stop.
2. **The wrongness mechanic is a real, gradeable condition**, not flavor text: a caller report can carry a misidentified landmark, a flipped cardinal direction, or a distance off by a wide margin (see above), and the mission must be checkable against what the observer's own optics/map/terrain association actually show — a caller being wrong is not the observer being told the answer is wrong first.
3. **A mission born from a caller request still grades on the existing star rubric** (rounds-to-effect, first-round accuracy, format, time vs. par) — the caller doesn't create a second grading system, it just adds a location-resolution step before the CFF the observer already knows how to build. Blindly copying a wrong caller report into a grid is graded the same as any other bad grid: on what the observer actually sent.
4. **The FDC leg is untouched.** Whatever chapter this lands in, HELLHOUND/HACKSAW/whoever is on that net keeps full doctrine enforcement, full readback, full rant-vs-snide behavior — this row only adds a second, non-doctrinal voice in front of the existing one.
5. **Fratricide and collateral-damage rules are unchanged and apply regardless of caller error** — a caller sending the observer toward a bad grid does not excuse the observer for firing on it; the auto-fail rules in CLAUDE.md govern the round that lands, not the report that preceded it.

---

## Humor rules

1. **The readback is sacred.** Jokes ride alongside doctrinal traffic, never replace it. (Existing CLAUDE.md rule; the campaign inherits it.)
2. **Sprinkle, don't soak.** Story chapters get at most one scripted gag plus the existing quip pools; the Epilogue is where the dial goes to 11.
3. **Continuity is the punchline.** The best jokes are HELLHOUND remembering the player's history. Track a few counters (fratricides, worst first-round miss, say-agains) and let quips reference them.
4. **The absurd is treated as routine.** Nobody in the fiction ever remarks that the crab is impossible. The paperwork is filed correctly.
5. **The player is the butt of the joke only when they've earned it** — wrong calls, unsafe calls, slow calls. Competence gets grudging respect.
6. **Rant vs. snide is a dial, not a switch.** HELLHOUND's script is a guideline, flexible not rigid. A **dangerous** deviation (danger close skipped, a correction walking fire toward friendlies or the village) earns the full rant. A **stupid-but-safe** deviation (wrong element order, absurd rounding, a malformed-but-unambiguous call) earns one snide line and the mission moves on. The doctrinal readback happens either way.
