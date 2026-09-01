#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "shitfire/math/grid.h"
#include "shitfire/scenario/world_config.h"

/* port of src/js/03-op-battery.js and the placement half of src/js/04-world.js
   (oracle-v1). Positions, kinds, names and path polylines only — all
   mesh/geometry construction (ribbons, boxes, boulder meshes, figures) is
   engine-side and skipped. Every field name tracks the JS property it mirrors
   so the fixture snapshots compare 1:1. All bodies live in src/scenario/. */
namespace shitfire { class HeightField; }

namespace shitfire { namespace scenario {

/* 03-op-battery.js:16 / 92 */
struct OpPos { double x = 0, z = 0, h = 0; };
struct BatteryPos { double x = 0, z = 0; };
/* 03-op-battery.js:17,66-68 — the observer eye the LOS checks use. */
struct EyePos { double x = 0, y = 0, z = 0; };

/* WORLD.facilities entries (04-world.js:553,568,583,594,599). Only the
   airfield carries th/len. */
struct Facility {
  std::string kind, name;
  double x = 0, z = 0;
  std::optional<double> th, len;
};

/* Village hut {x,z} (04:623) or town building {x,z,w,d} (04:707). */
struct Hut {
  double x = 0, z = 0;
  std::optional<double> w, d;
};

/* 04-world.js:731 — town.foot */
struct TownFoot {
  double ux = 0, uz = 0, aHalf = 0, bHalf = 0, cx = 0, cz = 0;
};

/* WORLD.villages entry — village (04:613) or town (04:670, tier:'town').
   `bare` marks the kaiju fallback `{ x: 0, z: 0 }` (05:732) that carries no
   other keys; it never occurs on an island that has villages. */
struct Village {
  std::string name;
  double x = 0, z = 0, r = 70;
  bool isTown = false;
  std::optional<TownFoot> foot;
  std::vector<Hut> huts;
  bool bare = false;
};

/* WORLD.civs entry (04:629,740) minus the mesh. x/z start at the settlement
   centre; updateCivilians (04:1135) drives them per frame. */
struct Civ {
  double bx = 0, bz = 0, wr = 0, phase = 0, x = 0, z = 0;
  bool hidden = false;
};

/* WORLD.rocks entry (04:794). name is null except the RK.named highest-score. */
struct Rock {
  double x = 0, z = 0, s = 0, h = 0, r = 0;
  std::optional<std::string> name;
};

/* 04-world.js:927 — WIND (rollWind) */
struct Wind { double dx = 0, dz = 0, speed = 0, fromMils = 0; };

/* VEG.placements entry (04:1051) minus mesh bookkeeping. type: 0 canopy,
   1 scatter. Visual-only downstream, ported so the stream (04:1004) advances
   in source order. */
struct VegPlacement {
  double x = 0, z = 0, h = 0;
  int type = 0;
  double s = 0, rot = 0, t = 0;
};

using Polyline = std::vector<WorldXZ>;

/* The plain-data WORLD (04:6) + WIND + VEG for one built island. */
struct WorldLayout {
  std::vector<Polyline> roads;      /* [0] coast road; then apron, streets, connectors */
  std::vector<Polyline> paths;
  std::vector<Facility> facilities;
  std::vector<Village> villages;    /* towns carry isTown / foot */
  std::vector<Civ> civs;
  std::vector<Rock> rocks;
  Wind wind;
  std::vector<VegPlacement> veg;
};

/* 04-world.js:10 — FNV-1a-style string hash, ToUint32 result. */
std::uint32_t strHash(const std::string& s);

/* 03-op-battery.js:2 — highest-point scan; the refinement loop's bounds
   re-read the mutating best, exactly like the JS. */
OpPos findOP(const HeightField& hf);

/* 03-op-battery.js:78 — flat low ground 1200-2800 m from the OP, scored
   toward 1800 m; falls back to OP+{800,800}. */
BatteryPos findBattery(const HeightField& hf, const OpPos& op);

/* 03-op-battery.js:63-68 — eye = OP + towerHeight + eyeHeight. */
EyePos eyeFor(const OpPos& op, const CameraConstants& cam);

/* 04-world.js:345 buildWorldFeatures + 04:928 rollWind + 04:988
   buildVegetation, placement halves only.
     terrainSeed  CONFIG.SEED.terrain
     demName      nullptr = procedural island (DEM null); else the DEM name
                  mixed into wSeed = (terrain ^ strHash(name)) >>> 0
     blackPalette TERRAIN_PALETTE === 'black' (Volume IV) — vegetation budget
                  only; nothing fixture-visible changes.
   PRNG streams (DETERMINISM_NOTE.md c): world stream 1 (04:358), stream 2
   (04:364), stream 3 (04:370), wind (04:929), vegetation (04:1004). Call
   order inside is the source order — placement retry loops, candidate scans
   and early-exits are ported verbatim. */
WorldLayout buildWorldFeatures(const HeightField& hf,
                               const OpPos& op,
                               const BatteryPos& battery,
                               const WorldConfig& W,
                               const GfxVegConfig& gfx,
                               double terrainSeed,
                               const std::string* demName,
                               bool blackPalette);

} }
