#pragma once
#include <optional>
#include <string>
#include <vector>
#include "shitfire/config/sim_config.h"
#include "shitfire/math/grid.h"
#include "shitfire/math/prng.h"
#include "shitfire/scenario/world_config.h"
#include "shitfire/scenario/world_layout.h"

/* port of src/js/05-units-scenario.js (oracle-v1) — genScenario for all 15
   types, the path helpers, and the state-mutating slice of updateScenario.
   Unit/mesh installation (placeUnits, stream 05:1061) is engine/wave-2 side.

   The ScenarioState mirrors the JS `S` object 1:1: a field is only "present"
   (has_value / flag true) when the JS generator for that type set the key, so
   the fixture snapshots (functions dropped, null kept) compare key-for-key.
   The scenario PRNG (05:225) travels in the struct — later draws (qfp H-hour
   install, battery volleys) continue the same stream. */
namespace shitfire { namespace scenario {

struct Kp { std::string id, name; double x = 0, z = 0; };          /* 05:784 */
struct FriendlyPos { double x = 0, z = 0, r = 0; };

/* WORLD3 path (05:84): legacy line { sx, sz, dx, dz, len } or road polyline
   { pts, cum, len }. */
struct PathState {
  bool isLine = true;
  double sx = 0, sz = 0, dx = 0, dz = 0;
  std::vector<WorldXZ> pts;
  std::vector<double> cum;
  double len = 0;
};

struct Veh { double off = 0; bool dead = false; double d = 0, x = 0, z = 0; };
struct StopState {
  double d = 0, dur = 0;
  std::optional<double> tArr;      /* null until the halt */
  bool resumed = false;
  std::string name;
};
struct BtyState { double next = 0, period = 0, tof = 0; };
struct TwinTgt {
  double x = 0, z = 0, eff = 0;
  bool everSuppressed = false;
  double suppressedUntil = 0;
  bool alive = true;
};
struct CallerState {
  double i = 0;
  std::string wrong;               /* 'clean' | 'dirflip' | 'dist' */
  double repAz = 0, repDist = 0;
  bool saidThanks = false, saidDead = false;
};
struct OccupiedState {
  std::string kind, name;          /* 'facility' | 'village' */
  double x = 0, z = 0;
  bool hasRing = false;            /* village mode carries ring[3] */
  std::vector<WorldXZ> ring;
};

struct ScenarioState {
  /* 05:233-236 — every scenario */
  double seed = 0;
  std::string type, difficulty;
  Mulberry32 rng{0};
  std::vector<Kp> kps;
  std::vector<FriendlyPos> friendlies;
  std::string tgtClass = "personnel";
  std::string posture = "standing";
  double effScale = 1, eff = 0;
  bool everSuppressed = false;
  double suppressedUntil = 0;
  double alerted = 0, dispersed = 0;
  WorldXZ enemy{0, 0};
  std::string brief;

