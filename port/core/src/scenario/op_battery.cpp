#include "shitfire/scenario/world_layout.h"
#include "shitfire/math/mils.h"
#include "shitfire/terrain/heightfield.h"
#include <cmath>

namespace shitfire { namespace scenario {

/* port of src/js/03-op-battery.js:2 (oracle-v1) — findOP.
   The refinement loops re-evaluate their bounds against the MUTATING best
   each iteration (JS `x <= best.x + 120` reads the live object), and the
   inner loop re-initialises from the live best.z. Ported verbatim — changing
   this to fixed bounds moves the OP on some islands. */
OpPos findOP(const HeightField& hf) {
  OpPos best; best.x = 0; best.z = 0; best.h = -1;
  for (double x = -3000; x <= 3000; x += 120)
    for (double z = -3000; z <= 3000; z += 120) {
      const double h = hf.H(x, z);
      if (h > best.h) { best.x = x; best.z = z; best.h = h; }
    }
  for (double x = best.x - 120; x <= best.x + 120; x += 10)
    for (double z = best.z - 120; z <= best.z + 120; z += 10) {
      const double h = hf.H(x, z);
      if (h > best.h) { best.x = x; best.z = z; best.h = h; }
    }
  return best;
}

/* port of src/js/03-op-battery.js:78 (oracle-v1) — findBattery. */
BatteryPos findBattery(const HeightField& hf, const OpPos& op) {
  bool found = false;
  BatteryPos best;
  double bestScore = 1e9;
  for (double x = -3400; x <= 3400; x += 150)
    for (double z = -3400; z <= 3400; z += 150) {
      const double h = hf.H(x, z);
      if (h < 4 || h > 30) continue;
      const double d = dist2(op.x, op.z, x, z);
      if (d < 1200 || d > 2800) continue;
      if (std::fabs(hf.H(x + 40, z) - h) > 3 ||
          std::fabs(hf.H(x, z + 40) - h) > 3) continue;
      const double score = std::fabs(d - 1800);
      if (score < bestScore) { bestScore = score; best.x = x; best.z = z; found = true; }
    }
  if (!found) { best.x = op.x + 800; best.z = op.z + 800; }
  return best;
}

/* port of src/js/03-op-battery.js:63-68 (oracle-v1) — the eye the LOS checks
   use: OP ground + tower + eye height. */
EyePos eyeFor(const OpPos& op, const CameraConstants& cam) {
  EyePos e;
  e.x = op.x;
  e.y = op.h + cam.towerHeight + cam.eyeHeight;
  e.z = op.z;
  return e;
}

} }
