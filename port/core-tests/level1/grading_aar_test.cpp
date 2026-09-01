#include <cmath>
#include <string>
#include <vector>

#include <catch2/catch_amalgamated.hpp>

#include "fixtures.h"
#include "shitfire/campaign/campaign_data.h"
#include "shitfire/campaign/campaign_state.h"
#include "shitfire/campaign/grading.h"
#include "shitfire/doctrine/parser_message.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/review/aar_analysis.h"
#include "shitfire/sim/mission_effects.h"
#include "tlog_json_util.h"

/* [grading] — the campaign grading gate.

   For every AAR recorded in the 33 level3 transcripts, rebuild the grading
   inputs from the transcript and assert the ported gradeMission + AAR
   analysis reproduce the recorded verdict, stars and correction efficiency
   EXACTLY. corrEff is genuinely recomputed (from the recorded missTrace,
   which holds the same Math.round'ed values the browser computed over —
   10-mission.js:1277); verdict and stars are recomputed from the recorded
   metrics through the ported rubric. Commendations are NOT recorded in any
   fixture — the medal logic is pinned by unit assertions below instead.

   Inputs that are not raw-recorded and how each is derived:
   - m.method       — recorded on the mission's cff parse entry.
   - m.imm          — the committed parser (doctrine/parser_message) run over
                      the recorded cff call text; cff-grid fills p.imm
                      (11-parser.js:396).
   - m.usedIllum    — an impact entry with shell 'illum' (10-mission.js:1130).
   - m.planned      — a firetgt/fireseries/firegroup/suppresstgt parse before
                      the AAR (planned launches, 10-mission.js:343+).
   - m.wasted       — recomputed from the rounded trace; the source compares
                      UNROUNDED distances (10-mission.js:1285), so a round
                      within 0.5 m of the 15% boundary could differ — no
                      fixture is near it.
   - m.failReason   — REPLAY-PENDING input: booked live by the mission
                      machine / scenario layer (10-mission.js:1209,
                      05-units-scenario.js:862/:886/:979/:1016) and recorded
                      only through the verdict it produces; derived here from
                      the verdict string, which still proves the verdict
                      assembly, the 0-star rule and every other consumer.
   - S.btyCas       — REPLAY-PENDING (battery skirmish only): a live scenario
                      counter (05-units-scenario.js:1046) recorded nowhere
                      (its sys line logs on the FIRST increment only, :1047).
                      mission_27's stars are asserted to sit in [1, graded
                      stars before the deduction]; wave-3 replay proves the
                      exact count.
   - a.outcome/pct  — raw-recorded effect state; its accumulation is the
                      wave-1 effects engine, proven by the wave-3 replay. */

namespace {

using shitfire::campaign::ChapterSpec;
using shitfire::campaign::GradeMissionView;
using shitfire::campaign::GradeScenarioView;
using shitfire::review::AarMissionView;
using shitfire::review::AarReport;
using shitfire::review::AarScenarioView;
using shitfire::review::BestScores;
using shitfire::review::TlogEntry;
namespace js = shitfire::js;
namespace sim = shitfire::sim;
namespace campaign = shitfire::campaign;
namespace review = shitfire::review;
namespace doctrine = shitfire::doctrine;

const std::string DASH = "\xE2\x80\x94";

/* 10-mission.js:1284-1285 — wasted recount over the (rounded) trace. */
double wastedFromTrace(const std::vector<double>& tr) {
  double wasted = 0;
  bool has = false;
  double prev = 0;
  for (double d : tr) {
    if (has && std::fabs(d - prev) < prev * 0.15 && d > 80) wasted++;
    prev = d;
    has = true;
  }
  return wasted;
}

sim::EffectOutcome outcomeFromName(const std::string& s) {
  if (s == "destroyed") return sim::EffectOutcome::Destroyed;
  if (s == "neutralized") return sim::EffectOutcome::Neutralized;
  if (s == "suppressed") return sim::EffectOutcome::Suppressed;
  return sim::EffectOutcome::None;
}

/* failReason from the verdict it produced (replay-pending input; header). */
std::string failReasonFromVerdict(const std::string& v) {
  if (v == "FAIL " + DASH + " FRATRICIDE") return "fratricide";
  if (v == "FAIL " + DASH + " CIVILIAN CASUALTIES") return "collateral";
  if (v == "FAIL " + DASH + " CONVOY ESCAPED") return "escaped";
  if (v == "FAIL " + DASH + " LANDFALL") return "escaped";
  if (v == "FAIL " + DASH + " POSITION OVERRUN") return "overrun";
  return "";
}

/* S.tgtClass per scenario type (05-units-scenario.js: bunker/wreck/battery/
   kaiju set 'point'; every other type is personnel; convoy never reads
   bands). */
sim::TargetClass targetClassFor(const std::string& type) {
  if (type == "bunker" || type == "wreck" || type == "battery" ||
      type == "kaiju")
    return sim::TargetClass::Point;
  return sim::TargetClass::Personnel;
}

struct MissionTrack {
  std::string cffMethod;   /* '' until a cff parse lands */
  bool imm = false;
  bool usedIllum = false;
  bool planned = false;
  double roundsFired = 0;
  void reset() { *this = MissionTrack{}; }
};

}  // namespace

