#include "shitfire/campaign/grading.h"

#include <algorithm>
#include <cmath>

/* port of src/js/15-campaign-menu.js:240-314 (oracle-v1). */
namespace shitfire { namespace campaign {

/* :247-253 — correctionEfficiency. */
std::optional<double> correctionEfficiency(const std::vector<double>& tr) {
  if (tr.size() < 2) return std::nullopt;             /* :249 (!tr || length<2) */
  double sum = 0;
  for (std::size_t i = 1; i < tr.size(); i++)
    sum += (tr[i - 1] - tr[i]) / std::max(tr[i - 1], 1.0);   /* :251 */
  return sum / static_cast<double>(tr.size() - 1);    /* :252 */
}

/* {easy:3, normal:4, hard:5}[d] || 5 (:313). */
double difficultyCap(const std::string& difficulty) {
  if (difficulty == "easy") return 3;
  if (difficulty == "normal") return 4;
  if (difficulty == "hard") return 5;
  return 5;                                           /* JS undefined || 5 */
}

/* :254-314 — gradeMission. */
double gradeMission(const GradeMissionView& m, const GradeScenarioView& S,
                    const ChapterSpec* activeChapter,
                    const sim::EffectAssessment& effect, double simNow) {
  /* :262 — any failReason (fratricide/collateral/escaped/overrun) is 0. */
  if (!m.failReason.empty()) return 0;
  if (m.intent == "illum") {                          /* :263-265 (12h) */
    if (m.roundsFired == 0) return 0;
  } else if (S.type == "kaiju") {                     /* :266-267 (11b) */
    if (effect.outcome != sim::EffectOutcome::Destroyed) return 0;
  } else {
    /* :268-277 — UX11: the star floor is 1 for a mission properly closed
       with RREMS; auto-fails already returned 0 above. */
    if (effect.outcome == sim::EffectOutcome::None) return 1;
    if (effect.outcome == sim::EffectOutcome::Suppressed && m.intent != "suppress")
      return 1;
  }
  double stars = 1;                                   /* :278 */
  if (m.notesCount == 0) stars++;                     /* :279 */
  if (m.adjustRounds <= 2) stars++;                   /* :280 */
  if (m.aimErr0 && *m.aimErr0 <= 200) stars++;        /* :281 — JS null check */
  const double par =
      (activeChapter && activeChapter->par && *activeChapter->par != 0)
          ? *activeChapter->par
          : 300;                                      /* :282 — (ch && par) || 300 */
  const double tEnd = (m.tEnd && *m.tEnd != 0) ? *m.tEnd : simNow;  /* JS falsy */
  if (tEnd - m.tStart <= par) stars++;                /* :283 */
  /* :284-291 — doctrinal deductions. */
  if (m.tInit > 120) stars = std::min(stars, 4.0);    /* :288 */
  if (m.wasted >= 2) stars = std::min(stars, 3.0);    /* :289 */
  const std::optional<double> eff = correctionEfficiency(m.missTrace);  /* :290 */
  if (eff && *eff < 0.25 && m.adjustRounds >= 3) stars = std::min(stars, 3.0); /* :291 */
  /* :292-293 — chapter method requirement (2.1/2.2). */
  if (activeChapter && !activeChapter->method.empty() && !m.method.empty() &&
      m.method != activeChapter->method)
    stars = std::min(stars, 2.0);
  /* :296 — TEMPO2: reqImmediate grades the one-transmission call. */
  if (activeChapter && activeChapter->reqImmediate && !m.imm)
    stars = std::min(stars, 2.0);
  /* :301 — TEMPO4: reqIllum demands light before (or during) the HE work. */
  if (activeChapter && activeChapter->reqIllum && !m.usedIllum)
    stars = std::min(stars, 2.0);
  /* :306 — SUGG4b: reqPlanned grades the planned-fires initiation. */
  if (activeChapter && activeChapter->reqPlanned && !m.planned)
    stars = std::min(stars, 2.0);
  /* :312 — ENEMY1: a star per two casualty rounds the battery took, floor 1. */
  if (S.btyCas != 0)
    stars = std::max(1.0, stars - std::floor(S.btyCas / 2));
  return std::min(stars, difficultyCap(S.difficulty));  /* :313 */
}

} }
