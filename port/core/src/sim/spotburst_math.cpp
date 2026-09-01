#include "shitfire/sim/spotburst_math.h"

#include <cmath>

#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"

namespace shitfire { namespace sim {

namespace {
/* JS Math.PI — the IEEE double nearest pi; identical to V8's constant. */
constexpr double kPi = 3.141592653589793;
}

// port of src/js/10-mission.js:39 (oracle-v1) —
// function otFactor(range) { return Math.max(1, Math.round(range / 1000)); }
double otFactor(double range) {
  const double r = js::round(range / 1000.0);
  return r < 1.0 ? 1.0 : r;   /* Math.max(1, r) */
}

// port of src/js/10-mission.js:53-54 (oracle-v1) —
// let devCorr = -Math.round(mils * otf / 10) * 10;
// if (Math.abs(devCorr) < 30) devCorr = 0;
double deviationCorrection(double mils, double otf) {
  double devCorr = -js::round(mils * otf / 10.0) * 10.0;
  if (std::fabs(devCorr) < 30.0) devCorr = 0.0;
  return devCorr;
}

// port of src/js/10-mission.js:40-56 (oracle-v1) — spotBurst(impact).
SpotResult spotBurst(double opX, double opZ,
                     double enemyX, double enemyZ,
                     double impactX, double impactZ) {
  const double azT = azTo(opX, opZ, enemyX, enemyZ);
  const double azB = azTo(opX, opZ, impactX, impactZ);
  double d = azB - azT;
  while (d > kPi) d -= kPi * 2.0;
  while (d < -kPi) d += kPi * 2.0;
  const double otRange = dist2(opX, opZ, enemyX, enemyZ);
  const double otf = otFactor(otRange);
  const double mils = d * MILS_PER_RAD;                    /* + = right */
  const double overShort =
      dist2(opX, opZ, impactX, impactZ) - otRange;         /* + = over */
  const double devCorr = deviationCorrection(mils, otf);
  return SpotResult{mils, otf, otRange, overShort, devCorr};
}

} }