TEST_CASE("all 33 level3 transcripts reproduce verdict/stars/corrEff",
          "[grading]") {
  const doctrine::ParserContext pctx;   /* declEastDeg 7, no named KPs */
  const double passMax = sim::MissionTunables{}.passMaxAdjustRounds;   /* 4 */
  const sim::EffectsTunables E;         /* 00-config.js:107-145 defaults */

  int transcriptsOk = 0, totalAars = 0;
  std::vector<std::string> mismatches, notesOut;

  for (const std::string& name : tlogjson::level3Files()) {
    const auto fx = fixtures::load("level3/" + name);
    const auto& meta = fx.at("meta");
    const std::string scnType = meta.at("scenarioType").get<std::string>();
    const std::string difficulty = meta.at("difficulty").get<std::string>();
    const ChapterSpec* ch = nullptr;
    if (!meta.at("chapter").is_null()) {
      ch = campaign::findChapter(meta.at("chapter").get<std::string>());
      REQUIRE(ch != nullptr);
    }

    MissionTrack trk;
    const bool isConvoy = scnType == "convoy";
    bool fileOk = true;
    auto flag = [&](const std::string& what) {
      mismatches.push_back(name + ": " + what);
      fileOk = false;
    };

    TlogEntry lastAar;
    int aarCount = 0;

    for (const auto& je : fx.at("tlog")) {
      auto pr = tlogjson::fromJson(je);
      REQUIRE(pr.errors.empty());
      const TlogEntry& e = pr.entry;

      if (e.kind == "parse") {
        const std::string pt = e.ptype ? *e.ptype : "";
        if (pt == "cff") {
          if (e.method.value) trk.cffMethod = *e.method.value;
          const doctrine::ParsedMessage p = doctrine::parseMessage(e.msg, pctx);
          if (p.type == "cff" && !p.imm.empty()) trk.imm = true;
        } else if (pt == "firetgt" || pt == "fireseries" || pt == "firegroup" ||
                   pt == "suppresstgt") {
          trk.planned = true;
        }
      } else if (e.kind == "impact") {
        trk.roundsFired++;
        if (e.shell && *e.shell == "illum") trk.usedIllum = true;
      } else if (e.kind == "aar") {
        aarCount++;
        totalAars++;
        lastAar = e;
        CAPTURE(name, aarCount);
        REQUIRE(e.trace.has_value());
        REQUIRE(e.rounds.has_value());
        REQUIRE(e.outcome.has_value());
        REQUIRE(e.effPct.has_value());
        REQUIRE(e.intent.has_value());
        REQUIRE(e.corrEff.present);
        REQUIRE(e.aimErr0.present);
        REQUIRE(e.tInit.has_value());
        REQUIRE(e.dur.has_value());
        REQUIRE(e.stars.has_value());
        REQUIRE(e.bracketed.has_value());
        REQUIRE(e.notes.has_value());

        /* --- corrEff: genuine recompute from the recorded trace --- */
        const auto ce = campaign::correctionEfficiency(*e.trace);
        if (ce.has_value() != e.corrEff.value.has_value())
          flag("corrEff null-state differs");
        else if (ce && js::round(*ce * 100) != *e.corrEff.value)
          flag("corrEff " + std::to_string(js::round(*ce * 100)) +
               " != recorded " + std::to_string(*e.corrEff.value));

        /* --- rebuild the analysis inputs --- */
        const sim::EffectAssessment a{ outcomeFromName(*e.outcome), *e.effPct };
        AarMissionView m;
        m.g.failReason = failReasonFromVerdict(e.msg);
        m.g.intent = *e.intent;
        m.g.roundsFired = trk.roundsFired;
        m.g.adjustRounds = *e.rounds;
        m.g.aimErr0 = e.aimErr0.value;
        m.g.tStart = 0;
        m.g.tEnd = *e.dur;
        m.g.tInit = *e.tInit;
        m.g.wasted = wastedFromTrace(*e.trace);
        m.g.missTrace = *e.trace;
        m.g.method = trk.cffMethod;
        m.g.imm = trk.imm;
        m.g.usedIllum = trk.usedIllum;
        m.g.planned = trk.planned;
        /* sides: only the recorded product long&&short is recoverable; both
           consumers (payload bracketed, diag bracket line) read the product */
        m.sidesLong = m.sidesShort = *e.bracketed;
        m.firstMiss = e.firstMiss.value;
        m.bdaClaim = e.bdaClaim ? *e.bdaClaim : "";
        m.sheafKind = e.sheaf ? *e.sheaf : "";
        m.sheafSource = e.sheafSrc ? *e.sheafSrc : "";
        m.fuzeKind = e.fuze ? *e.fuze : "";
        m.fuzeSource = e.fuzeSrc ? *e.fuzeSrc : "";
        m.shell = e.shell ? *e.shell : "he";
        /* recorded notes already include the end-of-mission appends
           (13-aar.js:247-263) — carry the count, arm the once-guards */
        m.notes.assign(static_cast<std::size_t>(*e.notes), std::string("note"));
        m.reqIllumNoted = m.reqPlannedNoted = m.btyCasNoted = true;

        AarScenarioView S;
        S.type = scnType;
        S.difficulty = difficulty;
        S.btyCas = 0;   /* replay-pending for battery — see header */
        S.vehDeadCount = isConvoy ? *e.effPct / 25 : 0;   /* pct = dead*25,
                                                    10-mission.js:1093 */

        sim::EffBandSet bands{};
        if (!isConvoy)
          bands = sim::effBands(
              ch && ch->asset == "mortar60" ? sim::AssetKind::Mortar60
                                            : sim::AssetKind::Battery155,
              targetClassFor(scnType), scnType == "kaiju" ? 2.0 : 0.0, E);

        BestScores best;
        campaign::CampState camp;
        AarReport r = review::analyzeAar(m, S, ch, a,
                                         isConvoy ? nullptr : &bands,
                                         /*simNow=*/0, passMax, best, camp);

        /* --- the gate asserts --- */
        if (r.verdict != e.msg)
          flag("verdict '" + r.verdict + "' != recorded '" + e.msg + "'");
        REQUIRE(r.stars.has_value());
        if (scnType == "battery") {
          /* stars replay-pending: the recorded grade carries the btyCas
             deduction (floor 1); computed here without it. */
          if (!(*e.stars >= 1 && *e.stars <= *r.stars))
            flag("battery stars " + std::to_string(*e.stars) +
                 " outside [1, " + std::to_string(*r.stars) + "]");
          else
            notesOut.push_back(name + ": stars replay-pending (S.btyCas live "
                               "counter unrecorded; recorded " +
                               std::to_string(*e.stars) + " within [1, " +
                               std::to_string(*r.stars) + "] computed without "
                               "the deduction)");
        } else if (*r.stars != *e.stars) {
          flag("stars " + std::to_string(*r.stars) + " != recorded " +
               std::to_string(*e.stars));
        }

        /* payload cross-checks (rounding paths through :364-379) */
        const TlogEntry& p = r.tlogAar;
        if (!p.dur || *p.dur != *e.dur) flag("payload dur drifted");
        if (!p.tInit || *p.tInit != *e.tInit) flag("payload tInit drifted");
        if (!p.bracketed || *p.bracketed != *e.bracketed)
          flag("payload bracketed drifted");
        if (!p.notes || *p.notes != *e.notes) flag("payload notes drifted");
        if (p.firstMiss.value.has_value() != e.firstMiss.value.has_value() ||
            (p.firstMiss.value && *p.firstMiss.value != *e.firstMiss.value))
          flag("payload firstMiss drifted");
        if (p.aimErr0.value.has_value() != e.aimErr0.value.has_value() ||
            (p.aimErr0.value && *p.aimErr0.value != *e.aimErr0.value))
          flag("payload aimErr0 drifted");
        if ((p.sheaf ? *p.sheaf : "") != (e.sheaf ? *e.sheaf : ""))
          flag("payload sheaf drifted");
        if ((p.fuze ? *p.fuze : "") != (e.fuze ? *e.fuze : ""))
          flag("payload fuze drifted");
        if ((p.bdaClaim ? *p.bdaClaim : "") != (e.bdaClaim ? *e.bdaClaim : ""))
          flag("payload bdaClaim drifted");
        if (!p.outcome || *p.outcome != *e.outcome)
          flag("payload outcome drifted");

        /* the career book keeps the score of the fail files */
        if (m.g.failReason == "fratricide" &&
            (!camp.career || camp.career->frat != 1))
          flag("career frat not booked");
        if (m.g.failReason == "collateral" &&
            (!camp.career || camp.career->collat != 1))
          flag("career collat not booked");

        trk.reset();   /* next mission on this transcript starts clean */
      }
    }

    /* result block agrees with the final recorded AAR */
    const auto& res = fx.at("result");
    if (res.is_null()) {
      if (aarCount != 0) flag("tutorial transcript has an AAR");
      if (fileOk)
        notesOut.push_back(name + ": no recorded AAR (tutorial, guns cold) " +
                           DASH + " covered by [tlog]");
    } else {
      if (aarCount == 0) flag("result block but no AAR entry");
      else {
        if (res.at("verdict").get<std::string>() != lastAar.msg)
          flag("result.verdict != last AAR msg");
        if (res.at("stars").get<double>() != *lastAar.stars)
          flag("result.stars != last AAR stars");
        if (res.at("rounds").get<double>() != *lastAar.rounds)
          flag("result.rounds != last AAR rounds");
        if (res.at("dur").get<double>() != *lastAar.dur)
          flag("result.dur != last AAR dur");
        if (res.at("outcome").get<std::string>() != *lastAar.outcome)
          flag("result.outcome != last AAR outcome");
      }
    }
    if (fileOk) transcriptsOk++;
  }

  CHECK(totalAars == 32);   /* 31 transcripts with one + mission_31's two */
  for (const auto& s : notesOut) UNSCOPED_INFO("note: " << s);
  for (const auto& s : mismatches) UNSCOPED_INFO("MISMATCH: " << s);
  WARN("grading: " << transcriptsOk << "/" << tlogjson::level3Files().size()
       << " transcripts reproduced (" << totalAars << " AAR entries); "
       << mismatches.size() << " mismatches");
  REQUIRE(mismatches.empty());
  REQUIRE(transcriptsOk == 33);
}

