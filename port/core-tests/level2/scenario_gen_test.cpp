#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "scenario_test_util.h"

/* Level 2 / [scenario_gen] — genScenario parity: all 15 types x seeds
   11/12/13 against the 45 gen_*.json fixtures. Captured on the default
   island (terrain 1337) from the skirmish menu: no active chapter (spread
   draw), difficulty normal, sim.now = 30.064 (MANIFEST fake clock: epoch
   +30000 ms, rAF at +64 ms; recoverable from every convoy t0 / kaiju t0 /
   assault ft0 in the fixtures — all three agree on 30.064 for all seeds).

   Gate: positions within 0.01 m, ids/names/counts/kinds exact (briefs are
   compared byte-for-byte — they embed the grid math). Worst per-type diff is
   WARN-printed. A 300 m miss is PRNG-order drift, not a tolerance problem. */

TEST_CASE("genScenario matches oracle for 15 types x seeds 11/12/13",
          "[scenario_gen]") {
  const auto& isl = scntest::island(1337, false);
  double worstAll = 0;
  std::string worstWhere;
  int passed = 0, total = 0;

  for (std::size_t ti = 0; ti < shitfire::scenario::SCN_TYPE_COUNT; ti++) {
    const std::string type = shitfire::scenario::SCN_TYPES[ti];
    for (const int seed : { 11, 12, 13 }) {
      CAPTURE(type, seed);
      total++;
      const auto fx = fixtures::load("level2/gen_" + type + "_" +
                                     std::to_string(seed) + ".json");
      REQUIRE(fx.at("meta").at("type").get<std::string>() == type);
      REQUIRE(fx.at("meta").at("seed").get<int>() == seed);
      REQUIRE(fx.at("meta").at("terrainSeed").get<int>() == 1337);
      const std::string difficulty =
          fx.at("meta").at("difficulty").get<std::string>();

      auto ctx = scntest::makeCtx(isl, difficulty, /*chapterActive=*/false,
                                  shitfire::scenario::ScnOpts{},
                                  /*now=*/30.064);
      ctx.terrainSeed = 1337;
      const auto S = shitfire::scenario::genScenario(
          type, static_cast<double>(seed), ctx);

      scntest::CmpState st;
      scntest::deepCompare(fx.at("scenario"), scntest::scnToJson(S),
                           "scenario", st);
      if (st.worst > worstAll) {
        worstAll = st.worst;
        worstWhere = type + "/" + std::to_string(seed) + " at " + st.worstPath;
      }
      INFO("worst numeric diff " << st.worst << " at " << st.worstPath);
      for (const auto& e : st.errors) { INFO(e); }
      CHECK(st.errors.empty());
      if (st.errors.empty()) passed++;
    }
  }
  WARN("scenario_gen: " << passed << "/" << total
       << " fixtures matched; worst numeric diff = " << worstAll << " ("
       << worstWhere << ")");
  REQUIRE(passed == total);
}
