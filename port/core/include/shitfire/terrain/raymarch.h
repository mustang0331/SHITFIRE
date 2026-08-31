#pragma once
#include <optional>

namespace shitfire {

class HeightField;

struct GroundHitResult { double x; double y; double z; double dist; };

/* port of src/js/01-math-terrain.js:78 (oracle-v1) — ray vs heightfield:
   6 m march then 9-step bisection. Ocean surface y=0 counts as a hit via
   Math.max(H, 0). Returns nullopt where the JS returns null (no hit within
   maxD). Loop structure ported verbatim — march order feeds nothing random
   here, but the same discipline applies everywhere. */
std::optional<GroundHitResult> groundHit(const HeightField& hf,
                                         double ox, double oy, double oz,
                                         double dx, double dy, double dz,
                                         double maxD);

}
