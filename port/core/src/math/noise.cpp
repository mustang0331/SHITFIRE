#include "shitfire/math/noise.h"
#include "shitfire/math/jsemantics.h"
#include <cmath>
#include <cstdint>

namespace shitfire {

/* port of src/js/01-math-terrain.js:10 (oracle-v1)
   JS sums three Math.imul results as doubles (exact: |sum| < 2^33), then the
   next bitwise op ToInt32s the sum — arithmetically identical to uint32
   wrap-around addition, which is what runs here after the ToInt32 views. */
double hash2(double ix, double iz, double seed) {
  const std::uint32_t ui = static_cast<std::uint32_t>(js::toInt32(ix));
  const std::uint32_t uz = static_cast<std::uint32_t>(js::toInt32(iz));
  const std::uint32_t us = static_cast<std::uint32_t>(js::toInt32(seed));
  std::uint32_t n = ui * 374761393u + uz * 668265263u + us * 974634211u;
  n = (n ^ (n >> 13)) * 1274126177u;
  n ^= n >> 16;
  return static_cast<double>(n) / 4294967296.0;
}

/* port of src/js/01-math-terrain.js:16 (oracle-v1) — expression association
   kept identical to the JS so /fp:precise reproduces it bit-for-bit. */
double vnoise(double x, double z, double seed) {
  const double ix = std::floor(x), iz = std::floor(z);
  const double fx = x - ix, fz = z - iz;
  const double ux = fx * fx * (3 - 2 * fx), uz = fz * fz * (3 - 2 * fz);
  const double a = hash2(ix, iz, seed),     b = hash2(ix + 1, iz, seed);
  const double c = hash2(ix, iz + 1, seed), d = hash2(ix + 1, iz + 1, seed);
  return a + (b - a) * ux + (c - a) * uz + (a - b - c + d) * ux * uz;
}

/* port of src/js/01-math-terrain.js:24 (oracle-v1) */
double fbm(double x, double z, double seed, int oct) {
  double amp = 0.5, f = 1, sum = 0, norm = 0;
  for (int i = 0; i < oct; i++) {
    sum += amp * vnoise(x * f, z * f, seed + i * 131);
    norm += amp; amp *= 0.5; f *= 2;
  }
  return sum / norm;
}

/* port of src/js/01-math-terrain.js:32 (oracle-v1) */
double clamp(double v, double a, double b) { return v < a ? a : v > b ? b : v; }

/* port of src/js/01-math-terrain.js:33 (oracle-v1) */
double lerp(double a, double b, double t) { return a + (b - a) * t; }

/* port of src/js/01-math-terrain.js:34 (oracle-v1) */
double smoothstep(double a, double b, double v) {
  const double t = clamp((v - a) / (b - a), 0, 1);
  return t * t * (3 - 2 * t);
}

}
