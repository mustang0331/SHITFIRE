#pragma once
#include <string>
#include "shitfire/config/sim_config.h"

namespace shitfire {

/* World coords: x = east, z = south (north is -z), island centred on origin.
   JS reads CONFIG.MAP globally; here the map block is passed in. */

struct EN { double e; double n; };
struct WorldXZ { double x; double z; };

/* port of src/js/01-math-terrain.js:109 (oracle-v1) */
EN worldToEN(double x, double z, const MapConfig& map);

/* port of src/js/01-math-terrain.js:112 (oracle-v1) */
WorldXZ enToWorld(double e, double n, const MapConfig& map);

/* port of src/js/01-math-terrain.js:115 (oracle-v1) — 6-digit 100 m grid,
   formatted exactly like the JS: floor(e/100) + ' ' + floor(n/100). */
std::string gridOf(double x, double z, const MapConfig& map);

}
