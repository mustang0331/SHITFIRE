#include "shitfire/doctrine/fdc_quips.h"

#include <cmath>

/* Quip pool data + pick() — port of src/js/07-voice-spot-audio.js:36-297
   (oracle-v1). Text is verbatim from the source (14a punch-up, applied at
   oracle-v1); nothing scored reads it (DETERMINISM_NOTE.md §a — the replay
   comparator never diffs raw FDC-line lists). Non-ASCII characters are
   written as UTF-8 byte escapes because core builds without /utf-8 (same
   convention as mission_machine.cpp's kEmDash). */

/* U+2014 EM DASH — the only non-ASCII character in the quip text. */
#define EMD "\xE2\x80\x94"

namespace shitfire { namespace doctrine {

// port of src/js/07-voice-spot-audio.js:36-290 (oracle-v1) — const QUIPS = {...}
QuipSet makeQuips() {
  QuipSet q;
  q.greet.lines = {   /* 07:37-46 */
    "HELLHOUND FIRES on this net. The guns are hot, the coffee is battery acid, and I am surrounded by men who could not find their own ass with both hands, a flashlight, and a map with an arrow on it. Give me a target, over.",
    "HELLHOUND FIRES. We have been sitting in this shit-stained gun pit since dawn watching flies fight over a dead rat. Send us some actual fucking work, over.",
    "HELLHOUND FIRES up. Somewhere on this miserable rock is a grid square that luck itself gave up on. Point me at it, over.",
    "HELLHOUND FIRES on the net. I have six guns, a full ammo rack, and exactly zero fucks left after the last observer we buried. Try not to join him, over.",
    "HELLHOUND FIRES. Speak clearly, speak correctly, and nobody has to bleed from embarrassment today. Over.",
    "HELLHOUND FIRES, live. Every gun bunny in this battery already has money down on how fast you get us in trouble. I took the short odds. Prove me right, over.",
    "This net has seen career-enders, mercy kills, and one observer who called fire on his own foxhole by accident. Let us find out which one you are, over.",
    "HELLHOUND FIRES. The last guy who kept me waiting is a cautionary tale they teach at the schoolhouse now. Do not audition for the sequel. Over.",
  };
  q.readbackTail.lines = {   /* 07:47-56 */
    "And MUSTANG " EMD " do try to hit the correct fucking island this time. The reef has suffered enough on your account.",
    "Rounds are cheap. My patience is a finite, dwindling, deeply resentful resource. Standing by.",
    "If anything friendly is standing on that grid, now is the moment to confess " EMD " before the paperwork writes itself without you.",
    "Copy all. This had better be worth the powder, son, or I am billing your next of kin for the shells.",
    "Outstanding. The tubes were getting cold and the crew had started a book club. I was two chapters into their grief.",
    "Solid copy. Somewhere a dead artillery instructor just felt a warm, deeply confused sense of pride.",
    "Received. Try not to make me explain this grid to a board of inquiry later, I hate the coffee they serve at those.",
    "Copy the call. For your sake, MUSTANG, I hope that grid is where you think it is and not where your last three usually land.",
  };
  q.corrSnark.lines = {   /* 07:57-66 */
    "That is one HELL of a walk, MUSTANG. Did the first round land in a different fucking hemisphere, or just a different war?",
    "Copy. For the record: the gun did not move. Your math had a stroke.",
    "A bold correction. Somewhere out there a map is being held upside down, and I believe I know exactly by whom.",
    "Shifting fire. The fish send their regards for your earlier donation " EMD " they are naming a reef after you.",
    "Copy the shift. At this rate we will hit the target sometime after the peace treaty is signed.",
    "Adjusting. I have seen drunk men throw horseshoes with tighter groupings than that.",
    "Noted. Somewhere a supply sergeant is crying into a crate of shells you are about to waste on empty dirt.",
    "Shifting. If persistence were the same thing as accuracy, MUSTANG, you would already have a medal and a nickname.",
  };
  q.ffeAck.lines = {   /* 07:67-74 */
    "FIRE FOR EFFECT. Now THIS is the part of the job I love.",
    "FIRE FOR EFFECT. Nothing on that grid is walking away from this, and nothing is coming to save it either.",
    "Copy FIRE FOR EFFECT. Six guns, one grid, zero fucking sympathy.",
    "FIRE FOR EFFECT. Drink it up, MUSTANG. Drriiink it up.",
    "FIRE FOR EFFECT. Somewhere on that grid a very bad day is about to get very short.",
    "Copy FFE. The tubes are happy. The tubes are always happiest right before this part.",
  };
  q.completeTail.lines = {   /* 07:75-82 */
    "That grid square has been redecorated down to the bedrock.",
    "Rounds complete. Whatever stood there is now a fine mist and somebody else's paperwork.",
    "Rounds complete. I have looked upon that smoke and found it good.",
    "Tubes cooling. Take a long look, son. That is what competence smells like.",
    "Rounds complete. If there is a burial detail scheduled for that grid, tell them to bring a very small box.",
    "Tubes cooling. The crew is already taking bets on whether there is enough left to identify.",
  };
  q.eomGood.lines = {   /* 07:83-90 */
    "Good effect. Well, hell " EMD " the observer can actually observe.",
    "Target neutralized. Mark this day, MUSTANG. It will not come again.",
    "A clean kill. I had frankly abandoned all hope of seeing one from you.",
    "End of mission logged. There is a competence in you, boy, and today it finally showed up for work.",
    "Target destroyed. Somewhere a recruiter is taking credit for this and has never once met you.",
    "Clean, logged, and closed. I am almost disappointed I do not get to yell at you tonight.",
  };
  q.eomBad.lines = {   /* 07:91-98 */
    "End of mission. The target is alive and well and telling this story at dinner parties. We converted the taxpayers' money into noise and regret.",
    "Logged. We will call that one \"suppressive\" and never, ever speak of it again.",
    "End of mission. The enemy remains. My disappointment, however, is total, permanent, and load-bearing.",
    "Copy end of mission. You abandoned that fire mission, MUSTANG. You ABANDONED it.",
    "End of mission. Congratulations " EMD " you have achieved the rare feat of making a war LESS efficient.",
    "Logged as the failure it was. The target is fine. Your reputation is not.",
  };
  q.badGrid.lines = {   /* 07:99-106 */
    "MUSTANG 12, that grid is not on anybody's map, full stop. Say again your target location, over.",
    "Negative copy. That location does not exist on this or any other earth. Say again, over.",
    "I plotted that grid twice, and both times it insulted me personally. Say again, over.",
    "That is not a grid, MUSTANG, that is a cry for help. Say again, over.",
    "That grid belongs to a parallel universe where you are competent. Say again, in this one, over.",
    "I ran that through the plotting board twice and it laughed both times. Say again, over.",
  };
  q.water.lines = {   /* 07:107-112 */
    "Be advised, that grid is in the fucking ocean. Engaging the fish as requested.",
    "That grid is wet, MUSTANG. We are now shelling the Pacific, which never did a thing to us. Firing.",
    "Copy. Naval gunfire it is. Bold fucking choice.",
    "Congratulations, you have drawn first blood in the war against plankton. Firing.",
  };
  q.noMission.lines = {   /* 07:116-124 (NET4 burst-depth pool) */
    "MUSTANG 12, there is no mission on this net. You are adjusting fire that exists only in your heart. Send a call for fire, over.",
    "MUSTANG 12, you are correcting rounds we never fired. I admire the confidence. Send a call for fire first, over.",
    "There is no mission, MUSTANG. There is only you, me, and this dead air you keep filling. Call for fire, over.",
    "Adjusting a mission that does not exist is a special kind of stupid I have not logged before, and I have logged a lot. Call for fire, over.",
    "MUSTANG 12, HELLHOUND. There is no mission running. The radio will not start talking back until you send a call for fire. Over.",
    "Negative, MUSTANG. Nothing is plotted, nothing is airborne, nothing is happening on this net but you. Warning order, location, description. Over.",
    "There is no fire mission open, MUSTANG. I cannot adjust a ghost. Send the call first, over.",
  };
  q.inFlight.lines = {   /* 07:125-130 */
    "Rounds are in the air, MUSTANG. Physics is doing its part. Do yours " EMD " wait for splash, over.",
    "Still flying. Unless you can steer a shell with your mind, wait for the splash like everyone else, over.",
    "Patience, MUSTANG. The rounds arrive when they arrive. Much like your competence, over.",
    "The shell is airborne, MUSTANG, not telepathic. It cannot hear you. Wait for splash, over.",
  };
  q.unknown.lines = {   /* 07:131-140 */
    "Say again, over. Slower, and in English this time.",
    "That transmission had a callsign and then a stroke. Say again, over.",
    "MUSTANG, I have heard clearer traffic from a dying radio at the bottom of a well. Say again, over.",
    "Say again, over. And this time pretend the radio is graded " EMD " because it is. By me.",
    "That transmission needs a priest, not a readback. Say again, over.",
    "MUSTANG, that was words, technically. Say it again as an actual call for fire, over.",
    "I do not know what net you think you are on, but it is not this one. Say again, over.",
  };
  q.careerFrat.lines = {   /* 07:144-148 */
    "And MUSTANG " EMD " before we begin: the friendlies have asked me to remind you which color smoke is theirs. All of them. They held a meeting.",
    "One administrative note, MUSTANG: your file now has a tab. Files should not have tabs. Send your traffic.",
    "Before you key up " EMD " the infantry took a vote, and the results were unanimous, and printable only if I remove several words. Send it.",
  };
  q.careerCollat.lines = {   /* 07:149-153 */
    "MUSTANG, the village council has formally requested you be issued a map with pictures. Motion carried unanimously. Proceed.",
    "Before you key up, MUSTANG " EMD " the huts are still no-strike. Yes, still. Yes, all of them. Send it.",
    "One note before we start: the village elder asked if you would be \"doing the thing\" again today. I told him I could not promise anything. Send it.",
  };
  q.careerVet.lines = {   /* 07:154-158 */
    "MUSTANG 12 on the net. The guns know your voice now, son. They perk up. It is almost touching.",
    "Ah, MUSTANG. The log says we have done this dance a few times. Lead, this time, over.",
    "MUSTANG 12. The new guys ask about you like you are a ghost story. I let them believe it. Over.",
  };
  q.complete.lines = {   /* 07:159-161 — single line, pick() draws nothing */
    "MUSTANG 12, mission is complete. Send end of mission with your BDA, over.",
  };
  q.suppressedOnly.lines = {   /* 07:166-169 */
    "MUSTANG 12, they are heads-down, not dead " EMD " that is suppression, and it expires. Continue the mission or send end of mission, your call, over.",
    "MUSTANG 12, you have bought silence, not results. The moment we stop, they get back on their guns. Correct and REPEAT, or close it out, over.",
  };
  q.noEffect.lines = {   /* 07:170-173 */
    "MUSTANG 12, rounds complete, and the target could not care less. Where you put that volley and where they are living are two different places. Correct and REPEAT, or send end of mission and we will both pretend this was training, over.",
    "MUSTANG 12, no effect observed. We just gave the taxpayers a very expensive drum solo. Fix your data and REPEAT, or end the mission, over.",
  };
  q.dangerClose.lines = {   /* 07:174-178 */
    "MUSTANG 12, that target is DANGER CLOSE to friendly infantry and you did not say the fucking proword. I do not drop shells next to our own people on a mumble. Say again the full call with DANGER CLOSE, over.",
    "Negative. Friendlies are close enough to that grid to read the lot numbers off the fuzes. You want this mission? Say DANGER CLOSE like you mean it, over.",
    "MUSTANG, six hundred meters is not a suggestion " EMD " it is the distance between \"mission complete\" and a letter to next of kin. Say DANGER CLOSE, over.",
  };
  q.wrongWay.lines = {   /* 07:179-183 */
    "MUSTANG. The rounds are getting FARTHER from the target. You are adjusting fire like a blind man swatting bees. Look at your burst. Look at your target. THINK. Then talk, over.",
    "Wrong way, son. WRONG WAY. Every correction you send wounds me personally. Left is left. Add is away. It is not fucking calculus, over.",
    "You are correcting AWAY from the target with the confidence of a man who has never once been right about anything. Fix it, over.",
  };
  q.rantLost.lines = {   /* 07:184-192 */
    "STOP. Just " EMD " stop. MUSTANG, I have drunk deep of your transmissions and found NOTHING. No grid. No direction. No sense. You are a forward observer. So OBSERVE something, and then TELL ME ABOUT IT in the format the field manual and forty years of hard-won doctrine intended. Warning order. Location. Description. OVER.",
    "MUSTANG, listen to me very carefully, because I will say this once. I do not know what you are doing on that hill, but it is not calling for fire. Take a breath. Look at your map. Send: warning order, target location, description. In that order. Like a professional. Over.",
    "I have half a mind to have the guns fire on YOUR position out of pure professional mercy. Instead: breathe. Look at the map. Warning order, location, description. In that order. Do not make me say it a third time, over.",
    "MUSTANG, that is the third time in a row I have had to say this. Stop transmitting and start reading. Warning order, location, description. In that order. Over.",
    "We are going in circles, MUSTANG, and only one of us finds it funny. Breathe. Look at the manual. Send it right this time, over.",
  };
  q.coachTimid.lines = {   /* 07:195-198 */
    "MUSTANG, you are NIBBLING. Your burst was {MISS} meters off and you sent a correction a quarter that size. That is not adjustment, that is negotiation. Bracket it " EMD " throw a correction big enough to land on the OTHER side, then cut it in half.",
    "Negative technique, MUSTANG. {MISS} meters of error, and your correction would not move a burst out of its own crater. Doctrine: eight hundred, four hundred, two hundred. Bold, then halve. Send it again with conviction.",
  };
  q.coachBracket.lines = {   /* 07:199-202 */
    "MUSTANG, every round you have fired has landed on the SAME side of that target. You are not adjusting, you are commuting. Send a correction large enough to cross the target " EMD " then split the difference. That is a bracket. Use one.",
    "Listen to me, MUSTANG. All short. Every single one. You cannot split a bracket you have never established. Overshoot it on purpose, then halve. That is the whole trade.",
  };
  q.coachBracketGood.lines = {   /* 07:203-206 */
    "THERE it is " EMD " one over, one short. You have a bracket, MUSTANG. Now halve it. Halve it again. Then fire for effect like a professional.",
    "Bracket established. Cut your correction in half each round from here and you will be in effect before I finish this cigarette.",
  };
  q.coachStagnant.lines = {   /* 07:207-210 */
    "MUSTANG, that correction accomplished NOTHING. Same burst, different paperwork. Look at the burst, look at the target, MEASURE the difference, and send me THAT number.",
    "We just spent a round and a minute of the war to move that burst almost nowhere. Measure the error. Send the whole error. Not a polite fraction of it.",
  };
  q.coachLoc.lines = {   /* 07:211-213 — single line */
    "MUSTANG " EMD " that first round is {MISS} meters off target. That is not the gun, son, that is your grid. Standard is two hundred meters. Work your map before you key that mic next time.",
  };
  q.snide.lines = {   /* 07:216-222 */
    "Copy your... whatever that was. The manual describes a format, MUSTANG. It is one page. There are pictures.",
    "I processed that call out of charity. Callsigns, warning order, location, description. In that order. Like your instructor begged you.",
    "That transmission was to a call for fire what a mudslide is to architecture. We will fire it anyway.",
    "Copy. And to think they told me the radio was a professional instrument.",
    "That was not a call for fire, that was a cry into the void that happened to contain a grid. We will fire it anyway.",
  };
  q.snideRound.lines = {   /* 07:223-228 */
    "The guns round to tens, son " EMD " your boutique little numbers are a fantasy and my time is real. Firing.",
    "Copy your artisanal correction. Deviation comes in tens, range in hundreds. We will do the arithmetic you would not. Firing.",
    "The fire direction computer just sighed out loud, MUSTANG. Round your numbers like a professional. Firing anyway.",
    "I am rounding that myself so the guns do not have to think about it, the way you clearly did not. Firing.",
  };
  q.nearCiv.lines = {   /* 07:229-233 */
    "Be advised " EMD " that grid sits close to the village. There are civilians on this rock, MUSTANG. Put one round in the market square and your war is over.",
    "Copy all. Check your map, son " EMD " the village is a short walk from your splash pattern. Adjust like you mean it.",
    "That grid is close enough to the huts that I can smell the cooking fire on the map. Mind your correction.",
  };
  q.unsafeCorr.lines = {   /* 07:234-238 */
    "NEGATIVE. CHECK YOUR CORRECTION, MUSTANG. That shift walks rounds onto {WHO}. Look at your burst, look at your map, and send me a correction that does not end careers. Say again, over.",
    "I am NOT sending that. Plot it, son. Your correction lands on {WHO}. You have one radio and you are currently using it for manslaughter. Say again, over.",
    "Check it again, MUSTANG. That number puts steel on {WHO}, not the enemy. I will not be the reason it is in a report. Say again, over.",
  };
  q.unsafeInsist.lines = {   /* 07:239-243 */
    "Your funeral, MUSTANG. Shifting fire as ordered. I am writing your name in the log as I do it.",
    "Copy. On your head, son. The board of inquiry meets Tuesdays. Shifting fire.",
    "Fine. Your call, your name on it, your conscience. Shifting fire " EMD " and I hope to hell you can live with it.",
  };
  q.spotSrc.lines = {   /* 07:245-252 */
    "A RECON PATROL THAT WENT HOME AN HOUR AGO",
    "THE OBSERVER YOU RELIEVED, WHO IS NOW ASLEEP",
    "AN ADJACENT COMPANY WITH BETTER THINGS TO DO",
    "A COASTWATCHER WHO SPEAKS THREE WORDS OF ENGLISH",
    "BATTALION S-TWO, WHO WERE VERY PLEASED WITH THEMSELVES",
    "AN AIR OBSERVER ON HIS WAY TO SOMEWHERE ELSE",
  };
  q.spotTail.lines = {   /* 07:253-260 */
    "That is a CUE, MUSTANG, not a target location. Somebody has to actually look. That somebody is you.",
    "I am not going to pretend that is a grid. Get your glass up and turn it into one.",
    "That report has been passed through four radios and two liars. Verify it yourself.",
    "You have been given a neighborhood. I require an address. Over.",
    "Do not read that back to me as a call for fire. I will know, and I will be unkind about it.",
    "Somebody saw something once. Go and see it properly, over.",
  };
  q.spotNone.lines = {   /* 07:261-264 */
    "MUSTANG 12, HELLHOUND " EMD " nobody has sent us a thing. No patrol report, no overlay, no cue. Start glassing and tell me what is out there, over.",
    "MUSTANG 12, HELLHOUND " EMD " the intelligence picture this morning consists of one rumour and a wet map. You are the sensor. Get to work, over.",
  };
  q.rantCiv.rants = {   /* 07:265-275 */
    {{"CHECK FIRE. CHECK FIRE.",
      "That round went into the VILLAGE, MUSTANG. The civilian village. The one on your map " EMD " plotted, labeled, and full of people who were having a perfectly fine morning until you found the radio.",
      "There is no report I can write that survives this. END OF MISSION. Get off my net and go sit somewhere quiet and think about maps."}},
    {{"CHECK FIRE. CHECK FIRE, DAMN IT ALL.",
      "You just serviced a fishing village with one-five-five, you catastrophic instrument. Those were CIVILIANS. The huts were on the sheet. The brief said no-strike.",
      "I have to make phone calls now, MUSTANG. Phone calls with generals on the other end. END OF MISSION."}},
    {{"CHECK FIRE. CHECK FIRE, DAMN YOU.",
      "That grid had a name on the sheet, MUSTANG, and it was not a target designation. It was a village. It had a well and a schoolhouse and now it has neither.",
      "I am not writing this one up as \"collateral.\" I am writing it up as what it was. END OF MISSION. Get off my net."}},
  };
  q.rantFrat.rants = {   /* 07:276-289 */
    {{"CHECK FIRE. CHECK FIRE. CHECK FIRE.",
      "You just dropped one-five-five on the FRIENDLY position, MUSTANG. Do you understand me? Those are OUR people in that smoke. I am looking at the plot, and the plot says YOU did this.",
      "I told them not to hand you a map. I TOLD them. You are done on this net, son. Get off my radio and go practice on a lake. END OF MISSION."}},
    {{"CHECK FIRE. DAMN IT, CHECK FIRE.",
      "That was the friendly position, you catastrophic son of a bitch. Men with radios are standing in that smoke right now saying your callsign with FEELING.",
      "I have seen bad observers. I have TRAINED bad observers. But you " EMD " you are a whole new church of wrong. END OF MISSION."}},
    {{"CHECK FIRE. CHECK FIRE.",
      "MUSTANG, you walked artillery onto our own infantry. There are two kinds of people in this war " EMD " those who read grids, and casualties. Today you manufactured the second kind out of the first.",
      "When this is over, you and I will have a conversation about maps, and you will not enjoy it. END OF MISSION. Get off my net."}},
    {{"CHECK FIRE. CHECK FIRE, YOU ABSOLUTE WEAPON.",
      "That was OUR position, MUSTANG. I am looking at the casualty count on our side of the ledger because of a call YOU made.",
      "I do not have a joke for this one. I do not have anything for this one. END OF MISSION. Get off my net."}},
  };
  return q;
}

namespace {
/* Shared body of pick() — 07:291-297. Returns the chosen index. */
int pickIndex(int size, int& last, IFlavorRng& rng) {
  /* arr.length === 1 → return arr[0] with NO draw (07:292). */
  if (size == 1) return 0;
  int i = static_cast<int>(std::floor(rng.next() * size));   /* 07:293 */
  if (i == last) i = (i + 1) % size;                          /* 07:294 */
  last = i;                                                   /* 07:295 */
  return i;
}
}

// port of src/js/07-voice-spot-audio.js:291-297 (oracle-v1) — pick(arr)
const std::string& pick(QuipPool& pool, IFlavorRng& rng) {
  return pool.lines[pickIndex(static_cast<int>(pool.lines.size()), pool.last,
                              rng)];
}
const std::array<std::string, 3>& pick(RantPool& pool, IFlavorRng& rng) {
  return pool.rants[pickIndex(static_cast<int>(pool.rants.size()), pool.last,
                              rng)];
}

/* String.replace(token, value) — first occurrence only. */
std::string substFirst(const std::string& line, const std::string& token,
                       const std::string& value) {
  const std::size_t at = line.find(token);
  if (at == std::string::npos) return line;
  std::string out = line;
  out.replace(at, token.size(), value);
  return out;
}

} }
