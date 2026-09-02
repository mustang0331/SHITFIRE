/* level3.replay — full mission replay of the 33 oracle transcripts through
   the SimCore facade (PARITY_TESTING.md §Level 3).

   Per transcript: rebuild MissionInit from the recorded metadata, feed every
   recorded observer transmission (the parse entry's raw msg) at its recorded
   sim time, advancing the clock in 16 ms frames (the capture's rAF grid —
   MANIFEST.json clock note), then assert:
     - parse classification per message identical;
     - impacts identical: order, msg, rounded distance-to-target (the
       recorded precision — dTgt is Math.round()ed at source,
       10-mission.js:1155), shell tag, sim time within ±0.5 s;
     - FDC doctrinal decision points identical in order (quip-pool identity
       for pool lines, full normalized text for deterministic lines) with
       sim times within ±0.5 s. The comparator NEVER diffs the six
       coin-gated flavor lines, quip selection, greet/career/spot-tail
       flavor, or caller-net lines (DETERMINISM_NOTE.md §a);
     - the final AAR payload exact (verdict, stars, adjusting rounds, dur,
       outcome, effPct, intent, sheaf/fuze + sources, shell, bdaClaim,
       firstMiss, aimErr0, tInit, corrEff, bracketed, trace, notes count).
       mission_31 carries TWO complete missions/AARs — both compared.

   Timing reconstruction: recorded TLOG times are Math.round(t*10)/10 of
   16 ms-grid frame times, so the mission start and each transmission time
   have up to six grid candidates per recorded 0.1 s value. The replay
   searches those candidates (first-candidate greedy, then a bounded
   backtracking bump of the latest send at or before the first mismatch) —
   scored positions are draw-order-deterministic, so the search only ever
   matters for moving-target distance rounding and dur/tInit rounding edges.

   Gated-flavor presence is reproduced from the recording itself: a
   ReplayFlavor overrides IFlavorRng::coin and answers each gate by looking
   for a recorded line of that gate's quip pool at the line's predicted
   delivery time (fdc_quips.h FlavorGate note). */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_amalgamated.hpp>
#include <nlohmann/json.hpp>

#include "fixtures.h"
#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/math/prng.h"
#include "shitfire/review/tlog.h"
#include "shitfire/sim/mission_clock.h"
#include "shitfire/sim_core.h"

namespace {

using nlohmann::json;
namespace sf = shitfire;

/* ---- recorded transcript ------------------------------------------------ */

struct RecEntry {
  double t = 0;
  std::string kind, call, msg;
  json raw;
};

struct Transcript {
  std::string name;
  std::string chapter;       /* "" = skirmish */
  std::string scenarioType;
  std::string difficulty;
  double seedMission = 1;
  double seedTerrain = 1337;
  std::vector<RecEntry> entries;
};

Transcript loadTranscript(const std::string& file) {
  const json j = fixtures::load("level3/" + file);
  Transcript tr;
  tr.name = file;
  const json& meta = j.at("meta");
  tr.chapter = meta.at("chapter").is_null()
                   ? ""
                   : meta.at("chapter").get<std::string>();
  tr.scenarioType = meta.at("scenarioType").get<std::string>();
  tr.difficulty = meta.at("difficulty").get<std::string>();
  tr.seedMission = meta.at("seedMission").get<double>();
  tr.seedTerrain = meta.at("seedTerrain").get<double>();
  for (const json& e : j.at("tlog")) {
    RecEntry r;
    r.t = e.at("t").get<double>();
    r.kind = e.at("kind").get<std::string>();
    r.call = e.value("call", std::string());
    r.msg = e.at("msg").is_string() ? e.at("msg").get<std::string>() : "";
    r.raw = e;
    tr.entries.push_back(std::move(r));
  }
  return tr;
}

/* ---- clock helpers ------------------------------------------------------ */

/* TLOG stamp: Math.round(sim.now * 10) / 10 (06-clock-comms.js:27). */
double jsRound1(double x) { return std::floor(x * 10 + 0.5) / 10; }

/* grid candidates: 16 ms-multiple frame times whose TLOG stamp equals t. */
std::vector<long long> gridCandidates(double tRec) {
  std::vector<long long> out;
  const long long center = static_cast<long long>(std::floor(tRec * 1000 + 0.5));
  for (long long m = ((center - 64) / 16) * 16; m <= center + 64; m += 16) {
    if (m < 0) continue;
    if (jsRound1(static_cast<double>(m) / 1000.0) == jsRound1(tRec))
      out.push_back(m);
  }
  return out;
}

/* first frame at or after a scheduled sim time (event delivery frame). */
long long deliveryFrameMs(double t) {
  const double raw = t * 1000.0;
  long long m = static_cast<long long>(std::ceil(raw / 16.0 - 1e-9)) * 16;
  if (static_cast<double>(m) < raw - 1e-9) m += 16;
  return m;
}

/* ---- callsign-swap normalization (06-clock-comms.js:218-225) ------------ */

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
  std::size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}