TEST_CASE("CAMPAIGN table matches 15-campaign-menu.js:10-183", "[grading]") {
  const auto& vols = campaign::campaignVolumes();
  REQUIRE(vols.size() == 7);

  /* volume spines: id, tab, need (UX11 ladder 0/0/4/9/15/20/Inf), tseed */
  const struct { const char* id; const char* tab; double need; double tseed; }
      V[] = { { "F", "FWD", 0, 1337 },   { "V1", "I", 0, 1337 },
              { "V2", "II", 4, 9021 },   { "V3", "III", 9, 5150 },
              { "V4", "IV", 15, 66600 }, { "EP", "EPI", 20, 1337 },
              { "V5", "V", -1, 0 } };
  for (std::size_t i = 0; i < 7; i++) {
    CAPTURE(V[i].id);
    CHECK(vols[i].id == V[i].id);
    CHECK(vols[i].tab == V[i].tab);
    if (V[i].need < 0) {
      CHECK(std::isinf(vols[i].need));           /* V5: need Infinity */
      CHECK_FALSE(vols[i].tseed.has_value());
      CHECK(vols[i].chapters.empty());
      CHECK_FALSE(vols[i].tease.empty());
    } else {
      CHECK(vols[i].need == V[i].need);
      REQUIRE(vols[i].tseed.has_value());
      CHECK(*vols[i].tseed == V[i].tseed);
    }
  }
  CHECK(vols[4].palette == "black");             /* V4 */
  CHECK(vols[4].tod == "overcast");

  /* the 26 implemented chapters: id/type/seed/par + grading-relevant flags */
  struct Row {
    const char* id; const char* type; double seed; double par;
    const char* method; bool strict; const char* asset;
    bool reqIllum, reqImmediate, reqPlanned, hardOnly;
    bool mask, fClose, lanes; const char* tod;
  };
  const Row R[] = {
    { "F.1", "wreck", 51, 0, "", false, "", false, false, false, false, false, false, false, "" },
    { "F.2", "wreck", 52, 0, "", false, "", false, false, false, false, false, false, false, "" },
    { "F.3", "wreck", 53, 420, "", false, "", false, false, false, false, false, false, false, "" },
    { "1.1", "troops", 101, 300, "", false, "", false, false, false, false, false, false, false, "" },
    { "1.2", "bunker", 102, 360, "", false, "", false, false, false, false, false, false, false, "" },
    { "1.3", "strongpoint", 103, 360, "", false, "", false, false, false, false, false, false, false, "" },
    { "1.4", "troops", 104, 210, "", false, "", false, false, false, false, false, false, false, "" },
    { "2.1", "troops", 201, 330, "polar", false, "", false, false, false, false, false, false, false, "" },
    { "2.2", "bunker", 202, 360, "shift", false, "", false, false, false, false, false, false, false, "night" },
    { "2.3", "troops", 203, 420, "", false, "", false, false, false, false, true, false, false, "" },
    { "2.4", "strongpoint", 204, 330, "", false, "", false, false, false, false, false, false, false, "" },
    { "2.5", "strongpoint", 205, 540, "", false, "", true, false, false, false, false, false, false, "night" },
    { "3.1", "convoy", 301, 420, "", false, "", false, false, false, false, false, false, false, "" },
    { "3.2", "assault", 302, 420, "", false, "", false, false, false, false, false, false, false, "" },
    { "3.3", "raid", 303, 390, "", false, "", false, false, false, false, false, false, false, "dawn" },
    { "3.4", "assault", 304, 390, "", false, "", false, false, false, false, false, false, false, "" },
    { "3.5", "assault", 305, 390, "", false, "", false, false, false, false, false, true, false, "" },
    { "3.6", "strongpoint", 306, 180, "", false, "", false, true, false, false, false, false, false, "" },
    { "4.1", "strongpoint", 401, 360, "", true, "", false, false, false, false, false, false, false, "" },
    { "4.2", "troops", 402, 330, "", false, "mortar60", false, false, false, false, false, false, false, "" },
    { "4.3", "bunker", 403, 240, "", false, "", false, false, false, true, false, false, false, "" },
    { "4.4", "assault", 404, 330, "", true, "", false, false, false, true, false, false, false, "" },
    { "4.5", "qfp", 405, 600, "", false, "", false, false, true, false, false, false, true, "" },
    { "E.1", "chow", 501, 300, "", false, "", false, false, false, false, false, false, false, "dusk" },
    { "E.2", "kaiju", 502, 420, "", false, "", false, false, false, false, false, false, false, "dawn" },
    { "E.3", "bunker", 503, 300, "", false, "sunlamp", false, false, false, false, false, false, false, "dusk" },
  };
  const auto& all = campaign::allChapters();
  REQUIRE(all.size() == 26);
  for (std::size_t i = 0; i < 26; i++) {
    const Row& x = R[i];
    const ChapterSpec& c = *all[i];
    CAPTURE(x.id);
    CHECK(c.id == x.id);
    CHECK(c.impl);
    CHECK(c.type == x.type);
    CHECK(c.seed == x.seed);
    if (x.par == 0) CHECK_FALSE(c.par.has_value());   /* F.1/F.2: no par */
    else { REQUIRE(c.par.has_value()); CHECK(*c.par == x.par); }
    CHECK(c.method == x.method);
    CHECK(c.strict == x.strict);
    CHECK(c.asset == x.asset);
    CHECK(c.reqIllum == x.reqIllum);
    CHECK(c.reqImmediate == x.reqImmediate);
    CHECK(c.reqPlanned == x.reqPlanned);
    if (x.hardOnly) {
      REQUIRE(c.diffs.size() == 1);
      CHECK(c.diffs[0] == "hard");
    } else CHECK(c.diffs.empty());
    CHECK(c.scn.mask == x.mask);
    CHECK(c.scn.fClose == x.fClose);
    CHECK(c.scn.lanes == x.lanes);
    CHECK(c.tod == x.tod);
    CHECK_FALSE(c.story.empty());
    CHECK_FALSE(c.outro.empty());
    /* the volume back-reference */
    CHECK(campaign::findChapter(c.id) == &c);
    CHECK(campaign::volumeOf(c.id) != nullptr);
  }
  CHECK(all[0]->tut.has_value());
  CHECK(all[0]->tut->noMission);
  CHECK(all[1]->tut.has_value());
  CHECK_FALSE(all[2]->tut.has_value());     /* F.3 rides the normal flow */
  CHECK(all[2]->coach.size() == 3);
  CHECK(all[11]->coach.size() == 2);        /* 2.5 */
  CHECK(campaign::volumeOf("2.5")->id == "V2");
  CHECK(campaign::findChapter("9.9") == nullptr);
}

