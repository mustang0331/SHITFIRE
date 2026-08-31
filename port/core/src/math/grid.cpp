#include "shitfire/math/grid.h"
#include <cmath>

namespace shitfire {

/* port of src/js/01-math-terrain.js:109 (oracle-v1) — association order kept:
   (originE + x) + 5000 and (originN + 5000) - z, exactly as JS evaluates. */
EN worldToEN(double x, double z, const MapConfig& map) {
  return { map.originE + x + 5000, map.originN + 5000 - z };
}

/* port of src/js/01-math-terrain.js:112 (oracle-v1) */
WorldXZ enToWorld(double e, double n, const MapConfig& map) {
  return { e - map.originE - 5000, 5000 - (n - map.originN) };
}

/* port of src/js/01-math-terrain.js:115 (oracle-v1) — 6-digit, 100 m.
   JS renders the two floors with template-literal number→string; both are
   integral doubles well inside long long range, so std::to_string of the
   cast produces the identical digits. */
std::string gridOf(double x, double z, const MapConfig& map) {
  const EN en = worldToEN(x, z, map);
  const long long ge = static_cast<long long>(std::floor(en.e / 100));
  const long long gn = static_cast<long long>(std::floor(en.n / 100));
  return std::to_string(ge) + ' ' + std::to_string(gn);
}

}
