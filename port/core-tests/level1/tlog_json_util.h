#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "shitfire/review/tlog.h"

/* Strict TlogEntry <-> JSON mapping for the level-3 transcript fixtures
   (schema: shitfire/review/tlog.h, recorded by 06-clock-comms.js TLOG.add).
   Test-only code shared by tlog_roundtrip_test.cpp and grading_aar_test.cpp.

   fromJson is STRICT: any key it does not recognize is reported — that is
   the "no field lost or renamed" half of the [tlog] gate in the decode
   direction; toJson re-emits every engaged field for the encode direction.

   `wall` (browser Date.now) is accepted on decode and re-emitted only on
   request: it is documented non-parity metadata (tlog.h) and no fixture
   carries it. */
namespace tlogjson {

using shitfire::review::JsProp;
using shitfire::review::TlogEntry;

struct ParseResult {
  TlogEntry entry;
  std::vector<std::string> errors;
};

inline ParseResult fromJson(const nlohmann::json& j) {
  ParseResult r;
  TlogEntry& e = r.entry;
  auto bad = [&r](const std::string& k, const char* want) {
    r.errors.push_back("field '" + k + "': expected " + want);
  };
  auto str = [&](const nlohmann::json& v, const std::string& k,
                 std::string& out) {
    if (v.is_string()) out = v.get<std::string>();
    else bad(k, "string");
  };
  auto num = [&](const nlohmann::json& v, const std::string& k, double& out) {
    if (v.is_number()) out = v.get<double>();
    else bad(k, "number");
  };
  auto optStr = [&](const nlohmann::json& v, const std::string& k,
                    std::optional<std::string>& out) {
    if (v.is_string()) out = v.get<std::string>();
    else bad(k, "string");
  };
  auto optNum = [&](const nlohmann::json& v, const std::string& k,
                    std::optional<double>& out) {
    if (v.is_number()) out = v.get<double>();
    else bad(k, "number");
  };
  auto optBool = [&](const nlohmann::json& v, const std::string& k,
                     std::optional<bool>& out) {
    if (v.is_boolean()) out = v.get<bool>();
    else bad(k, "boolean");
  };
  auto propStr = [&](const nlohmann::json& v, const std::string& k,
                     JsProp<std::string>& out) {
    out.present = true;
    if (v.is_null()) return;
    if (v.is_string()) out.value = v.get<std::string>();
    else bad(k, "string|null");
  };
  auto propNum = [&](const nlohmann::json& v, const std::string& k,
                     JsProp<double>& out) {
    out.present = true;
    if (v.is_null()) return;
    if (v.is_number()) out.value = v.get<double>();
    else bad(k, "number|null");
  };

  if (!j.is_object()) {
    r.errors.push_back("entry is not an object");
    return r;
  }
  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string& k = it.key();
    const nlohmann::json& v = it.value();
    /* base (06-clock-comms.js:26-31) */
    if (k == "t") num(v, k, e.t);
    else if (k == "wall") optNum(v, k, e.wall);
    else if (k == "state") str(v, k, e.state);
    else if (k == "seed") num(v, k, e.seed);
    else if (k == "ch") {
      if (v.is_null()) e.ch = std::nullopt;
      else if (v.is_string()) e.ch = v.get<std::string>();
      else bad(k, "string|null");
    }
    else if (k == "kind") str(v, k, e.kind);
    else if (k == "call") str(v, k, e.call);
    else if (k == "msg") str(v, k, e.msg);
    /* parse extras (12-cff-handling.js:907-908) */
    else if (k == "ptype") optStr(v, k, e.ptype);
    else if (k == "method") propStr(v, k, e.method);
    else if (k == "warno") propStr(v, k, e.warno);
    else if (k == "ffe") optBool(v, k, e.ffe);
    /* impact extras (10-mission.js:1130-1132, :1154-1156) */
    else if (k == "dTgt") optNum(v, k, e.dTgt);
    else if (k == "shell") optStr(v, k, e.shell);
    /* spot extras (07-voice-spot-audio.js:456, :476) */
    else if (k == "form") optStr(v, k, e.form);
    else if (k == "scn") optStr(v, k, e.scn);
    else if (k == "diff") optStr(v, k, e.diff);
    /* sys / syserr extras */
    else if (k == "selfLocErr") optNum(v, k, e.selfLocErr);
    else if (k == "noDisp") optBool(v, k, e.noDisp);
    else if (k == "err") optStr(v, k, e.err);
    else if (k == "len") optNum(v, k, e.len);
    /* aar extras (13-aar.js:364-379) */
    else if (k == "rounds") optNum(v, k, e.rounds);
    else if (k == "dur") optNum(v, k, e.dur);
    else if (k == "outcome") optStr(v, k, e.outcome);
    else if (k == "effPct") optNum(v, k, e.effPct);
    else if (k == "intent") optStr(v, k, e.intent);
    else if (k == "sheaf") optStr(v, k, e.sheaf);
    else if (k == "sheafSrc") optStr(v, k, e.sheafSrc);
    else if (k == "fuze") optStr(v, k, e.fuze);
    else if (k == "fuzeSrc") optStr(v, k, e.fuzeSrc);
    else if (k == "bdaClaim") optStr(v, k, e.bdaClaim);
    else if (k == "firstMiss") propNum(v, k, e.firstMiss);
    else if (k == "aimErr0") propNum(v, k, e.aimErr0);
    else if (k == "tInit") optNum(v, k, e.tInit);
    else if (k == "corrEff") propNum(v, k, e.corrEff);
    else if (k == "bracketed") optBool(v, k, e.bracketed);
    else if (k == "trace") {
      if (v.is_array()) {
        std::vector<double> tr;
        for (const auto& x : v) {
          if (x.is_number()) tr.push_back(x.get<double>());
          else bad(k, "number[]");
        }
        e.trace = tr;
      } else bad(k, "array");
    }
    else if (k == "notes") optNum(v, k, e.notes);
    else if (k == "stars") optNum(v, k, e.stars);
    else r.errors.push_back("unknown field '" + k + "' — schema drift");
  }
  return r;
}

