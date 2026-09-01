#include "shitfire/campaign/campaign_state.h"

#include <algorithm>

/* port of src/js/15-campaign-menu.js:194-239 + 13-aar.js:346 (oracle-v1).
   See campaign_state.h for the shape notes. */
namespace shitfire { namespace campaign {

namespace {

const ChapterStars* findRow(const CampState& s, const std::string& chId) {
  for (const auto& kv : s.data)
    if (kv.first == chId) return &kv.second;
  return nullptr;
}

double& diffSlot(ChapterStars& d, const std::string& diff) {
  /* the three difficulty keys are the only callers at oracle-v1 (see header);
     anything else falls through to 'normal' deliberately never happens. */
  if (diff == "easy") return d.easy;
  if (diff == "hard") return d.hard;
  return d.normal;
}

}  // namespace

/* 15-campaign-menu.js:211-214 — stars(chId). */
double campStars(const CampState& s, const std::string& chId) {
  const ChapterStars* d = findRow(s, chId);
  if (!d) return 0;
  return std::max(d->easy, std::max(d->normal, d->hard));
}

/* 15-campaign-menu.js:215-219 — record(chId, diff, stars). */
bool campRecord(CampState& s, const std::string& chId, const std::string& diff,
                double stars) {
  if (stars <= 0) return false;                       /* :216 */
  ChapterStars* d = nullptr;
  for (auto& kv : s.data)
    if (kv.first == chId) { d = &kv.second; break; }
  if (!d) {                                           /* :217 — get-or-create */
    s.data.emplace_back(chId, ChapterStars{});
    d = &s.data.back().second;
  }
  double& slot = diffSlot(*d, diff);
  if (slot < stars) {                                 /* :218 — (d[diff]||0) < stars */
    slot = stars;
    return true;                                      /* the JS saves here */
  }
  return false;
}

/* 15-campaign-menu.js:220-224 — total(): sum over ALL_CHAPTERS. */
double campTotal(const CampState& s) {
  double t = 0;
  for (const ChapterSpec* ch : allChapters()) t += campStars(s, ch->id);
  return t;
}

/* 15-campaign-menu.js:230 — volUnlocked. */
bool volUnlocked(const CampState& s, const VolumeSpec& vol) {
  return s.devUnlock || campTotal(s) >= vol.need;
}

/* 15-campaign-menu.js:231-238 — chUnlocked. */
bool chUnlocked(const CampState& s, const VolumeSpec& vol, const ChapterSpec& ch) {
  if (!ch.impl) return false;                         /* :232 */
  if (s.devUnlock) return true;                       /* :233 */
  if (!volUnlocked(s, vol)) return false;             /* :234 */
  /* :235-237 — index among the volume's implemented chapters; first is open,
     later ones need the previous implemented chapter starred (> 0). */
  const ChapterSpec* prev = nullptr;
  for (const ChapterSpec& c : vol.chapters) {
    if (!c.impl) continue;
    if (&c == &ch) return prev == nullptr || campStars(s, prev->id) > 0;
    prev = &c;
  }
  return false;                                       /* ch not in vol (JS i = -1) */
}

/* 15-campaign-menu.js:239 — starStr. */
std::string starStr(double n) {
  int full = static_cast<int>(n);
  if (full < 0) full = 0;
  if (full > 5) full = 5;
  std::string out;
  for (int i = 0; i < full; i++) out += "\xE2\x98\x85";      /* ★ */
  for (int i = full; i < 5; i++) out += "\xE2\x98\x86";      /* ☆ */
  return out;
}

/* 13-aar.js:346 — CAMP.data._career || (CAMP.data._career = {frat:0, collat:0,
   missions:0}). */
CareerRecord& careerRecord(CampState& s) {
  if (!s.career) s.career = CareerRecord{};
  return *s.career;
}

} }