TEST_CASE("unlock chain, persistence shapes and BEST update rule", "[grading]") {
  using campaign::CampState;
  const auto& vols = campaign::campaignVolumes();
  CampState s;

  /* fresh save: F/V1 open, V2+ locked; first chapter of an open volume open,
     later chapters gated on the previous being starred */
  CHECK(campaign::volUnlocked(s, vols[0]));
  CHECK(campaign::volUnlocked(s, vols[1]));
  CHECK_FALSE(campaign::volUnlocked(s, vols[2]));
  CHECK_FALSE(campaign::volUnlocked(s, vols[6]));   /* Infinity never */
  const auto& v1 = vols[1];
  CHECK(campaign::chUnlocked(s, v1, v1.chapters[0]));
  CHECK_FALSE(campaign::chUnlocked(s, v1, v1.chapters[1]));

  /* record: <=0 no-op, monotonic per difficulty, best-across for stars() */
  CHECK_FALSE(campaign::campRecord(s, "1.1", "normal", 0));
  CHECK(campaign::campRecord(s, "1.1", "normal", 3));
  CHECK_FALSE(campaign::campRecord(s, "1.1", "normal", 2));   /* lower loses */
  CHECK(campaign::campRecord(s, "1.1", "hard", 4));
  CHECK(campaign::campStars(s, "1.1") == 4);
  CHECK(campaign::campTotal(s) == 4);
  CHECK(campaign::chUnlocked(s, v1, v1.chapters[1]));   /* 1.1 starred */
  CHECK(campaign::volUnlocked(s, vols[2]));             /* need 4 met */
  CHECK_FALSE(campaign::volUnlocked(s, vols[3]));       /* need 9 not */

  /* stars on an id outside ALL_CHAPTERS never counts toward total */
  CHECK(campaign::campRecord(s, "X.9", "normal", 5));
  CHECK(campaign::campTotal(s) == 4);

  /* dev unlock opens everything, progress untouched */
  s.devUnlock = true;
  CHECK(campaign::volUnlocked(s, vols[6]));
  CHECK(campaign::chUnlocked(s, vols[4], vols[4].chapters[4]));
  s.devUnlock = false;

  /* career get-or-create (13-aar.js:346) */
  CHECK_FALSE(s.career.has_value());
  campaign::careerRecord(s).missions = 2;
  REQUIRE(s.career.has_value());
  CHECK(s.career->missions == 2);

  /* starStr (15-campaign-menu.js:239) */
  CHECK(campaign::starStr(3) ==
        "\xE2\x98\x85\xE2\x98\x85\xE2\x98\x85\xE2\x98\x86\xE2\x98\x86");
  CHECK(campaign::starStr(0) ==
        "\xE2\x98\x86\xE2\x98\x86\xE2\x98\x86\xE2\x98\x86\xE2\x98\x86");

  /* fmtTime (13-aar.js:235-238) */
  CHECK(review::fmtTime(117) == "1:57");
  CHECK(review::fmtTime(6) == "0:06");
  CHECK(review::fmtTime(300) == "5:00");
  CHECK(review::fmtTime(137) == "2:17");

  /* BEST (13-aar.js:320-328): pass-gated, fewer rounds then faster, time
     stored rounded */
  BestScores b;
  CHECK_FALSE(review::bestUpdate(b, "troops", "normal", 2, 100.6, false));
  CHECK(review::bestUpdate(b, "troops", "normal", 2, 100.6, true));
  REQUIRE(b.data.size() == 1);
  CHECK(b.data[0].first == "troops:normal");
  CHECK(b.data[0].second.time == 101);              /* Math.round(100.6) */
  CHECK_FALSE(review::bestUpdate(b, "troops", "normal", 2, 150, true));
  CHECK(review::bestUpdate(b, "troops", "normal", 2, 90, true));   /* faster */
  CHECK(review::bestUpdate(b, "troops", "normal", 1, 500, true));  /* fewer */
  CHECK(b.data[0].second.rounds == 1);

  /* difficulty caps + correctionEfficiency edges */
  CHECK(campaign::difficultyCap("easy") == 3);
  CHECK(campaign::difficultyCap("normal") == 4);
  CHECK(campaign::difficultyCap("hard") == 5);
  CHECK_FALSE(campaign::correctionEfficiency({}).has_value());
  CHECK_FALSE(campaign::correctionEfficiency({ 54 }).has_value());
  {
    const auto ce = campaign::correctionEfficiency({ 229, 200 });
    REQUIRE(ce.has_value());
    CHECK(shitfire::js::round(*ce * 100) == 13);    /* mission_04's 13% */
  }
}

