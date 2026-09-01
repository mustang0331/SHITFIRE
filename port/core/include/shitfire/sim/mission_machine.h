#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "shitfire/config/sim_config.h"
#include "shitfire/math/prng.h"
#include "shitfire/sim/events.h"
#include "shitfire/sim/mission_ballistics.h"
#include "shitfire/sim/mission_clock.h"
#include "shitfire/sim/mission_effects.h"
#include "shitfire/sim/mission_scenario_view.h"
#include "shitfire/sim/tunables.h"

/* The fire-mission state machine — port of src/js/10-mission.js (oracle-v1),
   sim layer only. Speech text, quip pools, TTS, the TLOG, the parser and the
   AAR live with their own porters; this machine books every consequence
   (rounds, ammo, effect, fail reasons) and reports through IMissionSink
   (events.h). Scenario/world state goes through IScenarioView
   (mission_scenario_view.h).

   NOT ported here (wave-2 reconciliation notes):
   - handlePosRep (:260-280): parser-facing, writes Scenario.posRep which the
     polar-mission resolution in 12-cff-handling reads; the scenario porter
     owns that field.
   - inferSheaf/inferFuze raw-text branches (:980-985, :1013-1019): observer
     transmission parsing — the wave-2 parser passes a requested SheafSpec /
     FuzeSpec instead. The desc-based inference IS here (sheaf cancel needs
     it, :1079).
   - noMissionReply (12-cff-handling.js:840): methods that would call it
     return false instead; the caller speaks NET4.
   - The cff-handling layer mutates Mission fields directly in the source
     (done/tEnd/bdaClaim/refined/recorded/otDirSent/corrs/eomChallenged/
     checkFire); Mission is a plain public struct here for the same reason. */
namespace shitfire { namespace sim {

enum class MethodKind { None, Grid, Polar, Shift };            /* mission.method */
enum class IntentKind { Destroy, Suppress, Illum };            /* mission.intent (G13/TEMPO4) */
enum class ImmKind { None, Suppress, Smoke };                  /* mission.imm (TEMPO2) */
enum class FailReason { None, Fratricide, Collateral };        /* mission.failReason */
enum class TgtRef { Main, Twin };                              /* mission.tgtRef (SUGG5) */
enum class Warno { Adjust, Ffe };                              /* fireMission's warno */

/* mission.sheaf / mission.fuze — {kind, source, why} (:984, :1018). */
struct SheafSpec { SheafKind kind = SheafKind::None; std::string source, why; };
struct FuzeSpec  { FuzeKind kind = FuzeKind::None; std::string source, why; };

/* The observer's sheaf request word (:981) — parallel/special canonicalise
   to linear but the ack speaks the requested word (:1083-1087). */
enum class RequestedSheaf { Converged, Circular, Open, Parallel, Linear, Special };

/* mission.mto (G11) — target number + the gist-scorer key list. */
struct MtoSpec {
  std::string tgt;
  std::vector<std::string> keys;
  bool read = false;
};

/* The meta bag fireMission accepts (:143-188). Empty strings / vectors and
   None enums stand in for the JS nulls. */
struct MissionMeta {
  std::vector<std::string> notes;
  std::string desc, gridStr;
  MethodKind method = MethodKind::None;
  IntentKind intent = IntentKind::Destroy;
  ImmKind imm = ImmKind::None;
  bool planned = false;
  bool amc = false;
  std::optional<MtoSpec> mto;
  SheafSpec sheaf;
  FuzeSpec fuze;
  ShellKind shell = ShellKind::He;
  double shotExtra = 0;
  bool fpf = false;
  std::vector<std::string> series;   /* empty = none (planSeries refuses empty) */
  std::vector<std::string> group;
};

/* port of src/js/10-mission.js:134-189 (oracle-v1) — the mission object.
   Public fields on purpose: the source's cff/AAR layers write them directly
   (see header note). */
struct Mission {
  Mission(Vec2 target, const MissionMeta& meta, double seedMission,
          double simNow, double scenarioT0,
          std::optional<double> aimErr0, TgtRef tgtRefIn, MissionPhase phase0);

