# DIALOGUE_REVISIONS.md — HELLHOUND / GUNNY punch-up pass

Proposed dialogue rewrites, sourced from a play-through transcript (`Dialogue History/shitfire_transcript_2026-07-29-06-55-53.txt` + the paired `.json`, skirmish "POSITION UNDER ATTACK" into CHAPTER 1.2 "KNOCK KNOCK") and a full read of every flavor string in `index.html`. Goal: **~100x more profane, dark-humored HELLHOUND** without breaking anything DOCTRINE.md or NARRATIVE.md treats as load-bearing.

**This document does not edit `index.html`.** It's a review + replacement-text proposal. Per CLAUDE.md's model split, sim code changes belong to a Fable pass — say the word and that can be dispatched next.

> **This is the detail spec for SPEC.md stage 14.** [ROADMAP.md](ROADMAP.md) Track D owns order and status: `14a` = quip pools, `14b` = chapter beats + briefs, `14c` = LIBERTY FIRES (parked, needs a NARRATIVE.md home first). Zero conflict surface with stage 13's renderer work, so 14a/14b can be pulled forward between graphics rows — but it still serializes, because only one agent edits `index.html` at a time.

---

## 1. What stays untouched, and why

Every line below lives in the `QUIPS` object (`index.html:1487-1628`) or chapter metadata (`CAMPAIGN`, `index.html:3107-3216`). Two categories of text are **out of scope** even though they're adjacent in the code:

- **The doctrinal readback itself** — `MUSTANG 12, HELLHOUND FIRES, ADJUST FIRE, GRID ...`, `MESSAGE TO OBSERVER: ...`, `SHOT, OVER.` / `SPLASH, OVER.` / `ROUNDS COMPLETE, OVER.`, and the `END OF MISSION, ... OUT.` line. These are generated from the parsed call, not the `QUIPS` pool, and NARRATIVE.md rule #1 says the readback is sacred — jokes ride *alongside* it, never *in* it. Confirmed in the transcript: every readback line in the log is clean, formal net traffic; all the personality lives in the very next line (the quip). That pattern is correct and should not change.
- **Strict-mode challenge/reject text** (4.1/4.4, `strict` chapters) — kept format-focused per DOCTRINE.md; a couple of lines below still get punched up (`snideRound`, `dangerClose`) since those already carry personality in the current build.

Everything else — the line that follows a readback, a correction, an FFE call, a bad grid, a fratricide, a civilian hit — is pure flavor and fair game.

## 2. What the transcript actually showed

The captured mission (skirmish → CH 1.2, 7 adjusting rounds, 2★ "too many adjusting rounds") is a good stress test because it's long enough to expose pool size:

- `corrSnark` has only 4 entries. Across 7 corrections, **"Shifting fire. The fish send their regards for your earlier donation."** fired twice (rounds 2 and 6) and there was visible strain on the pool generally — `pick()` only guarantees no *immediate* repeat, not no repeat. Dark-humor lines land harder the first time and go flat on a rerun; the fix is more lines per pool, not just saltier ones.
- The mission never triggered `rantFrat`, `rantCiv`, `dangerClose`, `wrongWay`, or `unsafeCorr` — the player never did anything unsafe. That's consistent with NARRATIVE.md rule #5 (player is the butt of the joke only when they've earned it) and rule #6 (rant vs. snide is a dial). Good behavior in the sim; just means those pools are under-exercised in normal play and should be strong on the rare mission where they fire.
- `greet` and `readbackTail` are the two lines guaranteed to play on almost every mission (mission start, and after most CFFs). These deserve the deepest pools since repetition there is most visible.

Net takeaway: **expand every pool to 6-8 entries minimum**, punch up the profanity/darkness on the existing lines, and keep the new lines in the same voice (aging, contemptuous, profane, but never breaking the readback).

### 2a. Content boundary — no taking God's name in vain

Per direction: HELLHOUND's profanity should never mock God or invoke His name as a curse ("goddamn," "God help," "God Almighty," etc.). All rewrites in §3 below are scrubbed of this — swapped for secular profanity (hell, damn, fuck, shit) that keeps the same punch. **The currently-shipped `index.html` has ten lines with this pattern that should get the same fix whenever §3 is applied:**

