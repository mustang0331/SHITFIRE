#pragma once
/* Shared support for the level-2 scenario parity tests: island cache,
   fixture-shaped JSON serialization of ScenarioState/WorldLayout, and a
   tolerance-aware deep comparator that mirrors the oracle capture's
   serialize() key set (tools/oracle-capture.js:134).

   Test-only code — exceptions, I/O and nlohmann are fine here, never in core. */
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "shitfire/config/sim_config.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_config.h"
#include "shitfire/scenario/world_layout.h"
#include "shitfire/terrain/heightfield.h"

namespace scntest {

using nlohmann::json;
namespace sc = shitfire::scenario;

/* ---- island cache --------------------------------------------------------- */

struct Island {
  shitfire::TerrainConfig tc;
  std::unique_ptr<shitfire::HeightField> hf;
  sc::OpPos op;
  sc::BatteryPos battery;
  sc::EyePos eye;
  sc::WorldLayout world;
};

inline const Island& island(int terrainSeed, bool blackPalette) {
  static std::map<int, std::unique_ptr<Island>> cache;
  auto it = cache.find(terrainSeed);
  if (it != cache.end()) return *it->second;
  auto isl = std::make_unique<Island>();
  isl->tc = shitfire::TerrainConfig{};
  isl->hf = std::make_unique<shitfire::HeightField>(
      isl->tc, static_cast<double>(terrainSeed));
  isl->op = sc::findOP(*isl->hf);
  isl->battery = sc::findBattery(*isl->hf, isl->op);
  isl->eye = sc::eyeFor(isl->op, sc::CameraConstants{});
  isl->world = sc::buildWorldFeatures(
      *isl->hf, isl->op, isl->battery, sc::WorldConfig{}, sc::GfxVegConfig{},
      static_cast<double>(terrainSeed), nullptr, blackPalette);
  return *cache.emplace(terrainSeed, std::move(isl)).first->second;
}

inline sc::GenContext makeCtx(const Island& isl, const std::string& difficulty,
                              bool chapterActive, const sc::ScnOpts& opts,
                              double now) {
  sc::GenContext ctx;
  ctx.hf = isl.hf.get();
  ctx.world = &isl.world;
  ctx.op = isl.op;
  ctx.eye = isl.eye;
  ctx.islandRadius = isl.tc.islandRadius;
  ctx.terrainSeed = 0;   /* set by caller (equals the island's seed) */
  ctx.difficulty = difficulty;
  ctx.chapterActive = chapterActive;
  ctx.opts = opts;
  ctx.now = now;
  return ctx;
}

/* ---- fixture-shaped serialization ---------------------------------------- */

inline json xz(const shitfire::WorldXZ& p) { return json{ { "x", p.x }, { "z", p.z } }; }

inline json villageJson(const sc::Village& v) {
  if (v.bare) return json{ { "x", v.x }, { "z", v.z } };
  json huts = json::array();
  for (const auto& h : v.huts) {
    json hj{ { "x", h.x }, { "z", h.z } };
    if (h.w) hj["w"] = *h.w;
    if (h.d) hj["d"] = *h.d;
    huts.push_back(hj);
  }
  json j{ { "name", v.name }, { "x", v.x }, { "z", v.z }, { "r", v.r },
          { "huts", huts } };
  if (v.isTown) j["tier"] = "town";
  if (v.foot)
    j["foot"] = json{ { "ux", v.foot->ux },       { "uz", v.foot->uz },
                      { "aHalf", v.foot->aHalf }, { "bHalf", v.foot->bHalf },
                      { "cx", v.foot->cx },       { "cz", v.foot->cz } };
  return j;
}

inline json pathJson(const sc::PathState& p) {
  if (p.isLine)
    return json{ { "sx", p.sx }, { "sz", p.sz }, { "dx", p.dx },
                 { "dz", p.dz }, { "len", p.len } };
  json pts = json::array(), cum = json::array();
  for (const auto& q : p.pts) pts.push_back(xz(q));
  for (const double c : p.cum) cum.push_back(c);
  return json{ { "pts", pts }, { "cum", cum }, { "len", p.len } };
}

/* Mirrors what serialize(S) keeps: functions (rng) dropped, null kept. */
inline json scnToJson(const sc::ScenarioState& S) {
  json j;
  j["seed"] = S.seed;
  j["type"] = S.type;
  j["difficulty"] = S.difficulty;
  json kps = json::array();
  for (const auto& k : S.kps)
    kps.push_back(json{ { "id", k.id }, { "name", k.name },
                        { "x", k.x }, { "z", k.z } });
  j["kps"] = kps;
  json fr = json::array();
  for (const auto& f : S.friendlies)
    fr.push_back(json{ { "x", f.x }, { "z", f.z }, { "r", f.r } });
  j["friendlies"] = fr;
  j["tgtClass"] = S.tgtClass;
  j["posture"] = S.posture;
  j["effScale"] = S.effScale;
  j["eff"] = S.eff;
  j["everSuppressed"] = S.everSuppressed;
  j["suppressedUntil"] = S.suppressedUntil;
  j["alerted"] = S.alerted;
  j["dispersed"] = S.dispersed;
  j["enemy"] = xz(S.enemy);
  j["brief"] = S.brief;
  if (S.compound) j["compound"] = xz(*S.compound);
  if (S.fireBase) j["fireBase"] = xz(*S.fireBase);
  if (S.team) j["team"] = xz(*S.team);
  if (S.bbq) j["bbq"] = xz(*S.bbq);
  if (S.fStart) j["fStart"] = xz(*S.fStart);
  if (S.hasVillage) j["village"] = S.village ? villageJson(*S.village) : json(nullptr);
  if (S.path) j["path"] = pathJson(*S.path);
  if (S.speed) j["speed"] = *S.speed;
  if (S.t0) j["t0"] = *S.t0;
  if (S.escaped.has_value()) j["escaped"] = *S.escaped;
  if (S.veh) {
    json veh = json::array();
    for (const auto& v : *S.veh)
      veh.push_back(json{ { "off", v.off }, { "dead", v.dead },
                          { "d", v.d }, { "x", v.x }, { "z", v.z } });
    j["veh"] = veh;
  }
  if (S.stop)
    j["stop"] = json{ { "d", S.stop->d },
                      { "dur", S.stop->dur },
                      { "tArr", S.stop->tArr ? json(*S.stop->tArr) : json(nullptr) },
                      { "resumed", S.stop->resumed },
                      { "name", S.stop->name } };
  if (S.bty)
    j["bty"] = json{ { "next", S.bty->next }, { "period", S.bty->period },
                     { "tof", S.bty->tof } };
  if (S.btyCas.has_value()) j["btyCas"] = *S.btyCas;
  if (S.aSpeed) j["aSpeed"] = *S.aSpeed;
  if (S.hasLastT) j["lastT"] = S.lastT ? json(*S.lastT) : json(nullptr);
  if (S.overrun.has_value()) j["overrun"] = *S.overrun;
  if (S.twinTgt)
    j["twinTgt"] = json{ { "x", S.twinTgt->x },
                         { "z", S.twinTgt->z },
                         { "eff", S.twinTgt->eff },
                         { "everSuppressed", S.twinTgt->everSuppressed },
                         { "suppressedUntil", S.twinTgt->suppressedUntil },
                         { "alive", S.twinTgt->alive } };
  if (S.hasAvenues) {
    json av = json::array();
    for (const auto& a : S.avenues) av.push_back(xz(a));
    j["avenues"] = av;
  }
  if (S.assaultAv.has_value()) j["assaultAv"] = *S.assaultAv;
  if (S.axisAz.has_value()) j["axisAz"] = *S.axisAz;
  if (S.hHour.has_value()) j["hHour"] = *S.hHour;
  if (S.hStarted.has_value()) j["hStarted"] = *S.hStarted;
  if (S.caller)
    j["caller"] = json{ { "i", S.caller->i },
                        { "wrong", S.caller->wrong },
                        { "repAz", S.caller->repAz },
                        { "repDist", S.caller->repDist },
                        { "saidThanks", S.caller->saidThanks },
                        { "saidDead", S.caller->saidDead } };
  if (S.occupied) {
    json oc{ { "kind", S.occupied->kind }, { "name", S.occupied->name },
             { "x", S.occupied->x }, { "z", S.occupied->z } };
    if (S.occupied->hasRing) {
      json ring = json::array();
      for (const auto& r : S.occupied->ring) ring.push_back(xz(r));
      oc["ring"] = ring;
    }
    j["occupied"] = oc;
  }
  if (S.ashore.has_value()) j["ashore"] = *S.ashore;
  if (S.armor.has_value()) j["armor"] = *S.armor;
  if (S.fAdvAz.has_value()) j["fAdvAz"] = *S.fAdvAz;
  if (S.fSpeed.has_value()) j["fSpeed"] = *S.fSpeed;
  if (S.fHold.has_value()) j["fHold"] = *S.fHold;
  if (S.fAdvMax.has_value()) j["fAdvMax"] = *S.fAdvMax;
  if (S.ft0.has_value()) j["ft0"] = *S.ft0;
  return j;
}

/* WORLD subset the chapter snapshots keep (oracle-capture.js:772). Civ x/z
   are time-driven; the chapters test overwrites them via updateCivilians with
   the recovered capture clock before comparing. */
inline json worldToJson(const sc::WorldLayout& w) {
  json j;
  json roads = json::array();
  for (const auto& r : w.roads) {
    json line = json::array();
    for (const auto& p : r) line.push_back(xz(p));
    roads.push_back(line);
  }
  j["roads"] = roads;
  json paths = json::array();
  for (const auto& p : w.paths) {
    json line = json::array();
    for (const auto& q : p) line.push_back(xz(q));
    paths.push_back(line);
  }
  j["paths"] = paths;
  json fac = json::array();
  for (const auto& f : w.facilities) {
    json fj{ { "kind", f.kind }, { "name", f.name }, { "x", f.x }, { "z", f.z } };
    if (f.th) fj["th"] = *f.th;
    if (f.len) fj["len"] = *f.len;
    fac.push_back(fj);
  }
  j["facilities"] = fac;
  json vil = json::array();
  for (const auto& v : w.villages) vil.push_back(villageJson(v));
  j["villages"] = vil;
  json civs = json::array();
  for (const auto& c : w.civs)
    civs.push_back(json{ { "bx", c.bx }, { "bz", c.bz }, { "wr", c.wr },
                         { "phase", c.phase }, { "x", c.x }, { "z", c.z },
                         { "hidden", c.hidden } });
  j["civs"] = civs;
  json rocks = json::array();
  for (const auto& r : w.rocks)
    rocks.push_back(json{ { "x", r.x }, { "z", r.z }, { "s", r.s },
                          { "h", r.h }, { "r", r.r },
                          { "name", r.name ? json(*r.name) : json(nullptr) } });
  j["rocks"] = rocks;
  return j;
}

/* ---- tolerance-aware deep compare ----------------------------------------- */

struct CmpState {
  double tol = 0.01;          /* the positional gate; names/counts stay exact */
  double worst = 0;           /* worst numeric abs diff seen (pass or fail) */
  std::string worstPath;
  std::vector<std::string> errors;
  void err(const std::string& path, const std::string& what) {
    if (errors.size() < 12) errors.push_back(path + ": " + what);
    else if (errors.size() == 12) errors.push_back("(more suppressed)");
  }
};

inline void deepCompare(const json& exp, const json& got,
                        const std::string& path, CmpState& st) {
  if (exp.is_number() && got.is_number()) {
    const double d = std::fabs(exp.get<double>() - got.get<double>());
    if (d > st.worst) { st.worst = d; st.worstPath = path; }
    if (d > st.tol) {
      std::ostringstream os;
      os.precision(17);
      os << "number diff " << d << " (exp " << exp.get<double>() << ", got "
         << got.get<double>() << ")";
      st.err(path, os.str());
    }
    return;
  }
  if (exp.type() != got.type()) {
    st.err(path, "type mismatch (exp " + std::string(exp.type_name()) +
                     ", got " + std::string(got.type_name()) + ")");
    return;
  }
  if (exp.is_object()) {
    for (auto it = exp.begin(); it != exp.end(); ++it)
      if (!got.contains(it.key())) st.err(path + "." + it.key(), "missing in port output");
    for (auto it = got.begin(); it != got.end(); ++it)
      if (!exp.contains(it.key())) st.err(path + "." + it.key(), "extra in port output");
    for (auto it = exp.begin(); it != exp.end(); ++it)
      if (got.contains(it.key()))
        deepCompare(it.value(), got[it.key()], path + "." + it.key(), st);
    return;
  }
  if (exp.is_array()) {
    if (exp.size() != got.size()) {
      st.err(path, "array size exp " + std::to_string(exp.size()) + " got " +
                       std::to_string(got.size()));
      return;
    }
    for (std::size_t i = 0; i < exp.size(); i++)
      deepCompare(exp[i], got[i], path + "[" + std::to_string(i) + "]", st);
    return;
  }
  if (exp != got) {
    st.err(path, "value mismatch (exp " + exp.dump() + ", got " +
                     got.dump().substr(0, 200) + ")");
  }
}

/* Recover the capture-time clock from the wandering civilians:
   x = bx + sin(t*0.14 + phase) * wr (04-world.js:1141). sim.now is quantized
   to whole milliseconds in multiples of 16 (rAF on the fake clock), so
   candidates are snapped to ms/1000 before verification. Returns NaN if no
   consistent t exists — a real parity failure. */
inline double recoverCivClock(const std::vector<sc::Civ>& mine,
                              const json& expCivs, double tMax = 100000) {
  if (mine.empty() || expCivs.empty()) return 0;
  const auto& c0 = mine[0];
  const double arg =
      std::max(-1.0, std::min(1.0, (expCivs[0].at("x").get<double>() - c0.bx) / c0.wr));
  const double A = std::asin(arg);
  const double TWO_PI = 6.283185307179586;
  const auto verify = [&](double t) {
    for (std::size_t i = 0; i < mine.size(); i++) {
      const double x = mine[i].bx + std::sin(t * 0.14 + mine[i].phase) * mine[i].wr;
      const double z = mine[i].bz + std::cos(t * 0.11 + mine[i].phase * 1.7) * mine[i].wr;
      if (std::fabs(x - expCivs[i].at("x").get<double>()) > 1e-6) return false;
      if (std::fabs(z - expCivs[i].at("z").get<double>()) > 1e-6) return false;
    }
    return true;
  };
  for (const double fam : { A, 3.141592653589793 - A }) {
    for (int n = -1;; n++) {
      const double t = (fam - c0.phase + TWO_PI * n) / 0.14;
      if (t > tMax) break;
      if (t < 0) continue;
      const long long ms = std::llround(t * 1000 / 16) * 16;
      for (const long long m : { ms - 16, ms, ms + 16 }) {
        if (m < 0) continue;
        const double tq = static_cast<double>(m) / 1000.0;
        if (verify(tq)) return tq;
      }
    }
  }
  return std::nan("");
}

}  // namespace scntest
