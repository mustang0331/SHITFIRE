#pragma once
#include <cstddef>

/* port of src/js/00-config.js (oracle-v1) — the CONFIG blocks the scenario /
   world-placement module consumes, plus the name tables it draws from.
   JS numbers stay doubles (porting rules table). Defaults are the compiled-in
   values, cited per block below. NOTHING computational lives in this header. */
namespace shitfire { namespace scenario {

/* 00-config.js:213 — CONFIG.WORLD.rock */
struct RockConfig {
  double count = 14;
  double sep = 460;
  double rMin = 11;
  double rMax = 27;
  double nMin = 4;
  double nMax = 10;
  double named = 3;
};

/* 00-config.js:195-216 — CONFIG.WORLD */
struct WorldConfig {
  double roadWidth = 8;
  double shoulderW = 3.2;
  double pathWidth = 2.8;
  double villageCount = 2;
  double civsPerVillage = 6;
  double townCount = 1;
  double townBuildingsMin = 14;   /* townBuildings: [14, 21] */
  double townBuildingsMax = 21;
  double townR = 135;
  double civsPerTown = 10;
  double townStreetHalf = 135;
  double townCrossHalf = 88;
  double townSetback = 21;
  RockConfig rock;
  double convoyStopDurLo = 60;    /* convoyStopDur: [60, 180] */
  double convoyStopDurHi = 180;
  double hutCollateralR = 24;
  double civCollateralR = 28;
};

/* 00-config.js:69-82 — CONFIG.MISSION */
struct MissionConfig {
  double fratricideRadius = 80;
  double passMaxAdjustRounds = 4;
  double targetRangeLo = 1500;    /* targetRange: [1500, 3200] */
  double targetRangeHi = 3200;
  double enemyOffsetLo = 150;     /* enemyOffset: [150, 250] */
  double enemyOffsetHi = 250;
  double skirmishRangeLo = 800;   /* skirmishRange: [800, 4200] */
  double skirmishRangeHi = 4200;
  double dangerCloseBattery = 600;  /* dangerClose: { battery, mortar60 } */
  double dangerCloseMortar60 = 250;
};

/* 00-config.js:226 — the CONFIG.GFX keys vegetation placement reads
   (placement positions only; rendering is engine-side). */
struct GfxVegConfig {
  bool veg = true;
  double vegBudget = 4000;
  double vegRadius = 2600;
  double vegMaxH = 6;
};

/* 00-config.js:22,40 — CONFIG.CAMERA keys the OP eye position needs. */
struct CameraConstants {
  double towerHeight = 300;
  double eyeHeight = 2.2;
};

/* 00-config.js:345 — KP_NAMES */
inline constexpr std::size_t KP_NAME_COUNT = 6;
extern const char* const KP_NAMES[KP_NAME_COUNT];

/* 04-world.js:8 — ROCK_NAMES */
inline constexpr std::size_t ROCK_NAME_COUNT = 8;
extern const char* const ROCK_NAMES[ROCK_NAME_COUNT];

/* 00-config.js:323-346 — SCN_META key insertion order (= SCN_TYPES). */
inline constexpr std::size_t SCN_TYPE_COUNT = 15;
extern const char* const SCN_TYPES[SCN_TYPE_COUNT];

} }
