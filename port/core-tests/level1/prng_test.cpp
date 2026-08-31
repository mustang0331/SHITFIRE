#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "shitfire/math/prng.h"
#include <cstdint>
#include <string>
#include <vector>

/* Level 1 / PRNG — the one test that must be bit-exact (PARITY_TESTING.md).
   6 seeds x 10,000 draws: the u32 stream EXACT, the double stream bit-exact.
   The u32 comparison is the diagnostic when doubles drift. Nothing merges
   past a failure here. */
TEST_CASE("mulberry32 streams match the oracle bit-for-bit", "[prng]") {
  const auto j = fixtures::load("level1/prng_streams.json");
  const int draws = j.at("draws").get<int>();

  for (const auto& seedVal : j.at("seeds")) {
    const std::uint32_t seed = seedVal.get<std::uint32_t>();
    CAPTURE(seed);
    const auto& stream = j.at("streams").at(std::to_string(seed));
    const auto u32s = stream.at("u32").get<std::vector<std::uint32_t>>();
    const auto dbls = stream.at("double").get<std::vector<double>>();
    REQUIRE(static_cast<int>(u32s.size()) == draws);
    REQUIRE(static_cast<int>(dbls.size()) == draws);

    /* Pass 1 — nextU32(): the pre-division stream, plus the derived double
       (u32 / 2^32, exactly how the JS closure computes its return). */
    shitfire::Mulberry32 rngU(seed);
    long long firstBad = -1, badCount = 0;
    std::uint32_t got0 = 0, exp0 = 0;
    for (int i = 0; i < draws; i++) {
      const std::uint32_t u = rngU.nextU32();
      const double dv = static_cast<double>(u) / 4294967296.0;
      if (u != u32s[i] || fixtures::bits(dv) != fixtures::bits(dbls[i])) {
        if (firstBad < 0) { firstBad = i; got0 = u; exp0 = u32s[i]; }
        badCount++;
      }
    }
    INFO("u32 stream: first mismatch at draw " << firstBad
         << " got " << got0 << " expected " << exp0);
    REQUIRE(badCount == 0);

    /* Pass 2 — next(): the public double API on a fresh instance must
       reproduce the recorded double stream bit-for-bit on its own. */
    shitfire::Mulberry32 rngD(seed);
    long long dFirst = -1, dBad = 0;
    double dGot = 0, dExp = 0;
    for (int i = 0; i < draws; i++) {
      const double v = rngD.next();
      if (fixtures::bits(v) != fixtures::bits(dbls[i])) {
        if (dFirst < 0) { dFirst = i; dGot = v; dExp = dbls[i]; }
        dBad++;
      }
    }
    INFO("double stream via next(): first mismatch at draw " << dFirst
         << " got " << dGot << " expected " << dExp);
    REQUIRE(dBad == 0);
  }
}
