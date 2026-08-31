#pragma once

namespace shitfire {

class HeightField;

/* port of src/js/01-math-terrain.js:97 (oracle-v1) — line of sight:
   ceil(d/15) samples along the segment, terrain + 0.4 m clearance blocks. */
bool hasLOS(const HeightField& hf,
            double ax, double ay, double az,
            double bx, double by, double bz);

}
