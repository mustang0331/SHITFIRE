#include "shitfire/terrain/raymarch.h"
#include "shitfire/terrain/heightfield.h"
#include <algorithm>

namespace shitfire {

/* port of src/js/01-math-terrain.js:78 (oracle-v1) — loop structure verbatim:
   march t = 6, 12, ... <= maxD (exact doubles), then 9 bisection steps.
   Ocean surface y=0 counts as a hit via max(H, 0). */
std::optional<GroundHitResult> groundHit(const HeightField& hf,
                                         double ox, double oy, double oz,
                                         double dx, double dy, double dz,
                                         double maxD) {
  const double step = 6;
  double tPrev = 0;
  for (double t = step; t <= maxD; t += step) {
    const double x = ox + dx * t, y = oy + dy * t, z = oz + dz * t;
    if (y <= std::max(hf.H(x, z), 0.0)) {
      double lo = tPrev, hi = t;
      for (int i = 0; i < 9; i++) {
        const double m = (lo + hi) / 2;
        const double my = oy + dy * m;
        if (my <= std::max(hf.H(ox + dx * m, oz + dz * m), 0.0)) hi = m;
        else lo = m;
      }
      const double hx = ox + dx * hi, hz = oz + dz * hi;
      return GroundHitResult{ hx, std::max(hf.H(hx, hz), 0.0), hz, hi };
    }
    tPrev = t;
  }
  return std::nullopt;
}

}