  Vec2 aim;                        /* the walking aimpoint */
  double aim0x, aim0z;             /* as transmitted (AAR shot plot) */
  std::vector<Vec2> rounds, ffeRounds;
  int adjustRounds = 0;
  std::optional<double> firstMiss, lastAdjDist;
  int wrongWay = 0;
  std::vector<double> missTrace;   /* Math.round'ed miss distances */
  struct { bool longSide = false, shortSide = false; } sides;
  bool bracketed = false;
  struct {
    bool loc = false, bracketGood = false, bracket = false,
         stagnant = false, intercept = false;
  } coached;
  int wasted = 0;
  std::vector<double> corrs;       /* cff pushes round(hypot(right, add)) */
  std::vector<std::string> notes;
  std::string desc, gridStr;
  MethodKind method;
  std::optional<double> aimErr0;   /* initial target-location error */
  double tInit;                    /* time-to-initiate (JFO standard) */
  double tStart;
  std::optional<double> tEnd;
  bool done = false;
  FailReason failReason = FailReason::None;
  int hits = 0;
  IntentKind intent;
  ImmKind imm;
  bool usedIllum = false;
  bool planned;
  TgtRef tgtRef;
  std::string bdaClaim;            /* empty = null */
  SheafSpec sheaf;
  FuzeSpec fuze;
  ShellKind shell;
  bool noDisp = false;             /* G3 sticky ungraded marker */
  bool checkFire = false, usedCheckFire = false;   /* G25 */
  bool amc;                        /* G24 */
  std::function<void()> pendingFire;               /* null = none */
  bool released = false;
  std::optional<MtoSpec> mto;
  bool cannotObserve = false;
  std::optional<double> tot;
  Mulberry32 rng;                  /* stream #9: (SEED.mission*1046527+7)>>>0 (:184) */
  double shotExtra;
  bool fpf;
  std::vector<std::string> series, group;
  bool ffeDone = false;            /* :893 */
  bool failRanted = false;         /* :1211 */
  std::optional<double> convoyLead;/* SUGG6 intercept verdict (:1173) */
  MissionPhase phase;              /* mirror of the machine state (:12) */
  bool otDirSent = false;          /* set by cff (12-cff:475) */
  bool refined = false, recorded = false;          /* RREMS (12-cff:761/:767) */
  bool eomChallenged = false;      /* strict-net EOM challenge (12-cff:738) */
};

/* assessEffect()'s result (:1089-1106). */
struct EffectAssessment { EffectOutcome outcome; double pct; };

/* RECTGT entry (:338): world point + filing time. */
struct RecTgt { double x = 0, z = 0, t = 0; };

/* The FPF plan (SUGG1, :380-443): four gun points on a 180 m line, its own
   PRNG stream #10 ((SEED.mission*2246822519+13)>>>0, :437). */
struct FpfState {
  FpfState(std::uint32_t seed) : rng(seed) {}
  Vec2 pts[4];
  double az = 0;
  bool adj[4] = {false, false, false, false};
  bool frat = false;
  Mulberry32 rng;
};

/* Stream #11 (:1255-1259) — troop-dispersal visual spread. The presentation
   layer calls this with the count of VISIBLE figures in mesh order (the
   source skips invisible figures BEFORE any draw, so draws happen per
   visible figure only) and applies delta i to visible figure i:
   nx = x + dx, nz = z + dz (note +cos, a screen offset, not an azimuth). */
std::vector<Vec2> disperseOffsets(double scenarioSeed, std::size_t visibleFigures);

class MissionMachine {
public:
  MissionMachine(SimClock& clock, EventQueue& queue, IMissionSink& sink,
                 IScenarioView& view, const SimTunables& tunables,
                 const MapConfig& map, Difficulty difficulty, AssetKind asset,
                 Vec2 op, Vec2 battery, bool chapterStrict);

  /* ---- lifecycle (10-mission.js) ---- */
  void fireMission(Vec2 target, Warno warno, const MissionMeta& meta); /* :132 */
  Vec2 applyCorrection(double add, double right);                     /* :765 */
  void fireAdjustRound();                                             /* :775 */
  void fireForEffect();                                               /* :810 */

  /* ---- observer/net ops (wave-2 cff layer calls these with parsed data;
          a false return means "no mission on the net" — the caller speaks
          the NET4 noMissionReply, which is not ported here) ---- */
  bool observerFire();                                                /* :690 */
  void setAtMyCommand(bool on);                                       /* :714 */
  void doNotLoad();                                                   /* :734 */
  bool cannotObserve();                                               /* :744 */
  bool timeOnTarget(double sec);                                      /* :751 */
  void tofQuery();                                                    /* :683 */
  void unitStatusReport();                                            /* :631 */
  bool tryMtoReadback(const std::string& raw);                        /* :216 */
  void setShell(ShellKind kind);                                      /* :1055 */
  void setFuze(FuzeKind kind);                                        /* :1036 */
  void setSheaf(RequestedSheaf kind, bool cancel);                    /* :1072 */

  /* ---- planned fires (SUGG1-8) ---- */
  void priorityTarget(const std::string& tgtNum);                     /* :293 */
  void planTarget(const std::string& digits);                        /* :320 */
  void fireTarget(const std::string& tgtNum,
                  const SheafSpec& sheaf, const FuzeSpec& fuze);      /* :343 */
  void suppressTarget(const std::string& tgtNum, double minutes,
                      const SheafSpec& sheaf, const FuzeSpec& fuze);  /* :561 */
  void planSeries(const std::string& name,
                  const std::vector<std::string>& tgts);              /* :488 */
  void fireSeries(const std::string& name);                           /* :504 */
  void planGroup(const std::string& name,
                 const std::vector<std::string>& tgts);               /* :527 */
  void fireGroup(const std::string& name);                            /* :542 */
  void planFpf(Vec2 center, double dirMils, bool saidDangerClose,
               const std::string& gridDigits);                        /* :405 */
  void adjustFpf(int gun, double right, double add, bool repeat);     /* :445 */
  void fireFpf();                                                     /* :468 */

