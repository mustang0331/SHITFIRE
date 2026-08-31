#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "shitfire/math/noise.h"
#include "shitfire/terrain/heightfield.h"
#include <cmath>
#include <utility>
#include <vector>

/* Level 1 / DEM sampling — the synthetic DEM is REGENERATED from the formula
   parameters recorded in the fixture (never hand-copied values), exactly as
   the oracle capture built it (tools/oracle-capture.js:611-614):
     data[j*n + i] = scale * hash2(i, j, seed) + offset
   hash2 is bit-exact, so the regenerated DEM is bit-identical to the one the
   browser sampled. Then the 200x200 grid + 108 edge cases within 1e-9. */
TEST_CASE("DEM H(x,z): regenerated DEM, grid and edge cases within 1e-9", "[terrain_dem]") {
  const auto j = fixtures::load("level1/hxz_dem.json");

  const auto& spec = j.at("demSpec");
  const int n = spec.at("n").get<int>();
  const double scale = spec.at("scale").get<double>();
  const double offset = spec.at("offset").get<double>();
  const double dseed = spec.at("seed").get<double>();

  shitfire::DemData dem;
  dem.n = n;
  dem.data.resize(static_cast<size_t>(n) * n);
  for (int jj = 0; jj < n; jj++)
    for (int i = 0; i < n; i++)
      dem.data[static_cast<size_t>(jj) * n + i] =
          scale * shitfire::hash2(i, jj, dseed) + offset;

  /* Capture ran at default CONFIG.TERRAIN (the fixture carries no
     terrainConfig block); out-of-map samples return oceanFloor = -30.
     The terrain seed is not read on the DEM path. */
  shitfire::HeightField hf(shitfire::TerrainConfig{}, 1337);
  hf.setDem(std::move(dem));

  {
    const auto& g = j.at("grid");
    const int gn = g.at("n").get<int>();
    const double step = g.at("step").get<double>();
    const double xFrom = g.at("xFrom").get<double>();
    const double zFrom = g.at("zFrom").get<double>();
    const auto vals = j.at("values").get<std::vector<double>>();
    REQUIRE(static_cast<int>(vals.size()) == gn * gn);

    double maxDiff = 0;
    for (int jj = 0; jj < gn; jj++) {
      for (int i = 0; i < gn; i++) {
        const double diff =
            std::fabs(hf.H(xFrom + i * step, zFrom + jj * step) -
                      vals[static_cast<size_t>(jj) * gn + i]);
        if (diff > maxDiff) maxDiff = diff;
      }
    }
    WARN("terrain_dem grid: max abs diff = " << maxDiff << " over "
         << gn * gn << " points");
    REQUIRE(maxDiff <= 1e-9);
  }

  {
    double maxDiff = 0;
    long long total = 0;
    double worstX = 0, worstZ = 0;
    for (const auto& e : j.at("edgeCases")) {
      const double x = e.at("x").get<double>(), z = e.at("z").get<double>();
      const double diff = std::fabs(hf.H(x, z) - e.at("h").get<double>());
      if (diff > maxDiff) { maxDiff = diff; worstX = x; worstZ = z; }
      total++;
    }
    WARN("terrain_dem edge cases: max abs diff = " << maxDiff << " at ("
         << worstX << ", " << worstZ << ") over " << total << " cases");
    CHECK(total == 108);
    REQUIRE(maxDiff <= 1e-9);
  }
}
