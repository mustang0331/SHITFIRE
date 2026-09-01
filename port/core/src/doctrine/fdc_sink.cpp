#include "shitfire/doctrine/fdc_sink.h"

#include <cmath>
#include <cstddef>

#include "shitfire/doctrine/fdc_spot.h"
#include "shitfire/math/grid.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"

/* FdcLineId / SysLineId -> radio sentence. Every composition cites the
   FDC.say / log(...'sys') call site in src/js it reproduces (oracle-v1).
   Non-ASCII is escaped because core builds without /utf-8. */
#define EMD "\xE2\x80\x94"    /* — U+2014 */
#define ARR "\xE2\x86\x92"    /* → U+2192 */
#define MULX "\xC3\x97"       /* × U+00D7 */
#define DIVS "\xC3\xB7"       /* ÷ U+00F7 */

namespace shitfire { namespace doctrine {

std::string shellWord(sim::ShellKind k) {
  switch (k) {
    case sim::ShellKind::Smoke: return "smoke";
    case sim::ShellKind::Illum: return "illum";
    default: return "he";
  }
}
std::string fuzeWord(sim::FuzeKind k) {
  switch (k) {
    case sim::FuzeKind::Vt: return "vt";
    case sim::FuzeKind::Time: return "time";
    case sim::FuzeKind::Delay: return "delay";
    default: return "pd";
  }
}

namespace {

std::string joinComma(const std::vector<std::string>& v) {
  std::string out;
  for (std::size_t i = 0; i < v.size(); i++) {
    if (i) out += ", ";
    out += v[i];
  }
  return out;
}

/* count of comma-joined items in a machine-joined list ("a, b, c"). */
int joinedCount(const std::string& joined) {
  if (joined.empty()) return 0;
  int n = 1;
  for (std::size_t at = joined.find(", "); at != std::string::npos;
       at = joined.find(", ", at + 2))
    n++;
  return n;
}

/* Math.abs(x).toFixed(0) for the spotting mils — ES toFixed rounds ties away
   from zero on the positive values used here, which is js::round. */
std::string fixed0Abs(double v) { return jsNum(js::round(std::fabs(v))); }

}  // namespace

FdcMissionSink::FdcMissionSink(FdcSpeaker& speaker, QuipSet& quips,
                               IFlavorRng& flavor, INetSink& net,
                               const FdcChapterContext& chapter,
                               const FdcCallsigns& calls, const MapConfig& map,
                               sim::Vec2 battery)
    : speaker_(speaker), quips_(quips), flavor_(flavor), net_(net),
      chapter_(chapter), calls_(calls), map_(map), battery_(battery) {}

double FdcMissionSink::sayLine(const sim::FdcLine& line) {
  /* Sanctioned-flavor presence gate (events.h): the sink's own coin. On a
     failed coin the source never called FDC.say (10-mission.js:824/:902), so
     the chain is untouched and no pick() is drawn; the returned time is
     meaningless and unread. */
  if (line.gateProbability > 0 && !(flavor_.next() < line.gateProbability))
    return speaker_.lastT();

  using Id = sim::FdcLineId;
  /* The two 3-line rants chain their own delays (10-mission.js:1213-1215). */
  if (line.id == Id::RantFratricide || line.id == Id::RantCollateral) {
    const auto& rant = pick(
        line.id == Id::RantFratricide ? quips_.rantFrat : quips_.rantCiv,
        flavor_);
    speaker_.say(rant[0], line.delay);   /* 1.0 (:1213) */
    speaker_.say(rant[1], 2.2);          /* :1214 */
    return speaker_.say(rant[2], 2.8);   /* :1215 */
  }
  return speaker_.say(composeFdc(line), line.delay);
}

std::string FdcMissionSink::composeFdc(const sim::FdcLine& line) {
  using Id = sim::FdcLineId;
  const bool sun = sunNet(chapter_);
  const bool m60 = chapter_.active && chapter_.asset == sim::AssetKind::Mortar60;
  switch (line.id) {
    /* 10-mission.js:68-69 — shotWord()/splashWord(); composed at say time
       like the source (sunNet() read where FDC.say is called). */
    case Id::ShotOver: return sun ? "DISCHARGE, OVER." : "SHOT, OVER.";
    case Id::SplashOver: return sun ? "SOLAR EVENT, OVER." : "SPLASH, OVER.";
    case Id::ReadyOver: return "READY, OVER.";   /* :663 */
    case Id::CheckFiringRefusal:                 /* :781 / :813 */
      return "NEGATIVE " EMD " WE ARE CHECK FIRING. CANCEL CHECK FIRING BEFORE I PUT ANYTHING ELSE UP, OVER.";
    case Id::CheckFiringRefusalFire:             /* :698 */
      return "NEGATIVE " EMD " WE ARE CHECK FIRING. CANCEL CHECK FIRING, THEN SAY FIRE, OVER.";
    case Id::BlackOnShell:                       /* :789 / :818 */
      return "NEGATIVE " EMD " BLACK ON " + upperAscii(line.text) +
             ". The gun line has nothing left to send. Over.";
    case Id::AmmoAmber:                          /* :627 */
      return "BE ADVISED " EMD " AMBER ON " + upperAscii(line.text) + ", " +
             jsNum(line.num0) +
             " ROUNDS REMAIN ON THE GUN LINE. Make them count, over.";
    case Id::PartialVolley: {                    /* :867 */
      const sim::Mission* m = machine_ ? machine_->activeMission() : nullptr;
      const std::string sh =
          upperAscii(shellWord(m ? m->shell : sim::ShellKind::He));
      return "BE ADVISED " EMD " PARTIAL VOLLEY, " + jsNum(line.num0) +
             " ROUND" + (line.num0 == 1 ? "" : "S") + " REMAINING ON " + sh +
             ", THEN WE ARE BLACK. Over.";
    }
    case Id::RoundsComplete: return "ROUNDS COMPLETE, OVER.";  /* :895 */
    case Id::FfeAckFlavor: return pick(quips_.ffeAck, flavor_);        /* :824 */
    case Id::CompleteTailFlavor: return pick(quips_.completeTail, flavor_); /* :902 */
    case Id::CompletePrompt: return pick(quips_.complete, flavor_);    /* :903 */
    case Id::SuppressedOnly: return pick(quips_.suppressedOnly, flavor_); /* :905 */
    case Id::NoEffect: return pick(quips_.noEffect, flavor_);          /* :910 */
    case Id::RantFratricide:
    case Id::RantCollateral:
      return "";   /* handled in sayLine (3-line chain) — unreachable */
    case Id::RantFratricideRepeat:               /* :1226 */
      return "MORE rounds on the friendlies, MUSTANG. Every one of these is a name I have to write down. FIX YOUR CORRECTION.";
    case Id::RantCollateralRepeat:               /* :1237 */
      return "That one hit the village TOO. Walk it OUT of there " EMD " now, MUSTANG.";
    case Id::WrongWayQuip: return pick(quips_.wrongWay, flavor_);      /* :1243 */
    case Id::CoachLoc:                           /* :1290 */
      return substFirst(pick(quips_.coachLoc, flavor_), "{MISS}",
                        jsNum(line.num0));
    case Id::CoachBracketGood: return pick(quips_.coachBracketGood, flavor_); /* :1293 */
    case Id::CoachBracket: return pick(quips_.coachBracket, flavor_);  /* :1296 */
    case Id::CoachStagnant: return pick(quips_.coachStagnant, flavor_); /* :1299 */
    case Id::DisperseAdvise:                     /* :1273 */
      return "Be advised, MUSTANG " EMD " they are dispersing. Every round you spend walking is a man of theirs finding a hole. Get on with it.";
    case Id::TofQueryNoMission:                  /* :685 */
      return "NO MISSION ON THE NET " EMD " time of flight to where, exactly? Over.";
    case Id::TimeOfFlight:                       /* :688 */
      return "TIME OF FLIGHT " + jsNum(line.num0) + " SECONDS, OVER.";
    case Id::FireOut: return "FIRE, OUT.";       /* :711 */
    case Id::FireNothingLaid:                    /* :703 */
      return "NOTHING IS LAID AND WAITING, MUSTANG. Send your correction first, over.";
    case Id::FireNotAtMyCommand:                 /* :704 */
      return "You are not AT MY COMMAND, MUSTANG " EMD " I fire when ready. Nothing is waiting on you, over.";
    case Id::AmcAdviceNoMission:                 /* :718 */
      return "AT MY COMMAND is method of fire and control " EMD " send it with your call for fire, over.";
    case Id::AmcCancelNoMission:                 /* :719 */
      return "No mission running, nothing to cancel, out.";
    case Id::AmcOut: return "AT MY COMMAND, OUT.";        /* :723 */
    case Id::CancelAmcOut: return "CANCEL AT MY COMMAND, OUT.";  /* :723 */
    case Id::DoNotLoadIdle:                      /* :735 */
      return "DO NOT LOAD, OUT. Nothing is loaded.";
    case Id::DoNotLoadOut: return "DO NOT LOAD, OUT.";    /* :741 */
    case Id::CannotObserveOut:                   /* :748-749 */
      return "CANNOT OBSERVE, OUT. We will fire the mission blind on your last data. Send END OF MISSION when you are done, over.";
    case Id::TimeOnTargetOut:                    /* :754 */
      return "TIME ON TARGET " + jsNum(line.num0) + ", OUT.";
    case Id::MtoWrongTargetNumber:               /* :222-223 */
      return "NEGATIVE, MUSTANG " EMD " the target number is ALPHA ALPHA " +
             line.text + ". Read the whole message back, over.";
    case Id::MtoNumberOnly:                      /* :229-230 */
      return "That is the target number, not the message. Read back the whole MTO, over.";
    case Id::MtoReadbackCorrect: return "READBACK CORRECT, OUT.";  /* :235 */
    case Id::MtoReadbackClose:                   /* :236 */
      return "CLOSE ENOUGH, MUSTANG. READBACK CORRECT, OUT.";
    case Id::NoSuchTargetPriority:               /* :297-299 */
      return "NEGATIVE " EMD " no target " + (ctx_ ? ctx_->tgtNum : "") +
             " on file. " +
             (!line.text.empty() ? "I hold " + line.text + ". "
                                 : "Record one at end of mission first. ") +
             "Over.";
    case Id::PrioritySet:                        /* :304-305 */
      return "TARGET " + line.text + " IS PRIORITY" +
             (line.num0 ? " " EMD " SHIFTING OFF THE OLD ONE" : "") +
             ". GUNS ARE LAID AND STANDING BY, OUT.";
    case Id::MidMissionRefusal:                  /* per refused op */
      switch (static_cast<sim::MissionOp>(static_cast<int>(line.num0))) {
        case sim::MissionOp::PlanTarget:         /* :322 */
          return "MUSTANG 12, we are mid-mission. Plan on your own time " EMD " finish this one, over.";
        case sim::MissionOp::PlanFpf:            /* :407 */
          return "MUSTANG 12, we are mid-mission. Plan your FPF when the net is quiet, over.";
        case sim::MissionOp::FireFpf:            /* :474 */
          return "MUSTANG 12, we are mid-mission. End it or let it ride " EMD " the FPF fires clean, over.";
        default:                                 /* :345 / :513 / :551 / :563 */
          return "MUSTANG 12, we are mid-mission. Finish this one, over.";
      }
    case Id::PlanNeedsGrid:                      /* :327 */
      return "A plan entry needs a GRID " EMD " six or eight digits. \"PLAN TARGET, GRID 245 523\", over.";
    case Id::OffMapSheet:                        /* :334 */
      return "That grid is off my map sheet, MUSTANG. Check your read and say again, over.";
    case Id::TargetPlanned: {                    /* :339 */
      const std::string d = ctx_ ? ctx_->digits : "";
      const std::string g = d.size() == 8
          ? d.substr(0, 4) + " " + d.substr(4)
          : d.size() == 6 ? d.substr(0, 3) + " " + d.substr(3) : d;
      return "TARGET " + line.text + " PLANNED AT GRID " + g +
             ". DATA IS ON THE GUNS, OUT.";
    }
    case Id::NoSuchTargetFire:                   /* :351-353 */
      return "NEGATIVE " EMD " no target " + (ctx_ ? ctx_->tgtNum : "") +
             " on file. " +
             (!line.text.empty() ? "I hold " + line.text + ". "
                                 : "Nothing is recorded on this net yet. ") +
             "Say again, over.";
    case Id::StandbyRelay:                       /* :358 */
      return "STAND BY " EMD " RE-LAYING ONTO TARGET " + line.text +
             ". That is what priority would have bought you, over.";
    case Id::FireTargetOut:                      /* :359 */
      return "FIRE TARGET " + line.text + ", OUT.";
    case Id::NoSuchTargetSuppress:               /* :569-571 */
      return "NEGATIVE " EMD " I have no target " + (ctx_ ? ctx_->tgtNum : "") +
             " on file. " +
             (!line.text.empty() ? "I hold " + line.text + ". "
                                 : "Nothing is recorded on this net yet. ") +
             "Say again, over.";
    case Id::SuppressTargetOut:                  /* :578 */
      return "SUPPRESS TARGET " + line.text +
             (line.num0 ? ", " + jsNum(line.num0) + " MINUTES" : "") + ", OUT.";
    case Id::SeriesNeedsTargets:                 /* :490 */
      return "A SERIES IS TARGETS, MUSTANG " EMD " \"PLAN SERIES MAX, TARGETS AB7101, AB7102\", OVER.";
    case Id::PlanMissingTargets: {               /* :496-497 / :535-536 */
      const std::string known =
          machine_ ? joinComma(machine_->recTgtKeys()) : "";
      return "NEGATIVE " EMD " " + line.text + " " +
             (joinedCount(line.text) == 1 ? "is" : "are") + " not on file. " +
             (!known.empty() ? "I hold " + known + ". "
                             : "Record targets before you plan with them. ") +
             "Over.";
    }
    case Id::SeriesEstablished:                  /* :501-502 */
      return "SERIES " + line.text + " ESTABLISHED " EMD " " +
             jsNum(line.num0) + " TARGETS, IN ORDER: " +
             joinComma(ctx_ ? ctx_->tgts : std::vector<std::string>{}) +
             ". CALL FOR IT BY NAME, OUT.";
    case Id::NoSuchSeries:                       /* :508-509 */
      return "NEGATIVE " EMD " no series " + (ctx_ ? ctx_->name : "") +
             " on this net. " +
             (!line.text.empty() ? "I hold " + line.text + ". "
                                 : "Plan one first. ") +
             "Over.";
    case Id::FiringSeries:                       /* :516 */
      return "FIRING SERIES " + line.text + " " EMD " " + jsNum(line.num0) +
             " TARGETS IN SEQUENCE, OUT.";
    case Id::GroupNeedsTwo:                      /* :529 */
      return "A GROUP IS TWO OR MORE, MUSTANG " EMD " \"PLAN GROUP A2B, TARGETS AB7101, AB7102\", OVER.";
    case Id::GroupEstablished:                   /* :540 */
      return "GROUP " + line.text + " ESTABLISHED " EMD " " + jsNum(line.num0) +
             " TARGETS, FIRED TOGETHER: " +
             joinComma(ctx_ ? ctx_->tgts : std::vector<std::string>{}) +
             ", OUT.";
    case Id::NoSuchGroup:                        /* :546-547 */
      return "NEGATIVE " EMD " no group " + (ctx_ ? ctx_->name : "") +
             " on this net. " +
             (!line.text.empty() ? "I hold " + line.text + ". "
                                 : "Plan one first. ") +
             "Over.";
    case Id::FiringGroup:                        /* :554 */
      return "FIRING GROUP " + line.text + " " EMD " ALL TARGETS TOGETHER, OUT.";
    case Id::NoFpfPlanned:                       /* :447 vs :470 */
      return (ctx_ && ctx_->ptype == "firefpf")
          ? "NEGATIVE " EMD " no FPF is planned on this net. Plan it before you need it, over."
          : "NEGATIVE " EMD " no FPF is planned on this net. \"PLAN FPF, GRID, DIRECTION\" first, over.";
    case Id::FourGunsOnly:                       /* :451 */
      return "I HAVE FOUR GUNS, MUSTANG, NOT " + jsNum(line.num0) +
             ". NUMBER 1 THROUGH 4, OVER.";
    case Id::FpfNumberAck: {                     /* :464-465 */
      std::string mid;
      if (line.text == "repeat") {
        mid = ", REPEAT";
      } else {
        if (line.num1)
          mid += std::string(", ") + (line.num1 > 0 ? "RIGHT" : "LEFT") + " " +
                 jsNum(std::fabs(line.num1));
        if (line.num2)
          mid += std::string(", ") + (line.num2 > 0 ? "ADD" : "DROP") + " " +
                 jsNum(std::fabs(line.num2));
      }
      return "NUMBER " + jsNum(line.num0) + mid + ", OUT.";
    }
    case Id::FpfDangerCloseQuip: return pick(quips_.dangerClose, flavor_); /* :430 */
    case Id::FpfEstablished:                     /* :438-440 */
      return "FPF ESTABLISHED " EMD " FOUR GUNS ON A 180 METER LINE, ATTITUDE " +
             fmtMils(line.num0) + ", CENTER GRID " + line.text +
             (line.num1 ? ", DANGER CLOSE" : "") +
             ". ADJUST EACH PIECE " EMD " NUMBER 1 IS UP, OVER.";
    case Id::FpfMarkOnFriendlies:                /* :396 */
      return "CHECK FIRE " EMD " THAT MARKING ROUND WAS ON THE FRIENDLIES. The FPF you just adjusted onto our own people is the FPF you will answer for when it fires, over.";
    case Id::FiringFpf:                          /* :477 */
      return "FIRING THE FPF " EMD " ALL GUNS, MAXIMUM RATE, OUT.";
    case Id::UnitStatusReport: {                 /* :640-644 */
      const std::string gs = gridOf(battery_.x, battery_.z, map_);
      const std::string head =
          line.text == "repeat"
              ? "MUSTANG 12, again, since apparently my word is not enough " EMD " "
              : "MUSTANG 12, HELLHOUND FIRES " EMD " ";
      if (m60)
        return head + "FIRE UNIT STATUS: TWO TUBES, 60 MIKE MIKE, GRID " + gs +
               ". HE " + jsNum(line.num0) + " ROUNDS, WHISKEY PAPA " +
               jsNum(line.num1) + ", ILLUM " + jsNum(line.num2) +
               ", MULTI-OPTION FUZE ON HAND. ON STATION AND READY FOR CALL FOR FIRE, OVER.";
      return head + "FIRE UNIT STATUS: SIX GUNS, 155, GRID " + gs + ". HE " +
             jsNum(line.num0) + " ROUNDS, SMOKE " + jsNum(line.num1) +
             ", ILLUM " + jsNum(line.num2) +
             "; FUZE PD, VT AND DELAY ON HAND. ON STATION AND READY FOR CALL FOR FIRE, OVER.";
    }
    case Id::ShellNoMission:                     /* :1057 */
      return "No mission on the net. Name the shell in your call for fire, over.";
    case Id::ShellOut:                           /* :1069 */
      return "SHELL " + (lowerAscii(line.text) == "illum"
                             ? std::string("ILLUMINATION")
                             : upperAscii(line.text)) + ", OUT.";
    case Id::FuzeNoMission:                      /* :1038 */
      return "No mission on the net " EMD " fuzes go on rounds, not on air, over.";
    case Id::FuzeVtDangerCloseRefusal:           /* :1045 */
      return "NEGATIVE ON VT, MUSTANG " EMD " I am not putting airburst over friendlies at danger close. FUZE PD stays, over.";
    case Id::FuzeOut:                            /* :1049 */
      return "FUZE " + upperAscii(line.text) + ", OUT.";
    case Id::SheafNoMission:                     /* :1074 */
      return "No mission on the net to shape a sheaf for, over.";
    case Id::SheafCancelOut: {                   /* :1080 — text "cancelled|reverted" */
      const std::size_t bar = line.text.find('|');
      const std::string a = line.text.substr(0, bar);
      const std::string b =
          bar == std::string::npos ? "" : line.text.substr(bar + 1);
      return "CANCEL " + upperAscii(a) + " SHEAF " EMD " " + upperAscii(b) +
             " SHEAF, OUT.";
    }
    case Id::SheafOut:                           /* :1087 */
      return upperAscii(line.text) + " SHEAF, OUT.";
  }
  return "";   /* unreachable — every id is covered */
}

// port of the log(..., 'sys') texts at the cited 10-mission.js sites.
void FdcMissionSink::sysLine(const sim::SysLine& line) {
  net_.line(NetLineKind::Sys, "", composeSys(line));
}

std::string FdcMissionSink::composeSys(const sim::SysLine& line) const {
  using Id = sim::SysLineId;
  switch (line.id) {
    case Id::InterceptDrill:                     /* :673-677 */
      return "Intercept drill: guns are laid on your point and waiting. Time of flight ~" +
             jsNum(line.num0) + " s, FIRE-to-shot ~" + jsNum(line.num1) +
             " s " EMD " call it " + jsNum(line.num2) +
             " s from your FIRE to splash. Estimate the column's speed (trucks on this road make roughly 3-4 m/s), multiply by " +
             jsNum(line.num2) +
             ", and mark the TRIGGER POINT that far up the road BEFORE your intercept point. The lead vehicle touches it " EMD " you say FIRE. Not before, not after.";
    case Id::CancelAmcReverting:                 /* :730 */
      return "CANCEL AT MY COMMAND " EMD " reverting to WHEN READY. The round already laid goes now.";
    case Id::DoNotLoadGuidance:                  /* :742 */
      return "DO NOT LOAD " EMD " the guns stay empty. Send a correction or FIRE FOR EFFECT to resume.";
    case Id::PriorityHint:                       /* :306-307 */
      return "Priority target set: " + line.text +
             ". Re-engage it with \"FIRE TARGET " + line.text +
             ", OVER\" and the rounds come fast " EMD " any other filed target waits for the re-lay.";
    case Id::PlanTargetHint:                     /* :340-341 */
      return line.text + " is on file " EMD " no rounds spent. Two words bring it down when they come: \"FIRE TARGET " +
             line.text +
             ", OVER\". Make it the PRIORITY TARGET and the guns stay laid on it.";
    case Id::FpfInstructions:                    /* :441-442 */
      return "Adjust each gun onto the line: \"NUMBER 1, RIGHT 20, DROP 25\" moves it (a marking round follows); \"NUMBER 2, REPEAT\" re-fires a piece unmoved. When the assault crosses the line: \"FIRE THE FPF\".";
    case Id::MarkingRoundCompromised:            /* :394 */
      return "That marking round landed INSIDE the friendly position. The FPF is compromised " EMD " the failure is recorded and rides the mission.";
    case Id::SuppressNoDuration:                 /* :580-582 */
      return "A suppress mission carries a duration " EMD " \"SUPPRESS TARGET " +
             line.text + ", 10 MINUTES, OVER\". Firing anyway.";
    case Id::TimeOnTargetFormatNote:             /* :759-761 */
      return "TIME ON TARGET " + jsNum(line.num0) +
             " acknowledged. Note: this trainer models a single firing unit, so TOT is accepted and graded as format but does not change impact timing.";
    case Id::SuppressedOnlyGuidance:             /* :906-908 */
      return "Target is SUPPRESSED " EMD " heads down, not casualties, and it wears off. Your call: continue the mission (send a correction, or REPEAT for another volley) or close with \"END OF MISSION, TARGET SUPPRESSED\" if suppression is all you needed.";
    case Id::NoEffectGuidance:                   /* :911-912 */
      return "No appreciable effect on the target. Refine your correction and REPEAT (\"right 50, repeat\"), or close with END OF MISSION and own the result.";
    case Id::FailureRecordedGuidance:            /* :1216-1218 */
      return "This mission is now a FAILURE " EMD " permanently, no stars, and the AAR will say why. But the target is still in the fight and the mission is still yours: adjust, get effect, and close it out with END OF MISSION.";
    case Id::AlertedMoving:                      /* :1251 */
      return "The infantry heard that one " EMD " they are moving and going flat.";
    case Id::DispersedProne:                     /* :1268-1270 */
      return "They have dispersed and gone prone. Casualty effect per round is now less than half " EMD " a volley that would have destroyed them standing may only neutralize, or worse. Adjust faster next time.";
    case Id::SpottingMasked:                     /* :1314 */
      return "SPOTTING: burst not observed " EMD " masked by terrain. Adjust off the map and the sound.";
    case Id::OtDirReminderMasked:                /* :1319-1320 */
      return "Before your first correction: send OT DIRECTION (\"DIRECTION 5920, OVER\"). The battery cannot orient LEFT/RIGHT or ADD/DROP without your observer-target line.";
    case Id::SpottingNormal: {                   /* :1326-1327 */
      const std::string lr = line.num0 >= 0 ? "RIGHT" : "LEFT";
      return "SPOTTING: burst " + fixed0Abs(line.num0) + " mils " + lr +
             " of target, spotting " + upperAscii(line.text) + ". OT range " +
             jsNum(js::round(line.num2)) + " m " ARR " OT factor " +
             jsNum(line.num1) +
             ". Deviation = mils " MULX " OT factor, to the nearest 10. Range: bracket it.";
    }
    case Id::SpottingEasy: {                     /* :1329-1336 */
      const std::string lr = line.num0 >= 0 ? "RIGHT" : "LEFT";
      /* Payload gap (fdc_sink.h header note): when a bracket is held the
         over/short word is not in the payload; the header slot prints
         BRACKETED instead. Text-only, never diffed. */
      const std::string os = upperAscii(line.text);
      const std::string devTxt =
          line.num2 == 0
              ? fixed0Abs(line.num0) + " mils " MULX " " + jsNum(line.num1) +
                    " is under the 30 m minimum " EMD " deviation is good, do not call it"
              : fixed0Abs(line.num0) + " mils " MULX " " + jsNum(line.num1) +
                    " = " +
                    jsNum(std::fabs(js::round(line.num0 * line.num1))) +
                    " " ARR " call " +
                    (line.num2 > 0 ? "RIGHT " : "LEFT ") +
                    jsNum(std::fabs(line.num2));
      const std::string brk =
          line.text == "bracketed"
              ? "You have a bracket " EMD " halve your last range correction"
              : "Spotting " + os + " " EMD " bracket it: " +
                    (os == "SHORT" ? "ADD" : "DROP") +
                    " enough to land on the far side";
      return "SPOTTING (EASY): burst " + fixed0Abs(line.num0) + " mils " + lr +
             ", " + os + ". OT factor " + jsNum(line.num1) + " (" +
             jsNum(js::round(line.num3)) + " m " DIVS " 1000). Deviation: " +
             devTxt + ". Range: " + brk + ".";
    }
    case Id::OtDirReminder:                      /* :1343-1345 */
      return "Before your first correction: send OT DIRECTION (\"DIRECTION 5920, OVER\" " EMD " grid azimuth to the target, nearest 10 mils). The battery cannot compute LEFT/RIGHT or ADD/DROP without your observer-target line, and will refuse the correction.";
  }
  return "";
}

void FdcMissionSink::stateChanged(sim::MissionPhase phase, double simNow) {
  if (tail_) tail_->stateChanged(phase, simNow);
}
void FdcMissionSink::impact(const sim::ImpactEvent& ev) {
  if (tail_) tail_->impact(ev);
}
void FdcMissionSink::presentation(const sim::PresentationEvent& ev) {
  if (tail_) tail_->presentation(ev);
}
// 10-mission.js:1271 — sendSpotReport('displace') re-cues the observer.
void FdcMissionSink::spotReportRequested(sim::SpotReportReason reason) {
  (void)reason;   /* Displace is the only reason the machine raises */
  if (spot_) spot_->sendSpotReport(SpotReason::Displace);
  if (tail_) tail_->spotReportRequested(reason);
}

} }