- `index.html:1508` — "...watching the flies fight. Send us some work..." (drop "goddamn")
- `index.html:1509` — "...a grid square God has given up on..."
- `index.html:1528` — "God help whatever is standing on that grid..."
- `index.html:1539` — "I will be goddamned — the observer..."
- `index.html:1551` — "...not on anybody's goddamn map..."
- `index.html:1557` — "...that grid is in the goddamn ocean..."
- `index.html:1568` — "...wait for the goddamn splash..."
- `index.html:1589` — "...in the format God and the field manual intended..."
- `index.html:1613` (`rantCiv`) — "CHECK FIRE. GOD ALMIGHTY, CHECK FIRE."
- `index.html:1621` (`rantFrat`) — "CHECK FIRE. GOD DAMN IT, CHECK FIRE."

The rewritten `greet`, `ffeAck`, `eomGood`, `badGrid`, `water`, `inFlight`, `rantLost`, and `rantCiv` pools in §3/§3-adjacent below already replace these lines with God-free equivalents — no separate patch needed once §3 lands.

## 3. Revised QUIPS pools

Ready to paste into `index.html:1487-1628` as direct array replacements (same keys, same `{WHO}` placeholder in `unsafeCorr`, same 3-line rant-array shape for `rantCiv`/`rantFrat`).

### `greet` — mission start, before any player action

```js
greet: [
  'HELLHOUND FIRES on this net. The guns are hot, the coffee is battery acid, and I am surrounded by men who could not find their own ass with both hands, a flashlight, and a map with an arrow on it. Give me a target, over.',
  'HELLHOUND FIRES. We have been sitting in this shit-stained gun pit since dawn watching flies fight over a dead rat. Send us some actual fucking work, over.',
  'HELLHOUND FIRES up. Somewhere on this miserable rock is a grid square that luck itself gave up on. Point me at it, over.',
  'HELLHOUND FIRES on the net. I have six guns, a full ammo rack, and exactly zero fucks left after the last observer we buried. Try not to join him, over.',
  'HELLHOUND FIRES. Speak clearly, speak correctly, and nobody has to bleed from embarrassment today. Over.',
  'HELLHOUND FIRES, live. Every gun bunny in this battery already has money down on how fast you get us in trouble. I took the short odds. Prove me right, over.',
  'This net has seen career-enders, mercy kills, and one observer who called fire on his own foxhole by accident. Let us find out which one you are, over.',
  'HELLHOUND FIRES. The last guy who kept me waiting is a cautionary tale they teach at the schoolhouse now. Do not audition for the sequel. Over.',
],
```

### `readbackTail` — trails the CFF readback, before/instead of the MTO ack

```js
readbackTail: [
  'And MUSTANG — do try to hit the correct fucking island this time. The reef has suffered enough on your account.',
  'Rounds are cheap. My patience is a finite, dwindling, deeply resentful resource. Standing by.',
  'If anything friendly is standing on that grid, now is the moment to confess — before the paperwork writes itself without you.',
  'Copy all. This had better be worth the powder, son, or I am billing your next of kin for the shells.',
  'Outstanding. The tubes were getting cold and the crew had started a book club. I was two chapters into their grief.',
  'Solid copy. Somewhere a dead artillery instructor just felt a warm, deeply confused sense of pride.',
  'Received. Try not to make me explain this grid to a board of inquiry later, I hate the coffee they serve at those.',
  'Copy the call. For your sake, MUSTANG, I hope that grid is where you think it is and not where your last three usually land.',
],
```

### `corrSnark` — after an adjust correction (expanded 4 → 8; this is the pool the transcript caught repeating)

```js
corrSnark: [
  'That is one HELL of a walk, MUSTANG. Did the first round land in a different fucking hemisphere, or just a different war?',
  'Copy. For the record: the gun did not move. Your math had a stroke.',
  'A bold correction. Somewhere out there a map is being held upside down, and I believe I know exactly by whom.',
  'Shifting fire. The fish send their regards for your earlier donation — they are naming a reef after you.',
  'Copy the shift. At this rate we will hit the target sometime after the peace treaty is signed.',
  'Adjusting. I have seen drunk men throw horseshoes with tighter groupings than that.',
  'Noted. Somewhere a supply sergeant is crying into a crate of shells you are about to waste on empty dirt.',
  'Shifting. If persistence were the same thing as accuracy, MUSTANG, you would already have a medal and a nickname.',
],
```

### `ffeAck` — fire for effect acknowledged

