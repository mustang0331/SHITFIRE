#pragma once
#include <vector>
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_layout.h"

/* port of src/js/05-units-scenario.js:1059-1276 (oracle-v1) — placeUnits(),
   the unit-install half the scenario porter left to wave 2/3: PRNG stream #7
   (seed (S.seed * 68168169 + 17) >>> 0, 05:1061, DETERMINISM_NOTE.md §c) and
   the two pieces of non-mesh state it mutates — occupied-village civilian
   hiding (05:1128-1130; the civs stay in the collateral model, 04-world.js
   WORLD2 note) and the hidden-flag reset (05:1079). The enemyAlive = true
   reset (05:1080) is the caller's: that global is mission-owned.

   Everything returned here is LAYOUT — figure/vehicle/emplacement positions
   and orientation draws taken in exact source order for the engine bridge to
   render. Nothing scored reads it (fratricide/collateral/effect all measure
   against S/WORLD state), but the draws sit at their source positions so the
   stream is right for any future consumer. Mesh y-placement (H(x,z) ground
   snapping), flashes, flames, smoke puffs and the legibility pass are
   renderer-side and carry no draws of their own within placeUnits' stream
   except where cited below. All bodies live in place_units.cpp. */
namespace shitfire { namespace scenario {

struct UnitFigure { bool visible = false; double x = 0, z = 0, rotY = 0; };

struct UnitVehicle {
  bool visible = false;
  double x = 0, z = 0, rotY = 0, rotZ = 0;
  double sx = 1, sy = 1, sz = 1;
  bool deadMat = false;              /* wreck: visSetMat(vehMatDead) 05:1210 */
};

struct UnitHut { bool visible = false; double x = 0, z = 0, rotY = 0; };

struct UnitLayout {
  UnitFigure troops[8];              /* units.troops (05:16) */
  UnitVehicle vehicles[4];           /* units.vehicles (05:17) */
  UnitHut huts[3];                   /* units.huts (05:20) */
  bool flagVisible = false;          /* units.flag at compound/fireBase */
  double flagX = 0, flagZ = 0;       /* pole base (05:1114) */
  bool bunkerVisible = false;        /* units.bunker */
  double bunkerX = 0, bunkerZ = 0;
  double bunkerSx = 1, bunkerSy = 1, bunkerSz = 1;   /* kaiju 05:1242 */
  bool fSquadVisible[6] = {};        /* units.fSquad visibility */
  double fSquadX[6] = {}, fSquadZ[6] = {};   /* static installs (callin/chow) */
  bool gullScale = false;            /* chow: troops at 0.5/0.42/0.5 (05:1250) */
};

/* placeUnits (05:1059). Mutates civs' hidden flags exactly as the source
   does (reset at 05:1079, occupied hiding at 05:1128-1130); the caller
   resets its enemyAlive global (05:1080). */
UnitLayout placeUnits(const ScenarioState& S, std::vector<Civ>& civs);

} }
