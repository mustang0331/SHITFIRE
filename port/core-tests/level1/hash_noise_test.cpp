#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "shitfire/math/noise.h"
#include <cmath>

/* Level 1 / hash + noise. hash2 is a pure integer hash ending in an exact
   divide by 2^32 — bit-exact required. vnoise/fbm are exact IEEE arithmetic
   over hash2, but the spec tolerance is 1e-12; the actual max diff is
   WARN-printed so it gets documented (expected: 0). */
TEST_CASE("hash2 bit-exact; vnoise/fbm within 1e-12", "[hash_noise]") {
  const auto j = fixtures::load("level1/hash_noise.json");
  const auto& samples = j.at("samples");

  {
    long long bad = 0, first = -1;
    long long total = 0;
    for (const auto& s : samples.at("hash2")) {
      const double got = shitfire::hash2(s.at("ix").get<double>(),
                                         s.at("iz").get<double>(),
                                         s.at("seed").get<double>());
      if (fixtures::bits(got) != fixtures::bits(s.at("v").get<double>())) {
        if (first < 0) first = total;
        bad++;
      }
      total++;
    }
    INFO("hash2: first mismatch at sample " << first << " of " << total);
    CHECK(total == j.at("counts").at("hash2").get<long long>());
    REQUIRE(bad == 0);
  }

  {
    double maxDiff = 0;
    long long total = 0;
    for (const auto& s : samples.at("vnoise")) {
      const double got = shitfire::vnoise(s.at("x").get<double>(),
                                          s.at("z").get<double>(),
                                          s.at("seed").get<double>());
      const double diff = std::fabs(got - s.at("v").get<double>());
      if (diff > maxDiff) maxDiff = diff;
      total++;
    }
    WARN("vnoise max abs diff = " << maxDiff << " over " << total << " samples");
    CHECK(total == j.at("counts").at("vnoise").get<long long>());
    REQUIRE(maxDiff <= 1e-12);
  }

  {
    double maxDiff = 0;
    long long total = 0;
    for (const auto& s : samples.at("fbm")) {
      const double got = shitfire::fbm(s.at("x").get<double>(),
                                       s.at("z").get<double>(),
                                       s.at("seed").get<double>(),
                                       s.at("oct").get<int>());
      const double diff = std::fabs(got - s.at("v").get<double>());
      if (diff > maxDiff) maxDiff = diff;
      total++;
    }
    WARN("fbm max abs diff = " << maxDiff << " over " << total << " samples");
    CHECK(total == j.at("counts").at("fbm").get<long long>());
    REQUIRE(maxDiff <= 1e-12);
  }
}
