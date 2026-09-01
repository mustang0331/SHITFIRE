#pragma once
#include <optional>
#include <string>
#include <vector>

#include "shitfire/campaign/campaign_data.h"
#include "shitfire/sim/mission_machine.h"   /* EffectAssessment (assessEffect view) */

/* The competency rubric — port of src/js/15-campaign-menu.js:240-314
   (oracle-v1): correctionEfficiency() and gradeMission(), the metrics ->
   0-5 star grading with difficulty caps.

   The JS gradeMission reads globals (Scenario, activeChapter, sim.now) and
   calls assessEffect(); the port takes them as parameters. assessEffect()'s
   result is passed in ONCE — the source calls it up to twice per grade
   (:265, :267) with an identical result (it is pure over unchanged
   mission/scenario state, 10-mission.js:1089-1106).

   All bodies live in grading.cpp. JS numbers stay double. */
namespace shitfire { namespace campaign {

/* The mission fields gradeMission reads (:254-313). String fields use ''
   where the JS holds null; escaped/overrun appear in failReason even though
   the wave-1 FailReason enum stops at collateral — the scenario layer writes
   those two straight onto mission.failReason as strings
   (05-units-scenario.js:862/:886/:979/:1016), and grading only cares that
   the field is truthy (:262). */
struct GradeMissionView {
  std::string failReason;          /* '' | 'fratricide' | 'collateral' |
                                      'escaped' | 'overrun' */
  std::string intent;              /* 'destroy' | 'suppress' | 'illum' */
  double roundsFired = 0;          /* m.rounds.length (:264) */
  double adjustRounds = 0;         /* m.adjustRounds */
  double notesCount = 0;           /* m.notes.length (:279) */
  std::optional<double> aimErr0;   /* nullopt = JS null (:281) */
  double tStart = 0;
  std::optional<double> tEnd;      /* nullopt/0 = JS falsy -> sim.now (:283) */
  double tInit = 0;
  double wasted = 0;
  std::vector<double> missTrace;   /* the Math.round'ed miss distances
                                      (10-mission.js:1277) */
  std::string method;              /* '' = null; 'grid'|'polar'|'shift' */
  bool imm = false;                /* truthiness of m.imm (:296) */
  bool usedIllum = false;          /* :301 */
  bool planned = false;            /* :306 */
};

/* The scenario fields gradeMission reads. */
struct GradeScenarioView {
  std::string type;                /* S.type (:264 kaiju gate) */
  std::string difficulty;          /* 'easy'|'normal'|'hard' (:313) */
  double btyCas = 0;               /* S.btyCas (:312, battery only) */
};

/* :247-253 — correctionEfficiency(m): average fractional reduction in miss
   distance per adjusting round over missTrace; nullopt when fewer than two
   entries. Doctrine's successive bracketing ~0.5. */
std::optional<double> correctionEfficiency(const std::vector<double>& missTrace);

/* {easy:3, normal:4, hard:5}[difficulty] || 5 (:313; same map at :366 and
   13-aar.js:400). */
double difficultyCap(const std::string& difficulty);

/* :254-314 — gradeMission(m). activeChapter may be null (skirmish — UX6:
   grading runs clean without one, 13-aar.js:390-397). simNow substitutes for
   a falsy m.tEnd exactly as `(m.tEnd || sim.now)` does. */
double gradeMission(const GradeMissionView& m, const GradeScenarioView& S,
                    const ChapterSpec* activeChapter,
                    const sim::EffectAssessment& effect, double simNow);

} }
