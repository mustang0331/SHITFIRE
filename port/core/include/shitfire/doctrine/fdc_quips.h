#pragma once
#include <array>
#include <string>
#include <vector>

/* FDC quip pools + pick() — port of src/js/07-voice-spot-audio.js:36-297
   (oracle-v1).

   Determinism contract (DETERMINISM_NOTE.md §a): quip LINE selection is
   SANCTIONED FLAVOR — pick() draws from an injected, harness-owned RNG
   (the browser's Math.random at 07:293), never from a mission PRNG stream.
   The replay comparator never diffs raw FDC-line lists, so the text here is
   carried as cited data; nothing scored reads it.

   Pool STRUCTURE is load-bearing (category ids + per-pool no-immediate-repeat
   rotation, QUIPS._last at 07:294-296); the line text is data. Bodies live in
   fdc_quips.cpp (port/README.md: nothing computational inline in a core
   header). */
namespace shitfire { namespace doctrine {

/* wave-3 reconciliation: the six probability-gated flavor sites
   (DETERMINISM_NOTE.md §a "gated-quip caveat"). Four of them air BEFORE a
   say() whose return time schedules impacts or shifts doctrinal-line times
   (ffeAck precedes SHOT at 10-mission.js:824-827; snideRound/corrSnark
   precede fireForEffect/fireAdjustRound at 12-cff-handling.js:714-721;
   readbackTail precedes fireMission at 12-cff:390-404), so a replay harness
   must reproduce the recorded PRESENCE of each gated line — the capture's
   Math.random stream itself is not reproducible headless (renderer/audio
   consumers interleave, DETERMINISM_NOTE.md §a). Each gate therefore
   identifies itself through coin() below. */
enum class FlavorGate {
  FfeAck,          /* 0.40 — 10-mission.js:824 */
  CompleteTail,    /* 0.50 — 10-mission.js:902 */
  ReadbackTail,    /* 0.45 — 12-cff-handling.js:390 */
  SnideRound,      /* 0.55 — 12-cff-handling.js:714 */
  CorrSnark,       /* 0.50 — 12-cff-handling.js:716 */
  CareerCallback,  /* 0.40 — 15-campaign-menu.js:635 */
};

/* The injected flavor RNG — one uniform [0,1) per call, standing in for
   Math.random() at every SANCTIONED FLAVOR site in the FDC layer:
   - pick() line selection            07-voice-spot-audio.js:293
   - caller ack selection             07-voice-spot-audio.js:556
   - the probability-gated tail/snark lines (each site documents its own
     gate probability where it draws — see fdc_sink.cpp / fdc_rules.cpp).
   Implementations must not throw (core compiles exceptions-off). */
class IFlavorRng {
public:
  virtual ~IFlavorRng() = default;
  virtual double next() = 0;
  /* wave-3 reconciliation: a gated-presence decision (Math.random() < p at
     the six sites above). Default body: next() < p — identical stream use
     to the source. predictedAirTime is the FDC.say time the line would get
     (max(now, lastT) + its delay), handed to replay harnesses so they can
     match the recorded transcript; the default ignores it. */
  virtual bool coin(double p, FlavorGate gate, double predictedAirTime);
};

/* One pool = one QUIPS key. `last` is the pool's _last rotation slot
   (07:294-296): a re-draw of the same index steps forward one. -1 = unset
   (JS undefined). */
struct QuipPool {
  std::vector<std::string> lines;
  int last = -1;
};

/* rantCiv / rantFrat (07:265-289) are pools of THREE-LINE rants; the FDC
   layer chains the three lines at delays 1.0/2.2/2.8 (10-mission.js:1213-1215
   via events.h FdcLineId::RantFratricide/RantCollateral). */
struct RantPool {
  std::vector<std::array<std::string, 3>> rants;
  int last = -1;
};

/* The full QUIPS object, category-for-category (07:36-290). */
struct QuipSet {
  QuipPool greet;              /* 07:37 */
  QuipPool readbackTail;       /* 07:47 — gated 0.45 at 12-cff-handling.js:390 */
  QuipPool corrSnark;          /* 07:57 — gated 0.50 at 12-cff-handling.js:716 */
  QuipPool ffeAck;             /* 07:67 — gated 0.40 at 10-mission.js:824 */
  QuipPool completeTail;       /* 07:75 — gated 0.50 at 10-mission.js:902 */
  QuipPool eomGood;            /* 07:83 */
  QuipPool eomBad;             /* 07:91 */
  QuipPool badGrid;            /* 07:99 */
  QuipPool water;              /* 07:107 */
  QuipPool noMission;          /* 07:116 */
  QuipPool inFlight;           /* 07:125 */
  QuipPool unknown;            /* 07:131 */
  QuipPool careerFrat;         /* 07:144 — campaign layer, gated 0.40 at 15-campaign-menu.js:635 */
  QuipPool careerCollat;       /* 07:149 */
  QuipPool careerVet;          /* 07:154 */
  QuipPool complete;           /* 07:159 — single line: pick() returns it with NO rng draw */
  QuipPool suppressedOnly;     /* 07:166 */
  QuipPool noEffect;           /* 07:170 */
  QuipPool dangerClose;        /* 07:174 */
  QuipPool wrongWay;           /* 07:179 */
  QuipPool rantLost;           /* 07:184 */
  QuipPool coachTimid;         /* 07:195 — {MISS} substitution */
  QuipPool coachBracket;       /* 07:199 */
  QuipPool coachBracketGood;   /* 07:203 */
  QuipPool coachStagnant;      /* 07:207 */
  QuipPool coachLoc;           /* 07:211 — single line, {MISS} substitution */
  QuipPool snide;              /* 07:216 */
  QuipPool snideRound;         /* 07:223 — gated 0.55 at 12-cff-handling.js:714 */
  QuipPool nearCiv;            /* 07:229 */
  QuipPool unsafeCorr;         /* 07:234 — {WHO} substitution */
  QuipPool unsafeInsist;       /* 07:239 */
  QuipPool spotSrc;            /* 07:245 — drawn by the spot module's SEEDED
                                  stream (07:480-482), never via pick() */
  QuipPool spotTail;           /* 07:253 */
  QuipPool spotNone;           /* 07:261 */
  RantPool rantCiv;            /* 07:265 */
  RantPool rantFrat;           /* 07:276 */
};

/* The full pools, text verbatim from 07-voice-spot-audio.js:36-290. */
QuipSet makeQuips();

/* port of src/js/07-voice-spot-audio.js:291-297 (oracle-v1) — pick(arr):
   single-line pools return their line WITHOUT drawing; otherwise one rng
   draw, and a collision with the pool's last pick steps forward one. */
const std::string& pick(QuipPool& pool, IFlavorRng& rng);
const std::array<std::string, 3>& pick(RantPool& pool, IFlavorRng& rng);

/* "{MISS}" / "{WHO}" substitution — String.replace with a plain string
   pattern replaces the FIRST occurrence only (12-cff-handling.js:686,698;
   10-mission.js:1290). */
std::string substFirst(const std::string& line, const std::string& token,
                       const std::string& value);

} }
