#include "shitfire/math/mils.h"
#include "shitfire/math/jsemantics.h"
#include <cmath>

namespace shitfire {

/* port of src/js/00-config.js:347 (oracle-v1): 6400 / (Math.PI * 2).
   3.141592653589793 is Math.PI — the closest double to pi, identical to
   C++ std::numbers::pi. Spelled as a literal so the value cannot drift with
   a header change. */
const double MILS_PER_RAD = 6400.0 / (3.141592653589793 * 2.0);

/* port of src/js/01-math-terrain.js:119 (oracle-v1) */
double azTo(double ax, double az, double bx, double bz) {
  return std::atan2(bx - ax, -(bz - az));
}

/* port of src/js/01-math-terrain.js:122 (oracle-v1) — JS `%` on doubles is
   std::fmod (result takes the dividend's sign; fmod is exact). */
double radToMils(double r) {
  return std::fmod(std::fmod(r * MILS_PER_RAD, 6400.0) + 6400.0, 6400.0);
}

/* port of src/js/01-math-terrain.js:123 (oracle-v1)
   JS: String(Math.round(m) % 6400).padStart(4, '0'). */
std::string fmtMils(double m) {
  const double rounded = js::round(m);
  const double rem = std::fmod(rounded, 6400.0);
  std::string s = std::to_string(static_cast<long long>(rem));
  while (s.size() < 4) s.insert(s.begin(), '0');
  return s;
}

/* port of src/js/01-math-terrain.js:187 (oracle-v1) — Math.hypot →
   std::hypot. If a fixture ever fails at the 1e-9 edge, the documented
   fallback is std::sqrt(x*x + z*z); see README hypot note. */
double dist2(double ax, double az, double bx, double bz) {
  return std::hypot(bx - ax, bz - az);
}

}
