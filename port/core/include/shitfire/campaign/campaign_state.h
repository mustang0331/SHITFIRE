#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "shitfire/campaign/campaign_data.h"

/* Campaign progression state — port of the CAMP persistence SHAPE
   (src/js/15-campaign-menu.js:194-238, oracle-v1) plus the career record
   showAAR keeps on it (13-aar.js:346-350). Pure data model: the browser's
   localStorage load()/save() (JSON under 'shitfire_campaign', BUILD3
   shape-guarded) is I/O and stays with the platform layer — (de)serialization
   of CampState is the caller's.

   In the JS, CAMP.data is one object holding chapter-id keys next to the
   '_devUnlock' and '_career' specials; the port splits those into typed
   fields (a serializer reassembles the JS shape). Chapter order is insertion
   order, kept explicit per the porting rules (key iteration).

   All bodies live in campaign_state.cpp. JS numbers stay double. */
namespace shitfire { namespace campaign {

/* CAMP.data[chId] — best stars per difficulty (:213, :217-218). 0 = none
   (JS `d.easy || 0`). */
struct ChapterStars {
  double easy = 0, normal = 0, hard = 0;
};

/* CAMP.data._career (13-aar.js:346) — feeds HELLHOUND's continuity quips
   (15-campaign-menu.js:634-639). */
struct CareerRecord {
  double frat = 0, collat = 0, missions = 0;
};

/* The CAMP.data object, typed. */
struct CampState {
  std::vector<std::pair<std::string, ChapterStars>> data;  /* insertion order */
  bool devUnlock = false;                 /* _devUnlock (:229, :509) */
  std::optional<CareerRecord> career;     /* _career — absent until first AAR */
};

/* CAMP.stars(chId) (:211-214) — best across difficulties; 0 when unknown. */
double campStars(const CampState& s, const std::string& chId);

/* CAMP.record(chId, diff, stars) (:215-219) — no-op for stars <= 0, only
   ever raises a difficulty's best. diff is 'easy'|'normal'|'hard' (any other
   string is booked as its own slot in the JS; here it is ignored — the three
   difficulties are the only callers at oracle-v1). Returns true when the
   record changed (the JS saves at that point). */
bool campRecord(CampState& s, const std::string& chId, const std::string& diff,
                double stars);

/* CAMP.total() (:220-224) — sum of stars over ALL_CHAPTERS (not over the
   stored keys: unknown ids never count). */
double campTotal(const CampState& s);

/* volUnlocked (:230) — DEV_UNLOCK || total >= vol.need. */
bool volUnlocked(const CampState& s, const VolumeSpec& vol);

/* chUnlocked (:231-238) — impl gate, volume gate, then the sequential chain:
   first implemented chapter of the volume, or previous implemented chapter
   starred (> 0 — completing, not starring high). */
bool chUnlocked(const CampState& s, const VolumeSpec& vol, const ChapterSpec& ch);

/* starStr (:239) — '★' x n + '☆' x (5-n), UTF-8. n is clamped to [0,5] the
   way String.repeat would throw otherwise (all callers pass 0-5). */
std::string starStr(double n);

/* 13-aar.js:346 — CAMP.data._career get-or-create. */
CareerRecord& careerRecord(CampState& s);

} }