  /* per-type keys; presence mirrors the JS key set exactly */
  std::optional<WorldXZ> compound;             /* strongpoint */
  std::optional<WorldXZ> fireBase;             /* battery, qfp */
  std::optional<WorldXZ> team;                 /* callin */
  std::optional<WorldXZ> bbq;                  /* chow */
  std::optional<WorldXZ> fStart;               /* assault */
  bool hasVillage = false;                     /* raid/occupied/kaiju set S.village */
  std::optional<Village> village;              /* nullopt while hasVillage = JS null */
  std::optional<PathState> path;               /* convoy, kaiju */
  std::optional<double> speed, t0;
  std::optional<bool> escaped;
  std::optional<std::vector<Veh>> veh;
  std::optional<StopState> stop;
  std::optional<BtyState> bty;
  std::optional<double> btyCas;
  std::optional<double> aSpeed;
  bool hasLastT = false;                       /* defense/qfp: key exists (null) */
  std::optional<double> lastT;                 /* value once set */
  std::optional<bool> overrun;
  std::optional<TwinTgt> twinTgt;
  bool hasAvenues = false;                     /* qfp */
  std::vector<WorldXZ> avenues;
  std::optional<double> assaultAv, axisAz, hHour;
  std::optional<bool> hStarted;
  std::optional<CallerState> caller;
  std::optional<OccupiedState> occupied;
  std::optional<bool> ashore;                  /* kaiju */
  std::optional<double> armor;
  std::optional<double> fAdvAz, fSpeed, fHold, fAdvMax, ft0;   /* assault */
};

/* activeChapter.scn options genScenario reads (05:237,316,534,756,772).
   effR = 0 means the key is absent. */
struct ScnOpts {
  bool mask = false, fClose = false, lanes = false;
  double effR = 0;
};

/* Everything genScenario reads from globals in the browser. */
struct GenContext {
  const HeightField* hf = nullptr;
  const WorldLayout* world = nullptr;
  OpPos op;
  EyePos eye;                       /* eyeFor(op, cam) */
  MissionConfig mission;
  WorldConfig worldCfg;
  MapConfig map;
  double islandRadius = 3900;       /* CONFIG.TERRAIN.islandRadius (spread draw 05:257) */
  double terrainSeed = 1337;        /* CONFIG.SEED.terrain — mixed into the stream seed */
  std::string difficulty = "normal";
  bool chapterActive = false;       /* spread = !activeChapter (05:247) */
  ScnOpts opts;
  double now = 0;                   /* sim.now at generation (convoy/kaiju t0, assault ft0) */
};

/* 05:224 — S.rng seeded (seed * 2654435761 ^ CONFIG.SEED.terrain) >>> 0.
   A scenario only reproduces on its island. PRNG call order is source order
   throughout; std::stable_sort only; no unordered iteration feeds placement. */
ScenarioState genScenario(const std::string& type, double seed, const GenContext& ctx);

/* 05:88 / 05:97 — path sampling. pathDir returns {dx,dz} in x/z. */
WorldXZ pathPoint(const PathState& p, double d);
WorldXZ pathDir(const PathState& p, double d);

/* wave-3 reconciliation: the live-mission side of updateScenario that the
   generation-time tick could not carry (its enemyAlive was hardcoded true
   and no caller could see a latch fire). The facade passes one of these per
   frame; null keeps the committed generation-time behavior byte-for-byte
   (level-2 tests pass null).
   - enemyAlive IN: the mission-owned global (05:897/:953/:992 gates; kaiju
     05:870 branches on it).
   - out-flags fire on the tick a latch/branch crosses; the caller owns the
     consequences the browser put next to them (mission fail bookkeeping
     05:857-865/:882-889/:972-982/:1012-1019, enemyVolley 05:903,
     H-hour install mesh + log 05:928-940, caller voice lines 05:918-919).
   - the qfp H-hour S.rng draws (3 per figure x 8, 05:930-937) ARE taken
     here when io is non-null — the flagged wave-2 seam closes at the exact
     source position. */
struct ScenarioTickIO {
  bool enemyAlive = true;      /* IN */
  bool volleyFired = false;    /* battery: enemyVolley(S) fired (05:903) */
  bool hHourStarted = false;   /* qfp: H-hour crossed this tick (05:927) */
  bool escapedNow = false;     /* convoy: escape latch crossed (05:857) */
  bool ashoreNow = false;      /* kaiju: landfall latch crossed (05:882) */
  bool overrunNow = false;     /* defense/qfp: wire latch crossed (05:972/:1012) */
  bool callerSaidDead = false; /* callin: saidDead latched this tick (05:918) */
  bool callerSaidSup = false;  /* callin: saidThanks latched this tick (05:919) */
};

/* The state-mutating slice of updateScenario (05:815): convoy piecewise
   time->distance + vehicle/enemy positions + escape latch, kaiju wade +
   ashore latch, defense/qfp advance (lastT, enemy toward objective, overrun
   latch), qfp H-hour trigger (S.rng install draws taken when io is passed —
   see ScenarioTickIO). battery volley timing updates bty.next; the fire
   branch reports through io->volleyFired — enemyVolley's gauss draws +
   scheduling live with the mission-side caller (10-mission.js gauss,
   05:1028). Mesh-only branches (assault squad, callin figures) have no
   state to mutate here.
   scenarioT0 = sim.now at mission start (15-campaign-menu.js:574). */
void updateScenarioTick(ScenarioState& S, double simNow, double scenarioT0,
                        const GenContext& ctx, ScenarioTickIO* io = nullptr);

/* 04-world.js:1135 updateCivilians, position math only (mesh skipped):
   x = bx + sin(t*0.14 + phase) * wr, z = bz + cos(t*0.11 + phase*1.7) * wr. */
void updateCivilians(std::vector<Civ>& civs, double t);

} }