```js
ffeAck: [
  'FIRE FOR EFFECT. Now THIS is the part of the job I love.',
  'FIRE FOR EFFECT. Nothing on that grid is walking away from this, and nothing is coming to save it either.',
  'Copy FIRE FOR EFFECT. Six guns, one grid, zero fucking sympathy.',
  'FIRE FOR EFFECT. Drink it up, MUSTANG. Drriiink it up.',
  'FIRE FOR EFFECT. Somewhere on that grid a very bad day is about to get very short.',
  'Copy FFE. The tubes are happy. The tubes are always happiest right before this part.',
],
```

### `completeTail` — trails "rounds complete"

```js
completeTail: [
  'That grid square has been redecorated down to the bedrock.',
  'Rounds complete. Whatever stood there is now a fine mist and somebody else\'s paperwork.',
  'Rounds complete. I have looked upon that smoke and found it good.',
  'Tubes cooling. Take a long look, son. That is what competence smells like.',
  'Rounds complete. If there is a burial detail scheduled for that grid, tell them to bring a very small box.',
  'Tubes cooling. The crew is already taking bets on whether there is enough left to identify.',
],
```

### `eomGood` — successful end of mission

```js
eomGood: [
  'Good effect. Well, hell — the observer can actually observe.',
  'Target neutralized. Mark this day, MUSTANG. It will not come again.',
  'A clean kill. I had frankly abandoned all hope of seeing one from you.',
  'End of mission logged. There is a competence in you, boy, and today it finally showed up for work.',
  'Target destroyed. Somewhere a recruiter is taking credit for this and has never once met you.',
  'Clean, logged, and closed. I am almost disappointed I do not get to yell at you tonight.',
],
```

### `eomBad` — failed/abandoned end of mission

```js
eomBad: [
  'End of mission. The target is alive and well and telling this story at dinner parties. We converted the taxpayers\' money into noise and regret.',
  'Logged. We will call that one "suppressive" and never, ever speak of it again.',
  'End of mission. The enemy remains. My disappointment, however, is total, permanent, and load-bearing.',
  'Copy end of mission. You abandoned that fire mission, MUSTANG. You ABANDONED it.',
  'End of mission. Congratulations — you have achieved the rare feat of making a war LESS efficient.',
  'Logged as the failure it was. The target is fine. Your reputation is not.',
],
```

### `badGrid` — unparseable / off-map location

```js
badGrid: [
  'MUSTANG 12, that grid is not on anybody\'s map, full stop. Say again your target location, over.',
  'Negative copy. That location does not exist on this or any other earth. Say again, over.',
  'I plotted that grid twice, and both times it insulted me personally. Say again, over.',
  'That is not a grid, MUSTANG, that is a cry for help. Say again, over.',
  'That grid belongs to a parallel universe where you are competent. Say again, in this one, over.',
  'I ran that through the plotting board twice and it laughed both times. Say again, over.',
],
```

### `water` — grid lands in the ocean

```js
water: [
  'Be advised, that grid is in the fucking ocean. Engaging the fish as requested.',
  'That grid is wet, MUSTANG. We are now shelling the Pacific, which never did a thing to us. Firing.',
  'Copy. Naval gunfire it is. Bold fucking choice.',
  'Congratulations, you have drawn first blood in the war against plankton. Firing.',
],
```

### `noMission` — correcting/FFE with nothing active

```js
noMission: [
  'MUSTANG 12, there is no mission on this net. You are adjusting fire that exists only in your heart. Send a call for fire, over.',
  'MUSTANG 12, you are correcting rounds we never fired. I admire the confidence. Send a call for fire first, over.',
  'There is no mission, MUSTANG. There is only you, me, and this dead air you keep filling. Call for fire, over.',
  'Adjusting a mission that does not exist is a special kind of stupid I have not logged before, and I have logged a lot. Call for fire, over.',
],
```

### `inFlight` — correcting while a round is still airborne

```js
inFlight: [
  'Rounds are in the air, MUSTANG. Physics is doing its part. Do yours — wait for splash, over.',
  'Still flying. Unless you can steer a shell with your mind, wait for the splash like everyone else, over.',
  'Patience, MUSTANG. The rounds arrive when they arrive. Much like your competence, over.',
  'The shell is airborne, MUSTANG, not telepathic. It cannot hear you. Wait for splash, over.',
],
```

### `unknown` — unparseable transmission