  /* ---- assessment ---- */
  EffectAssessment assessEffect() const;                              /* :1089 */
  bool missionAccomplished() const;                                   /* :1110 */
  bool coachOn() const;                                               /* :32 */

  /* ---- desc-based sheaf/fuze inference (:986-993 / :1020-1025; the
          raw-text branches stay with the parser porter) ---- */
  SheafSpec inferSheafFromDesc(const std::string& desc) const;
  FuzeSpec inferFuzeFromDesc(const std::string& desc) const;

  /* ---- state / registry access ---- */
  void setState(MissionPhase s);                                      /* :6 */
  MissionPhase state() const { return state_; }
  double stateSince() const { return stateSince_; }
  Mission* activeMission() { return active_; }
  const Mission* activeMission() const { return active_; }
  void setActiveMission(Mission* m) { active_ = m; }   /* SUGG5 tgt-number switch */
  Mission* findMission(const std::string& key);        /* MISSIONS['AA'+n] */
  std::vector<Mission*> openMissions();                               /* :122 */
  RecTgt* findRecTgt(const std::string& name);
  void putRecTgt(const std::string& name, double x, double z);   /* 12-cff:779 */
  std::vector<std::string> recTgtKeys() const;
  const std::string& priority() const { return priority_; }
  std::string nextFreeTgtNum() const;                                 /* :312 */
  FpfState* fpf() { return fpf_ ? fpf_.get() : nullptr; }

  /* ---- ammo (SUGG7) ---- */
  double ammoLeft(ShellKind shell) const;                             /* :630 */
  void ammoFull();                                                    /* :612 */

  /* ---- session wiring ---- */
  /* CONFIG.SEED.mission at fireMission time — campaign/skirmish layers set
     it per mission (15-campaign-menu.js:567). */
  void setMissionSeed(double seed) { seedMission_ = seed; }
  double missionSeed() const { return seedMission_; }
  SimTunables& tunables() { return tun_; }             /* SHIFT+D flips dispersion */
  const SimTunables& tunables() const { return tun_; }
  /* MISSIONS registry + active binding, wiped with every new scenario
     (:117-121). Planning state (RECTGT/PRIORITY/SERIES/GROUPS/FPF) survives
     scenarios and is wiped on an island change (SUGG2/SUGG4 comments). */
  void resetMissions();
  void resetPlanning();

private:
  void scheduleImpact(double t, Mission* m, std::function<void()> fn); /* :125 */
  bool armAtMyCommand(std::function<void()> fn);                      /* :658 */
  void markDispersion();                                              /* :107 */
  void resolveImpact(Vec2 impact, bool isFFE);                        /* :1118 */
  void assessFFE();                                                   /* :1352 */
  void addRoundEffect(Vec2 impact, bool isFFE);                       /* :935 */
  double missionSheafMult() const;                                    /* :995 */
  double missionFuzeMult() const;                                     /* :1027 */
  void fpfMarkRound(int g);                                           /* :381 */
  AmmoTunables::Stock& ammoPool();                                    /* :609 */
  const AmmoTunables::Stock& ammoPool() const;
  const AmmoTunables::Stock& ammoFullStock() const;
  void ammoSpend(ShellKind shell);                                    /* :619 */
  double& stockField(AmmoTunables::Stock& s, ShellKind shell) const;
  bool midMissionRefuse(MissionOp op);   /* the shared :322/:345/... gate */

  SimClock& clock_;
  EventQueue& queue_;
  IMissionSink& sink_;
  IScenarioView& view_;
  SimTunables tun_;
  MapConfig map_;
  Difficulty difficulty_;
  AssetKind asset_;
  Vec2 op_, battery_;
  bool chapterStrict_;
  double seedMission_;

  MissionPhase state_ = MissionPhase::Observing;
  double stateSince_ = 0;
  bool stateHinted_ = false;

  std::vector<std::unique_ptr<Mission>> missions_;   /* owning store */
  std::vector<std::pair<std::string, Mission*>> missionIndex_;  /* MISSIONS */
  Mission* active_ = nullptr;

  std::vector<std::pair<std::string, RecTgt>> recTgts_;   /* RECTGT, insertion order */
  std::string priority_;                                  /* PRIORITY ('' = null) */
  std::vector<std::pair<std::string, std::vector<std::string>>> series_;  /* SERIES */
  std::vector<std::pair<std::string, std::vector<std::string>>> groups_;  /* GROUPS */
  std::unique_ptr<FpfState> fpf_;                         /* FPF (null = none) */

  AmmoTunables::Stock ammoB155_, ammoM60_;                /* live books (:607) */
  std::vector<std::string> ammoWarned_;                   /* AMMO_WARNED keys */
  bool unitStatusSent_ = false;                           /* :602 */
};

} }