std::string normalizeCallsigns(std::string s) {
  replaceAll(s, "HACKSAW FIRES", "HELLHOUND FIRES");
  replaceAll(s, "SUNLAMP ACTUAL", "HELLHOUND FIRES");
  replaceAll(s, "HACKSAW", "HELLHOUND");
  replaceAll(s, "SUNLAMP", "HELLHOUND");
  return s;
}

/* ---- quip pool index ---------------------------------------------------- */

struct PoolIndex {
  /* pool name -> exact lines (swap-normalized). */
  std::map<std::string, std::vector<std::string>> exact;
  /* template pools: name -> (prefix, suffix) per line. */
  std::map<std::string, std::vector<std::pair<std::string, std::string>>> tmpl;
  std::set<std::string> excluded;   /* pool names never compared */
};

void addPool(PoolIndex& ix, const char* name,
             const std::vector<std::string>& lines) {
  auto& v = ix.exact[name];
  for (const std::string& l : lines) v.push_back(normalizeCallsigns(l));
}
void addTemplatePool(PoolIndex& ix, const char* name,
                     const std::vector<std::string>& lines,
                     const std::string& token) {
  auto& v = ix.tmpl[name];
  for (const std::string& l : lines) {
    const std::string n = normalizeCallsigns(l);
    const std::size_t p = n.find(token);
    if (p == std::string::npos) v.emplace_back(n, "");
    else v.emplace_back(n.substr(0, p), n.substr(p + token.size()));
  }
}