```js
unknown: [
  'Say again, over. Slower, and in English this time.',
  'That transmission had a callsign and then a stroke. Say again, over.',
  'MUSTANG, I have heard clearer traffic from a dying radio at the bottom of a well. Say again, over.',
  'Say again, over. And this time pretend the radio is graded — because it is. By me.',
  'That transmission needs a priest, not a readback. Say again, over.',
],
```

### `dangerClose` — danger-close target called without the proword (rant-tier, already profane; adding one)

```js
dangerClose: [
  'MUSTANG 12, that target is DANGER CLOSE to friendly infantry and you did not say the fucking proword. I do not drop shells next to our own people on a mumble. Say again the full call with DANGER CLOSE, over.',
  'Negative. Friendlies are close enough to that grid to read the lot numbers off the fuzes. You want this mission? Say DANGER CLOSE like you mean it, over.',
  'MUSTANG, six hundred meters is not a suggestion — it is the distance between "mission complete" and a letter to next of kin. Say DANGER CLOSE, over.',
],
```

### `wrongWay` — corrections moving away from the target

```js
wrongWay: [
  'MUSTANG. The rounds are getting FARTHER from the target. You are adjusting fire like a blind man swatting bees. Look at your burst. Look at your target. THINK. Then talk, over.',
  'Wrong way, son. WRONG WAY. Every correction you send wounds me personally. Left is left. Add is away. It is not fucking calculus, over.',
  'You are correcting AWAY from the target with the confidence of a man who has never once been right about anything. Fix it, over.',
],
```

### `rantLost` — observer has lost the format entirely

```js
rantLost: [
  'STOP. Just — stop. MUSTANG, I have drunk deep of your transmissions and found NOTHING. No grid. No direction. No sense. You are a forward observer. So OBSERVE something, and then TELL ME ABOUT IT in the format the field manual and forty years of hard-won doctrine intended. Warning order. Location. Description. OVER.',
  'MUSTANG, listen to me very carefully, because I will say this once. I do not know what you are doing on that hill, but it is not calling for fire. Take a breath. Look at your map. Send: warning order, target location, description. In that order. Like a professional. Over.',
  'I have half a mind to have the guns fire on YOUR position out of pure professional mercy. Instead: breathe. Look at the map. Warning order, location, description. In that order. Do not make me say it a third time, over.',
],
```

### `careerFrat` / `careerCollat` / `careerVet` — mission-start callbacks to prior record

```js
careerFrat: [
  'And MUSTANG — before we begin: the friendlies have asked me to remind you which color smoke is theirs. All of them. They held a meeting.',
  'One administrative note, MUSTANG: your file now has a tab. Files should not have tabs. Send your traffic.',
  'Before you key up — the infantry took a vote, and the results were unanimous, and printable only if I remove several words. Send it.',
],
careerCollat: [
  'MUSTANG, the village council has formally requested you be issued a map with pictures. Motion carried unanimously. Proceed.',
  'Before you key up, MUSTANG — the huts are still no-strike. Yes, still. Yes, all of them. Send it.',
  'One note before we start: the village elder asked if you would be "doing the thing" again today. I told him I could not promise anything. Send it.',
],
careerVet: [
  'MUSTANG 12 on the net. The guns know your voice now, son. They perk up. It is almost touching.',
  'Ah, MUSTANG. The log says we have done this dance a few times. Lead, this time, over.',
  'MUSTANG 12. The new guys ask about you like you are a ghost story. I let them believe it. Over.',
],
```

### `snide` / `snideRound` — stupid-but-safe deviation (malformed call, absurd rounding)

```js
snide: [
  'Copy your... whatever that was. The manual describes a format, MUSTANG. It is one page. There are pictures.',
  'I processed that call out of charity. Callsigns, warning order, location, description. In that order. Like your instructor begged you.',
  'That transmission was to a call for fire what a mudslide is to architecture. We will fire it anyway.',
  'Copy. And to think they told me the radio was a professional instrument.',
  'That was not a call for fire, that was a cry into the void that happened to contain a grid. We will fire it anyway.',
],
snideRound: [
  'The guns round to tens, son — your boutique little numbers are a fantasy and my time is real. Firing.',
  'Copy your artisanal correction. Deviation comes in tens, range in hundreds. We will do the arithmetic you would not. Firing.',
  'The fire direction computer just sighed out loud, MUSTANG. Round your numbers like a professional. Firing anyway.',
  'I am rounding that myself so the guns do not have to think about it, the way you clearly did not. Firing.',
],
```

