#include "shitfire/terrain/heightfield.h"
#include "shitfire/math/noise.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace shitfire {

HeightField::HeightField(const TerrainConfig& terrain, double terrainSeed)
    : t_(terrain), seed_(terrainSeed) {}

void HeightField::setTerrainSeed(double seed) { seed_ = seed; }

void HeightField::setDem(std::optional<DemData> dem) { dem_ = std::move(dem); }

/* port of src/js/01-math-terrain.js:41 (oracle-v1) */
double HeightField::H(double x, double z) const {
  return dem_ ? demSample(x, z) : Hproc(x, z);
}

/* port of src/js/01-math-terrain.js:44 (oracle-v1) — bilinear sample with the
   JS edge clamping (i1/j1 pinned to n-1). The 5000/10000 map extents are
   hard-coded in the JS function, not read from CONFIG.MAP; kept verbatim. */
double HeightField::demSample(double x, double z) const {
  const int n = dem_->n;
  const std::vector<double>& d = dem_->data;
  const double u = (x + 5000) / 10000, v = (z + 5000) / 10000;
  if (u < 0 || u > 1 || v < 0 || v > 1) return t_.oceanFloor;
  const double fx = u * (n - 1), fz = v * (n - 1);
  const int i0 = static_cast<int>(std::floor(fx));
  const int j0 = static_cast<int>(std::floor(fz));
  const int i1 = std::min(i0 + 1, n - 1), j1 = std::min(j0 + 1, n - 1);
  const double tx = fx - i0, tz = fz - j0;
  const double a = d[static_cast<size_t>(j0) * n + i0];
  const double b = d[static_cast<size_t>(j0) * n + i1];
  const double c = d[static_cast<size_t>(j1) * n + i0];
  const double e = d[static_cast<size_t>(j1) * n + i1];
  return a + (b - a) * tx + (c - a) * tz + (a - b - c + e) * tx * tz;
}

/* port of src/js/01-math-terrain.js:56 (oracle-v1) — procedural island.
   Math.hypot/atan2/cos/sin/pow/exp → std:: equivalents (tolerance-checked,
   never bit-checked). Expression association identical to the JS. */
double HeightField::Hproc(double x, double z) const {
  const TerrainConfig& T = t_;
  const double s = seed_;
  const double r = std::hypot(x, z);
  const double th = std::atan2(z, x);
  const double wob =
      (vnoise(std::cos(th) * 2.4 + 40, std::sin(th) * 2.4 + 40, s) * 2 - 1) *
      T.radiusWobble;
  const double R = T.islandRadius + wob;
  const double mask = 1 - smoothstep(0.72 * R, 1.04 * R, r);
  if (mask <= 0) return T.oceanFloor;

  /* Ridge fingers: ridged fBm. */
  const double n = fbm(x * 0.00155, z * 0.00155, s + 7, 5);
  const double ridge = std::pow(1 - std::fabs(2 * n - 1), 1.6) * T.ridgeHeight;
  /* Central volcanic massif. */
  const double dm = std::hypot(x - T.massifX, z - T.massifZ);
  const double massif =
      T.massifHeight * std::exp(-(dm * dm) / (T.massifRadius * T.massifRadius));
  double h = mask * (5 + ridge + massif) + (1 - mask) * T.oceanFloor;
  /* Coral beach shelf: flatten the low band near the waterline. */
  if (h > 0) h *= 0.42 + 0.58 * smoothstep(0, 14, h);
  return h;
}

}