PoolIndex buildPoolIndex() {
  sf::doctrine::QuipSet q = sf::doctrine::makeQuips();
  PoolIndex ix;
  /* excluded: presence is coin-gated flavor, or pure selection color that
     always airs but whose CATEGORY carries no decision (career lines are
     unreachable in the corpus — fresh profile). */
  for (const char* n : {"ffeAck", "completeTail", "readbackTail",
                        "snideRound", "corrSnark", "careerFrat",
                        "careerCollat", "careerVet"})
    ix.excluded.insert(n);
  addPool(ix, "ffeAck", q.ffeAck.lines);
  addPool(ix, "completeTail", q.completeTail.lines);
  addPool(ix, "readbackTail", q.readbackTail.lines);
  addPool(ix, "snideRound", q.snideRound.lines);
  addPool(ix, "corrSnark", q.corrSnark.lines);
  addPool(ix, "careerFrat", q.careerFrat.lines);
  addPool(ix, "careerCollat", q.careerCollat.lines);
  addPool(ix, "careerVet", q.careerVet.lines);
  /* compared pools — presence deterministic, selection flavor. */
  addPool(ix, "greet", q.greet.lines);
  addPool(ix, "eomGood", q.eomGood.lines);
  addPool(ix, "eomBad", q.eomBad.lines);
  addPool(ix, "badGrid", q.badGrid.lines);
  addPool(ix, "water", q.water.lines);
  addPool(ix, "noMission", q.noMission.lines);
  addPool(ix, "inFlight", q.inFlight.lines);
  addPool(ix, "unknown", q.unknown.lines);
  addPool(ix, "complete", q.complete.lines);
  addPool(ix, "suppressedOnly", q.suppressedOnly.lines);
  addPool(ix, "noEffect", q.noEffect.lines);
  addPool(ix, "dangerClose", q.dangerClose.lines);
  addPool(ix, "wrongWay", q.wrongWay.lines);
  addPool(ix, "rantLost", q.rantLost.lines);
  addPool(ix, "coachBracket", q.coachBracket.lines);
  addPool(ix, "coachBracketGood", q.coachBracketGood.lines);
  addPool(ix, "coachStagnant", q.coachStagnant.lines);
  addPool(ix, "snide", q.snide.lines);
  addPool(ix, "nearCiv", q.nearCiv.lines);
  addPool(ix, "unsafeInsist", q.unsafeInsist.lines);
  addPool(ix, "spotTail", q.spotTail.lines);
  addPool(ix, "spotNone", q.spotNone.lines);
  addTemplatePool(ix, "coachLoc", q.coachLoc.lines, "{MISS}");
  addTemplatePool(ix, "coachTimid", q.coachTimid.lines, "{MISS}");
  addTemplatePool(ix, "unsafeCorr", q.unsafeCorr.lines, "{WHO}");
  {
    std::vector<std::string> frat, civ;
    for (const auto& r : q.rantFrat.rants)
      for (const std::string& l : r) frat.push_back(l);
    for (const auto& r : q.rantCiv.rants)
      for (const std::string& l : r) civ.push_back(l);
    addPool(ix, "rantFrat", frat);
    addPool(ix, "rantCiv", civ);
  }
  return ix;
}

const PoolIndex& poolIndex() {
  static const PoolIndex ix = buildPoolIndex();
  return ix;
}

/* Signature of one FDC line: "X:" excluded, "P:<pool>" pool identity,
   "T:<text>" deterministic template (exact normalized text). */
std::string fdcSignature(const std::string& rawMsg) {
  const PoolIndex& ix = poolIndex();
  const std::string msg = normalizeCallsigns(rawMsg);
  for (const auto& kv : ix.exact) {
    for (const std::string& l : kv.second) {
      if (l == msg)
        return ix.excluded.count(kv.first) ? "X:" : "P:" + kv.first;
    }
  }
  for (const auto& kv : ix.tmpl) {
    for (const auto& ps : kv.second) {
      if (msg.size() >= ps.first.size() + ps.second.size() &&
          msg.compare(0, ps.first.size(), ps.first) == 0 &&
          msg.compare(msg.size() - ps.second.size(), ps.second.size(),
                      ps.second) == 0)
        return ix.excluded.count(kv.first) ? "X:" : "P:" + kv.first;
    }
  }
  return "T:" + msg;
}

/* ---- the replay flavor RNG ---------------------------------------------- */

class ReplayFlavor : public sf::doctrine::IFlavorRng {
public:
  explicit ReplayFlavor(const Transcript& tr) : rng_(0x5F3759DFu) {
    /* recorded air times per gate pool. */
    const PoolIndex& ix = poolIndex();
    static const std::map<sf::doctrine::FlavorGate, const char*> gates = {
        {sf::doctrine::FlavorGate::FfeAck, "ffeAck"},
        {sf::doctrine::FlavorGate::CompleteTail, "completeTail"},
        {sf::doctrine::FlavorGate::ReadbackTail, "readbackTail"},
        {sf::doctrine::FlavorGate::SnideRound, "snideRound"},
        {sf::doctrine::FlavorGate::CorrSnark, "corrSnark"},
    };
    for (const RecEntry& e : tr.entries) {
      if (e.kind != "fdc") continue;
      const std::string msg = normalizeCallsigns(e.msg);
      for (const auto& g : gates) {
        const auto& lines = ix.exact.at(g.second);
        if (std::find(lines.begin(), lines.end(), msg) != lines.end()) {
          times_[g.first].push_back(e.t);
          break;
        }
      }
    }
  }