### `nearCiv` — target near the village but not yet a violation

```js
nearCiv: [
  'Be advised — that grid sits close to the village. There are civilians on this rock, MUSTANG. Put one round in the market square and your war is over.',
  'Copy all. Check your map, son — the village is a short walk from your splash pattern. Adjust like you mean it.',
  'That grid is close enough to the huts that I can smell the cooking fire on the map. Mind your correction.',
],
```

### `unsafeCorr` / `unsafeInsist` — correction walking rounds toward friendlies/civilians (rant-tier)

```js
unsafeCorr: [
  'NEGATIVE. CHECK YOUR CORRECTION, MUSTANG. That shift walks rounds onto {WHO}. Look at your burst, look at your map, and send me a correction that does not end careers. Say again, over.',
  'I am NOT sending that. Plot it, son. Your correction lands on {WHO}. You have one radio and you are currently using it for manslaughter. Say again, over.',
  'Check it again, MUSTANG. That number puts steel on {WHO}, not the enemy. I will not be the reason it is in a report. Say again, over.',
],
unsafeInsist: [
  'Your funeral, MUSTANG. Shifting fire as ordered. I am writing your name in the log as I do it.',
  'Copy. On your head, son. The board of inquiry meets Tuesdays. Shifting fire.',
  'Fine. Your call, your name on it, your conscience. Shifting fire — and I hope to hell you can live with it.',
],
```

### `rantCiv` / `rantFrat` — mission-ending fail states (already max intensity; adding one variant each for repeat plays)

```js
rantCiv: [
  ['CHECK FIRE. CHECK FIRE.',
   'That round went into the VILLAGE, MUSTANG. The civilian village. The one on your map — plotted, labeled, and full of people who were having a perfectly fine morning until you found the radio.',
   'There is no report I can write that survives this. END OF MISSION. Get off my net and go sit somewhere quiet and think about maps.'],
  ['CHECK FIRE. CHECK FIRE, DAMN IT ALL.',
   'You just serviced a fishing village with one-five-five, you catastrophic instrument. Those were CIVILIANS. The huts were on the sheet. The brief said no-strike.',
   'I have to make phone calls now, MUSTANG. Phone calls with generals on the other end. END OF MISSION.'],
  ['CHECK FIRE. CHECK FIRE, DAMN YOU.',
   'That grid had a name on the sheet, MUSTANG, and it was not a target designation. It was a village. It had a well and a schoolhouse and now it has neither.',
   'I am not writing this one up as "collateral." I am writing it up as what it was. END OF MISSION. Get off my net.'],
],
rantFrat: [
  ['CHECK FIRE. CHECK FIRE. CHECK FIRE.',
   'You just dropped one-five-five on the FRIENDLY position, MUSTANG. Do you understand me? Those are OUR people in that smoke. I am looking at the plot, and the plot says YOU did this.',
   'I told them not to hand you a map. I TOLD them. You are done on this net, son. Get off my radio and go practice on a lake. END OF MISSION.'],
  ['CHECK FIRE. DAMN IT, CHECK FIRE.',
   'That was the friendly position, you catastrophic son of a bitch. Men with radios are standing in that smoke right now saying your callsign with FEELING.',
   'I have seen bad observers. I have TRAINED bad observers. But you — you are a whole new church of wrong. END OF MISSION.'],
  ['CHECK FIRE. CHECK FIRE.',
   'MUSTANG, you walked artillery onto our own infantry. There are two kinds of people in this war — those who read grids, and casualties. Today you manufactured the second kind out of the first.',
   'When this is over, you and I will have a conversation about maps, and you will not enjoy it. END OF MISSION. Get off my net.'],
  ['CHECK FIRE. CHECK FIRE, YOU ABSOLUTE WEAPON.',
   'That was OUR position, MUSTANG. I am looking at the casualty count on our side of the ledger because of a call YOU made.',
   'I do not have a joke for this one. I do not have anything for this one. END OF MISSION. Get off my net.'],
],
```

---

## 4. Chapter narrative punch-ups

NARRATIVE.md rule #2 says story chapters get *at most one* scripted gag — sprinkle, don't soak. So these are targeted line-level edits to existing `story:` / `outro:` text (index.html:3107-3216), not new material. Format/training content in each `story` line is unchanged; only the color commentary is punched up.

