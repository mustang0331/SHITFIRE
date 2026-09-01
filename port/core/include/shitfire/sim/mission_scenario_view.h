#pragma once
#include <cstddef>
#include <vector>

/* Sim-side view of the scenario/world state the mission machine reads and
   mutates (10-mission.js call sites cited per member). The SCENARIO porter's
   ScenarioState (scenario/ namespace) was not yet in the tree when this seam
   was cut, so this is the MINIMAL interface the mission code needs; wave-2
   reconciliation: the scenario side implements IScenarioView over its own
   state (or we adapt). Nothing here is ported scenario code — it is the
   boundary where 10-mission.js touched Scenario/WORLD/units globals.

   Contract notes:
   - Implementations must not throw (core is exceptions-off).
   - friendlies()/civilians()/huts() return snapshots in the source's
     iteration order (friendlyPositions() order; WORLD.civs order;
     villages-then-huts order) — the fratricide loop `break`s on the first
     violation (:1225-1228), so order is observable.
   - Mutators exist because 10-mission.js writes these scenario fields
     directly; the scenario side owns any visual consequence (e.g. dead
     vehicle material swap, :1184). */
namespace shitfire { namespace sim {

struct Vec2 { double x = 0, z = 0; };
struct Vec3 { double x = 0, y = 0, z = 0; };
struct Dir2 { double dx = 0, dz = 0; };

/* S.tgtClass: 'personnel' | 'point' (effBands, :930). */
enum class TargetClass { Personnel, Point };
/* S.posture: 'standing' | 'prone' (EFFECTS.posture lookup, :961-962). */
enum class Posture { Standing, Prone };

/* SCN_TYPES (00-config.js:323-346) — the mission machine branches on a
   handful of these (:937 convoy, :1091, :1113 kaiju, :1248 troops/raid/chow,
   :1382-1385, :669 convoy intercept coach). */
enum class ScenarioType {
  Strongpoint, Troops, Bunker, Convoy, Assault, Raid, Wreck,
  Battery, Defense, Callin, Occupied, Qfp, Twin, Chow, Kaiju,
};

/* friendlyPositions() element (05-units-scenario.js:792): {x, z} with an
   optional per-point radius override — `f.r || CONFIG.MISSION.fratricideRadius`
   (:1224, :392). hasR=false means fall back to the config radius. */
struct FriendlyPos { double x = 0, z = 0; double r = 0; bool hasR = false; };

/* Convoy vehicle view (S.veh[i]): position, path distance d, dead flag. */
struct VehicleView { double x = 0, z = 0; double d = 0; bool dead = false; };

class IScenarioView {
public:
  virtual ~IScenarioView() = default;

  /* ---- reads ---- */
  virtual ScenarioType type() const = 0;              /* S.type */
  virtual double scenarioSeed() const = 0;            /* S.seed (:1255) */
  virtual double scenarioT0() const = 0;              /* 05-units:82 (:152) */
  virtual Vec2 enemy() const = 0;                     /* S.enemy */
  virtual bool hasEnemy() const = 0;                  /* :148 `Scenario.enemy ?` */
  virtual bool hasTwin() const = 0;                   /* S.twinTgt (:943) */
  virtual Vec2 twin() const = 0;
  virtual TargetClass targetClass() const = 0;        /* S.tgtClass */
  virtual double effScale() const = 0;                /* S.effScale; <=0 = unset
                                                         (JS falsy -> 1, :931) */
  virtual double armor() const = 0;                   /* S.armor; <=0 = unset
                                                         (JS `|| 1`, :968) */
  virtual Posture posture() const = 0;                /* S.posture */
  virtual bool dispersed() const = 0;                 /* S.dispersed (:1248) */
  virtual int alerted() const = 0;                    /* S.alerted */
  virtual bool enemyAlive() const = 0;                /* global enemyAlive */
  virtual bool twinAlive() const = 0;                 /* S.twinTgt.alive */
  virtual double eff() const = 0;                     /* S.eff books */
  virtual double twinEff() const = 0;                 /* S.twinTgt.eff */
  virtual bool everSuppressed() const = 0;            /* S.everSuppressed */
  virtual bool twinEverSuppressed() const = 0;
  virtual std::vector<FriendlyPos> friendlies() const = 0;  /* friendlyPositions() */
  virtual std::vector<Vec2> civilians() const = 0;    /* WORLD.civs (:1233) */
  virtual std::vector<Vec2> huts() const = 0;         /* villages[].huts, village
                                                         order then hut order (:1234) */
  virtual double groundHeight(double x, double z) const = 0;  /* H(x, z) */
  virtual bool hasLOS(double ax, double ay, double az,
                      double bx, double by, double bz) const = 0; /* 01-math:97 */
  virtual Vec3 observerEye() const = 0;               /* eye.x/y/z (:1311) */

  /* ---- convoy (S.veh / S.path) ---- */
  virtual std::size_t vehicleCount() const = 0;
  virtual VehicleView vehicle(std::size_t i) const = 0;
  virtual bool convoyHasPath() const = 0;             /* Scenario.path truthy (:839) */
  virtual Dir2 pathDirAt(double d) const = 0;         /* pathDir(S.path, d) */
  virtual double convoyHeadD() const = 0;             /* convoyHeadD() (:843) */

  /* ---- writes (10-mission.js mutations of scenario state) ---- */
  virtual void addEff(double contrib, bool twinBooks) = 0;      /* :969 */
  /* Round-path suppression: everSuppressed = true;
     suppressedUntil = until (ASSIGNMENT — :953-958). */
  virtual void setSuppressed(double until, bool twinBooks) = 0;
  /* Screen-path suppression: everSuppressed = true;
     suppressedUntil = max(suppressedUntil, until) (08-burst-optics.js:476-477). */
  virtual void raiseSuppressedTo(double until) = 0;
  virtual void bumpAlerted() = 0;                     /* S.alerted++ (:1249) */
  virtual void setDispersed() = 0;                    /* S.dispersed = 1 (:1254) */
  virtual void setPostureProne() = 0;                 /* S.posture = 'prone' (:1267) */
  virtual void setEnemyAlive(bool alive) = 0;         /* enemyAlive = false */
  virtual void setTwinAlive(bool alive) = 0;          /* S.twinTgt.alive = false */
  virtual void killVehicle(std::size_t i) = 0;        /* v.dead = true (:1183) */
};

} }