  double next() override { return rng_.next(); }

  bool coin(double /*p*/, sf::doctrine::FlavorGate gate,
            double predictedAirTime) override {
    if (gate == sf::doctrine::FlavorGate::CareerCallback) return false;
    auto it = times_.find(gate);
    if (it == times_.end()) return false;
    auto& consumed = consumed_[gate];
    const double tPred = jsRound1(
        static_cast<double>(deliveryFrameMs(predictedAirTime)) / 1000.0);
    for (std::size_t i = 0; i < it->second.size(); i++) {
      if (consumed.count(i)) continue;
      if (std::fabs(it->second[i] - tPred) <= 0.75) {
        consumed.insert(i);
        return true;
      }
      if (it->second[i] > tPred + 0.75) break;   /* sorted by record order */
    }
    return false;
  }

  void resetRun() {
    consumed_.clear();
    rng_ = sf::Mulberry32(0x5F3759DFu);
  }

private:
  sf::Mulberry32 rng_;
  std::map<sf::doctrine::FlavorGate, std::vector<double>> times_;
  std::map<sf::doctrine::FlavorGate, std::set<std::size_t>> consumed_;
};

/* ---- comparators -------------------------------------------------------- */

struct CompareResult {
  bool pass = true;
  std::vector<std::string> errors;
  double firstFailT = 1e300;
  double worstImpactDiff = 0;
  double worstSkew = 0;

  void fail(double t, const std::string& what) {
    pass = false;
    if (t < firstFailT) firstFailT = t;
    if (errors.size() < 10) errors.push_back(what);
  }
};

std::string trunc(const std::string& s, std::size_t n = 90) {
  return s.size() <= n ? s : s.substr(0, n) + "...";
}

/* JS null / absent-aware field compare on the AAR payload. */
void compareAarField(CompareResult& cr, double t, const std::string& name,
                     const json& rec, const json& got) {
  if (rec == got) return;
  if (rec.is_number() && got.is_number() &&
      rec.get<double>() == got.get<double>())
    return;
  cr.fail(t, "aar field '" + name + "': recorded " + rec.dump() + ", got " +
                 got.dump());
}

json aarEntryToJson(const sf::review::TlogEntry& e) {
  json j;
  j["msg"] = e.msg;
  j["rounds"] = e.rounds.has_value() ? json(*e.rounds) : json();
  j["dur"] = e.dur.has_value() ? json(*e.dur) : json();
  j["outcome"] = e.outcome.has_value() ? json(*e.outcome) : json();
  j["effPct"] = e.effPct.has_value() ? json(*e.effPct) : json();
  j["intent"] = e.intent.has_value() ? json(*e.intent) : json();
  if (e.sheaf) j["sheaf"] = *e.sheaf;
  if (e.sheafSrc) j["sheafSrc"] = *e.sheafSrc;
  if (e.fuze) j["fuze"] = *e.fuze;
  if (e.fuzeSrc) j["fuzeSrc"] = *e.fuzeSrc;
  if (e.shell) j["shell"] = *e.shell;
  if (e.bdaClaim) j["bdaClaim"] = *e.bdaClaim;
  j["firstMiss"] =
      e.firstMiss.present
          ? (e.firstMiss.value ? json(*e.firstMiss.value) : json(nullptr))
          : json();
  j["aimErr0"] =
      e.aimErr0.present
          ? (e.aimErr0.value ? json(*e.aimErr0.value) : json(nullptr))
          : json();
  j["tInit"] = e.tInit.has_value() ? json(*e.tInit) : json();
  j["corrEff"] =
      e.corrEff.present
          ? (e.corrEff.value ? json(*e.corrEff.value) : json(nullptr))
          : json();
  j["bracketed"] = e.bracketed.has_value() ? json(*e.bracketed) : json();
  if (e.trace) j["trace"] = *e.trace;
  j["notes"] = e.notes.has_value() ? json(*e.notes) : json();
  if (e.stars) j["stars"] = *e.stars;
  if (e.noDisp) j["noDisp"] = *e.noDisp;
  return j;
}