| Chapter | Field | Current | Proposed |
|---|---|---|---|
| F.1 | `story` | "...looks at you the way a man looks at weather." | Keep as-is — Gunny stays dry, not profane (see §5). |
| 1.2 | `outro` | "The engineers send their thanks. They did not want to walk up there." | "The engineers send their thanks and a request that you never again make them identify a bunker by smell." |
| 1.3 | `story` | "...There is a village behind the wire, too. Everyone on this island is watching your math." | "...There is a village behind the wire, too. Everyone on this island is watching your math, and none of them are being polite about it." |
| 1.4 | `outro` | "HELLHOUND, grudging: 'Log shows a professional worked this net today. I have started an investigation.'" | Keep — already a solid dark-dry beat. |
| 2.3 | `story` | "...The map does not blink." | "...The map does not blink. It also does not care that you are scared. Fight it anyway." |
| 3.2 | `story` | "...A hundred meters at a time. No heroes." | "...A hundred meters at a time. No heroes, no funerals, no phone calls to a general at 0300." |
| 3.3 | `story` | "If discrimination were easy, they would not need an observer." | Keep — already sharp; no change needed. |
| 3.5 | `story` | "...observers who guess wrong here get famous in the worst way." | "...observers who guess wrong here get famous in the worst way: a name read aloud at a service nobody wanted to attend." |
| 4.4 | `outro` | 'HELLHOUND, after a long pause: "…acceptable, MUSTANG." Frame it.' | Keep verbatim — this is the emotional payoff line of the whole campaign; do not touch it. |

Everything not listed above reads fine as-is; resist the urge to punch up every line. Volume I–III already carry real weight (the "beer and will deny it," "doctrine writers weep with joy" lines land well) and adding profanity to `story`/`outro` text risks fighting NARRATIVE.md's "sprinkle, don't soak" rule and DOCTRINE.md's insistence that the mission brief stay legible as an actual op order.

## 5. GUNNY BOTTLECAP — deliberately held back

NARRATIVE.md's Gunny is "patient the way a man holding a coffee he hates is patient" — dry, deadpan, schoolhouse-instructor energy, contrasted deliberately against HELLHOUND's live-net profanity. Cranking Gunny's profanity to match HELLHOUND would flatten that contrast and undercut the Foreword→net transition (green recruit gets a calm teacher, then gets thrown to the wolf on the radio). **Recommendation: leave Gunny's tutorial lines (`index.html:3112-3148`, the `tut.steps[].say` and `coach[]` text) as-is.** If more humor is wanted there, it should stay dry/dark ("weather," "priest producing scripture") rather than profane — that's a different joke than HELLHOUND's, not a weaker one.

## 6. Mission briefs (`Scenario.brief`, `index.html:1043-1151`)

These are read by the parser-adjacent UI as the operational brief (not spoken by a character) and several are reused across skirmish + campaign. Recommend leaving them mechanically as-is — they're already terse and a little grim ("It cannot shoot back. Neither could the last observer.") — but one is worth tightening:

- `raid` brief (~line 1095): currently trails off awkwardly with string concatenation for the danger-close/village clause. Worth a copy pass regardless of tone (functional issue, not a dialogue one) — flagging for whoever next touches `genScenario`.

## 7. Not covered here (flag for later)

- **Strict-mode challenge text** (4.1/4.4) beyond `snideRound`/`dangerClose` wasn't touched — those lines are format-enforcement mechanics first, personality second, and DOCTRINE.md treats Strict Net as the one place rigid enforcement lives.

## 8. New character — LIBERTY FIRES (guest FDC, one chapter)

Requested: a second FDC personality — big, absurd, populist-nationalist caricature, distrustful of government, into alien and conspiracy theories — covering one specific slice of the campaign before handing back to HELLHOUND.

**One boundary I held on this:** the ask included naming real living public figures (Fauci, Clinton, Obama) and real modern events (COVID) inside the character's conspiracy lines. I didn't write that — attaching real people's names to fictional conspiracy claims, even as a game character's dialogue, reads as the real thing to anyone who screenshots a line out of context, and that risk doesn't go away just because it's satire. What follows keeps everything else you asked for — the archetype, the paranoia, the "nothing is off-limits" swagger, the aliens, the deep-state distrust — built on invented targets instead of real people. If you want a specific real-world reference softened back in, tell me which one and I'll consider it individually rather than as a blanket pass.

