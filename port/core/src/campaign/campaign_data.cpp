#include "shitfire/campaign/campaign_data.h"

#include <limits>

/* port of src/js/15-campaign-menu.js:10-183 (oracle-v1) — the CAMPAIGN
   volume/chapter table, ALL_CHAPTERS, and the c._vol back-reference, as
   compiled-in data. Every value below is the literal in the source row cited
   above it. Prose strings were machine-extracted from the JS and round-trip
   verified byte-for-byte (UTF-8 typographic characters as \x escapes, house
   style per scenario_gen.cpp:22-24).

   NOT here (behavior, other porters): the tutorial STEP ENGINE and its
   check()/dynamic-say closures (15-campaign-menu.js:20-23, :34-43, :338-378
   — tutorial porter; the step DATA rows are carried), the menu/briefing DOM
   and TTS (:316-520), and newMission() launch sequencing (:523-651 — wave-2
   assembly). */
namespace shitfire { namespace campaign {

namespace {

/* UTF-8 for the typographic characters the JS strings carry. */
const char* const DASH = "\xE2\x80\x94";   /* em dash */
const char* const HELLIP = "\xE2\x80\xA6"; /* horizontal ellipsis */

std::vector<VolumeSpec> buildCampaign() {
  std::vector<VolumeSpec> vols;

  /* ---- :11-52 — FOREWORD */
  {
    VolumeSpec v;
    v.id = "F"; v.tab = "FWD";
    v.name = std::string("FOREWORD ") + DASH + " THE SCHOOLHOUSE";
    v.need = 0; v.tseed = 1337;

    /* :12-24 — F.1 */
    {
      ChapterSpec c;
      c.id = "F.1"; c.title = "EYES BEFORE GUNS";
      c.blurb = "optics, mils, laser";
      c.type = "wreck"; c.seed = 51;
      c.story =
        "A training beach, stateside. Gunnery Sergeant Bottlecap holds a coffee he clearly "
        "hates and looks at you the way a man looks at weather. \"The actual FO fell off the "
        "boat. Nobody elaborates. That makes you the eyes. Let's find out if you have any.\"";
      c.outro = "Gunny initials a box on a clipboard without looking down at it.";
      TutSpec t;
      t.noMission = true;   /* :15 */
      t.done =
        "Eyes, glass, laser. That is the whole instrument. Tomorrow we find out if the brain "
        "is attached.";
      { TutStep s; s.on = "look"; s.amt = 5;   /* :18 */
        s.say = std::string(
          "First lesson is free: LOOK AROUND. Drag your eyes across the whole horizon ")
          + DASH + " mouse or stick. The heading readout up top is in mils, 6400 to a "
          "circle. Go.";
        t.steps.push_back(s); }
      { TutStep s; s.on = "binos";             /* :19 */
        s.say = std::string("Adequate. Now glass up ") + DASH +
          " press [B]. The reticle is graduated in mils; you will measure the world with it.";
        t.steps.push_back(s); }
      { TutStep s; s.on = "lase"; s.hasCheck = true;   /* :20-21 */
        s.say = std::string("Now LASE ") + DASH +
          " put the reticle on any piece of this island and press "
          "[L]. Range and grid come straight back. That laser is worth more than you are.";
        t.steps.push_back(s); }
      { TutStep s; s.on = "lase"; s.hasCheck = true;   /* :22-23 */
        s.say = std::string("Again ") + DASH +
          " a DIFFERENT spot, three hundred meters or more from the "
          "first. An observer confirms. He does not assume.";
        t.steps.push_back(s); }
      c.tut = t;
      v.chapters.push_back(c);
    }
    /* :25-44 — F.2 */
    {
      ChapterSpec c;
      c.id = "F.2"; c.title = "THE MAP IS NOT THE TERRITORY";
      c.blurb = "mapwork & terrain association";
      c.type = "wreck"; c.seed = 52;
      c.story =
        "\"Paper,\" Gunny says, producing a map sheet like a priest producing scripture. "
        "\"The island and this sheet are the same math. Every road, every hut, every tower "
        "is plotted. You will learn to stand in one and point at the other.\"";
      c.outro = "\"The map never lies. It just waits for you to.\"";
      TutSpec t;
      t.noMission = true;   /* :28 */
      t.done =
        "That is terrain association. You found a thing in the world on a piece of paper. "
        "Some officers never manage it.";
      { TutStep s; s.on = "map";               /* :31 */
        s.say = std::string("Map up ") + DASH +
          " press [M]. Roads, tracks, villages, facilities: plotted. "
          "Enemy: never. Finding HIM is the job.";
        t.steps.push_back(s); }
      { TutStep s; s.on = "library";           /* :32 */
        s.say = std::string("Now the real sheet ") + DASH +
          " press [P] and study the full 1:50,000. Print "
          "it if you have a printer; this trade is done on paper. Close it when you have the "
          "lay of the island.";
        t.steps.push_back(s); }
      { TutStep s; s.on = "msg"; s.hasCheck = true; s.sayIsDynamic = true;  /* :33-43 */
        s.missSay =
          "Negative. Plot it AGAIN. Find it on your map, read right then up, six digits, and "
          "send \"grid\" plus the number.";
        t.steps.push_back(s); }
      c.tut = t;
      v.chapters.push_back(c);
    }
    /* :45-51 — F.3 */
    {
      ChapterSpec c;
      c.id = "F.3"; c.title = "SAY IT LIKE YOU MEAN IT";
      c.blurb = "your first full call for fire";
      c.type = "wreck"; c.seed = 53; c.par = 420;
      c.story = std::string(
        "A derelict landing craft rusts on the beach below. \"That wreck has been dead "
        "thirty years,\" Gunny says. \"You are going to kill it again ") + DASH + " "
        "with a complete, correct call for fire. HELLHOUND is live. Try not to embarrass "
        "either of us.\"";
      c.outro =
        "Gunny signs your qualification sheet without looking up. \"Congratulations. You are "
        "somebody else's problem now.\"";
      c.coach = {
        "Locate the wreck " + std::string(DASH) + " binos [B], laser [L]. Then send it: "
          "callsigns, ADJUST FIRE, grid, description, OVER.",
        "When the round lands, watch the burst against the target. LEFT/RIGHT so many "
          "meters, ADD/DROP so many meters. Then FIRE FOR EFFECT when it is close.",
        "When the wreck is dead: END OF MISSION, target destroyed. Say it like you have "
          "done it before." };
      v.chapters.push_back(c);
    }
    vols.push_back(v);
  }

  /* ---- :53-66 — VOLUME I */
  {
    VolumeSpec v;
    v.id = "V1"; v.tab = "I";
    v.name = std::string("VOLUME I ") + DASH + " GREEN AS GRASS";
    v.need = 0; v.tseed = 1337;

    /* :54-56 — 1.1 */
    {
      ChapterSpec c;
      c.id = "1.1"; c.title = "TROOPS IN THE OPEN, BRAINS IN THE REAR";
      c.blurb = "grid fundamentals";
      c.type = "troops"; c.seed = 101; c.par = 300;
      c.story = std::string("A quiet-sector island, mostly secured ") + DASH +
        " a good place to be bad at "
        "your job. Somebody forgot to tell the platoon of enemy infantry sunbathing in the "
        "open. First real mission. First real rounds.";
      c.outro = "HELLHOUND logs the grid without comment, which from HELLHOUND is a medal.";
      v.chapters.push_back(c);
    }
    /* :57-59 — 1.2 */
    {
      ChapterSpec c;
      c.id = "1.2"; c.title = "KNOCK KNOCK";
      c.blurb = std::string("dug-in bunker ") + DASH + " bracket it";
      c.type = "bunker"; c.seed = 102; c.par = 360;
      c.story = std::string(
        "A bunker with a roof and opinions. Precision matters now: bracket it ") + DASH + " "
        "over, short, split the difference " + DASH + " and put one through the front "
        "door. The book calls it successive bracketing. Gunny called it \"haggling.\"";
      c.outro =
        "The engineers send their thanks and a request that you never again make them "
        "identify a bunker by smell.";
      v.chapters.push_back(c);
    }
    /* :60-62 — 1.3 */
    {
      ChapterSpec c;
      c.id = "1.3"; c.title = "HOLD THE LINE";
      c.blurb = "near the perimeter, not on it";
      c.type = "strongpoint"; c.seed = 103; c.par = 360;
      c.story =
        "A friendly strongpoint is getting pressed and the perimeter is shrinking by the "
        "minute. Your rounds go NEXT to our people, not ON them. There is a village behind "
        "the wire, too. Everyone on this island is watching your math, and none of them are "
        "being polite about it.";
      c.outro =
        "The strongpoint holds. Somebody inside the wire owes you a beer and will deny it.";
      v.chapters.push_back(c);
    }
    /* :63-65 — 1.4 */
    {
      ChapterSpec c;
      c.id = "1.4"; c.title = "DO IT AGAIN, SLOWER";
      c.blurb = "format discipline on the clock";
      c.type = "troops"; c.seed = 104; c.par = 210;
      c.story = std::string(
        "HELLHOUND has read your file aloud to the gun crews, twice, doing voices. Today the "
        "standard is the FULL format, correct, fast ") + DASH + " par is tight and the "
        "guns are bored. Prove the schoolhouse took.";
      c.outro =
        "HELLHOUND, grudging: \"Log shows a professional worked this net today. I have "
        "started an investigation.\"";
      v.chapters.push_back(c);
    }
    vols.push_back(v);
  }

  /* ---- :67-96 — VOLUME II (need 4, tseed 9021) */
  {
    VolumeSpec v;
    v.id = "V2"; v.tab = "II";
    v.name = std::string("VOLUME II ") + DASH + " THE RIDGE LINE";
    v.need = 4; v.tseed = 9021;

    /* :68-70 — 2.1 */
    {
      ChapterSpec c;
      c.id = "2.1"; c.title = "NUMBERS ON A COMPASS";
      c.blurb = std::string("polar ") + DASH + " dead laser, estimate by mil relation";
      c.type = "troops"; c.seed = 201; c.par = 330;
      c.method = "polar"; c.noLaser = true;
      c.story = std::string("A new island ") + DASH + " coral ridges, dead ground, "
        "terrain that hides "
        "things. The map is in a crate somewhere and the rangefinder battery died in the "
        "surf, so today YOU are the instrument: POLAR, off your own position. Direction from "
        "the compass. Distance by MIL RELATION " + DASH + " measure a known object in "
        "the reticle, size over mils times a thousand. [R] for the card.";
      c.outro =
        "Direction and distance out of a reticle and a memory. That is the oldest trick in "
        "the trade.";
      v.chapters.push_back(c);
    }
    /* :75-77 — 2.2 (TEMPO1 night, :71-74) */
    {
      ChapterSpec c;
      c.id = "2.2"; c.title = "OLD FRIENDS";
      c.blurb = std::string("shift from a known point ") + DASH + " after dark";
      c.type = "bunker"; c.seed = 202; c.par = 360;
      c.method = "shift"; c.tod = "night";
      c.story = std::string(
        "The registration points on this island have names ") + DASH + " the kind of "
        "names positions earn. The guns already know them cold, which matters tonight, "
        "because it is DARK out there and the map does not glow. SHIFT from a known point: "
        "direction, lateral, range. Ride on data somebody already paid for. Optics [O] "
        + DASH + " the island looks different through tubes.";
      c.outro =
        "The known points hold, even in the dark. Especially in the dark. That is why we "
        "register them before we need them.";
      v.chapters.push_back(c);
    }
    /* :78-80 — 2.3 */
    {
      ChapterSpec c;
      c.id = "2.3"; c.title = "DEFILADE BLUES";
      c.blurb = std::string("crest-masked target ") + DASH + " fire on what you cannot see";
      c.type = "troops"; c.seed = 203; c.par = 420;
      c.scn.mask = true;
      c.story = std::string(
        "Enemy infantry assembling in defilade behind the ridge. You will not see them, and "
        "you will not see your bursts ") + DASH + " just smoke drifting over the crest "
        "and the sound arriving late. Fight this one on the map. The map does not blink. It "
        "also does not care that you are scared. Fight it anyway.";
      c.outro = "You never saw them. They never saw it coming. Fair trade.";
      v.chapters.push_back(c);
    }
    /* :81-83 — 2.4 */
    {
      ChapterSpec c;
      c.id = "2.4"; c.title = "THE PERIMETER";
      c.blurb = "your choice of method, under pressure";
      c.type = "strongpoint"; c.seed = 204; c.par = 330;
      c.story = std::string(
        "Another strongpoint, worse ground, less time. Nobody is telling you which location "
        "method to use ") + DASH + " that is the point. Pick the right tool, first try, "
        "while the perimeter shrinks.";
      c.outro = "Right method, right rounds. The ridge line is yours now.";
      v.chapters.push_back(c);
    }
    /* :90-95 — 2.5 (TEMPO4, :84-89) */
    {
      ChapterSpec c;
      c.id = "2.5"; c.title = "FLARES OUT";
      c.blurb = std::string("night defense ") + DASH + " light first, then steel";
      c.type = "strongpoint"; c.seed = 205; c.par = 540;
      c.tod = "night"; c.reqIllum = true;
      c.story = std::string(
        "The perimeter again, except somebody turned the island off. A friendly strongpoint "
        "is taking fire in the dark and the assault is close enough to hear over the net. "
        "You cannot adjust what you cannot see: get ILLUMINATION up ") + DASH + " send "
        "the call with SHELL ILLUMINATION and walk the light over the attackers, big "
        "corrections, the flare does not need ten-meter precision. When they are lit, switch "
        "to steel: \"SHELL HE, OVER\", and fight it like any adjust mission, under your own "
        "light. A flare burns a minute, give or take " + DASH + " no two burn alike, "
        "so watch the light, not a clock. When it dies, buy another. Registration won the "
        "dark for the guns in 2.2 " + DASH + " tonight, light wins it for you.";
      c.outro =
        "The flare settles into the sea and the shooting stops. The strongpoint counts heads "
        "twice and comes up even both times. Nobody out there knows the light had a callsign.";
      c.coach = {
        "Light FIRST, killer. Send a normal call with ILLUMINATION in it. Adjust the flare "
          "in big steps " + std::string(DASH) + " hundreds, not tens " + DASH + " until the "
          "attackers are lit.",
        "When you can SEE them: \"SHELL HE, OVER\" " + std::string(DASH) + " same mission, "
          "new nature " + DASH + " then walk steel onto them under the light. No two flares "
          "burn the same, so when the light starts to die, send SHELL ILLUMINATION again and "
          "buy more " + DASH + " do not wait for the dark. Doctrine calls the whole "
          "dance COORDINATED ILLUMINATION." };
      v.chapters.push_back(c);
    }
    vols.push_back(v);
  }

  /* ---- :97-125 — VOLUME III (need 9, tseed 5150) */
  {
    VolumeSpec v;
    v.id = "V3"; v.tab = "III";
    v.name = std::string("VOLUME III ") + DASH + " THUNDER RUN";
    v.need = 9; v.tseed = 5150;

    /* :98-100 — 3.1 */
    {
      ChapterSpec c;
      c.id = "3.1"; c.title = "ROLLING STOCK";
      c.blurb = std::string("convoy ") + DASH + " lead it, or catch the pit stop";
      c.type = "convoy"; c.seed = 301; c.par = 420;
      c.story = std::string(
        "The big push is on and everything on this island is moving. An enemy column is "
        "running the coast road. Intel says they stop for fuel ") + DASH + " columns "
        "always stop. Lead a moving target, or ambush a sitting one. Choose.";
      c.outro = "Burning trucks on a coast road: the war's most honest progress report.";
      v.chapters.push_back(c);
    }
    /* :101-103 — 3.2 */
    {
      ChapterSpec c;
      c.id = "3.2"; c.title = "CLOSE ENOUGH TO SMELL IT";
      c.blurb = std::string("danger close ") + DASH + " say it, creep it";
      c.type = "assault"; c.seed = 302; c.par = 420;
      c.story = std::string(
        "Friendlies assaulting an objective with the enemy inside six hundred meters of "
        "them. That is DANGER CLOSE ") + DASH + " say the proword like you mean it, and "
        "creep the fire in from the safe side. A hundred meters at a time. No heroes, no "
        "funerals, no phone calls to a general at 0300.";
      c.outro =
        "The assault element walks the objective. Every one of them upright. That was the "
        "mission.";
      v.chapters.push_back(c);
    }
    /* :105-107 — 3.3 (TEMPO1 dawn, :104) */
    {
      ChapterSpec c;
      c.id = "3.3"; c.title = "UNINVITED GUESTS";
      c.blurb = std::string("raiders in the village ") + DASH + " huts are no-strike";
      c.type = "raid"; c.seed = 303; c.par = 390; c.tod = "dawn";
      c.story = std::string(
        "First light. Raiders are hitting the fishing village ") + DASH + " and the "
        "village is full of people whose war this is not. Fire on the raiders. The huts are "
        "no-strike. If discrimination were easy, they would not need an observer.";
      c.outro =
        "The raiders are gone. The village still stands. Nobody there will ever know your "
        "callsign, which is exactly right.";
      v.chapters.push_back(c);
    }
    /* :108-110 — 3.4 (title is double-quoted in the JS: EVERYONE'S MOVING) */
    {
      ChapterSpec c;
      c.id = "3.4"; c.title = "EVERYONE'S MOVING";
      c.blurb = "combined arms, friendlies advancing";
      c.type = "assault"; c.seed = 304; c.par = 390;
      c.story =
        "A full combined-arms push: our infantry advancing, their line breaking, the target "
        "shifting while your rounds are in the air. Timing is everything and everything is "
        "on the move.";
      c.outro = "Fires and maneuver, actually coordinated. Doctrine writers weep with joy.";
      v.chapters.push_back(c);
    }
    /* :111-113 — 3.5 */
    {
      ChapterSpec c;
      c.id = "3.5"; c.title = "THE WRONG KIND OF FAMOUS";
      c.blurb = std::string("friendlies interleaved ") + DASH + " do not make the news";
      c.type = "assault"; c.seed = 305; c.par = 390;
      c.scn.fClose = true;
      c.story =
        "The lines have collapsed into each other and the map looks like spilled paint. "
        "Friendlies are INSIDE your sheaf distances. The chapter title is the threat: "
        "observers who guess wrong here get famous in the worst way: a name read aloud at a "
        "service nobody wanted to attend.";
      c.outro = "Nobody wrote your name in any report today. Stay unfamous.";
      v.chapters.push_back(c);
    }
    /* :119-124 — 3.6 (TEMPO2, :114-118) */
    {
      ChapterSpec c;
      c.id = "3.6"; c.title = "SEND IT NOW";
      c.blurb = std::string("immediate suppression ") + DASH +
        " one transmission, rounds now";
      c.type = "strongpoint"; c.seed = 306; c.par = 180;
      c.reqImmediate = true;
      c.story = std::string(
        "The strongpoint on the low ridge is two minutes from being overrun and their radio "
        "is doing that thing where every voice on it is too calm. There is no time for three "
        "transmissions and a readback minuet. IMMEDIATE SUPPRESSION, a grid, OUT ")
        + DASH + " rounds now, paperwork later. The book has a page for exactly this "
        "day. It is a short page.";
      c.outro =
        "The attack stalls under the fire and breaks. Nobody at the strongpoint knows the "
        "format you used. That was the point of the format.";
      c.coach = {
        "This is the ONE-TRANSMISSION call, killer: \"IMMEDIATE SUPPRESSION, GRID, DANGER "
          "CLOSE, OUT.\" No warning order, no description, no MTO. Rounds first, paperwork "
          "after.",
        "The attackers are inside six hundred meters of the wire, so DANGER CLOSE rides in "
          "the call " + std::string(DASH) + " even the fast page of the book keeps that "
          "word. Lase, send, and keep fire coming: SUPPRESSION only lasts while rounds are "
          "landing." };
      v.chapters.push_back(c);
    }
    vols.push_back(v);
  }

  /* ---- :129-155 — VOLUME IV (need 15, tseed 66600, palette black,
     volume-wide overcast; TEMPO1 comment :126-128) */
  {
    VolumeSpec v;
    v.id = "V4"; v.tab = "IV";
    v.name = std::string("VOLUME IV ") + DASH + " BLACK SAND";
    v.need = 15; v.tseed = 66600;
    v.palette = "black"; v.tod = "overcast";

    /* :130-132 — 4.1 */
    {
      ChapterSpec c;
      c.id = "4.1"; c.title = "STRICT NET";
      c.blurb = std::string("doctrine enforced ") + DASH + " full format or silence";
      c.type = "strongpoint"; c.seed = 401; c.par = 360;
      c.strict = true;
      c.story = std::string(
        "A black-sand fortress island, and a new FDC posture: STRICT NET. Full format or the "
        "net stays silent ") + DASH + " callsigns, warning order, location, "
        "description, OVER, correct rounding, real surveillance. The book, verbatim, under "
        "fire.";
      c.outro = "The net stayed clean. Doctrine is just courtesy under fire.";
      v.chapters.push_back(c);
    }
    /* :136-138 — 4.2 (G18 comment :133-135: old scn effR:30 replaced by the
       mortar60 asset's own band set) */
    {
      ChapterSpec c;
      c.id = "4.2"; c.title = "TEN METERS";
      c.blurb = std::string("60mm mortars ") + DASH + " 8-digit precision";
      c.type = "troops"; c.seed = 402; c.par = 330;
      c.asset = "mortar60";
      c.story = std::string(
        "The 155s are displacing, so today you own a 60mm mortar section. Small tubes, tight "
        "sheaf, ten-meter grids ") + DASH + " EIGHT digits, read right then up. At this "
        "precision, \"close enough\" is a contradiction.";
      c.outro =
        "Ten-meter work with a sixty. The mortar section names a tube after you. It is the "
        "ugly one.";
      v.chapters.push_back(c);
    }
    /* :139-141 — 4.3 (hard-only) */
    {
      ChapterSpec c;
      c.id = "4.3"; c.title = "NO SECOND CHANCES";
      c.blurb = "hard only. one pass, tight clock.";
      c.type = "bunker"; c.seed = 403; c.par = 240;
      c.diffs = { "hard" };
      c.story =
        "One bunker, one clock, hard difficulty, no bracket wide enough to hide in. "
        "Everything you have learned, at speed, or nothing.";
      c.outro = "On time, on target, no excuses filed. There were none to file.";
      v.chapters.push_back(c);
    }
    /* :142-144 — 4.4 (strict + hard-only) */
    {
      ChapterSpec c;
      c.id = "4.4"; c.title = "THE MEAT GRINDER";
      c.blurb = std::string("the final exam ") + DASH + " strict, hard, everything at once";
      c.type = "assault"; c.seed = 404; c.par = 330;
      c.strict = true; c.diffs = { "hard" };
      c.story = std::string(
        "The last hill on the last island. Strict net, danger close, friendlies advancing, "
        "hard difficulty ") + DASH + " the whole trade in one mission. Pass this and "
        "even HELLHOUND will say something almost kind. Almost.";
      c.outro = std::string("HELLHOUND, after a long pause: \"") + HELLIP +
        "acceptable, MUSTANG.\" Frame it.";
      v.chapters.push_back(c);
    }
    /* :149-154 — 4.5 (SUGG4b, :145-148) */
    {
      ChapterSpec c;
      c.id = "4.5"; c.title = "DIG IN";
      c.blurb = std::string("defense in depth ") + DASH +
        " plan the lines, fire in two words";
      c.type = "qfp"; c.seed = 405; c.par = 600;
      c.reqPlanned = true; c.scn.lanes = true;
      c.story = std::string(
        "You took the hill yesterday. Tonight the paperwork says HOLD, and the enemy never "
        "reads the paperwork ") + DASH + " a counterattack steps off at H-hour, "
        "canalized down the one draw that leads up here. This is the fight you get to PLAN. "
        "While it is quiet: \"PLAN TARGET, GRID " + HELLIP + "\" on each phase line "
        + DASH + " RED, WHITE, BLUE, stacked in depth down the draw. When they cross "
        "RED, you do not build a call for fire. You say two words and the guns already know "
        "the rest.";
      c.outro =
        "The draw is a graveyard of a counterattack, one phase line at a time. HELLHOUND: "
        "\"A plan. An actual plan. I am framing the log, MUSTANG.\"";
      c.coach = {
        "Plan while it is QUIET, killer. A target on each phase line " + std::string(DASH) +
          " \"PLAN TARGET, GRID " + HELLIP + "\" costs nothing and files the data. PRIORITY "
          "TARGET the line you expect to fight on.",
        "When they cross a line: \"FIRE TARGET AB####\" " + std::string(DASH) +
          " two words, rounds "
          "now. Walk fire down YOUR OWN planned axis as they come. A series or a group of "
          "the lines works too. The full call is for people who did not plan." };
      v.chapters.push_back(c);
    }
    vols.push_back(v);
  }

  /* ---- :156-178 — EPILOGUE (need 20, tseed 1337; 11a/TEMPO1 comments
     :157-162) */
  {
    VolumeSpec v;
    v.id = "EP"; v.tab = "EPI";
    v.name = std::string("EPILOGUE ") + DASH + " SUNBURN";
    v.need = 20; v.tseed = 1337;

    /* :163-167 — E.1 */
    {
      ChapterSpec c;
      c.id = "E.1"; c.title = "THE GREAT CHOW RAID";
      c.blurb = "fire mission on a seagull flock. no, really.";
      c.type = "chow"; c.seed = 501; c.par = 300; c.tod = "dusk";
      c.story = std::string(
        "The war is won. The paperwork is not. The general is grilling on the beach, and "
        "nine hundred seagulls have chosen violence. The flock is assaulting the barbecue "
        "pit from the tideline, and the only tubes in range are yours. The cooks are a "
        "NO-FIRE line: Private Dombrowski and his potato salad WILL survive this. Fire "
        "mission, by the book ") + DASH + " the book does not care how stupid the war "
        "has become.";
      c.outro =
        "The flock is broken. The potato salad is intact. The general pins nothing on you, "
        "because officially none of this happened.";
      c.coach = {
        "Birds are TROOPS IN THE OPEN, killer. The field manual does not have a column for "
          "wingspan and I am not going to be the one to write it.",
        "Dombrowski is at the grill. If a correction walks one round onto that man or his "
          "salad, it is FRATRICIDE, it goes in the report, and I will read the report AT "
          "you." };
      v.chapters.push_back(c);
    }
    /* :168-172 — E.2 */
    {
      ChapterSpec c;
      c.id = "E.2"; c.title = "CLAWS OUT";
      c.blurb = "a moving target. it is also a crab.";
      c.type = "kaiju"; c.seed = 502; c.par = 420; c.tod = "dawn";
      c.story = std::string(
        "At 0400 the radar picket reported a contact wading through the surf line. At 0406 "
        "the picket stopped filing reports and started filing retirement paperwork. A crab "
        "the size of a church is making for the village, and battalion has ruled ")
        + DASH + " in writing " + DASH + " that it is a surface target and "
        "therefore yours. It is moving. Lead it. HELLHOUND has been briefed and has elected "
        "not to react.";
      c.outro =
        "The crab settles into the shallows. The village never knew. The after-action report "
        "lists one (1) hard structure, mobile, destroyed, and no further questions were "
        "taken.";
      c.coach = {
        "It is a MOVING TARGET, killer. Lead it like the convoy: fire where it is GOING to "
          "be, not where it makes you feel things.",
        "Big does not mean soft. That shell is a hard structure " + std::string(DASH) +
          " FUZE DELAY pays, and you will need sustained effect. Volley, correct, REPEAT." };
      v.chapters.push_back(c);
    }
    /* :173-177 — E.3 */
    {
      ChapterSpec c;
      c.id = "E.3"; c.title = "SUNLAMP ACTUAL";
      c.blurb = "a call for fire to a space cannon. same book.";
      c.type = "bunker"; c.seed = 503; c.par = 300;
      c.asset = "sunlamp"; c.tod = "dusk";
      c.story = std::string(
        "The last thing the war left behind is a bunker complex nobody can reach, so "
        "battalion has borrowed something from \"a partner force\": SUNLAMP, an "
        "intergalactic directed-energy space cannon satellite of mass destruction, now "
        "holding your net and PLEASED to be here. Same six elements. Same readback. Same "
        "corrections in the OT frame. The time of flight is a charging whine from everywhere "
        "at once, SHOT is DISCHARGE, SPLASH is SOLAR EVENT, and HELLHOUND ") + DASH + " "
        "patched in to observe " + DASH + " has never sounded more tired. Send the "
        "call.";
      c.outro =
        "END OF MISSION. The bunker is a smooth glass bowl. SUNLAMP thanks you for your "
        "business and hopes you will consider orbital fires for your future fire support "
        "needs. HELLHOUND says nothing at all, which from HELLHOUND is a standing ovation. "
        "The book closes.";
      c.coach = {
        "It is a SPACE CANNON, killer, and it still wants the six elements in three "
          "transmissions. Doctrine does not care where the tube is parked.",
        "It will ask for a ten-digit grid. It will settle for what you have. Send the call "
          "like it is any other gun, because to the book, it is." };
      v.chapters.push_back(c);
    }
    vols.push_back(v);
  }

  /* ---- :179-180 — VOLUME V (locked tease, no chapters, need Infinity) */
  {
    VolumeSpec v;
    v.id = "V5"; v.tab = "V";
    v.name = std::string("VOLUME V ") + DASH + " ON WINGS";
    v.need = std::numeric_limits<double>::infinity();
    v.tease = std::string("LOCKED ") + DASH + " AWAITING AIRCRAFT. CAS, SOMEDAY.";
    vols.push_back(v);
  }

  return vols;
}

}  // namespace

/* :10-181 — CAMPAIGN. */
const std::vector<VolumeSpec>& campaignVolumes() {
  static const std::vector<VolumeSpec> C = buildCampaign();
  return C;
}

/* :182 — ALL_CHAPTERS = CAMPAIGN.flatMap(v => v.chapters). */
const std::vector<const ChapterSpec*>& allChapters() {
  static const std::vector<const ChapterSpec*> A = [] {
    std::vector<const ChapterSpec*> a;
    for (const VolumeSpec& v : campaignVolumes())
      for (const ChapterSpec& c : v.chapters) a.push_back(&c);
    return a;
  }();
  return A;
}

const ChapterSpec* findChapter(const std::string& id) {
  for (const ChapterSpec* c : allChapters())
    if (c->id == id) return c;
  return nullptr;
}

/* :183 — c._vol back-reference. */
const VolumeSpec* volumeOf(const std::string& chapterId) {
  for (const VolumeSpec& v : campaignVolumes())
    for (const ChapterSpec& c : v.chapters)
      if (c.id == chapterId) return &v;
  return nullptr;
}

} }
