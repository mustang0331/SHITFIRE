#include "shitfire/scenario/place_units.h"

#include <cmath>

#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/prng.h"

/* placeUnits — port of src/js/05-units-scenario.js:1059-1276 (oracle-v1).
   PRNG prime directive: every rng() below sits at its exact source position;
   figures skipped by a cluster's count bail out BEFORE any draw, exactly as
   the forEach early-return does (05:1084). Note the +cos(a) convention in
   every install offset (a screen-space scatter, not an azimuth — 05:1086). */
namespace shitfire { namespace scenario {

namespace {

const double kPi = 3.141592653589793;

/* troopCluster (05:1082-1093): 3 draws per INSTALLED figure (a, r, rotY);
   figures with i >= n take none. */
void troopCluster(UnitLayout& L, Mulberry32& rng, double cx, double cz,
                  int n, double spread) {
  for (int i = 0; i < 8; i++) {
    if (i >= n) continue;
    const double a = rng.next() * kPi * 2;
    const double r = 4 + rng.next() * spread;
    L.troops[i].x = cx + std::sin(a) * r;
    L.troops[i].z = cz + std::cos(a) * r;
    L.troops[i].visible = true;
    L.troops[i].rotY = rng.next() * kPi * 2;
  }
}

/* the shared hut/flag block strongpoint (05:1106-1116), qfp (05:1182-1192)
   and battery (05:1227-1237) all stamp around a compound point c. */
void hutsAndFlag(UnitLayout& L, double cx, double cz) {
  const double hutOff[3][2] = { {0, 0}, {10, 7}, {-9, 8} };
  for (int i = 0; i < 3; i++) {
    L.huts[i].visible = true;
    L.huts[i].x = cx + hutOff[i][0];
    L.huts[i].z = cz + hutOff[i][1];
    L.huts[i].rotY = std::fmod(i * 0.7, kPi);
  }
  L.flagVisible = true;
  L.flagX = cx + 4;
  L.flagZ = cz - 5;
}

}  // namespace

UnitLayout placeUnits(const ScenarioState& S, std::vector<Civ>& civs) {
  /* 05:1061 — stream #7: (S.seed * 68168169 + 17) >>> 0. */
  Mulberry32 rng(js::toUint32(S.seed * 68168169.0 + 17.0));
  UnitLayout L;
  /* 05:1079 — WORLD2: occupation ends with the mission. */
  for (Civ& c : civs) c.hidden = false;
  /* 05:1080 enemyAlive = true is the caller's (mission-owned global). */

  if (S.type == "strongpoint") {   /* 05:1095-1119 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 8, 22);
    for (int i = 0; i < 2; i++) {
      const double a = rng.next() * kPi * 2;
      const double r = 12 + rng.next() * 20;
      L.vehicles[i].visible = true;
      L.vehicles[i].x = S.enemy.x + std::sin(a) * r;
      L.vehicles[i].z = S.enemy.z + std::cos(a) * r;
      L.vehicles[i].rotY = rng.next() * kPi * 2;
    }
    hutsAndFlag(L, S.compound->x, S.compound->z);
  } else if (S.type == "troops") {   /* 05:1120-1121 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 8, 26);
  } else if (S.type == "occupied") {   /* 05:1122-1150 */
    /* villagers indoors — hidden from view, kept in the collateral model. */
    if (S.occupied->kind == "village" && S.hasVillage && S.village) {
      for (Civ& c : civs)
        if (dist2(c.bx, c.bz, S.village->x, S.village->z) <
            S.village->r + 80)
          c.hidden = true;
    }
    /* spots: the ring, or the enemy-anchored fallback triple (05:1131). */
    std::vector<WorldXZ> spots;
    if (S.occupied->hasRing && !S.occupied->ring.empty()) {
      spots = S.occupied->ring;
    } else {
      spots = { S.enemy,
                { S.enemy.x + 25, S.enemy.z + 15 },
                { S.enemy.x - 20, S.enemy.z + 20 } };
    }
    for (int i = 0; i < 8; i++) {   /* all 8 install (05:1133) */
      const WorldXZ& s = spots[static_cast<std::size_t>(i) % spots.size()];
      const double a = rng.next() * kPi * 2;
      const double r = 3 + rng.next() * 7;
      L.troops[i].visible = true;
      L.troops[i].x = s.x + std::sin(a) * r;
      L.troops[i].z = s.z + std::cos(a) * r;
      L.troops[i].rotY = rng.next() * kPi * 2;
    }
    for (int i = 0; i < 2; i++) {   /* 05:1143-1150 */
      const WorldXZ& s = spots[static_cast<std::size_t>(i) % spots.size()];
      const double a = rng.next() * kPi * 2;
      const double r = 14 + rng.next() * 10;
      L.vehicles[i].visible = true;
      L.vehicles[i].x = s.x + std::sin(a) * r;
      L.vehicles[i].z = s.z + std::cos(a) * r;
      L.vehicles[i].rotY = rng.next() * kPi * 2;
    }
  } else if (S.type == "callin") {   /* 05:1151-1159 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 8, 20);
    for (int i = 0; i < 6; i++) {   /* team ring — no draws (05:1154) */
      const double a = (i / 6.0) * kPi * 2;
      L.fSquadVisible[i] = true;
      L.fSquadX[i] = S.team->x + std::sin(a) * 7;
      L.fSquadZ[i] = S.team->z + std::cos(a) * 7;
    }
  } else if (S.type == "defense") {   /* 05:1160-1162 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 8, 20);
  } else if (S.type == "twin") {   /* 05:1163-1176 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 4, 16);
    for (int i = 4; i < 8; i++) {
      const double a = rng.next() * kPi * 2;
      const double r = 4 + rng.next() * 16;
      L.troops[i].visible = true;
      L.troops[i].x = S.twinTgt->x + std::sin(a) * r;
      L.troops[i].z = S.twinTgt->z + std::cos(a) * r;
      L.troops[i].rotY = rng.next() * kPi * 2;
    }
  } else if (S.type == "qfp") {   /* 05:1177-1192 — quiet field, no draws */
    hutsAndFlag(L, S.fireBase->x, S.fireBase->z);
  } else if (S.type == "bunker") {   /* 05:1193-1198 */
    L.bunkerVisible = true;
    L.bunkerX = S.enemy.x;
    L.bunkerZ = S.enemy.z;
    troopCluster(L, rng, S.enemy.x + 14, S.enemy.z + 9, 3, 8);
  } else if (S.type == "convoy") {   /* 05:1199-1200 — driven per-frame */
    for (int i = 0; i < 4; i++) L.vehicles[i].visible = true;
  } else if (S.type == "assault") {   /* 05:1201-1203 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 8, 20);
    for (int i = 0; i < 6; i++) L.fSquadVisible[i] = true;
  } else if (S.type == "wreck") {   /* 05:1204-1210 */
    L.vehicles[0].visible = true;
    L.vehicles[0].x = S.enemy.x;
    L.vehicles[0].z = S.enemy.z;
    L.vehicles[0].rotY = rng.next() * kPi;   /* ONE draw (05:1208) */
    L.vehicles[0].rotZ = 0.35;
    L.vehicles[0].sx = 2.2; L.vehicles[0].sy = 1.3; L.vehicles[0].sz = 1.6;
    L.vehicles[0].deadMat = true;
  } else if (S.type == "battery") {   /* 05:1211-1237 */
    L.bunkerVisible = true;
    L.bunkerX = S.enemy.x;
    L.bunkerZ = S.enemy.z;
    troopCluster(L, rng, S.enemy.x + 12, S.enemy.z + 8, 3, 8);
    const double va = rng.next() * kPi * 2;   /* 05:1221 */
    L.vehicles[0].visible = true;
    L.vehicles[0].x = S.enemy.x + std::sin(va) * 26;
    L.vehicles[0].z = S.enemy.z + std::cos(va) * 26;
    L.vehicles[0].rotY = rng.next() * kPi * 2;   /* 05:1225 */
    hutsAndFlag(L, S.fireBase->x, S.fireBase->z);
  } else if (S.type == "kaiju") {   /* 05:1238-1244 — no draws */
    L.bunkerVisible = true;
    L.bunkerSx = 4.6; L.bunkerSy = 3.1; L.bunkerSz = 5.4;
    L.bunkerX = S.enemy.x;
    L.bunkerZ = S.enemy.z;
  } else if (S.type == "chow") {   /* 05:1245-1261 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 8, 16);
    L.gullScale = true;
    for (int i = 0; i < 3; i++) {   /* the cooks — no draws (05:1252) */
      const double a = i * 2.1, r = 5 + i * 3;
      L.fSquadVisible[i] = true;
      L.fSquadX[i] = S.bbq->x + std::sin(a) * r;
      L.fSquadZ[i] = S.bbq->z + std::cos(a) * r;
    }
  } else if (S.type == "raid") {   /* 05:1262-1269 */
    troopCluster(L, rng, S.enemy.x, S.enemy.z, 8, 18);
    L.vehicles[0].visible = true;
    L.vehicles[0].x = S.enemy.x + 14;
    L.vehicles[0].z = S.enemy.z - 10;
    L.vehicles[0].rotY = rng.next() * kPi * 2;   /* 05:1268 */
  }
  /* kp posts (05:1270-1274) mirror S.kps — data already on S; the
     legibility pass (05:1275) is renderer-side. */
  return L;
}

} }