CompareResult compareRun(const Transcript& tr,
                         const std::vector<sf::review::TlogEntry>& got) {
  CompareResult cr;

  /* parse classification, in order (PARITY level 3 bullet 1). */
  {
    std::vector<const RecEntry*> rec;
    for (const RecEntry& e : tr.entries)
      if (e.kind == "parse") rec.push_back(&e);
    std::vector<const sf::review::TlogEntry*> mine;
    for (const auto& e : got)
      if (e.kind == "parse") mine.push_back(&e);
    if (rec.size() != mine.size())
      cr.fail(std::min(rec.size(), mine.size()) < rec.size()
                  ? rec[std::min(rec.size(), mine.size())]->t
                  : (rec.empty() ? 0 : rec.back()->t),
              "parse entry count: recorded " + std::to_string(rec.size()) +
                  ", got " + std::to_string(mine.size()));
    const std::size_t n = std::min(rec.size(), mine.size());
    for (std::size_t i = 0; i < n; i++) {
      const json& r = rec[i]->raw;
      const auto& m = *mine[i];
      const std::string rPt = r.value("ptype", std::string());
      const std::string rMe = r.at("method").is_null()
                                  ? ""
                                  : r.at("method").get<std::string>();
      const std::string rWa = r.at("warno").is_null()
                                  ? ""
                                  : r.at("warno").get<std::string>();
      const bool rFfe = r.value("ffe", false);
      const std::string mPt = m.ptype.value_or("");
      const std::string mMe = m.method.value ? *m.method.value : "";
      const std::string mWa = m.warno.value ? *m.warno.value : "";
      const bool mFfe = m.ffe.value_or(false);
      if (rPt != mPt || rMe != mMe || rWa != mWa || rFfe != mFfe)
        cr.fail(rec[i]->t,
                "parse #" + std::to_string(i) + " (\"" + trunc(m.msg, 60) +
                    "\"): recorded " + rPt + "/" + rMe + "/" + rWa +
                    (rFfe ? "/ffe" : "") + ", got " + mPt + "/" + mMe + "/" +
                    mWa + (mFfe ? "/ffe" : ""));
    }
  }

  /* impacts: order, msg, dTgt exact, shell, time ±0.5 (bullet 2). */
  {
    std::vector<const RecEntry*> rec;
    for (const RecEntry& e : tr.entries)
      if (e.kind == "impact") rec.push_back(&e);
    std::vector<const sf::review::TlogEntry*> mine;
    for (const auto& e : got)
      if (e.kind == "impact") mine.push_back(&e);
    if (rec.size() != mine.size())
      cr.fail(std::min(rec.size(), mine.size()) < rec.size()
                  ? rec[std::min(rec.size(), mine.size())]->t
                  : (rec.empty() ? 0 : rec.back()->t),
              "impact count: recorded " + std::to_string(rec.size()) +
                  ", got " + std::to_string(mine.size()));
    const std::size_t n = std::min(rec.size(), mine.size());
    for (std::size_t i = 0; i < n; i++) {
      const json& r = rec[i]->raw;
      const auto& m = *mine[i];
      if (rec[i]->msg != m.msg)
        cr.fail(rec[i]->t, "impact #" + std::to_string(i) +
                               " msg: recorded \"" + rec[i]->msg +
                               "\", got \"" + m.msg + "\"");
      const double rD = r.value("dTgt", -1.0);
      const double mD = m.dTgt.value_or(-1.0);
      const double dd = std::fabs(rD - mD);
      if (dd > cr.worstImpactDiff) cr.worstImpactDiff = dd;
      if (rD != mD)
        cr.fail(rec[i]->t, "impact #" + std::to_string(i) +
                               " dTgt: recorded " + std::to_string(rD) +
                               ", got " + std::to_string(mD));
      const std::string rSh = r.value("shell", std::string());
      const std::string mSh = m.shell.value_or("");
      if (rSh != mSh)
        cr.fail(rec[i]->t, "impact #" + std::to_string(i) + " shell tag");
      const double skew = std::fabs(rec[i]->t - m.t);
      if (skew > cr.worstSkew) cr.worstSkew = skew;
      if (skew > 0.5)
        cr.fail(rec[i]->t, "impact #" + std::to_string(i) +
                               " time skew " + std::to_string(skew) +
                               " s (recorded t " + std::to_string(rec[i]->t) +
                               ", got " + std::to_string(m.t) + ")");
    }
  }

  /* FDC doctrinal decision points (bullet 3) — signature sequence. */
  {
    struct Line { double t; std::string sig; std::string msg; };
    std::vector<Line> rec, mine;
    for (const RecEntry& e : tr.entries) {
      if (e.kind != "fdc") continue;
      std::string s = fdcSignature(e.msg);
      if (s != "X:") rec.push_back({e.t, std::move(s), e.msg});
    }
    for (const auto& e : got) {
      if (e.kind != "fdc") continue;
      std::string s = fdcSignature(e.msg);
      if (s != "X:") mine.push_back({e.t, std::move(s), e.msg});
    }
    const std::size_t n = std::min(rec.size(), mine.size());
    for (std::size_t i = 0; i < n; i++) {
      if (rec[i].sig != mine[i].sig) {
        cr.fail(rec[i].t,
                "fdc #" + std::to_string(i) + " at t=" +
                    std::to_string(rec[i].t) + ": recorded [" +
                    trunc(rec[i].sig) + "], got [" + trunc(mine[i].sig) +
                    "] (recorded line \"" + trunc(rec[i].msg, 60) + "\")");
        break;   /* everything after a sequence break is noise */
      }
      const double skew = std::fabs(rec[i].t - mine[i].t);
      if (skew > cr.worstSkew) cr.worstSkew = skew;
      if (skew > 0.5) {
        cr.fail(rec[i].t, "fdc #" + std::to_string(i) + " [" +
                              trunc(rec[i].sig, 40) + "] time skew " +
                              std::to_string(skew) + " s");
        break;
      }
    }
    if (rec.size() != mine.size())
      cr.fail(rec.size() > n ? rec[n].t : (rec.empty() ? 0 : rec.back().t),
              "fdc doctrinal line count: recorded " +
                  std::to_string(rec.size()) + ", got " +
                  std::to_string(mine.size()) +
                  (rec.size() > n
                       ? " (first missing: \"" + trunc(rec[n].msg, 60) + "\")"
                       : mine.size() > n
                             ? " (first extra: \"" + trunc(mine[n].msg, 60) +
                                   "\")"
                             : ""));
  }

  /* the AAR(s) — exact payload (bullet 4). */
  {
    std::vector<const RecEntry*> rec;
    for (const RecEntry& e : tr.entries)
      if (e.kind == "aar") rec.push_back(&e);
    std::vector<const sf::review::TlogEntry*> mine;
    for (const auto& e : got)
      if (e.kind == "aar") mine.push_back(&e);
    if (rec.size() != mine.size())
      cr.fail(rec.empty() ? 1e299 : rec[0]->t,
              "aar count: recorded " + std::to_string(rec.size()) + ", got " +
                  std::to_string(mine.size()));
    const std::size_t n = std::min(rec.size(), mine.size());
    for (std::size_t i = 0; i < n; i++) {
      const json& r = rec[i]->raw;
      const json g = aarEntryToJson(*mine[i]);
      compareAarField(cr, rec[i]->t, "verdict", r.value("msg", json()),
                      g.value("msg", json()));
      for (const char* f :
           {"rounds", "dur", "outcome", "effPct", "intent", "sheaf",
            "sheafSrc", "fuze", "fuzeSrc", "shell", "bdaClaim", "firstMiss",
            "aimErr0", "tInit", "corrEff", "bracketed", "trace", "notes",
            "stars", "noDisp"}) {
        const json rv = r.contains(f) ? r.at(f) : json();
        const json gv = g.contains(f) ? g.at(f) : json();
        compareAarField(cr, rec[i]->t, f, rv, gv);
      }
      const double skew = std::fabs(rec[i]->t - mine[i]->t);
      if (skew > cr.worstSkew) cr.worstSkew = skew;
      if (skew > 0.5)
        cr.fail(rec[i]->t,
                "aar time skew " + std::to_string(skew) + " s");
    }
  }
  return cr;
}

