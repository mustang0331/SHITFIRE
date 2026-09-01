#include "shitfire/doctrine/fdc_spot.h"

#include <array>
#include <cmath>
#include <regex>

#include "shitfire/math/grid.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/prng.h"

#define EMD "\xE2\x80\x94"

namespace shitfire { namespace doctrine {

namespace {

/* 07:331-340 — the cardinal tables. */
const char* const CARD8[8] = {"NORTH", "NORTHEAST", "EAST", "SOUTHEAST",
                              "SOUTH", "SOUTHWEST", "WEST", "NORTHWEST"};
const char* const CARD4_ADJ[4] = {"NORTHERN", "EASTERN", "SOUTHERN", "WESTERN"};
const char* const CARD4_DIR[4] = {"NORTH", "EAST", "SOUTH", "WEST"};

std::string cardinal8(double rad) {   /* 07:337 */
  return CARD8[static_cast<int>(js::round(radToMils(rad) / 800.0)) % 8];
}
int quad4i(double rad) {              /* 07:338 */
  return static_cast<int>(js::round(radToMils(rad) / 1600.0)) % 4;
}
std::string quadrant4(double rad) { return CARD4_ADJ[quad4i(rad)]; }  /* 07:339 */
std::string quadDir4(double rad) { return CARD4_DIR[quad4i(rad)]; }   /* 07:340 */
double milsWrap(double m) {           /* 07:341 */
  return std::fmod(std::fmod(m, 6400.0) + 6400.0, 6400.0);
}
double roundTo(double v, double n) { return js::round(v / n) * n; }   /* 07:342 */

/* 07:347-348 — LANDMARK_SPOKEN, permanent map features only. */
std::string landmarkSpoken(const std::string& kind, const std::string& name) {
  if (kind == "airfield") return "THE AIRFIELD";
  if (kind == "mast") return "THE RADIO MAST";
  if (kind == "gun") return "THE OLD COASTAL GUN";
  if (kind == "fuel") return "THE FUEL POINT";
  if (kind == "ammo") return "THE AMMO DEPOT";
  return name;
}

/* 07:372-375 — KM_WORDS. */
std::string kmWords(double m) {
  if (m == 2000) return "TWO THOUSAND";
  if (m == 2500) return "TWO AND A HALF THOUSAND";
  if (m == 3000) return "THREE THOUSAND";
  if (m == 3500) return "THREE AND A HALF THOUSAND";
  if (m == 4000) return "FOUR THOUSAND";
  return jsNum(m);
}

/* 07:403-434 — SPOT_SCN, keyed by scenario type. */
struct SpotScn { const char *size, *act, *eq; };
const SpotScn* spotScnFor(const std::string& type) {
  static const std::pair<const char*, SpotScn> table[] = {
    {"strongpoint", {"SQUAD PLUS " EMD " EIGHT OR MORE DISMOUNTS AND TWO LIGHT VEHICLES",
                     "ASSAULTING A FRIENDLY STRONGPOINT",
                     "SMALL ARMS, ONE CREW-SERVED WEAPON, TWO SOFT-SKIN TRUCKS"}},
    {"troops",      {"SQUAD PLUS " EMD " EIGHT OR MORE DISMOUNTS",
                     "INFANTRY IN THE OPEN, NO OVERHEAD COVER",
                     "SMALL ARMS AND PACKS. NO VEHICLES SEEN"}},
    {"bunker",      {"ONE FORTIFIED POSITION, SQUAD SIZE",
                     "DUG IN AND OBSERVING. THEY ARE NOT GOING ANYWHERE",
                     "AUTOMATIC WEAPON IN A COVERED EMPLACEMENT, OVERHEAD PROTECTION"}},
    {"convoy",      {"FOUR VEHICLES IN COLUMN",
                     "MOTORISED COLUMN ON THE MOVE",
                     "CARGO TRUCKS. ESCORT UNKNOWN. NO ARMOUR REPORTED"}},
    {"assault",     {"PLATOON MINUS ON THE OBJECTIVE",
                     "HOLDING GROUND AGAINST A FRIENDLY ADVANCE. OUR PEOPLE ARE CLOSING ON THEM",
                     "DUG-IN SMALL ARMS AND AT LEAST ONE MACHINE GUN"}},
    {"wreck",       {"ONE DERELICT LANDING CRAFT",
                     "STATIC. ABANDONED. DEEPLY UNTHREATENING",
                     "NONE. IT IS SCRAP, AND IT IS YOURS"}},
    {"raid",        {"EIGHT OR MORE DISMOUNTS AND ONE VEHICLE",
                     "RAIDING A CIVILIAN VILLAGE",
                     "SMALL ARMS AND ONE ARMED PICKUP"}},
    {"battery",     {"ONE GUN, SECTION-STRENGTH CREW",
                     "DELIVERING INDIRECT FIRE ON A FRIENDLY POSITION. THEY ARE IN ACTION NOW",
                     "ONE MEDIUM MORTAR, EMPLACED, WITH A TRUCK STANDING OFF"}},
    {"defense",     {"SQUAD PLUS " EMD " EIGHT OR MORE DISMOUNTS",
                     "ADVANCING ON YOUR OBSERVATION POST. YOU ARE THE OBJECTIVE",
                     "SMALL ARMS AND AT LEAST ONE MACHINE GUN, MOVING LIGHT"}},
    {"occupied",    {"PLATOON MINUS, HOLDING GROUND",
                     "OCCUPYING A SETTLEMENT. CIVILIANS ARE STILL INSIDE THE STRUCTURES",
                     "SMALL ARMS, CREW-SERVED WEAPONS ON THE PERIMETER, TWO VEHICLES"}},
  };
  for (const auto& kv : table)
    if (type == kv.first) return &kv.second;
  return nullptr;
}

/* 07:435-441 — SPOT_UNIT. */
const char* const SPOT_UNIT[5] = {
  "UNKNOWN. NO MARKINGS, NO INSIGNIA",
  "ASSESSED AS THIRD BATTALION REMNANTS, UNCONFIRMED",
  "UNIDENTIFIED. MIXED KIT, MIXED DISCIPLINE",
  "IRREGULARS. THEY ARE NOT WEARING ANYBODY'S UNIFORM",
  "UNKNOWN. HIGHER HAS A GUESS, AND HIGHER HAS BEEN WRONG BEFORE",
};

/* 07:491-524 — CALLERS, the four archetypes (NARRATIVE.md contract). */
struct Caller {
  const char* cs;
  const char* open;
  const char* tgt;
  std::array<const char*, 3> ack;
  const char* sup;
  const char* dead;
};
const Caller CALLERS[4] = {
  {"RAZOR 3",
   "RAZOR 3, RAZOR 3, we are getting FUCKED UP out here, I need you to do something about that right now.",
   "machine gun",
   {"Copy " EMD " wait, copy what? Just shoot it, over, or whatever you say, OVER.",
    "I don't know what a grid is, man, I just need the trees to stop shooting at us.",
    "Doc's working on Guerrero " EMD " he's gonna be pissed he missed this. Still firing, same spot, GO."},
   "It stopped " EMD " it stopped, holy shit, okay. Stay on it, do not let them get back up.",
   "They're done. They're DONE. Whoever's on that gun, I owe you a beer and Guerrero owes you two."},
  {"GATOR 6",
   "GATOR 6. We got a truck full of very angry people shooting at us. Whenever's convenient.",
   "truck and dismounts",
   {"Copy whatever you're about to say. Just make it quiet, I've got a schedule to keep.",
    "Nobody's dead yet, so no rush, but I'd appreciate it before that changes.",
    "Funny thing, I was just telling the driver this route never has any trouble. Universe has a sense of humor."},
   "Well, they've stopped. Appreciated. We'll be on our way before they reconsider.",
   "That did it. Quiet as church out here. GATOR 6 out " EMD " got a schedule to keep."},
  {"IRON 2",
   "IRON 2, be advised, we are experiencing significant enemy contact from a, ah, fortified emplacement, over.",
   "emplacement",
   {"Copy your last, I think. Say again what a grid is, over. No, belay that, don't say again, just proceed.",
    "This is IRON 2, danger close, I mean " EMD " is it danger close? My guys are close to it. Use your judgment, over.",
    "Solid copy, HELLHOUND " EMD " sorry, MUSTANG " EMD " solid copy either way, out. Out. Over and out."},
   "Effect on target is, ah, effective. Outstanding. Continue to, uh, continue, over.",
   "Target is " EMD " I want to say neutralized? Destroyed. One of those. Fine work, MUSTANG. IRON 2, over and out."},
  {"SPADE 4",
   "SPADE 4. Gun. Ours are pinned.",
   "gun",
   {"Negative on grid. Don't have one. You got eyes, I don't.",
    "Still there. Do it again.",
    "Copy."},
   "They're down. Keep it coming.",
   "Good hit. SPADE 4 out."},
};

}  // namespace

FdcSpotReports::FdcSpotReports(sim::SimClock& clock, sim::EventQueue& queue,
                               FdcSpeaker& speaker, INetSink& net,
                               QuipSet& quips, IFlavorRng& flavor,
                               const FdcChapterContext& chapter,
                               const FdcCallsigns& calls,
                               const SpotTunables& spot, const MapConfig& map)
    : clock_(clock), queue_(queue), speaker_(speaker), net_(net),
      quips_(quips), flavor_(flavor), chapter_(chapter), calls_(calls),
      spot_(spot), map_(map) {}

void FdcSpotReports::bind(const scenario::ScenarioState* scn,
                          const scenario::WorldLayout* world,
                          const scenario::OpPos& op) {
  scn_ = scn;
  world_ = world;
  op_ = op;
}

// port of src/js/07-voice-spot-audio.js:343-346 (oracle-v1) — grid4(x, z):
// the 1 km grid square, two floor()ed numbers concatenated.
namespace {
std::string grid4(double x, double z, const MapConfig& map) {
  const EN en = worldToEN(x, z, map);
  return jsNum(std::floor(en.e / 1000.0)) + jsNum(std::floor(en.n / 1000.0));
}
}

// port of src/js/07-voice-spot-audio.js:351-371 (oracle-v1) — nearestLandmark.
// Scan order is source order: facilities, then villages, then named rocks.
bool FdcSpotReports::nearestLandmark(double x, double z, Landmark& out) const {
  if (!world_) return false;
  bool have = false;
  double bd = 1e300;   /* Infinity */
  for (const scenario::Facility& f : world_->facilities) {
    const double d = dist2(x, z, f.x, f.z);
    if (d < bd) { bd = d; out = {landmarkSpoken(f.kind, f.name), f.x, f.z, d}; have = true; }
  }
  for (const scenario::Village& v : world_->villages) {
    const double d = dist2(x, z, v.x, v.z);
    if (d < bd) { bd = d; out = {"THE VILLAGE OF " + v.name, v.x, v.z, d}; have = true; }
  }
  /* E7 — named outcrops are landmarks too (07:365-369). */
  for (const scenario::Rock& r : world_->rocks) {
    if (!r.name.has_value()) continue;
    const double d = dist2(x, z, r.x, r.z);
    if (d < bd) { bd = d; out = {*r.name, r.x, r.z, d}; have = true; }
  }
  return have && out.d <= spot_.landmarkMax;   /* 07:370 */
}

// port of src/js/07-voice-spot-audio.js:376-402 (oracle-v1) — spotLocation.
// Orients WITHOUT answering: the stated radius is a floor, only ever widened
// (07:316-324 comment block — measured cue areas, target always inside).
std::string FdcSpotReports::spotLocation(double x, double z,
                                         const std::string& diff) const {
  Landmark lm;
  const bool haveLm = nearestLandmark(x, z, lm);
  const double dOT = dist2(op_.x, op_.z, x, z);
  const double azOT = azTo(op_.x, op_.z, x, z);
  if (diff == "easy")
    return "GRID SQUARE " + grid4(x, z, map_) +
           (haveLm ? ", VICINITY OF " + lm.name : "");
  const bool norm = diff == "normal";
  if (haveLm) {
    /* radius floor, widened only to keep the statement TRUE (07:385). */
    const double R = std::fmax(norm ? 2000.0 : 3000.0,
                               std::ceil(lm.d / 500.0) * 500.0);
    const std::string dir = norm ? cardinal8(azTo(lm.x, lm.z, x, z))
                                 : quadDir4(azTo(lm.x, lm.z, x, z));
    return dir + " OF " + lm.name + ", WITHIN " + kmWords(R) + " METRES OF IT";
  }
  if (norm) {
    const double b = roundTo(radToMils(azOT), 800);
    const double k = std::floor(dOT / 1000.0);
    return "BEARING " + fmtMils(milsWrap(b - 400)) + " TO " +
           fmtMils(milsWrap(b + 400)) + " GRID FROM YOUR TOWER, " + jsNum(k) +
           " TO " + jsNum(k + 1) + " THOUSAND OUT";
  }
  const double b0 = std::floor(dOT / 2000.0) * 2;
  return quadrant4(azOT) + " QUADRANT FROM YOUR TOWER, " +
         (b0 == 0 ? std::string("INSIDE TWO THOUSAND")
                  : jsNum(b0) + " TO " + jsNum(b0 + 2) + " THOUSAND") +
         " OUT";
}

// port of src/js/07-voice-spot-audio.js:443-478 (oracle-v1) — sendSpotReport.
// Stream #8; draw order src, minutes, salute-coin, [unit] is sacred.
void FdcSpotReports::sendSpotReport(SpotReason reason) {
  if (!spot_.enabled || !scn_) return;
  const SpotScn* d = spotScnFor(scn_->type);
  if (!d) return;
  Mulberry32 rng(js::toUint32(
      scn_->seed * 30011.0 + (reason == SpotReason::Displace ? 977.0 : 7.0)));
  const std::string loc =
      spotLocation(scn_->enemy.x, scn_->enemy.z, scn_->difficulty);
  const std::string head = calls_.obs + ", " + fdcCallFor(chapter_, calls_);

  if (reason == SpotReason::Displace) {   /* 07:452-458 */
    speaker_.say(head + " " EMD " UPDATED SPOT REPORT. SIZE UNCHANGED. "
                 "ACTIVITY: DISPERSING AND GOING TO GROUND. LOCATION: " + loc +
                 ". RE-ACQUIRE AND SEND YOUR CORRECTION, OVER.",
                 spot_.gap);
    net_.spotMeta(loc, "update");
    return;
  }

  /* SPOT_SRC_SEEDED (07:480-482) — indexes the pool off the SEEDED stream,
     leaving the shared pick() rotation untouched. */
  const auto& srcPool = quips_.spotSrc.lines;
  const std::string src =
      srcPool[static_cast<std::size_t>(std::floor(rng.next() * srcPool.size()))];
  const double mins = 2 + std::floor(rng.next() * 18);   /* 07:461 */
  const bool salute = rng.next() < spot_.saluteChance;   /* 07:462 */
  if (salute) {   /* 07:463-468 */
    speaker_.say(head + " " EMD " I HAVE A SPOT REPORT FOR YOU, RELAYED FROM " +
                 src + ". SALUTE FORMAT. STAND BY, OVER.", spot_.lead);
    speaker_.say("SIZE: " + std::string(d->size) + ". ACTIVITY: " + d->act +
                 ". LOCATION: " + loc + ", OVER.", spot_.gap);
    speaker_.say("UNIT: " +
                 std::string(SPOT_UNIT[static_cast<int>(
                     std::floor(rng.next() * 5))]) +
                 ". TIME: " + jsNum(mins) + " MINUTES OLD. EQUIPMENT: " +
                 d->eq + ". OVER.", spot_.gap);
  } else {   /* 07:469-473 */
    speaker_.say(head + " " EMD " " + src +
                 " REPORTS ACTIVITY IN THIS GENERAL AREA " EMD " " + loc +
                 ". " + d->act + ". " + jsNum(mins) + " MINUTES OLD, OVER.",
                 spot_.lead);
    speaker_.say("THAT IS ALL THEY GAVE ME. " + std::string(d->size) + ", " +
                 d->eq + ". NOTHING FINER, OVER.", spot_.gap);
  }
  speaker_.say(pick(quips_.spotTail, flavor_), 1.7);   /* 07:475 */
  net_.spotMeta(loc, salute ? "salute" : "loose");     /* 07:476 */
}

// port of src/js/07-voice-spot-audio.js:526-529 (oracle-v1) — callerLine.
void FdcSpotReports::callerLine(const std::string& cs, const std::string& msg) {
  net_.line(NetLineKind::Caller, cs, msg);
}

// port of src/js/07-voice-spot-audio.js:533-539 (oracle-v1) — callerLocLine.
std::string FdcSpotReports::callerLocLine() const {
  const auto& S = *scn_;
  const Caller& c = CALLERS[static_cast<int>(S.caller->i)];
  const double tx = S.team ? S.team->x : 0, tz = S.team ? S.team->z : 0;
  Landmark lm;
  const bool haveLm = nearestLandmark(tx, tz, lm);
  std::string pos = "We're in grid square " + grid4(tx, tz, map_);
  if (haveLm)
    pos += ", " + lowerAscii(cardinal8(azTo(lm.x, lm.z, tx, tz))) + " of " +
           lowerAscii(lm.name);
  pos += ".";
  return pos + " The " + c.tgt + " is " +
         lowerAscii(cardinal8(S.caller->repAz)) + " of us, maybe " +
         jsNum(S.caller->repDist) +
         " meters. That's my best guess, don't quote me.";
}

// port of src/js/07-voice-spot-audio.js:541-548 (oracle-v1) — callerKickoff.
void FdcSpotReports::callerKickoff() {
  if (!scn_ || !scn_->caller.has_value()) return;
  const Caller& c = CALLERS[static_cast<int>(scn_->caller->i)];
  const std::string cs = c.cs;
  const std::string open = c.open;
  queue_.schedule(clock_.now + spot_.lead,
                  [this, cs, open]() { callerLine(cs, open); });
  queue_.schedule(clock_.now + spot_.lead + spot_.gap,
                  [this, cs]() { callerLine(cs, callerLocLine()); });
}

// port of src/js/07-voice-spot-audio.js:551-557 (oracle-v1) — callerReply.
void FdcSpotReports::callerReply(const std::string& raw) {
  if (!scn_ || !scn_->caller.has_value()) return;
  const Caller& c = CALLERS[static_cast<int>(scn_->caller->i)];
  static const std::regex reAgain("say again|repeat|where",
                                  std::regex::ECMAScript | std::regex::icase);
  if (std::regex_search(raw, reAgain))
    callerLine(c.cs, callerLocLine());
  else
    callerLine(c.cs,
               c.ack[static_cast<int>(std::floor(flavor_.next() * 3))]);
}

// port of src/js/07-voice-spot-audio.js:558-561 (oracle-v1) — callerSay.
void FdcSpotReports::callerSay(CallerCue cue) {
  if (!scn_ || !scn_->caller.has_value()) return;
  const Caller& c = CALLERS[static_cast<int>(scn_->caller->i)];
  callerLine(c.cs, cue == CallerCue::Sup ? c.sup : c.dead);
}

// CALLERS[i].cs.split(' ')[0].toLowerCase() — 12-cff-handling.js:927.
std::string FdcSpotReports::callerFirstWordLower(std::size_t i) const {
  std::string cs = CALLERS[i % 4].cs;
  const std::size_t sp = cs.find(' ');
  if (sp != std::string::npos) cs.resize(sp);
  return lowerAscii(cs);
}

} }
