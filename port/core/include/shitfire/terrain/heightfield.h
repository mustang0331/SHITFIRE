#pragma once
#include <optional>
#include <vector>
#include "shitfire/config/sim_config.h"

namespace shitfire {

/* Injectable DEM state — port of the JS module-level `let DEM = null`
   (src/js/01-math-terrain.js:40, oracle-v1). Row-major: data[j*n + i] with
   i the x index, matching demSample's d[j0*n + i0] and the oracle capture's
   qaSetDem generation loop (tools/oracle-capture.js:611-614). */
struct DemData {
  int n = 0;
  std::vector<double> data;
};

/* One JS closure = one class: in the browser H() closes over CONFIG.TERRAIN,
   CONFIG.SEED.terrain (read live) and the DEM slot; here they are members.
   Everything downstream (mesh, lase, LOS, impacts, map sheets) only sees H. */
class HeightField {
public:
  HeightField(const TerrainConfig& terrain, double terrainSeed);

  /* CONFIG.SEED.terrain is mutated live by the browser harness; mirror that. */
  void setTerrainSeed(double seed);
  /* nullopt = procedural terrain (JS qaSetDem(0, null)). */
  void setDem(std::optional<DemData> dem);

  /* port of src/js/01-math-terrain.js:41 (oracle-v1) — DEM/procedural dispatch */
  double H(double x, double z) const;
  /* port of src/js/01-math-terrain.js:44 (oracle-v1) — bilinear DEM sample,
     ocean floor outside the 10 km map square. Requires a DEM to be set. */
  double demSample(double x, double z) const;
  /* port of src/js/01-math-terrain.js:56 (oracle-v1) — procedural island */
  double Hproc(double x, double z) const;

private:
  TerrainConfig t_;
  double seed_;
  std::optional<DemData> dem_;
};

}