/* ---- one replay run ----------------------------------------------------- */

struct Sends {
  std::vector<double> recT;          /* recorded parse t per send */
  std::vector<std::string> raw;      /* raw msg per send */
};

CompareResult runReplay(const Transcript& tr, long long tStartMs,
                        const std::vector<long long>& sendMs,
                        ReplayFlavor& flavor) {
  flavor.resetRun();
  sf::SimCore core;
  core.setFlavorRng(&flavor);
  sf::MissionInit mi;
  mi.chapterId = tr.chapter;
  mi.skirmishType = tr.scenarioType;
  mi.difficulty = tr.difficulty;
  mi.missionSeed = tr.seedMission;
  mi.terrainSeed = tr.seedTerrain;
  mi.startMs = tStartMs;
  core.init(mi);
  std::size_t k = 0;
  std::size_t sendIdx = 0;
  for (const RecEntry& e : tr.entries) {
    if (e.kind != "parse") continue;
    core.advanceTo(sendMs[sendIdx]);
    core.submitMessage(e.msg);
    sendIdx++;
    (void)k;
  }
  /* let all trailing traffic land (the capture waited for FDC.lastT + 3.5
     then 2 s more — tools/oracle-capture.js:989-996). */
  const double lastRec = tr.entries.empty() ? 0 : tr.entries.back().t;
  core.advanceTo(deliveryFrameMs(lastRec + 12.0));
  for (int guard = 0; guard < 40000; guard++) {
    const auto parts = core.parts();
    (void)parts;
    if (core.now() > lastRec + 240.0) break;
    bool busy = false;
    /* drain while the say chain or scheduler still has work. */
    /* (EventQueue::size is available; speaker lastT via parts.) */
    if (core.parts().queue->size() > 0) busy = true;
    if (!busy) break;
    core.advance();
  }
  return compareRun(tr, core.tlog().entries());
}