TEST_CASE("commendations, note appends and G3 no-dispersion marking",
          "[grading]") {
  const sim::EffectsTunables E;
  const double passMax = sim::MissionTunables{}.passMaxAdjustRounds;
  const sim::EffBandSet bands =
      sim::effBands(sim::AssetKind::Battery155, sim::TargetClass::Personnel,
                    0.0, E);

  auto baseView = [] {
    AarMissionView m;
    m.g.intent = "destroy";
    m.g.roundsFired = 7;
    m.g.adjustRounds = 1;
    m.g.aimErr0 = 40;
    m.g.tStart = 0;
    m.g.tEnd = 100;
    m.g.tInit = 3;
    m.g.missTrace = { 54 };
    m.firstMiss = 54;
    m.sheafKind = "open";
    m.sheafSource = "inferred";
    m.fuzeKind = "pd";
    m.fuzeSource = "default";
    return m;
  };

  /* the 8 medal strings — CONTENT_INVENTORY.md §Commendations, incl. the
     oracle-v1 rename to HELL OF A HOLE IN ONE (13-aar.js:355) */
  {
    AarMissionView m = baseView();
    m.refined = true;
    m.otDirSent = true;
    AarScenarioView S{ "strongpoint", "hard", 0, 0 };
    const ChapterSpec* ch41 = campaign::findChapter("4.1");   /* strict, par 360 */
    BestScores best;
    campaign::CampState camp;
    const sim::EffectAssessment a{ sim::EffectOutcome::Neutralized, 23 };
    AarReport r = review::analyzeAar(m, S, ch41, a, &bands, 0, passMax, best,
                                     camp);
    CHECK(r.pass);
    REQUIRE(r.medals.size() == 7);
    CHECK(r.medals[0] == "HELL OF A HOLE IN ONE " + DASH +
                             " effect after a single adjusting round");
    CHECK(r.medals[1] == "EAGLE EYE " + DASH + " initial location within 50 m");
    CHECK(r.medals[2] == "ACTUALLY READ THE MANUAL " + DASH +
                             " clean call format, no coaching");
    CHECK(r.medals[3] == "FAST MOVER " + DASH + " done in well under par");
    CHECK(r.medals[4] == "IRON NET " + DASH + " passed under strict doctrine");
    CHECK(r.medals[5] == "BY THE BOOK " + DASH +
                             " closed the mission with refinement / record as "
                             "target (RREMS)");
    CHECK(r.medals[6] == "ORIENTED " + DASH +
                             " OT direction passed to the FDC");
    CHECK(r.stars.has_value());
    CHECK(*r.stars == 5);
    CHECK(r.campRecorded);
    CHECK(campaign::campStars(camp, "4.1") == 5);
    CHECK(r.bestWritten);   /* fresh BEST */
    REQUIRE(camp.career.has_value());
    CHECK(camp.career->missions == 1);
  }
  /* TEN-METER MAN rides the mortar60 asset (generic on the field) */
  {
    AarMissionView m = baseView();
    AarScenarioView S{ "troops", "normal", 0, 0 };
    BestScores best;
    campaign::CampState camp;
    const sim::EffectAssessment a{ sim::EffectOutcome::Neutralized, 11 };
    AarReport r = review::analyzeAar(m, S, campaign::findChapter("4.2"), a,
                                     &bands, 0, passMax, best, camp);
    bool ten = false;
    for (const auto& s : r.medals)
      if (s == "TEN-METER MAN " + DASH + " precision work with the sixty")
        ten = true;
    CHECK(ten);
  }
  /* no medals without the pass gate */
  {
    AarMissionView m = baseView();
    m.g.failReason = "fratricide";
    AarScenarioView S{ "strongpoint", "normal", 0, 0 };
    BestScores best;
    campaign::CampState camp;
    const sim::EffectAssessment a{ sim::EffectOutcome::Destroyed, 39 };
    AarReport r = review::analyzeAar(m, S, nullptr, a, &bands, 0, passMax,
                                     best, camp);
    CHECK(r.medals.empty());
    CHECK(r.verdict == "FAIL " + DASH + " FRATRICIDE");
    REQUIRE(r.stars.has_value());
    CHECK(*r.stars == 0);
    CHECK_FALSE(r.bestWritten);
    CHECK(camp.career->frat == 1);
  }
  /* reqIllum note appends once, before grading reads notes (13-aar:247-250) */
  {
    AarMissionView m = baseView();
    m.g.adjustRounds = 2;
    AarScenarioView S{ "strongpoint", "normal", 0, 0 };
    BestScores best;
    campaign::CampState camp;
    const sim::EffectAssessment a{ sim::EffectOutcome::Destroyed, 42 };
    AarReport r = review::analyzeAar(m, S, campaign::findChapter("2.5"), a,
                                     &bands, 0, passMax, best, camp);
    REQUIRE(m.notes.size() == 1);
    CHECK(m.reqIllumNoted);
    CHECK(m.notes[0].find("COORDINATED ILLUMINATION") != std::string::npos);
    REQUIRE(r.stars.has_value());
    CHECK(*r.stars == 2);              /* reqIllum cap, fought dark */
    CHECK(r.gradeLetter == "B");       /* one note */
  }
  /* btyCas: note text, star deduction with floor 1 (ENEMY1) */
  {
    AarMissionView m = baseView();
    m.g.adjustRounds = 3;
    m.g.missTrace = { 130, 76, 69 };
    AarScenarioView S{ "battery", "normal", 4, 0 };
    BestScores best;
    campaign::CampState camp;
    const sim::EffectAssessment a{ sim::EffectOutcome::Destroyed, 492 };
    const sim::EffBandSet pt =
        sim::effBands(sim::AssetKind::Battery155, sim::TargetClass::Point, 0.0,
                      E);
    AarReport r = review::analyzeAar(m, S, nullptr, a, &pt, 0, passMax, best,
                                     camp);
    REQUIRE(m.notes.size() == 1);
    CHECK(m.notes[0].find("The battery put 4 rounds") != std::string::npos);
    REQUIRE(r.stars.has_value());
    CHECK(*r.stars == 1);   /* mission_27's shape: 3 - floor(4/2) = 1 */
  }
  /* G3 — noDisp: payload flagged, no stars, nothing recorded */
  {
    AarMissionView m = baseView();
    m.noDisp = true;
    AarScenarioView S{ "troops", "normal", 0, 0 };
    BestScores best;
    campaign::CampState camp;
    const sim::EffectAssessment a{ sim::EffectOutcome::Neutralized, 20 };
    AarReport r = review::analyzeAar(m, S, campaign::findChapter("1.1"), a,
                                     &bands, 0, passMax, best, camp);
    CHECK_FALSE(r.stars.has_value());
    CHECK_FALSE(r.tlogAar.stars.has_value());
    REQUIRE(r.tlogAar.noDisp.has_value());
    CHECK(*r.tlogAar.noDisp);
    CHECK_FALSE(r.campRecorded);
    CHECK(campaign::campStars(camp, "1.1") == 0);
  }
}
