#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "scenario_test_util.h"

/* Level 2 / [chapters] — the 74 ch_*.json snapshots: 26 chapters x allowed
   difficulties, generated from each fixture's recorded parameters and the
   chapter table (test-local copy of the CAMPAIGN array, 15-campaign-menu.js:
   10-183 at oracle-v1 — id/type/seed/diffs/scn/volume terrain seed).

   Launch path reproduced (15-campaign-menu.js:523-575): per-volume island
   (buildTerrain/placeOP/placeBattery/buildWorldFeatures at the volume's
   terrain seed), then Scenario = genScenario(type, chapter.seed) with
   activeChapter set (no spread draw) and the chapter's scn opts.

   The oracle snapshot was taken ONE rAF tick (16 ms) after generation, so a
   single updateScenario tick runs before comparing (moves convoy vehicles /
   the kaiju; a no-op for every other chapter type). sim.now at generation is
   recovered from the fixture where the type embeds it (convoy t0 + 220/3.5,
   kaiju t0, assault ft0); no other generated field reads the clock.

   Civilian x/z wander with sim.now (04-world.js:1135); the capture clock is
   recovered from the civ set itself (see recoverCivClock) and my civs are
   advanced to it before the world compare — bx/bz/wr/phase/hidden prove the
   generation, x/z then prove the wander formula. */

namespace {

struct ChapterSpec {
  const char* id;        /* CAMPAIGN id, dots */
  const char* file;      /* id with underscores (fixture filename) */
  const char* type;
  double seed;
  int tseed;
  bool hardOnly;
  bool mask, fClose, lanes;
};

const ChapterSpec CHAPTERS[] = {
  { "F.1", "F_1", "wreck",       51,  1337,  false, false, false, false },
  { "F.2", "F_2", "wreck",       52,  1337,  false, false, false, false },
  { "F.3", "F_3", "wreck",       53,  1337,  false, false, false, false },
  { "1.1", "1_1", "troops",      101, 1337,  false, false, false, false },
  { "1.2", "1_2", "bunker",      102, 1337,  false, false, false, false },
  { "1.3", "1_3", "strongpoint", 103, 1337,  false, false, false, false },
  { "1.4", "1_4", "troops",      104, 1337,  false, false, false, false },
  { "2.1", "2_1", "troops",      201, 9021,  false, false, false, false },
  { "2.2", "2_2", "bunker",      202, 9021,  false, false, false, false },
  { "2.3", "2_3", "troops",      203, 9021,  false, true,  false, false },
  { "2.4", "2_4", "strongpoint", 204, 9021,  false, false, false, false },
  { "2.5", "2_5", "strongpoint", 205, 9021,  false, false, false, false },
  { "3.1", "3_1", "convoy",      301, 5150,  false, false, false, false },
  { "3.2", "3_2", "assault",     302, 5150,  false, false, false, false },
  { "3.3", "3_3", "raid",        303, 5150,  false, false, false, false },
  { "3.4", "3_4", "assault",     304, 5150,  false, false, false, false },
  { "3.5", "3_5", "assault",     305, 5150,  false, false, true,  false },
  { "3.6", "3_6", "strongpoint", 306, 5150,  false, false, false, false },
  { "4.1", "4_1", "strongpoint", 401, 66600, false, false, false, false },
  { "4.2", "4_2", "troops",      402, 66600, false, false, false, false },
  { "4.3", "4_3", "bunker",      403, 66600, true,  false, false, false },
  { "4.4", "4_4", "assault",     404, 66600, true,  false, false, false },
  { "4.5", "4_5", "qfp",         405, 66600, false, false, false, true },
  { "E.1", "E_1", "chow",        501, 1337,  false, false, false, false },
  { "E.2", "E_2", "kaiju",       502, 1337,  false, false, false, false },
  { "E.3", "E_3", "bunker",      503, 1337,  false, false, false, false },
};

}  // namespace

