#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "shitfire/campaign/campaign_data.h"
#include "shitfire/campaign/campaign_state.h"
#include "shitfire/campaign/grading.h"
#include "shitfire/review/tlog.h"
#include "shitfire/sim/mission_effects.h"

/* The AAR analysis half — port of src/js/13-aar.js:213-470 (oracle-v1) MINUS
   everything canvas/DOM: verdict + why assembly, the pre-grading note
   appends, pass/grade-letter, skirmish BEST bookkeeping, the adjustment-
   doctrine diagnosis lines, the career record, commendations, the TLOG aar
   payload and the star grade. The shot-plot renderer (renderShotPlot,
   :1-211) and the aarBox HTML (:381-468) are presentation (P6). fmtTime
   (:235-238) is here because 13-aar.js owns it.

   The JS showAAR reads globals (mission, Scenario, activeChapter, sim.now,
   CONFIG.MISSION, TLOG, BEST, CAMP) and calls assessEffect()/effBands();
   the port takes all of them as parameters. analyzeAar MUTATES its inputs
   exactly where the source does: m.notes (the reqIllum/reqPlanned/btyCas
   appends, :247-263), m.hits (convoy, :265), best (:320-328), camp (career
   :345-350, chapter stars :399).

   All bodies live in aar_analysis.cpp. JS numbers stay double. */
namespace shitfire { namespace review {

/* 13-aar.js:235-238 — fmtTime(s): "m:ss". */
std::string fmtTime(double s);

/* The JS outcome strings for the committed enum (10-mission.js:1102-1104):
   'none' | 'suppressed' | 'neutralized' | 'destroyed'. */
std::string outcomeName(sim::EffectOutcome o);

/* BEST — skirmish best scores per 'type:difficulty' key (13-aar.js:217-233
   shape; localStorage 'shitfire_best' I/O is the caller's). */
struct BestScore {
  double rounds = 0;   /* adjusting rounds */
  double time = 0;     /* Math.round(dur) at store time (:325) */
};
struct BestScores {
  std::vector<std::pair<std::string, BestScore>> data;  /* insertion order */
};

/* 13-aar.js:320-328 — the pass-gated better-of update (fewer rounds, then
   faster; dur compared RAW, stored rounded). Returns true when written. */
bool bestUpdate(BestScores& best, const std::string& type,
                const std::string& difficulty, double adjustRounds, double dur,
                bool pass);

/* The mission fields the analysis half reads beyond the grading slice.
   String '' stands for the JS nulls throughout. */
struct AarMissionView {
  campaign::GradeMissionView g;    /* grading slice; g.notesCount is refreshed
                                      from notes.size() after the appends */
  bool sidesLong = false;          /* m.sides.long / m.sides.short */
  bool sidesShort = false;
  std::optional<double> firstMiss; /* raw; payload rounds it (:371) */
  std::string bdaClaim;            /* '' = null */
  std::string sheafKind, sheafSource;  /* '' kind = m.sheaf null (:367) */
  std::string fuzeKind, fuzeSource;    /* '' kind = m.fuze null (:368) */
  std::string shell = "he";        /* payload records it only when != 'he' */
  bool noDisp = false;             /* G3 — ungraded mission */
  bool refined = false, recorded = false;   /* RREMS (:360) */
  bool otDirSent = false;          /* :361 */
  double hits = 0;                 /* effect rounds in band; convoy: dead
                                      vehicles (recomputed here, :265) */
  std::vector<std::string> notes;  /* m.notes — appended to here */
  bool reqIllumNoted = false;      /* the :247-263 once-guards */
  bool reqPlannedNoted = false;
  bool btyCasNoted = false;
};

/* The scenario fields the analysis half reads. */
struct AarScenarioView {
  std::string type;
  std::string difficulty;
  double btyCas = 0;               /* :259-263, :312 */
  double vehDeadCount = 0;         /* convoy: S.veh.filter(v => v.dead).length */
};

struct AarReport {
  std::string verdict;             /* :282-319 */
  std::string why;
  bool pass = false;               /* :279 */
  std::string gradeLetter;         /* :280 — ['A','B','C','D'][min(notes,3)] */
  double dur = 0;                  /* :281 — raw (payload rounds it) */
  std::optional<double> corrEff;   /* :331 — raw (payload rounds *100) */
  std::vector<std::string> diag;   /* :329-344 */
  std::vector<std::string> medals; /* :352-363 — the 8 commendations */
  TlogEntry tlogAar;               /* :364-379 — kind 'aar', msg = verdict,
                                      extras engaged exactly as recorded */
  std::optional<double> stars;     /* gradeMission; absent when noDisp */
  bool bestWritten = false;        /* :320-328 outcome */
  bool campRecorded = false;       /* :399 — chapter best raised */
};

/* 13-aar.js:239-403 minus DOM — see header note. `a` is assessEffect()'s
   result at mission end; `bands` is effBands() for the engagement (may be
   null for convoy, :274 — no non-convoy verdict path reads it null).
   passMaxAdjustRounds is CONFIG.MISSION.passMaxAdjustRounds. The returned
   tlogAar is the entry payload — the caller logs it through
   TranscriptLog::add exactly as TLOG.add('aar', '', verdict, extras). */
AarReport analyzeAar(AarMissionView& m, const AarScenarioView& S,
                     const campaign::ChapterSpec* activeChapter,
                     const sim::EffectAssessment& a,
                     const sim::EffBandSet* bands, double simNow,
                     double passMaxAdjustRounds, BestScores& best,
                     campaign::CampState& camp);

} }
