#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "shitfire/terrain/heightfield.h"
#include <cmath>
#include <string>
#include <vector>

namespace {

shitfire::TerrainConfig cfgFrom(const nlohmann::json& tc) {
  shitfire::TerrainConfig c;
  c.meshSegments = tc.at("meshSegments").get<double>();
  c.islandRadius = tc.at("islandRadius").get<double>();
  c.radiusWobble = tc.at("radiusWobble").get<double>();
  c.ridgeHeight = tc.at("ridgeHeight").get<double>();
  c.massifHeight = tc.at("massifHeight").get<double>();
  c.massifRadius = tc.at("massifRadius").get<double>();
  c.massifX = tc.at("massifX").get<double>();
  c.massifZ = tc.at("massifZ").get<double>();
  c.oceanFloor = tc.at("oceanFloor").get<double>();
  c.demRes = tc.at("demRes").get<double>();
  c.demMaxElev = tc.at("demMaxElev").get<double>();
  c.demSeaLum = tc.at("demSeaLum").get<double>();
  c.patchSize = tc.at("patchSize").get<double>();
  c.patchDiv = tc.at("patchDiv").get<double>();
  return c;
}

}

/* Level 1 / procedural terrain — H_proc over the captured 200x200 grid
   (value[j*n+i] = H(xFrom+i*step, zFrom+j*step)) for all four terrain seeds.
   Transcendental-fed (hypot/atan2/cos/sin/pow/exp), so tolerance 1e-9; the
   actual per-seed max abs diff is WARN-printed for the record. */
TEST_CASE("H_proc grids match oracle within 1e-9 for all 4 seeds", "[terrain_proc]") {
  for (const int seed : {1337, 9021, 5150, 66600}) {
    CAPTURE(seed);
    const auto j =
        fixtures::load("level1/hxz_proc_" + std::to_string(seed) + ".json");
    REQUIRE(j.at("terrainSeed").get<int>() == seed);

    const auto& g = j.at("grid");
    const int n = g.at("n").get<int>();
    const double step = g.at("step").get<double>();
    const double xFrom = g.at("xFrom").get<double>();
    const double zFrom = g.at("zFrom").get<double>();
    const auto vals = j.at("values").get<std::vector<double>>();
    REQUIRE(static_cast<int>(vals.size()) == n * n);

    shitfire::HeightField hf(cfgFrom(j.at("terrainConfig")),
                             j.at("terrainSeed").get<double>());

    double maxDiff = 0;
    double worstX = 0, worstZ = 0;
    for (int jj = 0; jj < n; jj++) {
      for (int i = 0; i < n; i++) {
        const double x = xFrom + i * step, z = zFrom + jj * step;
        const double diff =
            std::fabs(hf.H(x, z) - vals[static_cast<size_t>(jj) * n + i]);
        if (diff > maxDiff) { maxDiff = diff; worstX = x; worstZ = z; }
      }
    }
    WARN("terrain_proc seed " << seed << ": max abs diff = " << maxDiff
         << " at (" << worstX << ", " << worstZ << ") over " << n * n
         << " points");
    REQUIRE(maxDiff <= 1e-9);
  }
}
