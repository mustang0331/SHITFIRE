#include "shitfire/review/aar_analysis.h"

#include <cmath>
#include <cstdio>

#include "shitfire/math/jsemantics.h"

/* port of src/js/13-aar.js:213-470 (oracle-v1), analysis half — see the
   header for the DOM/canvas exclusions. */
namespace shitfire { namespace review {

namespace {

/* UTF-8 for the typographic characters the verdict/diagnosis strings carry
   (house style, scenario_gen.cpp:22-24). */
const char* const DASH = "\xE2\x80\x94";   /* em dash */
const char* const LE = "\xE2\x89\xA4";     /* less-than-or-equal sign */
const char* const GE = "\xE2\x89\xA5";     /* greater-than-or-equal sign */
const char* const STAR = "\xE2\x98\x85";   /* black star */

/* JS template-literal number rendering for the integral values these
   strings interpolate (same helper as tlog.cpp — file-local by design). */
std::string jsNum(double v) {
  if (std::isfinite(v) && std::floor(v) == v && std::fabs(v) < 9.007199254740992e15) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
    return buf;
  }
  char buf[40];
  std::snprintf(buf, sizeof buf, "%.17g", v);
  return buf;
}

}  // namespace

/* 13-aar.js:235-238 — fmtTime. */
std::string fmtTime(double s) {
  const double m = std::floor(s / 60);
  const double r = std::floor(std::fmod(s, 60));   /* JS % on doubles */
  char buf[48];
  std::snprintf(buf, sizeof buf, "%lld:%02lld", static_cast<long long>(m),
                static_cast<long long>(r));
  return buf;
}

/* 10-mission.js:1102-1104 — the outcome strings. */
std::string outcomeName(sim::EffectOutcome o) {
  switch (o) {
    case sim::EffectOutcome::Destroyed: return "destroyed";
    case sim::EffectOutcome::Neutralized: return "neutralized";
    case sim::EffectOutcome::Suppressed: return "suppressed";
    default: return "none";
  }
}

/* 13-aar.js:320-328 — BEST update. */
bool bestUpdate(BestScores& best, const std::string& type,
                const std::string& difficulty, double adjustRounds, double dur,
                bool pass) {
  if (!pass) return false;                            /* :320 */
  const std::string key = type + ":" + difficulty;    /* :321 */
  BestScore* prev = nullptr;
  for (auto& kv : best.data)
    if (kv.first == key) { prev = &kv.second; break; }
  const bool better =
      !prev || adjustRounds < prev->rounds ||
      (adjustRounds == prev->rounds && dur < prev->time);   /* :323-324 */
  if (!better) return false;
  BestScore s;
  s.rounds = adjustRounds;
  s.time = js::round(dur);                            /* :325 — stored rounded */
  if (prev) *prev = s;
  else best.data.emplace_back(key, s);
  return true;                                        /* :326 — BEST.save() is I/O */
}

/* 13-aar.js:239-403 minus DOM — analyzeAar. */
AarReport analyzeAar(AarMissionView& m, const AarScenarioView& S,
                     const campaign::ChapterSpec* activeChapter,
                     const sim::EffectAssessment& a,
                     const sim::EffBandSet* bands, double simNow,
                     double passMaxAdjustRounds, BestScores& best,
                     campaign::CampState& camp) {
  AarReport r;

  /* :247-250 — TEMPO4: the reqIllum shortfall lands once, before grading
     reads m.notes. */
  if (activeChapter && activeChapter->reqIllum && !m.g.usedIllum &&
      !m.reqIllumNoted) {
    m.reqIllumNoted = true;
    m.notes.push_back(std::string(
        "This chapter demanded COORDINATED ILLUMINATION ") + DASH +
        " you fought it dark. Effective, maybe, but the skill under test was "
        "the light (chapter capped at 2" + STAR + ").");
  }
  /* :253-256 — SUGG4b: same shape for the planned-fires requirement. */
  if (activeChapter && activeChapter->reqPlanned && !m.g.planned &&
      !m.reqPlannedNoted) {
    m.reqPlannedNoted = true;
    m.notes.push_back(std::string(
        "This chapter demanded PLANNED FIRES ") + DASH +
        " plan targets while it is quiet, open in two words when they come. "
        "You fought it cold with a full call (chapter capped at 2" + STAR + ").");
  }
  /* :259-263 — ENEMY1: casualties countable only at the end; cost, never a
     fail. */
  if (S.btyCas != 0 && !m.btyCasNoted) {
    m.btyCasNoted = true;
    m.notes.push_back(
        "The battery put " + jsNum(S.btyCas) + " round" +
        (S.btyCas == 1 ? "" : "s") +
        " inside the friendly position while you worked. Friendly casualties "
        "taken under enemy fire cost you a star per two rounds " + DASH +
        " silence the gun faster.");
  }
  m.g.notesCount = static_cast<double>(m.notes.size());

  const bool isConvoy = S.type == "convoy";           /* :264 */
  if (isConvoy) m.hits = S.vehDeadCount;              /* :265 */

  /* :271-273 — accomplished (G13). */
  const bool accomplished =
      (m.g.intent == "illum" && m.g.roundsFired > 0) ||
      a.outcome == sim::EffectOutcome::Destroyed ||
      a.outcome == sim::EffectOutcome::Neutralized ||
      (a.outcome == sim::EffectOutcome::Suppressed && m.g.intent == "suppress");

  const bool frat = m.g.failReason == "fratricide";   /* :275 */
  const bool collat = m.g.failReason == "collateral"; /* :276 */
  const bool escaped = m.g.failReason == "escaped";   /* :277 */
  const bool overrun = m.g.failReason == "overrun";   /* :278 — ENEMY2 */
  r.pass = !frat && !collat && !escaped && !overrun && accomplished &&
           m.g.adjustRounds <= passMaxAdjustRounds;   /* :279 */
  {
    const char* letters[] = { "A", "B", "C", "D" };   /* :280 */
    int n = static_cast<int>(m.notes.size());
    r.gradeLetter = letters[n < 3 ? n : 3];
  }
  const double tEnd = (m.g.tEnd && *m.g.tEnd != 0) ? *m.g.tEnd : simNow;
  r.dur = tEnd - m.g.tStart;                          /* :281 */

  /* :282-319 — the verdict chain. */
  const std::string pct = jsNum(a.pct);
  if (frat) {
    r.verdict = std::string("FAIL ") + DASH + " FRATRICIDE";
    r.why = "Rounds impacted a friendly element. Automatic failure.";
  } else if (collat) {
    r.verdict = std::string("FAIL ") + DASH + " CIVILIAN CASUALTIES";
    r.why = "Rounds impacted a civilian village. Collateral damage is an "
            "automatic failure, same as fratricide.";
  } else if (escaped) {
    r.verdict = S.type == "kaiju"
                    ? std::string("FAIL ") + DASH + " LANDFALL"
                    : std::string("FAIL ") + DASH + " CONVOY ESCAPED";
    r.why = S.type == "kaiju"
                ? "The crab reached the village. The report will describe it "
                  "as \"weather\"."
                : "Fewer than three vehicles destroyed before the column ran "
                  "off the map.";
  } else if (overrun) {                               /* :291-294 */
    r.verdict = std::string("FAIL ") + DASH + " POSITION OVERRUN";
    r.why = "The assault reached the tower before you broke it. The last "
            "correction you never sent was the one that mattered.";
  } else if (S.type == "kaiju" &&
             a.outcome != sim::EffectOutcome::Destroyed) {  /* :295-298 — 11b */
    r.verdict = std::string("FAIL ") + DASH + " IT IS STILL WALKING";
    r.why = "Assessed effect " + pct + "% of the " +
            jsNum(bands ? bands->destroyPct : 0) +
            "% that stops it. It does not do \"combat-ineffective\". It is a "
            "crab.";
  } else if (m.g.intent == "illum") {                 /* :299-303 — 12h */
    const bool lit = m.g.roundsFired > 0;
    r.verdict = lit ? std::string("PASS ") + DASH + " ILLUMINATION PROVIDED"
                    : std::string("FAIL ") + DASH + " NO ILLUMINATION FIRED";
    r.why = lit ? jsNum(m.g.roundsFired) + " illumination round" +
                      (m.g.roundsFired == 1 ? "" : "s") + " over the area."
                : "Mission closed without a round in the air.";
  } else if (a.outcome == sim::EffectOutcome::None) { /* :304-311 */
    r.verdict = isConvoy
                    ? std::string("FAIL ") + DASH + " CONVOY STILL EFFECTIVE"
                    : std::string("FAIL ") + DASH + " TARGET STILL EFFECTIVE";
    r.why = isConvoy
                ? "Only " + jsNum(m.hits) + "/4 vehicles destroyed (need 3)."
                : (m.g.roundsFired > 0
                       ? "Assessed effect " + pct + "% " + DASH + " below the " +
                             jsNum(bands ? bands->neutralizePct : 0) +
                             "% that renders a unit combat-ineffective "
                             "(FM 6-30). The fires never touched them."
                       : "Mission ended without a round fired.");
  } else if (a.outcome == sim::EffectOutcome::Suppressed && !accomplished) {
    r.verdict = std::string("MARGINAL ") + DASH + " TARGET SUPPRESSED ONLY"; /* :312-315 */
    r.why = "Heads went down while the rounds fell (" + pct + "% casualties " +
            DASH + " neutralization needs " +
            jsNum(bands ? bands->neutralizePct : 0) +
            "%), and suppression wears off. They are back on their guns. You "
            "had the option to continue: correction, REPEAT.";
  } else if (a.outcome == sim::EffectOutcome::Suppressed) {   /* :316 */
    r.verdict = std::string("PASS ") + DASH + " TARGET SUPPRESSED";
    r.why = "Suppression was the mission. They stopped shooting while it "
            "mattered.";
  } else if (!r.pass) {                               /* :317 */
    r.verdict = std::string("MARGINAL ") + DASH + " TOO MANY ADJUSTING ROUNDS";
    r.why = "Target " + outcomeName(a.outcome) + ", but " +
            jsNum(m.g.adjustRounds) + " adjusting rounds used (pass " + LE +
            " " + jsNum(passMaxAdjustRounds) + ").";
  } else if (a.outcome == sim::EffectOutcome::Destroyed) {    /* :318 */
    r.verdict = isConvoy ? std::string("PASS ") + DASH + " CONVOY DESTROYED"
                         : std::string("PASS ") + DASH + " TARGET DESTROYED";
    r.why = isConvoy ? ""
                     : "Assessed " + pct + "% casualties " + DASH + " past the " +
                           jsNum(bands ? bands->destroyPct : 0) +
                           "% that takes a unit off the books permanently.";
  } else {                                            /* :319 */
    r.verdict = std::string("PASS ") + DASH + " TARGET NEUTRALIZED";
    r.why = "Assessed " + pct + "% casualties " + DASH +
            " combat-ineffective (" + GE +
            jsNum(bands ? bands->neutralizePct : 0) +
            "%), short of destruction (" +
            jsNum(bands ? bands->destroyPct : 0) + "%).";
  }

  /* :320-328 — skirmish BEST. */
  r.bestWritten =
      bestUpdate(best, S.type, S.difficulty, m.g.adjustRounds, r.dur, r.pass);

  /* :329-344 — adjustment-doctrine diagnosis. */
  const std::vector<double>& tr = m.g.missTrace;      /* :330 */
  r.corrEff = campaign::correctionEfficiency(tr);     /* :331 */
  if (m.g.tInit > 120)                                /* :333-334 */
    r.diag.push_back("Call for fire took " + fmtTime(m.g.tInit) +
                     " to initiate (standard: within 2:00 of identifying the "
                     "target). Locate faster " + DASH +
                     " the target is not waiting for you.");
  if (m.g.aimErr0 && *m.g.aimErr0 > 200)              /* :335-336 */
    r.diag.push_back("Initial target location was " +
                     jsNum(js::round(*m.g.aimErr0)) + " m off (standard: " +
                     LE + "200 m). Everything after that was recovery " + DASH +
                     " work the map, or lase and read the grid, before you "
                     "transmit.");
  if (r.corrEff && *r.corrEff < 0.3 && tr.size() >= 3)   /* :337-338 */
    r.diag.push_back("Correction efficiency " +
                     jsNum(js::round(*r.corrEff * 100)) + "% per round " +
                     DASH + " doctrine's successive bracketing roughly halves "
                     "the miss each time (~50%). Your corrections were too "
                     "small for the error you were seeing.");
  if (tr.size() >= 3 && !(m.sidesLong && m.sidesShort))  /* :339-340 */
    r.diag.push_back(std::string(
        "You never bracketed the target ") + DASH + " every adjusting round "
        "landed on the same side of it. Fire a correction big enough to cross "
        "the target, then split the difference.");
  if (m.g.wasted >= 2)                                /* :341-342 */
    r.diag.push_back(jsNum(m.g.wasted) +
                     " adjusting rounds changed the miss by less than 15% " +
                     DASH + " rounds and time spent for nothing.");
  if (r.diag.empty() && r.pass && m.g.adjustRounds <= 3)   /* :343-344 */
    r.diag.push_back("Clean adjustment: bold first correction, bracket, "
                     "split, effect. That is the method.");

  /* :345-350 — career record (feeds HELLHOUND's continuity quips). */
  campaign::CareerRecord& car = campaign::careerRecord(camp);
  car.missions++;
  if (frat) car.frat++;
  if (collat) car.collat++;
  /* CAMP.save() (:350) is I/O — the caller persists. */

  /* :352-363 — commendations. */
  const double parC =
      (activeChapter && activeChapter->par && *activeChapter->par != 0)
          ? *activeChapter->par
          : 300;                                      /* :352 */
  if (r.pass) {
    if (m.g.adjustRounds <= 1)
      r.medals.push_back(std::string("HELL OF A HOLE IN ONE ") + DASH +
                         " effect after a single adjusting round");
    if (m.g.aimErr0 && *m.g.aimErr0 <= 50)
      r.medals.push_back(std::string("EAGLE EYE ") + DASH +
                         " initial location within 50 m");
    if (m.notes.empty())
      r.medals.push_back(std::string("ACTUALLY READ THE MANUAL ") + DASH +
                         " clean call format, no coaching");
    if (r.dur <= parC * 0.6)
      r.medals.push_back(std::string("FAST MOVER ") + DASH +
                         " done in well under par");
    if (activeChapter && activeChapter->strict)
      r.medals.push_back(std::string("IRON NET ") + DASH +
                         " passed under strict doctrine");
    if (m.refined || m.recorded)
      r.medals.push_back(std::string("BY THE BOOK ") + DASH +
                         " closed the mission with refinement / record as "
                         "target (RREMS)");
    if (m.otDirSent)
      r.medals.push_back(std::string("ORIENTED ") + DASH +
                         " OT direction passed to the FDC");
    if (activeChapter && activeChapter->asset == "mortar60")
      r.medals.push_back(std::string("TEN-METER MAN ") + DASH +
                         " precision work with the sixty");
  }

  /* the star grade — the source calls gradeMission twice (:379 payload,
     :398 star row) with identical pure inputs; computed once here. */
  std::optional<double> stars;
  if (!m.noDisp)
    stars = campaign::gradeMission(
        m.g, campaign::GradeScenarioView{ S.type, S.difficulty, S.btyCas },
        activeChapter, a, simNow);

  /* :364-379 — the TLOG aar payload (G13 graded-outcome fields). */
  TlogEntry& e = r.tlogAar;
  e.kind = "aar";
  e.call = "";
  e.msg = r.verdict;
  e.rounds = m.g.adjustRounds;
  e.dur = js::round(r.dur);
  e.outcome = outcomeName(a.outcome);                 /* :366 */
  e.effPct = a.pct;
  e.intent = m.g.intent;
  if (!m.sheafKind.empty()) {                         /* :367 */
    e.sheaf = m.sheafKind;
    e.sheafSrc = m.sheafSource;
  }
  if (!m.fuzeKind.empty()) {                          /* :368 */
    e.fuze = m.fuzeKind;
    e.fuzeSrc = m.fuzeSource;
  }
  if (!m.shell.empty() && m.shell != "he") e.shell = m.shell;   /* :369 */
  if (!m.bdaClaim.empty()) e.bdaClaim = m.bdaClaim;   /* :370 */
  e.firstMiss.present = true;                         /* :371 — null or rounded */
  if (m.firstMiss) e.firstMiss.value = js::round(*m.firstMiss);
  e.aimErr0.present = true;                           /* :372 */
  if (m.g.aimErr0) e.aimErr0.value = js::round(*m.g.aimErr0);
  e.tInit = js::round(m.g.tInit);                     /* :373 */
  e.corrEff.present = true;                           /* :374 */
  if (r.corrEff) e.corrEff.value = js::round(*r.corrEff * 100);
  e.bracketed = m.sidesLong && m.sidesShort;          /* :375 */
  e.trace = m.g.missTrace;                            /* :376 — slice() */
  e.notes = static_cast<double>(m.notes.size());      /* :377 */
  if (m.noDisp) e.noDisp = true;                      /* :378 */
  if (stars) e.stars = *stars;                        /* :379 — UX6 */

  /* :381-403 minus DOM — G3: a no-dispersion mission records nothing (and
     deliberately does NOT record zero); otherwise grade and, on a chapter,
     persist the best (skirmish never writes campaign progress, UX6). */
  r.stars = stars;
  if (!m.noDisp && activeChapter)
    r.campRecorded =
        campaign::campRecord(camp, activeChapter->id, S.difficulty, *stars);

  return r;
}

} }