inline nlohmann::json toJson(const TlogEntry& e, bool includeWall = false) {
  nlohmann::json j;
  j["t"] = e.t;
  if (includeWall && e.wall) j["wall"] = *e.wall;
  j["state"] = e.state;
  j["seed"] = e.seed;
  j["ch"] = e.ch ? nlohmann::json(*e.ch) : nlohmann::json(nullptr);
  j["kind"] = e.kind;
  j["call"] = e.call;
  j["msg"] = e.msg;
  auto putProp = [&j](const char* k, const auto& p) {
    if (!p.present) return;
    if (p.value) j[k] = *p.value;
    else j[k] = nullptr;
  };
  if (e.ptype) j["ptype"] = *e.ptype;
  putProp("method", e.method);
  putProp("warno", e.warno);
  if (e.ffe) j["ffe"] = *e.ffe;
  if (e.dTgt) j["dTgt"] = *e.dTgt;
  if (e.shell) j["shell"] = *e.shell;
  if (e.form) j["form"] = *e.form;
  if (e.scn) j["scn"] = *e.scn;
  if (e.diff) j["diff"] = *e.diff;
  if (e.selfLocErr) j["selfLocErr"] = *e.selfLocErr;
  if (e.noDisp) j["noDisp"] = *e.noDisp;
  if (e.err) j["err"] = *e.err;
  if (e.len) j["len"] = *e.len;
  if (e.rounds) j["rounds"] = *e.rounds;
  if (e.dur) j["dur"] = *e.dur;
  if (e.outcome) j["outcome"] = *e.outcome;
  if (e.effPct) j["effPct"] = *e.effPct;
  if (e.intent) j["intent"] = *e.intent;
  if (e.sheaf) j["sheaf"] = *e.sheaf;
  if (e.sheafSrc) j["sheafSrc"] = *e.sheafSrc;
  if (e.fuze) j["fuze"] = *e.fuze;
  if (e.fuzeSrc) j["fuzeSrc"] = *e.fuzeSrc;
  if (e.bdaClaim) j["bdaClaim"] = *e.bdaClaim;
  putProp("firstMiss", e.firstMiss);
  putProp("aimErr0", e.aimErr0);
  if (e.tInit) j["tInit"] = *e.tInit;
  putProp("corrEff", e.corrEff);
  if (e.bracketed) j["bracketed"] = *e.bracketed;
  if (e.trace) j["trace"] = *e.trace;
  if (e.notes) j["notes"] = *e.notes;
  if (e.stars) j["stars"] = *e.stars;
  return j;
}

/* The 33 level3 transcript fixture filenames. */
inline const std::vector<std::string>& level3Files() {
  static const std::vector<std::string> F = {
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
  return F;
}

}  // namespace tlogjson