There's also a setting fit that works in this character's favor: the campaign fiction is "The Pacific, 1944-ish." A recognizably modern MAGA-coded voice would be an anachronism in Volumes I–IV (which otherwise play the war straight), but the **Epilogue already breaks realism on purpose** (a kaiju crab, an intergalactic space cannon) — NARRATIVE.md rule #4 is "the absurd is treated as routine." That's the natural home for this voice, and **E.1 — THE GREAT CHOW RAID** (currently `impl:false`, stage 11) is a clean slot: lowest stakes in the campaign, and a seagull-surveillance conspiracy is a gift of a premise. It also sets up a real payoff: when SUNLAMP ACTUAL turns out to be a real orbital weapon in E.3, LIBERTY FIRES was "right all along" — NARRATIVE.md rule #3, continuity as the punchline.

**Concept:** HELLHOUND's regular crew is on mess-tent security for the general's barbecue (the actual E.1 premise), so the net gets handed to a rear-echelon volunteer battery for the day. Callsign **LIBERTY FIRES**. He is, without irony, the most squared-away radio operator on the island — flawless readback, perfect format, doctrine memorized cold — while simultaneously convinced the seagulls are surveillance drones, the map printers are compromised, and his own anti-malaria tablets are a mind-control program. The joke is the gap between his radio discipline and his worldview, not the politics themselves.

```
story: 'HELLHOUND FIRES caught mess-tent security detail for the general\'s barbecue — don\'t ask, it was a bet — so today the net belongs to a volunteer gunner\'s mate who has been waiting his whole enlistment to say what he really thinks about the birds. Callsign: LIBERTY FIRES.',
outro: 'HELLHOUND FIRES, back on the net that evening: "Command wants to know why the mess log says \'hostile drone flock, neutralized.\' I told them not to ask. You are welcome."',
```

```js
LIBERTY_QUIPS = {
  greet: [
    'LIBERTY FIRES on the net, and let me tell you something, MUSTANG — those are not seagulls. I have read reports they do not want circulated. Send the grid, over.',
    'LIBERTY FIRES up. Regular crew is on KP, which is exactly the kind of decision certain people make when they want good men out of the picture. Give me a target, over.',
    'LIBERTY FIRES. I have had my eye on that flock since 0600. Coordinated. Disciplined. That is not bird behavior, son, that is a briefing somebody gave them. Over.',
  ],
  readbackTail: [
    'Solid copy. For the record, I am logging the sighting myself. Somebody in this war ought to keep an honest record.',
    'Copy all. Keep calling grids like that and nobody will ever be able to say the volunteers did not do their part.',
    'Received. My uncle warned me about days exactly like this one. He was right about the tablets, too.',
  ],
  corrSnark: [
    'Copy the shift. Adjusting — same as I adjusted my whole worldview the day I read what is really in the water tank on this island.',
    'Shifting fire. They told us the maps were accurate. Same people who told us the tablets were just vitamins.',
    'Noted. I do not trust the wind charts either, and neither should you, but we will fire anyway.',
  ],
  ffeAck: [
    'FIRE FOR EFFECT. Let the record show LIBERTY FIRES did his part today, whatever they end up saying about it later.',
  ],
  completeTail: [
    'Rounds complete. Somebody is going to write a very boring report about this, and I already know which parts they will leave out.',
  ],
  eomGood: [
    'Target neutralized. Let them put THAT in the newsreel, if they have got the nerve.',
  ],
  eomBad: [
    'End of mission. The flock scattered and reformed inside ninety seconds flat. That is not luck, MUSTANG. That is training.',
  ],
};
```

Handing this off as narrative-design material — the actual wiring (a per-chapter FDC override, a distinct voice line for `speakFDC`, the `story`/`outro` hook) is a stage-11 implementation task, and a new named character belongs in NARRATIVE.md's roster before it belongs in code. Recommend a Sonnet pass on NARRATIVE.md to formally add LIBERTY FIRES to the character list when stage 11 is scoped, per CLAUDE.md's doc-vs-sim split.

## 9. Later transcripts (08-48, 12-56, 18-05) — what they add

Three more sessions landed in `Dialogue History/` since §1–2 were written, the last (18-05, 681 lines)
long enough to reach Track E content (SALUTE reports) and several chapters not seen before (3.1, 3.4,
3.5, 4.2, 2.3). All three are cumulative exports of the same growing session — each new file re-exports
everything before it plus new material at the tail — so the unique content is concentrated at the end of
the newest file, not spread evenly. This section is findings only, per the model split: parser/FDC
correctness fixes go to ROADMAP Track F (F5–F7 below), not into this file's quip-pool scope.