TEST_CASE("chapter launches match oracle for 26 chapters x difficulties",
          "[chapters]") {
  double worstAll = 0;
  std::string worstWhere;
  int passed = 0, total = 0;

  for (const auto& ch : CHAPTERS) {
    const std::vector<std::string> diffs =
        ch.hardOnly ? std::vector<std::string>{ "hard" }
                    : std::vector<std::string>{ "easy", "normal", "hard" };
    for (const auto& diff : diffs) {
      CAPTURE(ch.id, diff);
      total++;
      const auto fx = fixtures::load("level2/ch_" + std::string(ch.file) +
                                     "_" + diff + ".json");
      REQUIRE(fx.at("meta").at("chapter").get<std::string>() == ch.id);
      REQUIRE(fx.at("meta").at("difficulty").get<std::string>() == diff);
      REQUIRE(fx.at("meta").at("seedMission").get<double>() == ch.seed);
      REQUIRE(fx.at("meta").at("seedTerrain").get<int>() == ch.tseed);
      REQUIRE(fx.at("scenario").at("type").get<std::string>() == ch.type);

      const auto& isl = scntest::island(ch.tseed, ch.tseed == 66600);

      /* recover sim.now at generation where the type embeds it */
      double nowGen = 0;
      const auto& fs = fx.at("scenario");
      const std::string type = ch.type;
      if (type == "convoy") nowGen = fs.at("t0").get<double>() + 220.0 / 3.5;
      else if (type == "kaiju") nowGen = fs.at("t0").get<double>();
      else if (type == "assault") nowGen = fs.at("ft0").get<double>();

      shitfire::scenario::ScnOpts opts;
      opts.mask = ch.mask;
      opts.fClose = ch.fClose;
      opts.lanes = ch.lanes;
      auto ctx = scntest::makeCtx(isl, diff, /*chapterActive=*/true, opts, nowGen);
      ctx.terrainSeed = static_cast<double>(ch.tseed);
      auto S = shitfire::scenario::genScenario(type, ch.seed, ctx);

      /* one rAF tick after generation, exactly as captured */
      const double tick = nowGen + 0.016;
      shitfire::scenario::updateScenarioTick(S, tick, nowGen, ctx);

      scntest::CmpState st;
      scntest::deepCompare(fs, scntest::scnToJson(S), "scenario", st);

      /* OP + BATTERY */
      scntest::deepCompare(fx.at("op"),
                           nlohmann::json{ { "x", isl.op.x },
                                           { "z", isl.op.z },
                                           { "h", isl.op.h } },
                           "op", st);
      scntest::deepCompare(fx.at("battery"),
                           nlohmann::json{ { "x", isl.battery.x },
                                           { "z", isl.battery.z } },
                           "battery", st);

      /* WORLD — advance my civs to the recovered capture clock first */
      auto worldCivs = isl.world.civs;   /* copy; layout itself stays pristine */
      const auto& expCivs = fx.at("world").at("civs");
      REQUIRE(expCivs.size() == worldCivs.size());
      const double tCiv = scntest::recoverCivClock(worldCivs, expCivs);
      if (std::isnan(tCiv)) {
        st.err("world.civs", "no capture clock reproduces the civ x/z set — "
                             "bx/bz/wr/phase drift (generation), not wander");
      } else {
        shitfire::scenario::updateCivilians(worldCivs, tCiv);
      }
      shitfire::scenario::WorldLayout wl = isl.world;
      wl.civs = worldCivs;
      scntest::deepCompare(fx.at("world"), scntest::worldToJson(wl), "world", st);

      if (st.worst > worstAll) {
        worstAll = st.worst;
        worstWhere = std::string(ch.id) + "/" + diff + " at " + st.worstPath;
      }
      INFO("worst numeric diff " << st.worst << " at " << st.worstPath);
      for (const auto& e : st.errors) { INFO(e); }
      CHECK(st.errors.empty());
      if (st.errors.empty()) passed++;
    }
  }
  WARN("chapters: " << passed << "/" << total
       << " fixtures matched; worst numeric diff = " << worstAll << " ("
       << worstWhere << ")");
  REQUIRE(passed == total);
}
