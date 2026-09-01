#include <catch2/catch_amalgamated.hpp>
#include <cmath>
#include <string>

#include "fixtures.h"
#include "shitfire/sim/spotburst_math.h"

/* Level 1 / spotBurst — all 200 recorded cases (level1/spotburst.json:
   synthetic impacts ringed around the live chapter-1.1 target, OP/enemy
   geometry recorded in capturedUnder).

   Comparison standard (P1 precedent + coordinator ruling): otf and devCorr
   are rounded integers — EXACT, compared by IEEE bit pattern. mils, otRange
   and overShort pass through transcendentals (atan2 / hypot), which the
   README pins as tolerance-checked, never bit-checked — asserted within
   1e-9 with the actual max diffs and bit-exact tallies reported. */
TEST_CASE("spotBurst reproduces all 200 fixture cases", "[spotburst]") {
  const auto j = fixtures::load("level1/spotburst.json");
  const auto& cap = j.at("capturedUnder");
  const double opX = cap.at("OP").at("x").get<double>();
  const double opZ = cap.at("OP").at("z").get<double>();
  const double enX = cap.at("enemy").at("x").get<double>();
  const double enZ = cap.at("enemy").at("z").get<double>();

  long long total = 0;
  long long exactMils = 0, exactOtRange = 0, exactOverShort = 0;
  long long badOtf = 0, badDevCorr = 0;
  double maxDMils = 0, maxDOtRange = 0, maxDOverShort = 0;
  std::string firstBad;

  for (const auto& c : j.at("cases")) {
    const double ix = c.at("impact").at(0).get<double>();
    const double iz = c.at("impact").at(1).get<double>();
    const shitfire::sim::SpotResult got =
        shitfire::sim::spotBurst(opX, opZ, enX, enZ, ix, iz);

    const double wMils = c.at("mils").get<double>();
    const double wOtf = c.at("otf").get<double>();
    const double wOtRange = c.at("otRange").get<double>();
    const double wOverShort = c.at("overShort").get<double>();
    const double wDevCorr = c.at("devCorr").get<double>();

    if (fixtures::bits(got.mils) == fixtures::bits(wMils)) exactMils++;
    if (fixtures::bits(got.otRange) == fixtures::bits(wOtRange)) exactOtRange++;
    if (fixtures::bits(got.overShort) == fixtures::bits(wOverShort))
      exactOverShort++;
    maxDMils = std::fmax(maxDMils, std::fabs(got.mils - wMils));
    maxDOtRange = std::fmax(maxDOtRange, std::fabs(got.otRange - wOtRange));
    maxDOverShort =
        std::fmax(maxDOverShort, std::fabs(got.overShort - wOverShort));

    if (fixtures::bits(got.otf) != fixtures::bits(wOtf)) {
      if (firstBad.empty()) firstBad = "otf " + c.dump();
      badOtf++;
    }
    if (fixtures::bits(got.devCorr) != fixtures::bits(wDevCorr)) {
      if (firstBad.empty()) firstBad = "devCorr " + c.dump();
      badDevCorr++;
    }
    total++;
  }

  WARN("spotburst parity: cases=" << total
       << "  bit-exact mils=" << exactMils << "/" << total
       << " otRange=" << exactOtRange << "/" << total
       << " overShort=" << exactOverShort << "/" << total
       << "  max|diff| mils=" << maxDMils
       << " otRange=" << maxDOtRange
       << " overShort=" << maxDOverShort);
  INFO("first integer mismatch: " << firstBad);
  CHECK(total == 200);
  REQUIRE(badOtf == 0);        /* rounded integer — exact */
  REQUIRE(badDevCorr == 0);    /* rounded integer — exact */
  REQUIRE(maxDMils <= 1e-9);
  REQUIRE(maxDOtRange <= 1e-9);
  REQUIRE(maxDOverShort <= 1e-9);
}