/* ---- candidate search --------------------------------------------------- */

struct ReplayOutcome {
  CompareResult best;
  int attempts = 0;
};

ReplayOutcome replayWithSearch(const Transcript& tr) {
  ReplayFlavor flavor(tr);
  /* send list from parse entries. */
  Sends sends;
  for (const RecEntry& e : tr.entries)
    if (e.kind == "parse") sends.recT.push_back(e.t);
  const std::vector<long long> startCands =
      gridCandidates(tr.entries.empty() ? 30.1 : tr.entries[0].t);
  std::vector<std::vector<long long>> sendCands;
  for (double t : sends.recT) sendCands.push_back(gridCandidates(t));

  ReplayOutcome out;
  out.best.pass = false;
  out.best.errors = {"no attempt ran"};
  std::vector<std::size_t> pick(sendCands.size(), 0);

  for (std::size_t si = 0; si < startCands.size(); si++) {
    std::fill(pick.begin(), pick.end(), 0);
    for (int attempt = 0; attempt < 64; attempt++) {
      /* materialize send times: candidate, forced strictly increasing and
         after the start frame. */
      std::vector<long long> sendMs(sendCands.size());
      long long prev = startCands[si];
      bool feasible = true;
      for (std::size_t i = 0; i < sendCands.size(); i++) {
        long long chosen = -1;
        for (std::size_t c = pick[i]; c < sendCands[i].size(); c++) {
          if (sendCands[i][c] > prev) { chosen = sendCands[i][c]; pick[i] = c; break; }
        }
        if (chosen < 0) { feasible = false; break; }
        sendMs[i] = chosen;
        prev = chosen;
      }
      if (!feasible) break;
      const CompareResult cr = runReplay(tr, startCands[si], sendMs, flavor);
      out.attempts++;
      if (cr.pass) { out.best = cr; return out; }
      if (!out.best.pass &&
          (out.best.errors.size() == 1 || cr.errors.size() < out.best.errors.size() ||
           cr.firstFailT > out.best.firstFailT))
        out.best = cr;
      /* bump the latest send at or before the first failure. */
      long long failMs = deliveryFrameMs(cr.firstFailT);
      int bump = -1;
      for (int i = static_cast<int>(sendMs.size()) - 1; i >= 0; i--) {
        if (sendMs[i] <= failMs && pick[i] + 1 < sendCands[i].size()) {
          bump = i;
          break;
        }
      }
      if (bump < 0) break;   /* nothing to vary before the failure — next start */
      pick[static_cast<std::size_t>(bump)]++;
      for (std::size_t i = static_cast<std::size_t>(bump) + 1;
           i < pick.size(); i++)
        pick[i] = 0;
    }
  }
  return out;
}

