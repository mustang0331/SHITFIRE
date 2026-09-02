#include "shitfire/doctrine/fdc_rules.h"

#include <cmath>
#include <regex>
#include <utility>

#include "shitfire/doctrine/lexicon_strings.h"
#include "shitfire/math/grid.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/sim/mission_ballistics.h"

/* The FDC rule engine — port of src/js/12-cff-handling.js (oracle-v1) plus
   handlePosRep (10-mission.js:260-280) and the raw-text sheaf/fuze request
   extraction (10-mission.js:980-985 / :1013-1019). Every function cites its
   source lines. Non-ASCII is escaped (core builds without /utf-8). */
#define EMD "\xE2\x80\x94"     /* — U+2014 */
#define STAR5 "\xE2\x98\x85"   /* ★ U+2605 */
#define DEG "\xC2\xB0"         /* ° U+00B0 */
#define HELLIP "\xE2\x80\xA6"  /* … U+2026 */

namespace shitfire { namespace doctrine {

namespace {

/* parseInt on a digits-only slice — manual, so nothing can throw. */
double parseIntStr(const std::string& s) {
  double v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') break;
    v = v * 10 + (c - '0');
  }
  return v;
}

/* JS truthiness for a double field. */
bool truthy(double v) { return v != 0; }

std::string joinList(const std::vector<std::string>& v, const char* sep) {
  std::string out;
  for (std::size_t i = 0; i < v.size(); i++) {
    if (i) out += sep;
    out += v[i];
  }
  return out;
}

std::string trimmed(const std::string& s) {
  std::size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  std::size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string sheafKindWord(sim::SheafKind k) {
  switch (k) {
    case sim::SheafKind::Converged: return "converged";
    case sim::SheafKind::Circular: return "circular";
    case sim::SheafKind::Open: return "open";
    case sim::SheafKind::Linear: return "linear";
    default: return "";
  }
}
std::string fuzeKindWord(sim::FuzeKind k) {
  switch (k) {
    case sim::FuzeKind::Vt: return "vt";
    case sim::FuzeKind::Time: return "time";
    case sim::FuzeKind::Delay: return "delay";
    case sim::FuzeKind::Pd: return "pd";
    default: return "";
  }
}
std::string shellKindWord(sim::ShellKind k) {
  switch (k) {
    case sim::ShellKind::Smoke: return "smoke";
    case sim::ShellKind::Illum: return "illum";
    default: return "he";
  }
}
std::string outcomeWord(sim::EffectOutcome o) {
  switch (o) {
    case sim::EffectOutcome::Suppressed: return "suppressed";
    case sim::EffectOutcome::Neutralized: return "neutralized";
    case sim::EffectOutcome::Destroyed: return "destroyed";
    default: return "none";
  }
}
int outcomeRank(const std::string& w) {   /* 12-cff:752 */
  return w == "suppressed" ? 1 : w == "neutralized" ? 2
       : w == "destroyed" ? 3 : 0;
}

/* SUGG5 mission-scoped types (12-cff-handling.js:1023-1024). */
bool isRoutedType(const std::string& t) {
  static const char* const kTypes[] = {
    "adjust", "repeat", "eom", "direction", "shell", "fuze", "sheaf",
    "tot", "cannotobserve", "fire", "amc", "donotload"};
  for (const char* k : kTypes)
    if (t == k) return true;
  return false;
}

}  // namespace

/* ---- pure rule seams --------------------------------------------------- */

// port of src/js/01-math-terrain.js:184-186 (oracle-v1)
double declMilsFor(double declEastDeg) { return declEastDeg / 360.0 * 6400.0; }
double gridToMag(double mils, double declEastDeg) {
  return std::fmod(std::fmod(mils - declMilsFor(declEastDeg), 6400.0) + 6400.0,
                   6400.0);
}
double magToGrid(double mils, double declEastDeg) {
  return std::fmod(std::fmod(mils + declMilsFor(declEastDeg), 6400.0) + 6400.0,
                   6400.0);
}

// port of src/js/12-cff-handling.js:477-511 (oracle-v1) — the G2 direction
// diagnosis: off(), the two declination candidates, and the decisive-fit
// rule (candidate < 45 AND beats the raw error by > 60).
DirDiagnosis diagnoseDirection(double sentMils, double trueAzMils,
                               double declEastDeg, double* errOut) {
  auto off = [sentMils](double t) {   /* :477 — JS %, sign-preserving */
    return std::fabs(std::fmod(sentMils - t + 3200.0, 6400.0) - 3200.0);
  };
  const double err = off(trueAzMils);
  if (errOut) *errOut = err;
  const double magErr = off(gridToMag(trueAzMils, declEastDeg));   /* :491 */
  const double revErr = off(magToGrid(trueAzMils, declEastDeg));   /* :492 */
  auto fits = [err](double candErr) {                              /* :501 */
    return candErr < 45.0 && err - candErr > 60.0;
  };
  if (fits(magErr)) return DirDiagnosis::Magnetic;                 /* :502 */
  if (fits(revErr)) return DirDiagnosis::Reversed;                 /* :506 */
  if (err > 80.0) return DirDiagnosis::Rough;                      /* :510 */
  return DirDiagnosis::None;
}

// port of src/js/12-cff-handling.js:626 / :714 (oracle-v1) — deviation in
// tens, range in hundreds; fifties only on the correction entering FFE.
bool strictRoundingViolation(double right, double add, bool ffe) {
  return truthy(std::fmod(std::fabs(right), 10.0)) ||
         truthy(std::fmod(std::fabs(add), ffe ? 50.0 : 100.0));
}

// port of src/js/12-cff-handling.js:167-179 (oracle-v1) — the strict-net
// format gates, in source order. "" = the call passes.
std::string strictChallenge(const ParsedMessage& p,
                            const FdcChapterContext& chapter,
                            const FdcCallsigns& calls) {
  const std::string shortCall = fdcShortFor(chapter, calls);
  /* G17 — the RIGHT callsign: on a mortar net, addressing HELLHOUND is a
     miss (:170-173). */
  if (p.raw.find(lowerAscii(shortCall)) == std::string::npos ||
      p.raw.find("mustang") == std::string::npos)
    return (p.raw.find("hellhound") != std::string::npos &&
            shortCall != "HELLHOUND")
        ? "HELLHOUND is the one-five-five battery and they are not on this "
          "net. You are talking to " + fdcCallFor(chapter, calls) +
          ". Say again your last, addressed to the right people, over."
        : "this net runs callsigns. Identify yourself and your addressee and "
          "say again your last, over.";
  if (p.warno == "none")   /* :174-175 */
    return "no warning order heard. ADJUST FIRE or FIRE FOR EFFECT, then "
           "location. Say again, over.";
  if (p.desc.empty())      /* :176-177 */
    return "no target description. I do not shoot at adjectives. Say again "
           "with description, over.";
  bool hasOver = false;    /* :178-179 */
  for (const std::string& t : p.toks)
    if (t == "over") { hasOver = true; break; }
  if (!hasOver)
    return "transmissions end with OVER on this net. The book is not a "
           "suggestion tonight. Say again, over.";
  return "";
}

// port of src/js/12-cff-handling.js:104-130 (oracle-v1) — NET17 cheap
// location validation for TRANSMISSION 2. "" = workable.
std::string locError(const ParsedMessage& p,
                     const std::vector<scenario::Kp>& kps, bool havePosRep,
                     bool sunlampNet, const MapConfig& map, QuipSet& quips,
                     IFlavorRng& flavor) {
  if (p.method == "shift") {   /* :105-109 */
    const scenario::Kp* kp = nullptr;
    for (const scenario::Kp& k : kps)
      if (k.id == p.kpId) { kp = &k; break; }
    if (!kp)
      return "NEGATIVE. There is no known point " + p.kpId +
             " registered on this net. Say again, over.";
    return "";
  }
  if (p.method == "polar") {   /* :110-116 */
    if (!havePosRep)
      return "MUSTANG 12, HELLHOUND " EMD " I cannot resolve a POLAR mission. "
             "I do not hold your position. Send a position report first " EMD
             " \"POSITION GRID 245 523\" " EMD " then your call, over.";
    if (p.distM < 100 || p.distM > 12000) return pick(quips.badGrid, flavor);
    return "";
  }
  const std::size_t len = p.digits.size();   /* :117-119 */
  const bool okLen = !p.digits.empty() &&
                     (len == 6 || len == 8 || (len == 10 && sunlampNet));
  if (!okLen) return pick(quips.badGrid, flavor);
  double e, n;   /* :120-123 */
  if (len == 6) {
    e = parseIntStr(p.digits.substr(0, 3)) * 100 + 50;
    n = parseIntStr(p.digits.substr(3)) * 100 + 50;
  } else if (len == 8) {
    e = parseIntStr(p.digits.substr(0, 4)) * 10 + 5;
    n = parseIntStr(p.digits.substr(4)) * 10 + 5;
  } else {
    e = parseIntStr(p.digits.substr(0, 5)) + 0.5;
    n = parseIntStr(p.digits.substr(5)) + 0.5;
  }
  const WorldXZ w = enToWorld(e, n, map);   /* :124-128 */
  const EN en = worldToEN(w.x, w.z, map);
  if (en.e < map.originE || en.e >= map.originE + 10000 ||
      en.n < map.originN || en.n >= map.originN + 10000)
    return pick(quips.badGrid, flavor);
  return "";
}

// port of src/js/12-cff-handling.js:133-144 (oracle-v1) — locEchoOf: the
// terse T2 readback (string slices, unlike handleCFF's parseInt echo).
std::string locEchoOf(const ParsedMessage& p) {
  if (p.method == "polar")
    return "DIRECTION " + fmtMils(std::fmod(p.dirMils.value_or(0), 6400.0)) +
           ", DISTANCE " + jsNum(p.distM);
  if (p.method == "shift") {
    std::string s = "SHIFT KNOWN POINT " + p.kpId;
    if (p.dirMils.has_value())
      s += ", DIRECTION " + fmtMils(std::fmod(*p.dirMils, 6400.0));
    if (truthy(p.sRight))
      s += std::string(", ") + (p.sRight > 0 ? "RIGHT" : "LEFT") + " " +
           jsNum(std::fabs(p.sRight));
    if (truthy(p.sAdd))
      s += std::string(", ") + (p.sAdd > 0 ? "ADD" : "DROP") + " " +
           jsNum(std::fabs(p.sAdd));
    return s;
  }
  if (p.digits.size() == 6)
    return "GRID " + p.digits.substr(0, 3) + " " + p.digits.substr(3);
  return "GRID " + p.digits;
}

// port of src/js/10-mission.js:980-985 (oracle-v1) — inferSheaf's raw-text
// (requested) branch; the desc/default branches are the machine's.
bool sheafRequestFromRaw(const std::string& raw, sim::SheafSpec& out) {
  static const std::regex re(
      R"(\b(converged|open|parallel|linear|circular|special)\s+sheaf\b)");
  std::smatch m;
  if (!std::regex_search(raw, m, re)) return false;
  const std::string word = m[1].str();
  out.kind = (word == "parallel" || word == "special") ? sim::SheafKind::Linear
           : word == "converged" ? sim::SheafKind::Converged
           : word == "open" ? sim::SheafKind::Open
           : word == "circular" ? sim::SheafKind::Circular
           : sim::SheafKind::Linear;
  out.source = "requested";
  out.why = upperAscii(word) + " SHEAF requested in the call";
  return true;
}

// port of src/js/10-mission.js:1013-1019 (oracle-v1) — inferFuze's raw-text
// (requested) branch.
bool fuzeRequestFromRaw(const std::string& raw, sim::FuzeSpec& out) {
  static const std::regex re(
      R"(\bfuze\s+(vt|victor tango|variable time|time|delay|quick|pd|point detonating)\b)");
  std::smatch m;
  if (!std::regex_search(raw, m, re)) return false;
  const std::string word = m[1].str();
  out.kind = (word == "victor tango" || word == "variable time")
                 ? sim::FuzeKind::Vt
             : (word == "point detonating" || word == "quick")
                 ? sim::FuzeKind::Pd
             : word == "vt" ? sim::FuzeKind::Vt
             : word == "time" ? sim::FuzeKind::Time
             : word == "delay" ? sim::FuzeKind::Delay
             : sim::FuzeKind::Pd;
  out.source = "requested";
  out.why = "FUZE " + upperAscii(word) + " requested in the call";
  return true;
}

/* ---- engine ------------------------------------------------------------ */

FdcEngine::FdcEngine(sim::SimClock& clock, sim::EventQueue& queue,
                     sim::MissionMachine& machine, sim::IScenarioView& view,
                     FdcSpeaker& speaker, FdcMissionSink& sink,
                     FdcSpotReports* spot, QuipSet& quips, IFlavorRng& flavor,
                     INetSink& net, const FdcChapterContext& chapter,
                     const FdcCallsigns& calls, const MapConfig& map,
                     const scenario::OpPos& op, sim::Vec2 battery,
                     double declEastDeg)
    : clock_(clock), queue_(queue), machine_(machine), view_(view),
      speaker_(speaker), sink_(sink), spot_(spot), quips_(quips),
      flavor_(flavor), net_(net), chapter_(chapter), calls_(calls), map_(map),
      op_(op), battery_(battery), declEastDeg_(declEastDeg) {}

void FdcEngine::bindScenario(const scenario::ScenarioState* scn) {
  scn_ = scn;
  posRep_.reset();   /* Scenario rebuilt fresh -> posRep expires (10:258) */
  extras_.clear();   /* mission registry is wiped with the scenario (10:120) */
}

void FdcEngine::sayNet(const std::string& msg, double delay) {
  speaker_.say(msg, delay);
}
void FdcEngine::sysLog(const std::string& msg) {
  net_.line(NetLineKind::Sys, "", msg);
}
double FdcEngine::dangerCloseR() const {   /* 10-mission.js:79-82 */
  const sim::AssetKind a =
      chapter_.active ? chapter_.asset : sim::AssetKind::Battery155;
  return sim::dangerCloseRadius(a, machine_.tunables().mission);
}

FdcEngine::MissionExtra& FdcEngine::extraFor(const sim::Mission* m) {
  for (auto& kv : extras_)
    if (kv.first == m) return kv.second;
  extras_.emplace_back(m, MissionExtra{});
  return extras_.back().second;
}

ParserContext FdcEngine::parserContext() const {
  ParserContext ctx;
  ctx.declEastDeg = declEastDeg_;
  if (scn_)
    for (const scenario::Kp& k : scn_->kps)
      ctx.kps.push_back(KnownPointRef{k.id, k.name});
  return ctx;
}

/* Object.keys(MISSIONS) with !done — registration is 'AA'+mto.tgt and only
   missions WITH an mto register (10-mission.js:192), which is exactly the
   set openMissions() walks. */
std::vector<std::string> FdcEngine::openMissionKeys() const {
  std::vector<std::string> keys;
  for (sim::Mission* m :
       const_cast<sim::MissionMachine&>(machine_).openMissions())
    if (m->mto.has_value()) keys.push_back("AA" + m->mto->tgt);
  return keys;
}

std::unique_ptr<ParsedMessage> FdcEngine::cloneNoInner(const ParsedMessage& p) {
  auto c = std::make_unique<ParsedMessage>();
  c->type = p.type; c->method = p.method; c->warno = p.warno; c->ffe = p.ffe;
  c->raw = p.raw; c->toks = p.toks; c->tgtNum = p.tgtNum; c->on = p.on;
  c->sec = p.sec; c->kind = p.kind; c->cancel = p.cancel; c->refine = p.refine;
  c->record = p.record; c->bda = p.bda; c->digits = p.digits; c->imm = p.imm;
  c->desc = p.desc; c->kpId = p.kpId; c->sRight = p.sRight; c->sAdd = p.sAdd;
  c->dirMils = p.dirMils; c->distM = p.distM; c->vertM = p.vertM;
  c->corr = p.corr; c->any = p.any; c->stray = p.stray; c->rep = p.rep;
  c->shell = p.shell; c->minutes = p.minutes; c->name = p.name;
  c->tgts = p.tgts; c->gun = p.gun; c->repeat = p.repeat; c->right = p.right;
  c->add = p.add;
  return c;   /* inner stays null — the CFFQ never stores a correction */
}

// port of src/js/10-mission.js:979-993 / :1012-1025 (oracle-v1) — the raw
// half here, the desc/default half in the committed machine.
sim::SheafSpec FdcEngine::inferSheaf(const std::string& desc,
                                     const std::string& raw) const {
  sim::SheafSpec s;
  if (sheafRequestFromRaw(raw, s)) return s;
  return machine_.inferSheafFromDesc(desc);
}
sim::FuzeSpec FdcEngine::inferFuze(const std::string& desc,
                                   const std::string& raw) const {
  sim::FuzeSpec f;
  if (fuzeRequestFromRaw(raw, f)) return f;
  return machine_.inferFuzeFromDesc(desc);
}

/* ---- CFFQ (G22) -------------------------------------------------------- */

// port of src/js/12-cff-handling.js:27-30 (oracle-v1)
void FdcEngine::cffqReset() {
  cffq_.on = false; cffq_.stage = 0; cffq_.warno = "none";
  cffq_.loc.reset(); cffq_.raw.clear(); cffq_.toks.clear();
  cffq_.kpId.clear();   /* NET15 */
}

// port of src/js/12-cff-handling.js:31-34 (oracle-v1)
void FdcEngine::cffqAdd(const ParsedMessage& p) {
  cffq_.raw = (cffq_.raw.empty() ? "" : cffq_.raw + " ") + p.raw;
  cffq_.toks.insert(cffq_.toks.end(), p.toks.begin(), p.toks.end());
}

// port of src/js/12-cff-handling.js:39-45 (oracle-v1) — the 75 s lapse.
void FdcEngine::tick() {
  const double kLapse = 75;   /* CFFQ_LAPSE (:38) */
  if (!cffq_.on) return;
  if (clock_.now - cffq_.t0 < kLapse) return;
  cffqReset();
  sayNet("MUSTANG 12, you started a call for fire and never finished it. "
         "Net is clear. Start again when you have all six elements, out.",
         0.4);
}

// port of src/js/12-cff-handling.js:46-55 (oracle-v1)
void FdcEngine::cffqStart(const std::string& warno, const ParsedMessage& p) {
  cffqReset();
  cffq_.on = true; cffq_.stage = 1; cffq_.warno = warno; cffq_.t0 = clock_.now;
  cffqAdd(p);
  sayNet(calls_.obs + ", " + fdcCallFor(chapter_, calls_) + ", " +
             (warno == "ffe" ? "FIRE FOR EFFECT" : "ADJUST FIRE") + ", OUT.",
         machine_.tunables().fdc.readbackDelay);
  sayNet("SEND YOUR TARGET LOCATION, OVER.", 1.5);
}

// port of src/js/12-cff-handling.js:59-64 (oracle-v1)
void FdcEngine::cffqLocation(const ParsedMessage& p,
                             const std::string& locEcho) {
  cffq_.stage = 2; cffq_.loc = cloneNoInner(p); cffq_.t0 = clock_.now;
  cffqAdd(p);
  sayNet(locEcho + ", OUT.", machine_.tunables().fdc.readbackDelay);
  sayNet("SEND YOUR TARGET DESCRIPTION, OVER.", 1.5);
}

// port of src/js/12-cff-handling.js:67-96 (oracle-v1) — NET5 (location falls
// back to the completing transmission) + NET14 (refusal restores the queue).
void FdcEngine::cffqComplete(const std::string& desc, const ParsedMessage& p) {
  const ParsedMessage* locSrc = cffq_.loc ? cffq_.loc.get() : &p;   /* NET5 */
  /* NET14 snapshot BEFORE folding T3 in. */
  std::string snapWarno = cffq_.warno;
  std::unique_ptr<ParsedMessage> snapLoc =
      cffq_.loc ? cloneNoInner(*cffq_.loc) : nullptr;
  std::string snapRaw = cffq_.raw;
  std::vector<std::string> snapToks = cffq_.toks;
  cffqAdd(p);
  std::unique_ptr<ParsedMessage> merged = cloneNoInner(*locSrc);
  merged->type = "cff";
  merged->warno = cffq_.warno != "none" ? cffq_.warno : locSrc->warno;
  merged->desc = desc;
  merged->raw = cffq_.raw;
  merged->toks = cffq_.toks;
  cffqReset();
  const sim::Mission* m = machine_.activeMission();
  if (!handleCFF(*merged, /*staged=*/true) && !(m = machine_.activeMission(),
                                                m && !m->done)) {
    cffq_.warno = snapWarno;
    cffq_.loc = std::move(snapLoc);
    cffq_.raw = snapRaw;
    cffq_.toks = std::move(snapToks);
    cffq_.on = true; cffq_.stage = 2; cffq_.t0 = clock_.now;
  }
}

/* ---- handleCFF (12-cff-handling.js:147-414) ---------------------------- */

bool FdcEngine::handleCFF(const ParsedMessage& p, bool staged) {
  const double rbDelay = machine_.tunables().fdc.readbackDelay;
  sim::Mission* mission = machine_.activeMission();
  if (mission && !mission->done) {   /* :148-159 */
    /* SUGG5 — two-target scenarios accept a second CFF (:153-155). */
    if (view_.hasTwin() && mission->mto.has_value() &&
        machine_.openMissions().size() < 2) {
      sayNet("SECOND MISSION ACCEPTED - two on the net. LEAD EVERY "
             "TRANSMISSION WITH THE TARGET NUMBER from here, OUT.", 0.9);
    } else {
      sayNet("MUSTANG 12, we are mid-mission. Adjust, fire for effect, or "
             "send end of mission, over.", 1.0);
      return false;   /* NET14 */
    }
  }
  const bool m60 = chapter_.active && chapter_.asset == sim::AssetKind::Mortar60;
  /* 60mm precision shoots demand the full 8-digit grid (:161-165). */
  if (m60 && p.method == "grid" && p.digits.size() == 6) {
    sayNet("MUSTANG 12, HELLHOUND " EMD " this is a 60 MIKE MIKE precision "
           "shoot. Six digits is a neighborhood; I need an ADDRESS. Send 8 "
           "digits, ten-meter precision, over.", 1.1);
    return false;   /* NET14 */
  }
  /* strict net (Volume IV) — :167-184. */
  if (chapter_.active && chapter_.strict) {
    const std::string challenge = strictChallenge(p, chapter_, calls_);
    if (!challenge.empty()) {
      sayNet("MUSTANG 12, HELLHOUND " EMD " STRICT NET: " + challenge, 1.1);
      return false;   /* NET14 */
    }
  }
  /* target location -> world point, any method (:186-247). */
  double cx = 0, cz = 0;
  std::string locStr;
  const std::vector<scenario::Kp> noKps;
  const std::vector<scenario::Kp>& kps = scn_ ? scn_->kps : noKps;
  if (p.method == "shift") {   /* :187-204 */
    const scenario::Kp* kp = nullptr;
    for (const scenario::Kp& k : kps)
      if (k.id == p.kpId) { kp = &k; break; }
    if (!kp) {
      sayNet("NEGATIVE. There is no known point " + p.kpId +
             " registered on this net. Say again, over.", 1.0);
      return false;   /* NET14 */
    }
    const double az = p.dirMils.has_value()
        ? std::fmod(*p.dirMils, 6400.0) / MILS_PER_RAD
        : azTo(op_.x, op_.z, kp->x, kp->z);
    const double fx = std::sin(az), fz = -std::cos(az);
    const double rx = std::cos(az), rz = std::sin(az);
    cx = kp->x + fx * p.sAdd + rx * p.sRight;
    cz = kp->z + fz * p.sAdd + rz * p.sRight;
    locStr = "SHIFT KNOWN POINT " + p.kpId;
    if (p.dirMils.has_value())
      locStr += ", DIRECTION " + fmtMils(std::fmod(*p.dirMils, 6400.0));
    if (truthy(p.sRight))
      locStr += std::string(", ") + (p.sRight > 0 ? "RIGHT" : "LEFT") + " " +
                jsNum(std::fabs(p.sRight));
    if (truthy(p.sAdd))
      locStr += std::string(", ") + (p.sAdd > 0 ? "ADD" : "DROP") + " " +
                jsNum(std::fabs(p.sAdd));
    if (p.vertM.has_value() && truthy(*p.vertM))
      locStr += std::string(", ") + (*p.vertM > 0 ? "UP" : "DOWN") + " " +
                jsNum(std::fabs(*p.vertM));
  } else if (p.method == "polar") {   /* :205-225 */
    /* G9 — no position report, no polar mission (:208-213). */
    if (!posRep_.has_value()) {
      sayNet("MUSTANG 12, HELLHOUND " EMD " I cannot resolve a POLAR mission. "
             "I do not hold your position. Send a position report first " EMD
             " \"POSITION GRID 245 523\" " EMD " then your call, over.", 1.1);
      return false;   /* NET14 */
    }
    if (p.distM < 100 || p.distM > 12000) {   /* :214-217 */
      sayNet(pick(quips_.badGrid, flavor_), 1.0);
      return false;   /* NET14 */
    }
    /* Resolved from the REPORTED position, not the true one (:218-223). */
    const double az = std::fmod(p.dirMils.value_or(0), 6400.0) / MILS_PER_RAD;
    cx = posRep_->x + std::sin(az) * p.distM;
    cz = posRep_->z - std::cos(az) * p.distM;
    locStr = "DIRECTION " + fmtMils(std::fmod(p.dirMils.value_or(0), 6400.0)) +
             ", DISTANCE " + jsNum(p.distM);
    if (p.vertM.has_value() && truthy(*p.vertM))
      locStr += std::string(", ") + (*p.vertM > 0 ? "UP" : "DOWN") + " " +
                jsNum(std::fabs(*p.vertM));
  } else if (p.digits.size() == 6) {   /* :226-231 */
    const double e3 = parseIntStr(p.digits.substr(0, 3));
    const double n3 = parseIntStr(p.digits.substr(3));
    const WorldXZ w = enToWorld(e3 * 100 + 50, n3 * 100 + 50, map_);
    cx = w.x; cz = w.z;
    locStr = "GRID " + jsNum(e3) + " " + jsNum(n3);
    if (sunNet(chapter_))   /* 11c — cheerful, and it proceeds anyway (:230) */
      sayNet("A six-digit grid! Charmingly analog. SUNLAMP would cheerfully "
             "accept TEN digits " EMD " one-meter fidelity " EMD " but we "
             "will simply aim at the middle of your hundred-meter square. "
             "Proceeding!", 1.3);
  } else if (p.digits.size() == 8) {   /* :232-238 */
    const double e4 = parseIntStr(p.digits.substr(0, 4));
    const double n4 = parseIntStr(p.digits.substr(4));
    const WorldXZ w = enToWorld(e4 * 10 + 5, n4 * 10 + 5, map_);
    cx = w.x; cz = w.z;
    locStr = "GRID " + p.digits;
    if (sunNet(chapter_))
      sayNet("We do LOVE a ten-digit grid here at SUNLAMP " EMD " one-meter "
             "fidelity, zero-meter ambiguity. Eight will absolutely do. "
             "Proceeding!", 1.3);
  } else if (p.digits.size() == 10 && sunNet(chapter_)) {   /* :239-243 */
    const double e5 = parseIntStr(p.digits.substr(0, 5));
    const double n5 = parseIntStr(p.digits.substr(5));
    const WorldXZ w = enToWorld(e5 + 0.5, n5 + 0.5, map_);
    cx = w.x; cz = w.z;
    locStr = "GRID " + p.digits;
  } else {   /* :244-247 */
    sayNet(pick(quips_.badGrid, flavor_), 1.0);
    return false;   /* NET14 */
  }
  {   /* map-sheet bounds (:248-253) */
    const EN en = worldToEN(cx, cz, map_);
    if (en.e < map_.originE || en.e >= map_.originE + 10000 ||
        en.n < map_.originN || en.n >= map_.originN + 10000) {
      sayNet(pick(quips_.badGrid, flavor_), 1.0);
      return false;   /* NET14 */
    }
  }
  /* danger-close gate (:254-264) — F2 per-asset radius, F6 tolerant proword. */
  double minF = 1e300;
  for (const sim::FriendlyPos& f : view_.friendlies())
    minF = std::fmin(minF, dist2(cx, cz, f.x, f.z));
  const bool dcSent = saidDangerClose(p.raw);
  if (minF <= dangerCloseR() && !dcSent) {
    sayNet(pick(quips_.dangerClose, flavor_), 1.2);
    return false;   /* NET14 */
  }
  if (dcSent) locStr += ", DANGER CLOSE";
  /* TEMPO2 — immediate missions are exempt from format notes (:265-270). */
  std::vector<std::string> notes =
      !p.imm.empty() ? std::vector<std::string>{}
                     : formatNotes(p, fdcCallFor(chapter_, calls_));
  /* G23 — the conditional vertical (:277-295). */
  if (p.method == "polar" || p.method == "shift") {
    double refH = op_.h;
    if (p.method == "shift") {
      for (const scenario::Kp& k : kps)
        if (k.id == p.kpId) { refH = view_.groundHeight(k.x, k.z); break; }
    }
    const double dH = view_.groundHeight(cx, cz) - refH;
    const bool needVert = std::fabs(dH) >= 35;
    const bool sentVert = p.vertM.has_value();
    if (needVert && !sentVert)
      notes.push_back(
          "Height difference to the target is " + jsNum(js::round(dH)) +
          " m " EMD " at 35 m or more the vertical shift is part of the "
          "call. Send " + (dH > 0 ? "UP" : "DOWN") + " " +
          jsNum(js::round(std::fabs(dH) / 5) * 5) + ".");
    else if (!needVert && sentVert)
      notes.push_back("You sent a vertical shift, but the height difference "
                      "is only " + jsNum(js::round(dH)) +
                      " m. Below 35 m it is omitted.");
    else if (sentVert && truthy(std::fmod(std::fabs(*p.vertM), 5.0)))
      notes.push_back("Vertical shift is sent to the nearest 5 m.");
  }
  if (chapter_.active && !chapter_.requiredMethod.empty() &&
      p.method != chapter_.requiredMethod)   /* :296-297 */
    notes.push_back("This chapter required a " +
                    upperAscii(chapter_.requiredMethod) +
                    " target location " EMD " you sent " +
                    upperAscii(p.method) + " (chapter capped at 2" STAR5 ").");
  if (chapter_.active && chapter_.reqImmediate && p.imm.empty()) /* :303-304 */
    notes.push_back("This chapter demanded IMMEDIATE SUPPRESSION " EMD " one "
                    "transmission, rounds NOW. The deliberate call works, but "
                    "it cost time the pinned squad did not have (chapter "
                    "capped at 2" STAR5 ").");
  const std::string warno = p.warno == "none" ? "adjust" : p.warno;  /* :305 */
  const std::string warnoText =
      warno == "ffe" ? "FIRE FOR EFFECT" : "ADJUST FIRE";
  /* G22/G14/G26 — the readback (:317-325). */
  const std::string immText =
      p.imm == "smoke" ? "IMMEDIATE SMOKE" : "IMMEDIATE SUPPRESSION";
  if (!p.imm.empty())
    sayNet(immText + ", " + locStr + ", OUT.", 0.7);
  else if (staged)
    sayNet((p.desc.empty() ? std::string("NO DESCRIPTION")
                           : upperAscii(p.desc)) +
               (saidDangerClose(p.raw) ? ", DANGER CLOSE" : "") + ", OUT.",
           rbDelay);
  else
    sayNet(calls_.obs + ", " + fdcCallFor(chapter_, calls_) + ", " +
               warnoText + ", " + locStr +
               (p.desc.empty() ? "" : ", " + upperAscii(p.desc)) + ", OUT.",
           rbDelay);
  /* G15/G16 — method of engagement before the MTO (:332-345). */
  const sim::SheafSpec sheaf = inferSheaf(p.desc, p.raw);
  sim::FuzeSpec fuze = inferFuze(p.desc, p.raw);
  if ((fuze.kind == sim::FuzeKind::Vt || fuze.kind == sim::FuzeKind::Time) &&
      saidDangerClose(p.raw)) {   /* :341-345 */
    if (fuze.source == "requested")
      sayNet("NEGATIVE ON VT, MUSTANG " EMD " no airburst over friendlies at "
             "danger close. FUZE PD, over.", 1.2);
    fuze = sim::FuzeSpec{sim::FuzeKind::Pd, "fdc-override",
                         "airburst is not fired DANGER CLOSE " EMD
                         " PD substituted"};
  }
  /* 12h — shell nature from the call (:350-354). */
  static const std::regex reShell(
      R"(\bshell\s+(he|smoke|wp|white phosphorus|illum\w*)\b)");
  static const std::regex reIllum(R"(\billum(?:ination)?\b)");
  std::smatch mSh;
  const bool haveSh = std::regex_search(p.raw, mSh, reShell);
  const std::string shWord = haveSh ? mSh[1].str() : "";
  const std::string shell =
      p.imm == "smoke" ? "smoke"
      : haveSh ? (shWord == "wp" || shWord == "white phosphorus" ? "smoke"
                  : shWord.rfind("illum", 0) == 0 ? "illum" : shWord)
      : std::regex_search(p.raw, reIllum) ? "illum" : "he";
  const sim::ShellKind shellKind =
      shell == "smoke" ? sim::ShellKind::Smoke
      : shell == "illum" ? sim::ShellKind::Illum : sim::ShellKind::He;
  /* G11 — the MTO, built locally, handed via meta (:355-386). */
  std::optional<sim::MtoSpec> mtoSpec;
  if (p.imm.empty()) {
    const std::string natureTxt =
        shell == "he" ? "HE, FUZE " + upperAscii(fuzeKindWord(fuze.kind))
        : shell == "smoke" ? "SMOKE" : "ILLUMINATION, FUZE TIME";
    sim::MtoSpec spec;
    if (sunNet(chapter_)) {   /* 11c (:360-363) */
      spec.keys = {"aperture", "array", "adjust", "effect", "directed",
                   "energy"};
      spec.tgt = "7003";
    } else if (m60) {   /* NET22 (:365-372) */
      spec.keys = {"tube", "section", "adjust", "effect",
                   "round", "60", "mike", shell == "he" ? "he" : shell};
      spec.tgt = "7002";
    } else {   /* :373-375 */
      spec.keys = {"gun", "battery", "adjust", "effect", "round", "155"};
      if (shell == "he") {
        spec.keys.push_back("he");
        spec.keys.push_back("fuze");
        spec.keys.push_back(fuzeKindWord(fuze.kind));
      } else {
        spec.keys.push_back(shell);
      }
      spec.tgt = "7001";
    }
    /* SUGG5 — the second slot shifts its number by ten (:376-379). */
    if (machine_.openMissions().size() >= 1)
      spec.tgt = jsNum(parseIntStr(spec.tgt) + 10);
    mtoSpec = spec;
    sayNet(
        sunNet(chapter_)
            ? "MESSAGE TO OBSERVER: ONE APERTURE IN ADJUST, FULL ARRAY IN "
              "EFFECT, DIRECTED ENERGY, TARGET NUMBER ALPHA ALPHA 7003. "
              "THANK YOU FOR CHOOSING SUNLAMP, OVER."
        : m60
            ? "MESSAGE TO OBSERVER: ONE TUBE IN ADJUST, SECTION IN EFFECT, " +
                  jsNum(machine_.tunables().ballistics.ffeRounds) +
                  " ROUNDS, 60 MIKE MIKE " +
                  (shell == "he" ? "HE" : natureTxt) +
                  ", TARGET NUMBER ALPHA ALPHA " + spec.tgt + ", OVER."
            : "MESSAGE TO OBSERVER: ONE GUN IN ADJUST, BATTERY IN EFFECT, 1 "
              "ROUND, 155 " + natureTxt + ", TARGET NUMBER ALPHA ALPHA " +
                  spec.tgt + ", OVER.",
        1.4);
  }
  /* deviation policy (:387-390): sloppy-but-safe earns snide; clean gets
     the 45%-gated readback color. SANCTIONED FLAVOR gate (12-cff:390). */
  if (notes.size() >= 2) sayNet(pick(quips_.snide, flavor_), 1.1);
  /* wave-3 reconciliation: gate via coin() so replay reproduces recorded
     presence (fdc_quips.h FlavorGate note) — this line precedes fireMission,
     so its presence shifts the first round's SHOT time. */
  else if (flavor_.coin(0.45, FlavorGate::ReadbackTail,
                        std::fmax(clock_.now, speaker_.lastT()) + 1.1))
    sayNet(pick(quips_.readbackTail, flavor_), 1.1);
  /* near-civilian advisory (:392-395) — non-blocking. */
  double minCiv = 1e300;
  for (const sim::Vec2& hh : view_.huts())
    minCiv = std::fmin(minCiv, dist2(cx, cz, hh.x, hh.z));
  if (minCiv < 250) sayNet(pick(quips_.nearCiv, flavor_), 1.2);
  if (view_.groundHeight(cx, cz) < 0) sayNet(pick(quips_.water, flavor_), 1.0);
  machine_.setState(sim::MissionPhase::MissionSent);   /* :397 */
  sysLog("SHEAF: " + upperAscii(sheafKindWord(sheaf.kind)) + " (" +
         sheaf.source + ") " EMD " " + sheaf.why + ".");   /* :398 */
  sysLog("FUZE: " + upperAscii(fuzeKindWord(fuze.kind)) + " (" + fuze.source +
         ") " EMD " " + fuze.why + ".");                   /* :399 */
  if (shell != "he")   /* :400 */
    sysLog("SHELL: " +
           std::string(shell == "illum"
                           ? "ILLUMINATION " EMD " light, not fires"
                           : "SMOKE " EMD " a screen, not casualties") + ".");
  /* :404-412 — hand everything to fireMission. */
  sim::MissionMeta meta;
  meta.notes = notes;
  meta.desc = p.desc;
  meta.gridStr = locStr;
  meta.method = p.method == "polar" ? sim::MethodKind::Polar
              : p.method == "shift" ? sim::MethodKind::Shift
              : sim::MethodKind::Grid;
  meta.mto = mtoSpec;
  meta.amc = p.raw.find("at my command") != std::string::npos;   /* G24 */
  meta.sheaf = sheaf;
  meta.fuze = fuze;
  meta.shell = shellKind;
  meta.imm = p.imm == "suppress" ? sim::ImmKind::Suppress
           : p.imm == "smoke" ? sim::ImmKind::Smoke : sim::ImmKind::None;
  meta.intent = (!p.imm.empty() || shell == "smoke") ? sim::IntentKind::Suppress
              : shell == "illum" ? sim::IntentKind::Illum
              : sim::IntentKind::Destroy;   /* G13/12h */
  machine_.fireMission(sim::Vec2{cx, cz},
                       warno == "ffe" ? sim::Warno::Ffe : sim::Warno::Adjust,
                       meta);
  noMissionStreak_ = 0;   /* 10-mission.js:133 — the player got unstuck */
  return true;   /* NET14 — accepted, mission open */
}

/* ---- safety stops (G25, 12-cff:430-466) -------------------------------- */

void FdcEngine::handleCheckFiring() {   /* :430-445 */
  sayNet("CHECK FIRING, CHECK FIRING " EMD " ALL GUNS SAFE, OUT.", 0.5);
  sim::Mission* mission = machine_.activeMission();
  if (!mission || mission->done) {
    sysLog("CHECK FIRING acknowledged. No mission in progress " EMD
           " nothing was in the air.");
    return;
  }
  mission->checkFire = true;
  mission->usedCheckFire = true;
  machine_.setState(sim::MissionPhase::CheckFiring);
  /* :439 reads `state` AFTER the setState above, so inFlight is always
     false in the source — ported bug-for-bug. */
  const bool inFlight = machine_.state() == sim::MissionPhase::Shot ||
                        machine_.state() == sim::MissionPhase::FireForEffect;
  sysLog(std::string("CHECK FIRING " EMD " the battery will fire nothing "
                     "further until you resume. ") +
         (inFlight ? "Rounds already in the air will still impact; a "
                     "check-firing call cannot recall a shell in flight. "
                   : "") +
         "Send a correction or FIRE FOR EFFECT when you are ready to "
         "continue.");
}

void FdcEngine::handleCeaseLoading() {   /* :446-457 */
  sayNet("CEASE LOADING, OUT.", 0.5);
  sim::Mission* mission = machine_.activeMission();
  if (!mission || mission->done) {
    sysLog("CEASE LOADING acknowledged. No mission in progress.");
    return;
  }
  mission->checkFire = true;
  mission->usedCheckFire = true;
  machine_.setState(sim::MissionPhase::CeaseLoading);
  sysLog("CEASE LOADING " EMD " no further rounds will be loaded. Anything "
         "already loaded or in the air still goes downrange. Send a "
         "correction or FIRE FOR EFFECT to resume.");
}

bool FdcEngine::liftCheckFire() {   /* :460-466 */
  sim::Mission* mission = machine_.activeMission();
  if (!mission || !mission->checkFire) return false;
  mission->checkFire = false;
  sayNet("CANCEL CHECK FIRING " EMD " BATTERY IS HOT, OUT.", 0.6);
  machine_.setState(sim::MissionPhase::Adjusting);
  return true;
}

/* ---- OT direction (G2/G8, 12-cff:470-513) ------------------------------ */

void FdcEngine::handleDirection(const ParsedMessage& p) {
  sim::Mission* mission = machine_.activeMission();
  if (!mission || mission->done) { noMissionReply(); return; }   /* NET4 */
  mission->otDirSent = true;
  const double sent = p.dirMils.value_or(0);
  const double trueAz =
      radToMils(azTo(op_.x, op_.z, mission->aim.x, mission->aim.z));
  sayNet("DIRECTION " + fmtMils(std::fmod(sent, 6400.0)) + ", OUT.", 1.0);
  double err = 0;
  const DirDiagnosis diag =
      diagnoseDirection(sent, trueAz, declEastDeg_, &err);
  const double D = js::round(declMilsFor(declEastDeg_));   /* :493 */
  if (diag == DirDiagnosis::Magnetic) {   /* :502-505 */
    if (machine_.coachOn())
      sayNet("MUSTANG, that is your MAGNETIC reading. The net runs on grid. "
             "Add your G-M angle " EMD " " + jsNum(D) +
             " mils " EMD " and send it again, over.", 1.4);
    mission->notes.push_back(
        "Sent OT direction as a magnetic azimuth. The HUD reads MAG; the net "
        "wants GRID. Add " + jsNum(D) + " mils (declination " +
        jsNum(declEastDeg_) + DEG " E).");
  } else if (diag == DirDiagnosis::Reversed) {   /* :506-509 */
    if (machine_.coachOn())
      sayNet("MUSTANG, you have your G-M angle the wrong way round " EMD
             " that is " + jsNum(D) + " mils PAST the line, not short of it. "
             "Grid is magnetic PLUS the angle. Send it again, over.", 1.4);
    mission->notes.push_back(
        "Applied the G-M angle in the wrong direction (subtracted instead of "
        "added). GRID = MAG + " + jsNum(D) + " mils.");
  } else if (diag == DirDiagnosis::Rough && machine_.coachOn()) { /* :510-512 */
    sayNet("MUSTANG, that direction is off your own line of sight by roughly " +
           jsNum(js::round(err)) + " mils. Your left and right corrections "
           "are computed against it " EMD " check your compass, over.", 1.4);
  }
}

/* ---- handleAdjust (12-cff:515-722) ------------------------------------- */

void FdcEngine::handleAdjust(const ParsedMessage& p) {
  sim::Mission* mission = machine_.activeMission();
  if (!mission || mission->done) {   /* :516-520 */
    noMissionReply();
    sysHint();
    return;
  }
  liftCheckFire();   /* G25 — before the state gate (:521-524) */
  MissionExtra& extra = extraFor(mission);
  /* G11 — skipped MTO readback (:530-542): strict blocks ONCE, forgiving
     coaches once and moves on. */
  if (mission->mto.has_value() && !mission->mto->read && !extra.mtoNagged) {
    extra.mtoNagged = true;
    mission->notes.push_back(
        "Never read back the message to observer " EMD " the readback is "
        "your only check that the FDC has the right target number.");
    if (chapter_.active && chapter_.strict) {
      sayNet("MUSTANG 12, HELLHOUND " EMD " STRICT NET: you did not read "
             "back my message to observer. Read it back " EMD " target "
             "number ALPHA ALPHA " + mission->mto->tgt +
             " " EMD " then send your correction, over.", 1.1);
      return;
    }
    if (machine_.coachOn())
      sayNet("Copy " EMD " but you never read back my MTO, MUSTANG. That "
             "readback is how we both know you are shooting the target I "
             "think you are shooting. Continuing, out.", 1.4);
  }
  /* G8 — no OT direction, no correction; grid missions only (:553-566). */
  if (p.any && !mission->otDirSent &&
      mission->method == sim::MethodKind::Grid) {
    if (!extra.dirNagged) {
      extra.dirNagged = true;
      mission->notes.push_back(
          "Sent a correction before OT DIRECTION on a grid mission " EMD
          " the FDC cannot orient left/right or add/drop without the "
          "observer-target line.");
      sayNet("MUSTANG 12, HELLHOUND " EMD " I cannot execute that. I have no "
             "OT DIRECTION for this mission: without your observer-target "
             "line I do not know which way your LEFT is or along what line "
             "to ADD. Send DIRECTION " EMD " grid azimuth to the target, "
             "nearest 10 mils " EMD " then your correction, over.", 1.1);
    } else {
      sayNet("STILL no OT direction, MUSTANG. DIRECTION first, then the "
             "correction, over.", 1.0);
    }
    return;
  }
  /* G13 — only an ACCOMPLISHED mission gets "send end of mission" (:572). */
  if (machine_.state() == sim::MissionPhase::EomQuery &&
      machine_.missionAccomplished()) {
    sayNet("MUSTANG 12, rounds are complete and the target is done. Send end "
           "of mission, over.", 1.0);
    return;
  }
  /* F5 — stray numbers refuse the half-correction (:580-587). */
  if (p.stray > 0) {
    sayNet("MUSTANG 12, I have " +
           (p.stray == 1 ? std::string("a number")
                         : jsNum(p.stray) + " numbers") +
           " in that correction I cannot attach to anything. Say again with "
           "the proword " EMD " LEFT or RIGHT, then ADD or DROP, over.", 1.0);
    mission->notes.push_back(
        "A correction contained a number with no proword attached. Deviation "
        "first (\"LEFT/RIGHT nnn\"), then range (\"ADD/DROP nnn\").");
    return;
  }
  if (machine_.state() != sim::MissionPhase::Adjusting &&
      machine_.state() != sim::MissionPhase::EomQuery) {   /* :588 */
    sayNet(pick(quips_.inFlight, flavor_), 1.0);
    return;
  }
  /* G23/G16 — height of burst (:599-621). */
  if (truthy(p.corr.vert)) {
    const double v = p.corr.vert;
    if (mission->fuze.kind == sim::FuzeKind::Vt ||
        mission->fuze.kind == sim::FuzeKind::Time) {
      sayNet(std::string(v > 0 ? "UP" : "DOWN") + " " + jsNum(std::fabs(v)) +
             ", OUT.", 0.9);
      if (truthy(std::fmod(std::fabs(v), 5.0)))
        mission->notes.push_back(
            "Height of burst is sent in multiples of 5 m.");
    } else {
      sayNet(std::string(v > 0 ? "UP" : "DOWN") + " " + jsNum(std::fabs(v)) +
             " " EMD " NEGATIVE, MUSTANG, we are firing FUZE QUICK. Height "
             "of burst does nothing on a point-detonating round. Deviation "
             "and range only, over.", 1.1);
      mission->notes.push_back(
          "Sent a height-of-burst correction against fuze quick. HOB applies "
          "to airburst (VT/time) fuzes; with PD it has no effect.");
      if (truthy(std::fmod(std::fabs(v), 5.0)))
        mission->notes.push_back(
            "Height of burst is sent in multiples of 5 m.");
    }
    /* vertical-only transmission is fully handled (:620) */
    if (!truthy(p.corr.right) && !truthy(p.corr.add) && !p.ffe) return;
  }
  /* strict-net rounding (:625-632). */
  if (chapter_.active && chapter_.strict && p.any &&
      strictRoundingViolation(p.corr.right, p.corr.add, p.ffe)) {
    sayNet(p.ffe
               ? "MUSTANG 12, HELLHOUND " EMD " STRICT NET: deviation in "
                 "TENS, range in FIFTIES entering fire for effect. Recompute "
                 "and say again, over."
               : "MUSTANG 12, HELLHOUND " EMD " STRICT NET: deviation in "
                 "TENS, range in HUNDREDS. Fifties are for the correction "
                 "that takes us into fire for effect, not this one. "
                 "Recompute and say again, over.",
           1.0);
    return;
  }
  /* dangerous-deviation gate (:636-691). */
  if (p.any) {
    const sim::Vec2 a = mission->aim;
    const double otAz = azTo(op_.x, op_.z, a.x, a.z);
    const double nx =
        a.x + std::sin(otAz) * p.corr.add + std::cos(otAz) * p.corr.right;
    const double nz =
        a.z - std::cos(otAz) * p.corr.add + std::sin(otAz) * p.corr.right;
    std::string danger;   /* "" = null */
    const bool declaredDC =
        mission->gridStr.find("DANGER CLOSE") != std::string::npos;
    if (declaredDC) {
      /* NET23 — creeping fire + toward-friendlies proximity (:644-672). */
      const double corrMag = std::hypot(p.corr.right, p.corr.add);
      const std::string keyDC =
          jsNum(p.corr.right) + "," + jsNum(p.corr.add);
      if (corrMag > 100 &&
          (!extra.creepKey.has_value() || *extra.creepKey != keyDC)) {
        extra.creepKey = keyDC;
        if (!extra.creepNoted) {
          extra.creepNoted = true;
          mission->notes.push_back(
              "Danger close demands creeping fire " EMD " corrections of 100 "
              "m or less; a " + jsNum(js::round(corrMag)) +
              " m correction was sent.");
        }
        sayNet("MUSTANG, this is DANGER CLOSE " EMD " we CREEP. One hundred "
               "meters or less, from the safe side. Send it smaller, or send "
               "it again as-is and own it, over.", 1.0);
        return;
      }
      if (corrMag > 100)
        sayNet("Sent as-is. On your head, MUSTANG, out.", 1.0);
      extra.creepKey.reset();
      double dNew = 1e300, dOld = 1e300;
      for (const sim::FriendlyPos& f : view_.friendlies()) {
        dNew = std::fmin(dNew, dist2(nx, nz, f.x, f.z));
        dOld = std::fmin(dOld, dist2(a.x, a.z, f.x, f.z));
      }
      if (dNew < dOld && dNew < dangerCloseR() * 0.5)
        danger = "the friendly position";
    } else {
      for (const sim::FriendlyPos& f : view_.friendlies()) {
        const double r =
            f.hasR ? f.r : machine_.tunables().mission.fratricideRadius;
        if (dist2(nx, nz, f.x, f.z) < r + 50) {
          danger = "the friendly position";
          break;
        }
      }
    }
    if (danger.empty()) {   /* :679-682 */
      for (const sim::Vec2& hh : view_.huts())
        if (dist2(nx, nz, hh.x, hh.z) < 70) { danger = "the village"; break; }
    }
    const std::string key = jsNum(p.corr.right) + "," + jsNum(p.corr.add);
    if (!danger.empty() &&
        (!extra.unsafeKey.has_value() || *extra.unsafeKey != key)) {
      extra.unsafeKey = key;   /* :684-688 — refuse; identical resend overrides */
      sayNet(substFirst(pick(quips_.unsafeCorr, flavor_), "{WHO}", danger),
             1.0);
      return;
    }
    if (!danger.empty()) sayNet(pick(quips_.unsafeInsist, flavor_), 1.0);
    extra.unsafeKey.reset();
  }
  /* timid-correction coach (:694-701). */
  if (p.any && machine_.coachOn() && !extra.coachedTimid &&
      mission->lastAdjDist.has_value()) {
    const double corrMag = std::hypot(p.corr.right, p.corr.add);
    if (*mission->lastAdjDist > 150 &&
        corrMag < *mission->lastAdjDist * 0.45) {
      extra.coachedTimid = true;
      sayNet(substFirst(pick(quips_.coachTimid, flavor_), "{MISS}",
                        jsNum(js::round(*mission->lastAdjDist))),
             1.6);
    }
  }
  /* apply + readback (:702-711). */
  std::vector<std::string> parts;
  if (p.any) {
    mission->corrs.push_back(js::round(std::hypot(p.corr.right, p.corr.add)));
    if (truthy(p.corr.right))
      parts.push_back(std::string(p.corr.right > 0 ? "RIGHT" : "LEFT") + " " +
                      jsNum(std::fabs(p.corr.right)));
    if (truthy(p.corr.add))
      parts.push_back(std::string(p.corr.add > 0 ? "ADD" : "DROP") + " " +
                      jsNum(std::fabs(p.corr.add)));
    machine_.applyCorrection(p.corr.add, p.corr.right);
  }
  if (p.ffe) parts.push_back("FIRE FOR EFFECT");
  else if (p.rep && machine_.state() == sim::MissionPhase::EomQuery)
    parts.push_back("REPEAT");   /* G13 (:710) */
  sayNet(joinList(parts, ", ") + ", OUT.", 1.0);
  /* stupid-but-safe snark — SANCTIONED FLAVOR gates 0.55 (:714) / 0.5 (:716).
     wave-3 reconciliation: both precede fireForEffect/fireAdjustRound below,
     so presence shifts that round's SHOT time — decided via coin()
     (fdc_quips.h FlavorGate note). */
  if (p.any && strictRoundingViolation(p.corr.right, p.corr.add, p.ffe) &&
      flavor_.coin(0.55, FlavorGate::SnideRound,
                   std::fmax(clock_.now, speaker_.lastT()) + 1.3))
    sayNet(pick(quips_.snideRound, flavor_), 1.3);
  else if (std::fabs(p.corr.right) + std::fabs(p.corr.add) > 150 &&
           flavor_.coin(0.5, FlavorGate::CorrSnark,
                        std::fmax(clock_.now, speaker_.lastT()) + 1.0))
    sayNet(pick(quips_.corrSnark, flavor_), 1.0);
  /* G13 (:720-721). */
  if (p.ffe || (p.rep && machine_.state() == sim::MissionPhase::EomQuery))
    machine_.fireForEffect();
  else
    machine_.fireAdjustRound();
}

/* ---- handleEOM (12-cff:724-802) ---------------------------------------- */

void FdcEngine::handleEOM(const ParsedMessage& p) {
  sim::Mission* mission = machine_.activeMission();
  if (!mission) { noMissionReply(); return; }   /* NET4 */
  /* NET26 — a second EOM on a closed mission acknowledges and stops. */
  if (mission->done) {
    sayNet("MUSTANG 12, that mission is already closed. The net is yours "
           "when you have new traffic, out.", 0.9);
    return;
  }
  /* strict net: EOM carries surveillance (:738-744). */
  static const std::regex reBda("neutralized|destroyed|suppressed");
  if (chapter_.active && chapter_.strict && !mission->done &&
      !mission->eomChallenged && mission->intent != sim::IntentKind::Illum &&
      !std::regex_search(p.bda, reBda)) {
    mission->eomChallenged = true;
    sayNet("MUSTANG 12, HELLHOUND " EMD " STRICT NET: end of mission carries "
           "surveillance. NEUTRALIZED, DESTROYED, or SUPPRESSED. Say again "
           "with your assessment, over.", 1.1);
    return;
  }
  if (!mission->done) {   /* :745-756 */
    mission->done = true;
    if (!mission->tEnd.has_value()) mission->tEnd = clock_.now;
    static const std::regex reClaim("destroyed|neutralized|suppressed");
    std::smatch mClaim;
    mission->bdaClaim =
        std::regex_search(p.bda, mClaim, reClaim) ? mClaim[0].str() : "";
    const std::string actual = outcomeWord(machine_.assessEffect().outcome);
    if (!mission->bdaClaim.empty() &&
        outcomeRank(mission->bdaClaim) > outcomeRank(actual))
      mission->notes.push_back(
          "Surveillance overclaimed: you reported \"" + mission->bdaClaim +
          "\" " EMD " the assessed effect was " +
          (actual == "none" ? std::string("no effect") : upperAscii(actual)) +
          ". BDA you cannot back up sends the next unit into a live "
          "position.");
  }
  /* RREMS refinement (:759-765). */
  if (p.refine.any) {
    machine_.applyCorrection(p.refine.add, p.refine.right);
    mission->refined = true;
    std::string txt = "REFINEMENT ";
    if (truthy(p.refine.right))
      txt += std::string(p.refine.right > 0 ? "RIGHT " : "LEFT ") +
             jsNum(std::fabs(p.refine.right));
    if (truthy(p.refine.add))
      txt += std::string(truthy(p.refine.right) ? ", " : "") +
             (p.refine.add > 0 ? "ADD " : "DROP ") +
             jsNum(std::fabs(p.refine.add));
    sayNet(txt + ", OUT.", 1.0);
  }
  /* record as target (:766-783) — SUGG2/SUGG8/G27. */
  if (p.record) {
    mission->recorded = true;
    const std::string tn = machine_.nextFreeTgtNum();
    machine_.putRecTgt(tn, mission->aim.x, mission->aim.z);
    sayNet("RECORDED AS TARGET " + tn + ", OUT.", 1.0);
    sysLog("Target " + tn + " is on file at " +
           gridOf(mission->aim.x, mission->aim.z, map_) +
           ". You can re-engage it with \"SUPPRESS TARGET " + tn +
           ", 5 MINUTES, OVER\".");
  }
  sayNet("END OF MISSION" + (p.bda.empty() ? "" : ", " + upperAscii(p.bda)) +
         ", OUT.", 1.2);   /* :784 */
  if (mission->failReason != sim::FailReason::Fratricide &&
      mission->failReason != sim::FailReason::Collateral) {   /* :785-788 */
    sayNet(pick(machine_.missionAccomplished() ? quips_.eomGood
                                               : quips_.eomBad, flavor_),
           1.2);
  }
  /* SUGG5 — closing one of two switches the net to the survivor (:791-799). */
  sim::Mission* other = nullptr;
  for (sim::Mission* m : machine_.openMissions())
    if (m != mission) { other = m; break; }
  if (other) {
    const std::string closedNum =
        mission->mto.has_value() ? "AA" + mission->mto->tgt : "THAT MISSION";
    const std::string otherNum =
        other->mto.has_value() ? "AA" + other->mto->tgt : "the other mission";
    machine_.setActiveMission(other);
    /* JS restores the state global directly; setState is the only exposed
       door (header reconciliation note). */
    machine_.setState(other->phase);
    sayNet(closedNum + " IS CLOSED - " + otherNum + " IS STILL YOURS, OVER.",
           1.4);
    return;
  }
  machine_.setState(sim::MissionPhase::Aar);   /* :800 */
  queue_.schedule(speaker_.lastT() + 1.6, [this]() {   /* :801 */
    if (hooks.showAar) hooks.showAar();
  });
}

/* ---- hints + streaks (12-cff:804-848) ---------------------------------- */

std::string FdcEngine::expectedHint() const {   /* :804-829 */
  const sim::Mission* mission = machine_.activeMission();
  switch (machine_.state()) {
    case sim::MissionPhase::Adjusting:
      return std::string(
                 mission && !mission->otDirSent &&
                         mission->method == sim::MethodKind::Grid
                     ? "Send OT DIRECTION first (\"direction 5920, over\") "
                       EMD " then an OT-line correction, "
                     : "Send an OT-line correction, ") +
             "e.g. \"right 50, add 100, over\" " EMD " or \"fire for effect, "
             "over\" once rounds are on target. [R] in binos for the "
             "mil-relation card.";
    case sim::MissionPhase::EomQuery: {
      if (mission && !mission->done && !machine_.missionAccomplished()) {
        const sim::EffectAssessment a = machine_.assessEffect();
        return a.outcome == sim::EffectOutcome::Suppressed
            ? "Target suppressed only " EMD " that wears off. Continue "
              "(\"right 50, repeat\" or bare REPEAT) or close with \"end of "
              "mission, target suppressed, over\"."
            : "No effect yet " EMD " the mission is still open. Correct and "
              "REPEAT (\"drop 100, repeat\"), or close with END OF MISSION "
              "and own the result.";
      }
      return "Close with RREMS: \"end of mission, target neutralized, over\" "
             EMD " you may add a refinement (\"left 10, drop 50\") and "
             "\"record as target\" first.";
    }
    case sim::MissionPhase::Shot:
    case sim::MissionPhase::MissionSent:
    case sim::MissionPhase::FireForEffect:
      return "Rounds are in progress " EMD " wait for SPLASH, observe the "
             "burst, then correct.";
    default:
      /* UX4 — example digits are off-map on purpose. */
      return "Send a call for fire " EMD " grid: \"adjust fire, grid 123 "
             "456, infantry in the open, over\" (with YOUR target grid) " EMD
             " or polar: \"adjust fire, direction 4400, distance 2100, "
             "troops in the open, over\".";
  }
}
void FdcEngine::sysHint() { sysLog("HINT: " + expectedHint()); }   /* :830 */

void FdcEngine::noMissionReply() {   /* :840-848 (NET4) */
  noMissionStreak_++;
  if (noMissionStreak_ >= 3) {
    noMissionStreak_ = 0;
    sayNet(pick(quips_.rantLost, flavor_), 1.0);
  } else {
    sayNet(pick(quips_.noMission, flavor_), 1.0);
  }
}

/* ---- NET7 notes (12-cff:854-863, net side only) ------------------------ */

void FdcEngine::addNote(const std::string& text) {
  net_.line(NetLineKind::Note, "", text);   /* TLOG verbatim, no parse */
}

/* ---- onPlayerMessage (12-cff:864-1016) --------------------------------- */

void FdcEngine::submitMessage(const std::string& raw) {
  /* NET7 — the note channel (:865-866). */
  static const std::regex reNote(R"(^\s*note[:,\s]+(.+)$)",
                                 std::regex::ECMAScript | std::regex::icase);
  std::smatch mNote;
  if (std::regex_search(raw, mNote, reNote)) {
    addNote(trimmed(mNote[1].str()));
    return;
  }
  /* NET18 — stale-backlog cap lives HERE, not in say() (:867-875). */
  speaker_.capBacklog();
  /* NET25 — busy-net acknowledgement, synchronous (:876-883). */
  if (speaker_.lastT() - clock_.now > 2.5)
    sysLog("Net is mid-transmission " EMD " your traffic is copied. Stand "
           "by.");
  /* squelch() is WebAudio — not ported. */
  /* BUILD4/NET9 — oversize refusal before parsing (:885-894). */
  if (raw.size() > 1000) {
    net_.line(NetLineKind::Obs, calls_.obs,
              upperAscii(raw.substr(0, 120)) + " " HELLIP);
    sayNet("MUSTANG, that transmission is longer than my enlistment. Say it "
           "like traffic, over.", 0.8);
    net_.sysErr("oversize transmission refused",
                static_cast<double>(raw.size()));
    return;
  }
  net_.line(NetLineKind::Obs, calls_.obs, upperAscii(raw));   /* :895 */
  /* :899-906 — the NET9 parser guard. parseMessage cannot throw in this
     port (core compiles exceptions-off; the parser is total), so the catch
     arm has no C++ counterpart. */
  const ParsedMessage p = parseMessage(raw, parserContext());
  net_.parseClass(raw, tlogParseClass(p));   /* :907-908 */
  /* tutorial intercept (:911-917). */
  if (hooks.tutorialMsg) {
    if (hooks.tutorialMsg(p)) return;
    if (hooks.tutorialNoMission &&
        (p.type == "cff" || p.type == "adjust" || p.type == "repeat")) {
      sysLog("GUNNY BOTTLECAP: Guns are cold today, killer. Follow the "
             "drill.");
      return;
    }
  }
  /* NET1 — the caller's net (:918-935). */
  sim::Mission* mission = machine_.activeMission();
  if (scn_ && scn_->type == "callin" && scn_->caller.has_value() && spot_ &&
      !cffq_.on &&
      !(mission && mission->mto.has_value() && !mission->mto->read) &&
      (p.type == "unknown" || p.type == "sayagain") &&
      lowerAscii(raw).find(spot_->callerFirstWordLower(
          static_cast<std::size_t>(scn_->caller->i))) != std::string::npos) {
    spot_->callerReply(raw);
    return;
  }
  if (p.type != "unknown" && p.type != "empty") unknownStreak_ = 0;  /* :936 */
  /* G22 — the three-transmission CFF staging (:937-1002). */
  if (!(mission && !mission->done)) {
    if (p.type == "warno") { cffqStart(p.warno, p); return; }   /* :945 */
    if (p.type == "adjust" && p.ffe && !p.any) {                /* :949 */
      cffqStart("ffe", p);
      return;
    }
    /* NET15 — a bare shift announcement is T1 (:955-960). */
    if (!cffq_.on && p.type == "cff" && p.method == "shift" &&
        !truthy(p.sRight) && !truthy(p.sAdd) && !p.dirMils.has_value() &&
        p.desc.empty()) {
      cffqStart(p.warno != "none" ? p.warno : "adjust", p);
      cffq_.kpId = p.kpId;
      return;
    }
    if (cffq_.on) {
      /* NET15 — T2 of a declared shift parses as a correction (:965-975). */
      if (!cffq_.kpId.empty() && cffq_.stage == 1 && p.type == "adjust" &&
          p.any && !p.ffe) {
        static const std::regex reDir(R"(\bdirection\s+(\d+)\b)");
        static const std::regex reUD(R"(\b(up|down)\s+(\d+)\b)");
        std::smatch mDir, mUD;
        const bool hasDir = std::regex_search(p.raw, mDir, reDir);
        const bool hasUD = std::regex_search(p.raw, mUD, reUD);
        ParsedMessage synth;
        synth.type = "cff";
        synth.method = "shift";
        synth.warno = "none";
        synth.kpId = cffq_.kpId;
        synth.sRight = p.corr.right;
        synth.sAdd = p.corr.add;
        if (hasDir) synth.dirMils = parseIntStr(mDir[1].str());
        if (hasUD)
          synth.vertM = (mUD[1].str() == "up" ? 1.0 : -1.0) *
                        parseIntStr(mUD[2].str());
        synth.desc = "";
        synth.raw = p.raw;
        synth.toks = p.toks;
        cffqLocation(synth, locEchoOf(synth));
        return;
      }
      /* T2 — a location with no warning order of its own (:977-985). */
      if (p.type == "cff" && cffq_.stage == 1 && p.warno == "none") {
        const std::string le = locError(p, scn_ ? scn_->kps : std::vector<scenario::Kp>{},
                                        posRep_.has_value(), sunNet(chapter_),
                                        map_, quips_, flavor_);
        if (!le.empty()) {   /* NET17 (:980-981) */
          sayNet(le, 1.0);
          cffq_.t0 = clock_.now;
          return;
        }
        if (!p.desc.empty()) { cffqComplete(p.desc, p); return; }   /* NET5 */
        cffqLocation(p, locEchoOf(p));
        return;
      }
      /* T3 — the description (:989-997). */
      if (cffq_.stage == 2 && (p.type == "unknown" || p.type == "warno")) {
        static const std::regex reOOB(R"(\b(over|out|break)\b)");
        const std::string d =
            descClean(std::regex_replace(p.raw, reOOB, ""));   /* F7 */
        if (!d.empty()) {
          unknownStreak_ = 0;
          cffqComplete(d, p);
          return;
        }
      }
      /* anything deliberate abandons the queue (:1000). */
      if (p.type == "cff" || p.type == "eom") cffqReset();
    }
  }
  /* G11 — a pending MTO readback claims unknown/warno FIRST (:1011-1014). */
  if ((p.type == "unknown" || p.type == "warno") &&
      machine_.tryMtoReadback(p.raw)) {
    unknownStreak_ = 0;
    return;
  }
  dispatchParsed(p, false);
}

/* ---- dispatchParsed (12-cff:1025-1178) --------------------------------- */

void FdcEngine::dispatchParsed(const ParsedMessage& p, bool routed) {
  /* Compose context for the machine-emitted lines that need transmission
     words the payload does not carry (fdc_sink.h note). */
  FdcComposeContext cctx;
  cctx.ptype = p.type;
  cctx.tgtNum = p.tgtNum;
  cctx.digits = p.digits;
  cctx.name = p.name;
  cctx.tgts = p.tgts;
  sink_.setContext(&cctx);
  struct ContextClear {
    FdcMissionSink& s;
    ~ContextClear() { s.setContext(nullptr); }
  } clearOnExit{sink_};

  /* NET17 — CORRECTION replaces the current transmission (:1031-1051). */
  if (p.type == "correction") {
    const ParsedMessage* q = p.inner.get();
    bool bare = !q || q->type == "empty";
    if (!bare && q->type == "unknown") {
      static const std::regex reOO(R"(\b(over|out)\b)");
      bare = trimmed(std::regex_replace(q->raw, reOO, "")).empty();
    }
    if (bare) {
      sayNet("CORRECTION heard. Send the corrected transmission, over.", 0.9);
      return;
    }
    if (cffq_.on && q->type == "cff" && q->warno == "none") {   /* :1039-1048 */
      const std::string le = locError(*q, scn_ ? scn_->kps : std::vector<scenario::Kp>{},
                                      posRep_.has_value(), sunNet(chapter_),
                                      map_, quips_, flavor_);
      if (!le.empty()) { sayNet(le, 1.0); cffq_.t0 = clock_.now; return; }
      cffq_.loc.reset();
      cffq_.stage = 1;
      if (!q->desc.empty()) { cffqComplete(q->desc, *q); return; }
      cffqLocation(*q, locEchoOf(*q));
      return;
    }
    dispatchParsed(*q, routed);
    return;
  }
  /* SUGG5 — routed traffic re-enters with the active mission switched
     (:1052-1065). */
  if (p.type == "routed") {
    sim::Mission* m2 = machine_.findMission(p.tgtNum);
    if (!m2 || m2->done) {
      const std::vector<std::string> open = openMissionKeys();
      sayNet("NEGATIVE - no open mission under " + p.tgtNum + ". " +
             (open.empty() ? "" : "On the net: " + joinList(open, ", ") + ". ") +
             "Over.", 1.0);
      return;
    }
    machine_.setActiveMission(m2);
    machine_.setState(m2->phase);   /* JS: state = m2.phase (header note) */
    if (p.inner) dispatchParsed(*p.inner, true);
    return;
  }
  /* two open missions demand the target number (:1066-1070). */
  if (machine_.openMissions().size() > 1 && isRoutedType(p.type) && !routed) {
    sayNet("MUSTANG 12 - TWO MISSIONS ON THIS NET. Lead with the target "
           "number: \"TARGET " +
           joinList(openMissionKeys(), "\" or \"TARGET ") +
           "\", then your traffic. Over.", 1.0);
    return;
  }

  sim::Mission* mission = machine_.activeMission();
  /* SUGG4b/streak — a planned-fires initiation that opens a mission resets
     the no-mission streak (the source resets inside fireMission, 10:133). */
  const sim::Mission* beforePtr = mission;
  auto streakReset = [this, beforePtr]() {
    const sim::Mission* now = machine_.activeMission();
    if (now && now != beforePtr) noMissionStreak_ = 0;
  };

  const std::string& t = p.type;
  if (t == "empty") { return; }                            /* :1072 */
  if (t == "sayagain") {                                   /* :1073-1075 */
    if (!speaker_.lastMsg().empty())
      sayNet("I SAY AGAIN: " + speaker_.lastMsg(), 1.0);
    return;
  }
  if (t == "cff") { handleCFF(p, false); return; }         /* :1076 */
  if (t == "direction") { handleDirection(p); return; }    /* :1077 */
  if (t == "adjust") { handleAdjust(p); return; }          /* :1078 */
  if (t == "checkfire") { handleCheckFiring(); return; }   /* :1079 */
  if (t == "ceaseload") { handleCeaseLoading(); return; }  /* :1080 */
  if (t == "amc") { machine_.setAtMyCommand(p.on); return; } /* :1081 */
  if (t == "fire") {                                       /* :1082 */
    if (!machine_.observerFire()) noMissionReply();        /* NET4 */
    return;
  }
  if (t == "donotload") { machine_.doNotLoad(); return; }  /* :1083 */
  if (t == "cannotobserve") {                              /* :1084 */
    if (!machine_.cannotObserve()) noMissionReply();       /* NET4 */
    return;
  }
  if (t == "reqsplash") {                                  /* NET24 :1086-1090 */
    sayNet(mission && !mission->done
               ? "SPLASH is standard on this net, MUSTANG " EMD " you will "
                 "hear it five seconds out, over."
               : "Nothing is in the air to splash for. Send the call first, "
                 "over.", 0.9);
    return;
  }
  if (t == "recordtgt") {                                  /* :1091-1101 */
    if (mission && !mission->rounds.empty()) {
      const std::string tn = machine_.nextFreeTgtNum();
      machine_.putRecTgt(tn, mission->aim.x, mission->aim.z);
      mission->recorded = true;
      sayNet("RECORDED AS TARGET " + tn + ", OUT.", 1.0);
      sysLog("Target " + tn + " is on file at " +
             gridOf(mission->aim.x, mission->aim.z, map_) + ".");
    } else {
      sayNet("Nothing worth recording yet " EMD " adjust onto something "
             "first, over.", 0.9);
    }
    return;
  }
  if (t == "mark") {                                       /* :1102-1104 */
    sayNet("NEGATIVE " EMD " no marking rounds on this net. Orient off the "
           "spot report and your map sheet, over.", 0.9);
    return;
  }
  if (t == "tot") {                                        /* :1105 */
    if (!machine_.timeOnTarget(p.sec)) noMissionReply();   /* NET4 */
    return;
  }
  if (t == "suppresstgt") {                                /* :1106 */
    machine_.suppressTarget(p.tgtNum, p.minutes.value_or(0),
                            inferSheaf("", p.raw), inferFuze("", p.raw));
    streakReset();
    return;
  }
  if (t == "prioritytgt") { machine_.priorityTarget(p.tgtNum); return; }
  if (t == "firetgt") {                                    /* :1108 */
    machine_.fireTarget(p.tgtNum, inferSheaf("", p.raw),
                        inferFuze("", p.raw));
    streakReset();
    return;
  }
  if (t == "plantgt") { machine_.planTarget(p.digits); return; }  /* :1109 */
  if (t == "planfpf") {                                    /* :1110 */
    /* Source order: mid-mission refusal precedes the grid/direction checks
       (10:406 before :410). The machine's planFpf carries the refusal and
       returns before reading its arguments (fdc_rules.h header note). */
    if (mission && !mission->done) {
      machine_.planFpf(sim::Vec2{0, 0}, 0, false, "");
      return;
    }
    if (p.digits.size() != 6 && p.digits.size() != 8) {    /* 10:410-413 */
      sayNet("SAY AGAIN THE FPF GRID " EMD " six or eight digits, over.", 1.0);
      return;
    }
    if (!p.dirMils.has_value()) {                          /* 10:414-416 */
      sayNet("AN FPF HAS AN ATTITUDE " EMD " SEND DIRECTION WITH IT (\"PLAN "
             "FPF, GRID, DIRECTION\"), OVER.", 1.0);
      return;
    }
    WorldXZ w;                                             /* 10:418-425 */
    if (p.digits.size() == 6)
      w = enToWorld(parseIntStr(p.digits.substr(0, 3)) * 100 + 50,
                    parseIntStr(p.digits.substr(3)) * 100 + 50, map_);
    else
      w = enToWorld(parseIntStr(p.digits.substr(0, 4)) * 10 + 5,
                    parseIntStr(p.digits.substr(4)) * 10 + 5, map_);
    machine_.planFpf(sim::Vec2{w.x, w.z}, *p.dirMils, saidDangerClose(p.raw),
                     p.digits);
    return;
  }
  if (t == "fpfadj") {                                     /* :1111 */
    machine_.adjustFpf(static_cast<int>(p.gun), p.right, p.add, p.repeat);
    return;
  }
  if (t == "firefpf") {                                    /* :1112 */
    machine_.fireFpf();
    streakReset();
    return;
  }
  if (t == "tofquery") { machine_.tofQuery(); return; }    /* :1113 */
  if (t == "planseries") { machine_.planSeries(p.name, p.tgts); return; }
  if (t == "fireseries") {                                 /* :1115 */
    machine_.fireSeries(p.name);
    streakReset();
    return;
  }
  if (t == "plangroup") { machine_.planGroup(p.name, p.tgts); return; }
  if (t == "firegroup") {                                  /* :1117 */
    machine_.fireGroup(p.name);
    streakReset();
    return;
  }
  if (t == "sheaf") {                                      /* :1118 */
    const sim::RequestedSheaf k =
        p.kind == "converged" ? sim::RequestedSheaf::Converged
        : p.kind == "circular" ? sim::RequestedSheaf::Circular
        : p.kind == "open" ? sim::RequestedSheaf::Open
        : p.kind == "parallel" ? sim::RequestedSheaf::Parallel
        : p.kind == "special" ? sim::RequestedSheaf::Special
        : sim::RequestedSheaf::Linear;
    machine_.setSheaf(k, p.cancel);
    return;
  }
  if (t == "fuze") {                                       /* :1119 */
    machine_.setFuze(p.kind == "vt" ? sim::FuzeKind::Vt
                     : p.kind == "time" ? sim::FuzeKind::Time
                     : p.kind == "delay" ? sim::FuzeKind::Delay
                     : sim::FuzeKind::Pd);
    return;
  }
  if (t == "shell") {                                      /* :1120 */
    machine_.setShell(p.kind == "smoke" ? sim::ShellKind::Smoke
                      : p.kind == "illum" ? sim::ShellKind::Illum
                      : sim::ShellKind::He);
    return;
  }
  if (t == "unitstatus") { machine_.unitStatusReport(); return; }  /* :1121 */
  if (t == "posrep") { handlePosRep(p); return; }          /* :1122 */
  if (t == "otfactor") {                                   /* G7 :1123-1130 */
    sayNet("MUSTANG, the OT factor is YOUR arithmetic " EMD " it turns your "
           "mils into meters. I do not want it. Send me the RESULT: left or "
           "right, add or drop. What I do need on a grid mission is your OT "
           "DIRECTION, over.", 1.1);
    if (mission && !mission->done)
      mission->notes.push_back(
          "Transmitted the OT factor. It is observer-side arithmetic; only "
          "OT DIRECTION goes to the FDC (grid missions).");
    return;
  }
  if (t == "repeat") { handleRepeat(p); return; }          /* :1131-1166 */
  if (t == "eom") { handleEOM(p); return; }                /* :1167 */
  /* default (:1168-1177) */
  unknownStreak_++;
  if (unknownStreak_ >= 3) {
    unknownStreak_ = 0;
    sayNet(pick(quips_.rantLost, flavor_), 1.0);
  } else {
    sayNet(pick(quips_.unknown, flavor_), 1.0);
  }
  sysHint();
}

// port of src/js/12-cff-handling.js:1131-1166 (oracle-v1) — the REPEAT case.
void FdcEngine::handleRepeat(const ParsedMessage& p) {
  sim::Mission* mission = machine_.activeMission();
  /* NET6 — a shell word riding on REPEAT switches the nature first. */
  if (!p.shell.empty() && mission && !mission->done) {
    const sim::ShellKind kind =
        p.shell == "smoke" ? sim::ShellKind::Smoke
        : p.shell == "illum" ? sim::ShellKind::Illum : sim::ShellKind::He;
    if (kind != mission->shell) {
      mission->shell = kind;
      mission->intent =
          kind == sim::ShellKind::Illum ? sim::IntentKind::Illum
          : kind == sim::ShellKind::Smoke ? sim::IntentKind::Suppress
          : (mission->imm != sim::ImmKind::None ? sim::IntentKind::Suppress
                                                : sim::IntentKind::Destroy);
      sayNet("SHELL " + (p.shell == "illum" ? std::string("ILLUMINATION")
                                            : upperAscii(p.shell)) + ", OUT.",
             0.8);
    }
  }
  if (mission && !mission->done &&
      machine_.state() == sim::MissionPhase::Adjusting) {   /* :1144-1145 */
    sayNet("REPEAT, OUT.", 1.0);
    machine_.fireAdjustRound();
  } else if (mission && !mission->done &&
             machine_.state() == sim::MissionPhase::EomQuery) {
    /* G13/NET6 (:1146-1158). */
    if (mission->shell == sim::ShellKind::Illum) {
      sayNet("REPEAT, OUT.", 1.0);
      machine_.fireAdjustRound();
    } else if (machine_.missionAccomplished()) {
      sayNet("MUSTANG 12, the target is done " EMD " I am not re-attacking a "
             "dead position. Send end of mission, over.", 1.0);
    } else {
      sayNet("REPEAT, OUT.", 1.0);
      machine_.fireForEffect();
    }
  } else if (mission && !mission->done) {   /* NET27 (:1159-1164) */
    sayNet(pick(quips_.inFlight, flavor_), 1.0);
  } else {   /* NET4 (:1165) */
    noMissionReply();
    sysHint();
  }
}

// port of src/js/10-mission.js:260-280 (oracle-v1) — handlePosRep (G9),
// assigned to this layer by mission_machine.h. posRep is engine-owned
// (fdc_rules.h reconciliation note).
void FdcEngine::handlePosRep(const ParsedMessage& p) {
  double px, pz;
  if (p.digits.size() == 6) {
    const double e3 = parseIntStr(p.digits.substr(0, 3));
    const double n3 = parseIntStr(p.digits.substr(3));
    const WorldXZ w = enToWorld(e3 * 100 + 50, n3 * 100 + 50, map_);
    px = w.x; pz = w.z;
  } else if (p.digits.size() == 8) {
    const double e4 = parseIntStr(p.digits.substr(0, 4));
    const double n4 = parseIntStr(p.digits.substr(4));
    const WorldXZ w = enToWorld(e4 * 10 + 5, n4 * 10 + 5, map_);
    px = w.x; pz = w.z;
  } else {
    sayNet("MUSTANG 12, send your position as a 6 or 8 digit grid, over.",
           0.9);
    return;
  }
  posRep_ = PosRep{px, pz, p.digits};
  const double err = dist2(px, pz, op_.x, op_.z);
  sayNet("POSITION GRID " +
             (p.digits.size() == 6
                  ? p.digits.substr(0, 3) + " " + p.digits.substr(3)
                  : p.digits) + ", OUT.", 0.9);
  /* No hint when the report is wrong — the TLOG carries the truth (:276-279). */
  net_.posRepMeta(p.digits, js::round(err));
}

} }
