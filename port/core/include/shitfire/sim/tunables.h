#pragma once
#include "shitfire/config/sim_config.h"

/* Sim-layer tunables — port of the CONFIG blocks the mission machine reads
   (oracle-v1, src/js/00-config.js). Data only: every default below is the
   literal value in 00-config.js at tag oracle-v1, cited per block. JS numbers
   stay doubles (porting rules table); call sites needing JS integer semantics
   convert via js::toInt32 / js::toUint32.

   Owned by the P2 wave-1 MISSION porter. SEED/MAP/TERRAIN already live in
   shitfire/config/sim_config.h (P1) and are reused, not duplicated. */
namespace shitfire { namespace sim {

/* port of src/js/00-config.js:315 (oracle-v1) — let DIFFICULTY = 'normal'.
   The three difficulty keys used by BALLISTICS.firstRound / .followUp and by
   coachOn() (10-mission.js:32: coaching silent on hard). */
enum class Difficulty { Easy, Normal, Hard };

/* port of src/js/00-config.js:51-68 (oracle-v1) — CONFIG.BALLISTICS. */
struct BallisticsTunables {
  double tofDivisor = 300;
  double tofBase = 8;
  double tofMin = 15;
  double tofMax = 40;
  double splashLead = 5;
  /* firstRound dispersion band [lo, hi] metres, uniform (00-config.js:53). */
  struct Band { double lo, hi; };
  Band firstRoundEasy{40, 90};
  Band firstRoundNormal{60, 150};
  Band firstRoundHard{90, 220};
  /* followUp dispersion, metres 1-sigma (00-config.js:54-55). */
  struct FollowUp { double range, defl; };
  FollowUp followUpEasy{12, 7};
  FollowUp followUpNormal{18, 10};
  FollowUp followUpHard{26, 15};
  double ffeRounds = 6;
  double ffeStagger0 = 0.25;   /* ffeStagger: [0.25, 0.9] (00-config.js:56) */
  double ffeStagger1 = 0.9;
  /* 00-config.js:67 — G3 test aid. false = every round on the aimpoint;
     such a mission is ungraded (mission.noDisp marks it permanently). */
  bool dispersion = true;
};

/* port of src/js/00-config.js:69-82 (oracle-v1) — CONFIG.MISSION. */
struct MissionTunables {
  double fratricideRadius = 80;
  double passMaxAdjustRounds = 4;
  double targetRangeLo = 1500;    /* targetRange: [1500, 3200] */
  double targetRangeHi = 3200;
  double enemyOffsetLo = 150;     /* enemyOffset: [150, 250] */
  double enemyOffsetHi = 250;
  double skirmishRangeLo = 800;   /* skirmishRange: [800, 4200] */
  double skirmishRangeHi = 4200;
  /* dangerClose: { battery: 600, mortar60: 250 } (00-config.js:81) */
  double dangerCloseBattery = 600;
  double dangerCloseMortar60 = 250;
};

/* port of src/js/00-config.js:107-145 (oracle-v1) — CONFIG.EFFECTS, the
   graded terminal-effect model (G13). Replaces the old hitsNeed/effectRadius
   pair entirely (CONTENT_INVENTORY.md §2). */
struct EffectsTunables {
  double suppressSec = 90;
  double screenRadius = 120;   /* screen: { radius: 120, sec: 90 } (:111) */
  double screenSec = 90;
  double illumBurnLo = 50;     /* illumBurn: [50, 130] (:117) */
  double illumBurnHi = 130;
  double postureStanding = 1.0;   /* posture: { standing: 1.0, prone: 0.4 } (:118) */
  double postureProne = 0.4;
  /* Per-asset band sets (:133-144): full credit inside rFull, half to rHalf,
     quarter (suppression credit) to rSupp; perRound = % contribution at full
     effect; pass thresholds per target class. */
  struct BandSet {
    double rFull, rHalf, rSupp, perRound, neutralizePct, destroyPct;
  };
  BandSet artyPersonnel{30, 50, 75, 8, 10, 30};
  BandSet artyPoint{15, 25, 45, 55, 40, 100};
  BandSet mortar60Personnel{20, 35, 50, 2, 10, 30};
  BandSet mortar60Point{10, 18, 35, 25, 40, 100};
};

/* port of src/js/00-config.js:311-314 (oracle-v1) — CONFIG.AMMO (SUGG7),
   per-session stock per asset pool, decremented per round actually fired. */
struct AmmoTunables {
  struct Stock { double he, smoke, illum; };
  Stock b155{240, 48, 36};
  Stock m60{96, 24, 24};
};

/* port of src/js/00-config.js:169-171 (oracle-v1) — CONFIG.FDC, timing keys
   only. shotDelay feeds impact scheduling (10-mission.js:804,827) and
   readbackDelay is carried for the wave-2 FDC layer. The callsign strings
   (obs/fdc/fdc60/fdcSun) are speech content and stay with the wave-2 FDC
   porter — nothing scored reads them. */
struct FdcTunables {
  double readbackDelay = 1.3;
  double shotDelay = 4.0;
};

/* port of src/js/00-config.js:214-215 (oracle-v1) — the two CONFIG.WORLD keys
   the mission machine reads for the collateral auto-fail check
   (10-mission.js:1231-1234). The rest of CONFIG.WORLD is world-gen and
   belongs to the scenario/world porters. */
struct WorldCollateralTunables {
  double hutCollateralR = 24;
  double civCollateralR = 28;
};

/* Aggregate handed to the mission machine. `seed` reuses the P1 SeedConfig
   (00-config.js:10) — seed.mission is the value CONFIG.SEED.mission holds at
   fireMission time (campaign/skirmish layers overwrite it per mission). */
struct SimTunables {
  SeedConfig seed;
  BallisticsTunables ballistics;
  MissionTunables mission;
  EffectsTunables effects;
  AmmoTunables ammo;
  FdcTunables fdc;
  WorldCollateralTunables world;
};

} }
