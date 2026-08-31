#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "shitfire/terrain/heightfield.h"
#include "shitfire/terrain/los.h"
#include "shitfire/terrain/raymarch.h"
#include <cmath>
#include <vector>

/* Level 1 / rays — 500 groundHit rays (same hit/miss; hit distance within
   0.01 m) and 500 hasLOS pairs (same boolean) on procedural terrain seed
   1337. Origins/directions are the literal doubles stored in the fixture. */
TEST_CASE("groundHit and hasLOS match oracle over 500+500 samples", "[raylos]") {
  const auto j = fixtures::load("level1/raylos.json");
  const double maxDist = j.at("maxDist").get<double>();

  shitfire::HeightField hf(shitfire::TerrainConfig{},
                           j.at("terrainSeed").get<double>());

  {
    long long total = 0, mism = 0, firstBad = -1;
    double maxDd = 0;
    for (const auto& ray : j.at("rays")) {
      const auto o = ray.at("o").get<std::vector<double>>();
      const auto d = ray.at("d").get<std::vector<double>>();
      const auto res = shitfire::groundHit(hf, o[0], o[1], o[2],
                                           d[0], d[1], d[2], maxDist);
      const bool expHit = !ray.at("hit").is_null();
      if (res.has_value() != expHit) {
        if (firstBad < 0) firstBad = total;
        mism++;
      } else if (res) {
        const double dd =
            std::fabs(res->dist - ray.at("hit").at("dist").get<double>());
        if (dd > maxDd) maxDd = dd;
        if (dd > 0.01) {
          if (firstBad < 0) firstBad = total;
          mism++;
        }
      }
      total++;
    }
    WARN("groundHit: max |dist diff| = " << maxDd << " m over " << total
         << " rays");
    INFO("groundHit: first mismatching ray index " << firstBad);
    CHECK(total == 500);
    REQUIRE(mism == 0);
  }

  {
    long long total = 0, mism = 0, firstBad = -1;
    for (const auto& p : j.at("los")) {
      const auto a = p.at("a").get<std::vector<double>>();
      const auto b = p.at("b").get<std::vector<double>>();
      const bool vis =
          shitfire::hasLOS(hf, a[0], a[1], a[2], b[0], b[1], b[2]);
      if (vis != p.at("vis").get<bool>()) {
        if (firstBad < 0) firstBad = total;
        mism++;
      }
      total++;
    }
    INFO("hasLOS: first mismatching pair index " << firstBad);
    CHECK(total == 500);
    REQUIRE(mism == 0);
  }
}
