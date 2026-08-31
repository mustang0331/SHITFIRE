#pragma once

namespace shitfire {

/* port of src/js/00-config.js:10-21 (oracle-v1) — only the CONFIG blocks P1
   needs (SEED / MAP / TERRAIN); the rest of CONFIG lands in P2.
   JS numbers stay doubles (porting rules table); call sites that need JS
   integer semantics convert via js::toInt32 / js::toUint32. */

struct SeedConfig {
  double terrain = 1337;
  double mission = 1;
};

struct MapConfig {          /* SW corner easting/northing in metres */
  double size = 10000;
  double originE = 20000;
  double originN = 50000;
};

struct TerrainConfig {
  double meshSegments = 300;   /* render-side; carried so the block is complete */
  double islandRadius = 3900;
  double radiusWobble = 800;
  double ridgeHeight = 95;
  double massifHeight = 165;
  double massifRadius = 950;
  double massifX = -520;
  double massifZ = -360;
  double oceanFloor = -30;
  double demRes = 512;
  double demMaxElev = 240;
  double demSeaLum = 10;
  double patchSize = 1200;     /* near-field LOD patch (render-side) */
  double patchDiv = 4;
};

struct SimConfig {
  SeedConfig seed;
  MapConfig map;
  TerrainConfig terrain;
};

}
