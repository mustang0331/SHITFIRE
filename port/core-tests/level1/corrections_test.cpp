#include <catch2/catch_amalgamated.hpp>
#include <cmath>
#include <string>

#include "fixtures.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/sim/spotburst_math.h"

/* Level 1 / corrections — the otFactor range sweep plus the doctrinal
   deviation-correction rounding (mils x OT factor, nearest 10 m, opposite
   the spotting) and the 30 m HE minimum, from level1/spotburst.json.
   Everything here is add/mul/div/floor on recorded inputs — exact IEEE
   arithmetic, so every check is bit-exact. */

TEST_CASE("otFactor matches the 49-point range sweep exactly", "[corrections]") {
  const auto j = fixtures::load("level1/spotburst.json");
  long long total = 0, bad = 0;
  std::string firstBad;
  for (const auto& c : j.at("otFactorSweep")) {
    const double got = shitfire::sim::otFactor(c.at("range").get<double>());
    if (fixtures::bits(got) != fixtures::bits(c.at("otf").get<double>())) {
      if (firstBad.empty()) firstBad = c.dump();
      bad++;
    }
    total++;
  }
  INFO("first mismatch: " << firstBad);
  CHECK(total == 49);
  REQUIRE(bad == 0);
}

TEST_CASE("deviation correction: rounding and the 30 m minimum, exact",
          "[corrections]") {
  const auto j = fixtures::load("level1/spotburst.json");
  long long total = 0, bad = 0, clamped = 0, nonzero = 0;
  std::string firstBad;
  for (const auto& c : j.at("cases")) {
    const double mils = c.at("mils").get<double>();
    const double otf = c.at("otf").get<double>();
    const double want = c.at("devCorr").get<double>();
    const double got = shitfire::sim::deviationCorrection(mils, otf);
    if (fixtures::bits(got) != fixtures::bits(want)) {
      if (firstBad.empty()) firstBad = c.dump();
      bad++;
    }
    /* Decompose the shipped expression to prove the clamp specifically:
       raw = -Math.round(mils*otf/10)*10; |raw| < 30 must zero out
       (10-mission.js:53-54). */
    const double raw = -shitfire::js::round(mils * otf / 10.0) * 10.0;
    if (std::fabs(raw) < 30.0) {
      clamped++;
      if (want != 0.0 && firstBad.empty()) firstBad = "unclamped " + c.dump();
      if (want != 0.0) bad++;
    } else {
      nonzero++;
      /* every called correction is a >=30 m multiple of 10 */
      if (std::fabs(want) < 30.0 ||
          std::fmod(std::fabs(want), 10.0) != 0.0) {
        if (firstBad.empty()) firstBad = "bad multiple " + c.dump();
        bad++;
      }
    }
    total++;
  }
  WARN("corrections parity: cases=" << total << "  under-30m-clamped="
       << clamped << "  called=" << nonzero);
  INFO("first mismatch: " << firstBad);
  CHECK(total == 200);
  CHECK(clamped > 0);       /* the 30 m minimum is actually exercised */
  CHECK(nonzero > 0);
  REQUIRE(bad == 0);
}

/* JS Math.round is floor(x+0.5) — half toward +infinity, NOT half away from
   zero (porting rules table). The correction negates AFTER rounding, so the
   boundary behaviour is asymmetric; these synthetic points pin it. */
TEST_CASE("deviation correction boundary semantics (JS Math.round)",
          "[corrections]") {
  using shitfire::sim::deviationCorrection;
  /* mils*otf = 25  -> 2.5 rounds to 3  -> -30 (survives the minimum) */
  REQUIRE(deviationCorrection(2.5, 10.0) == -30.0);
  /* mils*otf = -25 -> -2.5 rounds to -2 (toward +inf) -> +20 -> under 30 -> 0 */
  REQUIRE(deviationCorrection(-2.5, 10.0) == 0.0);
  /* mils*otf = -35 -> -3.5 rounds to -3 (toward +inf) -> +30 stays */
  REQUIRE(deviationCorrection(-3.5, 10.0) == 30.0);
  /* mils*otf = 24  -> 2.4 rounds to 2 -> -20 -> 0 */
  REQUIRE(deviationCorrection(2.4, 10.0) == 0.0);
  /* mils*otf = 29  -> 2.9 rounds to 3 -> -30 stays */
  REQUIRE(deviationCorrection(2.9, 10.0) == -30.0);
}