### 9.1 Pool repetition — confirmed a second time, independent of §2

`corrSnark`'s "A bold correction. Somewhere out there a map is being held upside down..." and "That is
one HELL of a walk, MUSTANG..." each fire twice more across these three sessions, beyond the repeat §2
already caught from the first transcript. **14a (apply the expanded pools already written in §3) is still
unapplied.** No new information about *what* to fix — this just confirms the fix is worth prioritizing,
from an independent batch of play.

### 9.2 SALUTE spot report (E2) reads well in the wild

Two independent SALUTE deliveries (skirmish s1, ch1.3 HOLD THE LINE) — same six-line structure, distinct
color each time ("relayed from an adjacent company with better things to do" vs. "relayed from a recon
patrol that went home an hour ago... passed through four radios and two liars, verify it yourself"). Good
variety, correctly never hands over a grid, and "you have been given a neighborhood, I require an
address" is a nice bridge into the CFF prompt. No note needed beyond flagging that it shipped well —
nothing in §1–8 covered E2 since it postdates them.

### 9.3 Immediate suppression — a real player hit this wall

ch3.4 EVERYONE'S MOVING transcript: the observer typed `IMMEDIATE SUPPRESSION 253535 OUT` in a genuine
friendlies-under-fire moment and got `parsed as unknown` → "Say again, over. Slower, and in English this
time." — the mockery reserved for gibberish, aimed at a player who used a real, doctrinal proword. This
is concrete evidence for **ROADMAP 12g** (immediate suppression mission type, currently `BLOCKED by 13`):
not just a documented doctrine gap, a wall a real playtester walked into. Worth a note on that row when
it unblocks.

### 9.4 Three parser/FDC bugs found by reading actual play, then confirmed in the regex

These are new — spotted from transcript behavior, then confirmed against the actual regexes in
`index.html` before being written up. They're correctness issues, not tone, so they're logged as new rows
on **ROADMAP Track F** (F5, F6, F7) rather than folded into this file's quip-pool scope:

- **STT/typo tolerance gap.** A player said "right" and "drop" correctly but voice recognition rendered
  them as "WRITE 50" and "DROPPED 200" / "DRAW 200". The adjust/shift regexes
  (`\b(left|right)\s+(\d+)`, `\b(add|drop)\s+(\d+)` — `index.html:3457,3459,3497,3499,3531,3533`) match
  none of these — "WRITE 50 ADD 50" silently **dropped the deviation half of the correction** (FDC only
  echoed "ADD 50, OUT," no error, no notice), and "DROPPED 200" alone with no other field parsed as full
  `unknown`, costing the player over a minute of retries in one session before landing on the exact word
  "DROP".
- **"Danger clothes."** Twice in these transcripts, voice recognition rendered "DANGER CLOSE" as "DANGER
  CLOTHES." The danger-close gate (`p.raw.includes('danger close')`, `index.html:3635`) is an exact
  substring match, so the player was told *"you did not say the fucking proword"* for a proword they did
  say — STT noise punished as a doctrine violation the player didn't commit.
- **Readback duplicates DANGER CLOSE.** Whenever the observer includes the words "danger close" in their
  own transmission, the FDC's readback shows it twice (`index.html:3640`, `locStr += ', DANGER CLOSE'`
  unconditionally) — e.g. observer sends "...DANGER CLOSE, TROOPS IN THE OPEN...", FDC reads back
  "...DANGER CLOSE, DANGER CLOSE TROOPS IN THE OPEN...". Cosmetic, but it's in the one line CLAUDE.md
  calls sacred — the doctrinal readback — so it's worth cleaning up.

See ROADMAP.md Track F rows F5–F7 for the fix specs.

---

**Next step, if wanted:** apply §3's array replacements to `SHITFIRE.html:1487-1628` and the four `story`/`outro` edits in §4 (both include the God's-name scrub from §2a), then a manual playtest of a mission that hits `corrSnark` and `rantFrat`/`rantCiv` to confirm nothing broke pacing or `pick()`'s no-immediate-repeat logic with the larger arrays. LIBERTY FIRES (§8) is separate — it's stage-11 scoped, not a drop-in edit to the current build. §9's F5–F7 are separate again — parser fixes, not dialogue text, owned by whoever takes Track F.