const char* kTranscripts[] = {
    "mission_01_f1-tutorial-eyes.json",
    "mission_02_f2-tutorial-map.json",
    "mission_03_f3-first-call-for-fire.json",
    "mission_04_ch1-1-troops-five-star.json",
    "mission_05_ch1-2-bunker-bracket.json",
    "mission_06_ch1-3-strongpoint.json",
    "mission_07_ch1-4-format-on-the-clock.json",
    "mission_08_ch2-1-polar-dead-laser.json",
    "mission_09_ch2-2-shift-known-point-night.json",
    "mission_10_ch2-3-defilade-masked.json",
    "mission_11_ch2-4-perimeter-easy.json",
    "mission_12_ch2-5-coordinated-illumination.json",
    "mission_13_ch3-1-convoy-pit-stop.json",
    "mission_14_ch3-2-danger-close-creep.json",
    "mission_15_ch3-3-village-raid.json",
    "mission_16_ch3-4-combined-arms.json",
    "mission_17_ch3-5-interleaved-friendlies.json",
    "mission_18_ch3-6-immediate-suppression.json",
    "mission_19_ch4-1-strict-net.json",
    "mission_20_ch4-2-60mm-precision.json",
    "mission_21_ch4-3-no-second-chances.json",
    "mission_22_ch4-4-meat-grinder.json",
    "mission_23_ch4-5-quick-fire-plan.json",
    "mission_24_e1-great-chow-raid.json",
    "mission_25_e2-kaiju-moving-target.json",
    "mission_26_e3-sunlamp-actual.json",
    "mission_27_sk-counterbattery.json",
    "mission_28_sk-defend-the-op.json",
    "mission_29_sk-call-for-support.json",
    "mission_30_sk-occupied-settlement.json",
    "mission_31_sk-two-targets.json",
    "mission_32_fail-fratricide.json",
    "mission_33_fail-collateral.json",
};

}  // namespace

TEST_CASE("level3 full mission replay", "[replay]") {
  struct Row {
    std::string name;
    bool pass;
    double worstImpact, worstSkew;
    int attempts;
    std::vector<std::string> errors;
  };
  std::vector<Row> rows;
  int failures = 0;
  for (const char* file : kTranscripts) {
    const Transcript tr = loadTranscript(file);
    const ReplayOutcome oc = replayWithSearch(tr);
    rows.push_back({file, oc.best.pass, oc.best.worstImpactDiff,
                    oc.best.worstSkew, oc.attempts, oc.best.errors});
    if (!oc.best.pass) failures++;
  }
  std::printf("\n%-46s %-6s %12s %10s %9s\n", "transcript", "result",
              "worst dTgt", "worst dt", "attempts");
  for (const Row& r : rows) {
    std::printf("%-46s %-6s %12.3f %10.3f %9d\n", r.name.c_str(),
                r.pass ? "PASS" : "FAIL", r.worstImpact, r.worstSkew,
                r.attempts);
    if (!r.pass)
      for (const std::string& e : r.errors)
        std::printf("    - %s\n", e.c_str());
  }
  std::fflush(stdout);
  REQUIRE(failures == 0);
}
