#pragma once
#include <string>

namespace shitfire {

/* port of src/js/00-config.js:347 (oracle-v1): 6400 / (Math.PI * 2).
   Defined in mils.cpp so the divide is compiled exactly once, under the
   core's own /fp:precise flags. */
extern const double MILS_PER_RAD;

/* port of src/js/01-math-terrain.js:119 (oracle-v1) — azimuth in radians,
   0 = grid north (world: x = east, z = south). */
double azTo(double ax, double az, double bx, double bz);

/* port of src/js/01-math-terrain.js:122 (oracle-v1) —
   ((r * MILS_PER_RAD) % 6400 + 6400) % 6400, JS `%` = std::fmod. */
double radToMils(double r);

/* port of src/js/01-math-terrain.js:123 (oracle-v1) —
   String(Math.round(m) % 6400).padStart(4, '0'). */
std::string fmtMils(double m);

/* port of src/js/01-math-terrain.js:187 (oracle-v1) — 2D distance.
   Math.hypot → std::hypot (tolerance-checked; see README hypot note). */
double dist2(double ax, double az, double bx, double bz);

}
