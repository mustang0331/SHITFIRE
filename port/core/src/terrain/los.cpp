#include "shitfire/terrain/los.h"
#include "shitfire/terrain/heightfield.h"
#include <cmath>

namespace shitfire {

/* port of src/js/01-math-terrain.js:97 (oracle-v1) — sample interior points
   of the segment at ~15 m spacing; terrain + 0.4 m clearance blocks.
   Math.hypot(x,y,z) → 3-arg std::hypot: only ceil(d/15) consumes d, so a
   last-ULP hypot difference flips nothing unless d sits within a ULP of a
   multiple of 15 (tolerance-class risk, checked by the raylos fixture). */
bool hasLOS(const HeightField& hf,
            double ax, double ay, double az,
            double bx, double by, double bz) {
  const double d = std::hypot(bx - ax, by - ay, bz - az);
  const double n = std::ceil(d / 15);
  for (double i = 1; i < n; i++) {
    const double t = i / n;
    const double x = ax + (bx - ax) * t, y = ay + (by - ay) * t,
                 z = az + (bz - az) * t;
    if (hf.H(x, z) + 0.4 > y) return false;
  }
  return true;
}

}
