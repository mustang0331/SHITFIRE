#pragma once
#include <string>

/* The event-sink seam between the wave-1 mission machine and the wave-2
   FDC / SimCore layers (P2 assembly plan). The mission machine BOOKS every
   consequence itself (rounds, ammo, effect, fail reasons — the GFX2
   "model first, show second" contract, 10-mission.js:1136-1147) and reports
   outward only through this interface. The wave-2 FDC rules layer composes
   the actual radio sentences from FdcLineId + payload; the wave-2 SimCore /
   renderer consumes presentation events; the TLOG (campaign porter) records
   ImpactEvents.

   Sink contract (hard rules):
   - No sink implementation may throw: core compiles exceptions-off.
   - sayLine must implement the FDC.say timing chain VERBATIM
     (06-clock-comms.js:200-233): t = max(sim.now, lastT) + delay;
     lastT = t; deliver at t; RETURN t. The return value is load-bearing —
     fireAdjustRound/fireForEffect schedule impacts at tShot + tof
     (10-mission.js:804-807, 827/884), so the chain state (lastT) lives in
     the FDC layer, shared with every other line it speaks.
   - Lines whose PRESENCE is probability-gated flavor carry gateProbability
     in (0,1); the sink owns that coin (harness-seeded Math.random — a
     SANCTIONED FLAVOR site per DETERMINISM_NOTE.md §a, e.g. the 40% ffeAck
     gate at 10-mission.js:824). The mission PRNG streams are never used for
     it. gateProbability == 0 means the line always airs. When the coin
     FAILS the sink must leave the lastT chain untouched (in the source the
     gated FDC.say is simply never called) — the returned time is then
     meaningless and no machine path reads it. */
namespace shitfire { namespace sim {

/* The setState() vocabulary (grep of setState('...') across src/js at
   oracle-v1). The mission machine emits the subset it sets in
   10-mission.js (MissionSent, AtMyCommand, Shot, Adjusting, FireForEffect,
   EomQuery, DoNotLoad, Observing); CheckFiring / CeaseLoading / Aar are set
   by the wave-2 cff-handling / AAR layers through the same enum. */
enum class MissionPhase {
  Observing,       /* 'OBSERVING' */
  MissionSent,     /* 'MISSION SENT' */
  AtMyCommand,     /* 'AT MY COMMAND' */
  Shot,            /* 'SHOT' */
  Adjusting,       /* 'ADJUSTING' */
  FireForEffect,   /* 'FIRE FOR EFFECT' */
  EomQuery,        /* 'EOM?' */
  DoNotLoad,       /* 'DO NOT LOAD' */
  CheckFiring,     /* 'CHECK FIRING' (12-cff-handling.js:438) */
  CeaseLoading,    /* 'CEASE LOADING' (12-cff-handling.js:454) */
  Aar,             /* 'AAR' (13-aar.js:377) */
};

/* One id per FDC.say call site ported in the wave-1 mission scope, cited to
   its 10-mission.js line. The FDC layer owns the words (including the
   HELLHOUND->HACKSAW/SUNLAMP callsign swap at delivery time,
   06-clock-comms.js:218-225, and the sunlamp SHOT/SPLASH prowords,
   10-mission.js:68-69) and the quip-pool pick() calls. */
enum class FdcLineId {
  ShotOver,                 /* :804 / :827 — SHOT (or DISCHARGE on sunlamp) */
  SplashOver,               /* :805 / :828 — SPLASH (or SOLAR EVENT) */
  ReadyOver,                /* :663 — AMC: guns laid, READY, OVER */
  CheckFiringRefusal,       /* :781 / :813 — refuse new rounds under the hold */
  CheckFiringRefusalFire,   /* :698 — FIRE sent while check firing */
  BlackOnShell,             /* :789 / :818 — text=shell */
  AmmoAmber,                /* :627 — num0=rounds remaining, text=shell */
  PartialVolley,            /* :867 — num0=rounds available */
  RoundsComplete,           /* :895 */
  FfeAckFlavor,             /* :824 — QUIPS.ffeAck, gateProbability 0.4 */
  CompleteTailFlavor,       /* :902 — QUIPS.completeTail, gateProbability 0.5 */
  CompletePrompt,           /* :903 — QUIPS.complete */
  SuppressedOnly,           /* :905 — QUIPS.suppressedOnly */
  NoEffect,                 /* :910 — QUIPS.noEffect */
  RantFratricide,           /* :1213-1215 — FDC layer plays the 3-line rant
                               (pick(QUIPS.rantFrat)) at delays 1.0/2.2/2.8 */
  RantFratricideRepeat,     /* :1226 — one grim line on repeat hits */
  RantCollateral,           /* :1213-1215 via failHit with QUIPS.rantCiv */
  RantCollateralRepeat,     /* :1237 */
  WrongWayQuip,             /* :1243 — QUIPS.wrongWay */
  CoachLoc,                 /* :1290 — num0=round(dTgt), {MISS} substitution */
  CoachBracketGood,         /* :1293 */
  CoachBracket,             /* :1296 */
  CoachStagnant,            /* :1299 */
  DisperseAdvise,           /* :1273 — "they are dispersing" coach line */
  TofQueryNoMission,        /* :685 */
  TimeOfFlight,             /* :688 — num0=round(tof) */
  FireOut,                  /* :711 — FIRE, OUT */
  FireNothingLaid,          /* :703 — AMC on, nothing pending */
  FireNotAtMyCommand,       /* :704 — AMC off */
  AmcAdviceNoMission,       /* :718 — AT MY COMMAND with no mission */
  AmcCancelNoMission,       /* :719 */
  AmcOut,                   /* :723 (on) */
  CancelAmcOut,             /* :723 (off) */
  DoNotLoadIdle,            /* :735 — no mission */
  DoNotLoadOut,             /* :741 */
  CannotObserveOut,         /* :748 */
  TimeOnTargetOut,          /* :754 — num0=seconds */
  MtoWrongTargetNumber,     /* :222 — text=correct target number */
  MtoNumberOnly,            /* :229 */
  MtoReadbackCorrect,       /* :235 — verbatim readback */
  MtoReadbackClose,         /* :236 — gist readback */
  NoSuchTargetPriority,     /* :297 — text=known numbers, comma-joined */
  PrioritySet,              /* :304 — text=target number, num0=1 if shifted */
  MidMissionRefusal,        /* :322/:345/:407/:473/:513/:551/:563 —
                               num0=(double)MissionOp of the refused action */
  PlanNeedsGrid,            /* :327 */
  OffMapSheet,              /* :334 */
  TargetPlanned,            /* :339 — text=target number, num0=digit count */
  NoSuchTargetFire,         /* :351 — text=known numbers */
  StandbyRelay,             /* :358 — re-laying onto a non-priority target */
  FireTargetOut,            /* :359 — text=target number, num0=1 if priority */
  NoSuchTargetSuppress,     /* :569 — text=known numbers */
  SuppressTargetOut,        /* :578 — text=target number, num0=minutes (0=none) */
  SeriesNeedsTargets,       /* :490 */
  PlanMissingTargets,       /* :496 / :535 — text=missing numbers */
  SeriesEstablished,        /* :501 — text=name, num0=count */
  NoSuchSeries,             /* :508 — text=known names */
  FiringSeries,             /* :516 — text=name, num0=count */
  GroupNeedsTwo,            /* :529 */
  GroupEstablished,         /* :540 — text=name, num0=count */
  NoSuchGroup,              /* :546 — text=known names */
  FiringGroup,              /* :554 — text=name */
  NoFpfPlanned,             /* :447 / :470 */
  FourGunsOnly,             /* :451 — num0=gun number sent */
  FpfNumberAck,             /* :464 — num0=gun, num1=right, num2=add (repeat
                               when num1==num2==0 and text=="repeat") */
  FpfDangerCloseQuip,       /* :430 — QUIPS.dangerClose */
  FpfEstablished,           /* :438 — text=center grid digits,
                               num0=attitude mils, num1=1 if danger close */
  FpfMarkOnFriendlies,      /* :396 — CHECK FIRE, marking round on friendlies */
  FiringFpf,                /* :477 */
  UnitStatusReport,         /* :640-644 — num0=he, num1=smoke, num2=illum,
                               text="repeat" when asked again (SUGG7) */
  ShellNoMission,           /* :1057 — shell named with no mission */
  ShellOut,                 /* :1069 — text=shell name */
  FuzeNoMission,            /* :1038 */
  FuzeVtDangerCloseRefusal, /* :1045 — VT refused danger close, PD retained */
  FuzeOut,                  /* :1049 — text=fuze name */
  SheafNoMission,           /* :1074 */
  SheafCancelOut,           /* :1080 — text=cancelled kind, then the
                               reverted-to kind in text after a '|' */
  SheafOut,                 /* :1087 — text=requested kind name */
};

/* The mission ops MidMissionRefusal can name (which handler refused). */
enum class MissionOp {
  PlanTarget, FireTarget, PlanFpf, FireFpf, FireSeries, FireGroup,
  SuppressTarget,
};

struct FdcLine {
  FdcLineId id;
  double delay = 0.9;         /* seconds after the previous FDC line */
  double gateProbability = 0; /* 0 = always; else sanctioned-flavor gate */
  double num0 = 0, num1 = 0, num2 = 0;
  std::string text;
};

/* System/coach lines (log(..., 'sys') call sites in the ported scope). Text
   composition is wave-2; payload carries every number the sentence needs. */
enum class SysLineId {
  InterceptDrill,           /* :673 — num0=round(tof), num1=round(shotDelay),
                               num2=lead */
  CancelAmcReverting,       /* :730 */
  DoNotLoadGuidance,        /* :742 */
  PriorityHint,             /* :306 — text=target number */
  PlanTargetHint,           /* :340 — text=target number */
  FpfInstructions,          /* :441 */
  MarkingRoundCompromised,  /* :394 */
  SuppressNoDuration,       /* :581 — text=target number */
  TimeOnTargetFormatNote,   /* :759 — num0=seconds */
  SuppressedOnlyGuidance,   /* :906 */
  NoEffectGuidance,         /* :911 */
  FailureRecordedGuidance,  /* :1216 */
  AlertedMoving,            /* :1251 */
  DispersedProne,           /* :1268 */
  SpottingMasked,           /* :1314 */
  OtDirReminderMasked,      /* :1319 */
  SpottingNormal,           /* :1326 — num0=mils (signed), num1=otf,
                               num2=otRange; text="over"/"short" spotting */
  SpottingEasy,             /* :1335 — num0=mils (signed), num1=otf,
                               num2=devCorr, num3=otRange; text="bracketed"
                               when a bracket is held (:1332), else
                               "over"/"short" spotting */
  OtDirReminder,            /* :1343 */
};

struct SysLine {
  SysLineId id;
  double num0 = 0, num1 = 0, num2 = 0, num3 = 0;
  std::string text;
};

/* 12h/10-mission shell natures (mission.shell: 'he' | 'smoke' | 'illum'). */
enum class ShellKind { He, Smoke, Illum };

/* One impact TLOG entry (10-mission.js:1130-1132 / :1154-1156). The TLOG
   layer applies its own t-rounding; dTgt is already Math.round()ed here as
   the source does. */
struct ImpactEvent {
  double x = 0, z = 0, y = 0;    /* world impact point; y = burst height used */
  ShellKind shell = ShellKind::He;
  bool isFFE = false;
  double dTgtRounded = 0;        /* Math.round(dist2(impact, enemy)) */
  double adjustRound = 0;        /* mission.adjustRounds at booking (adjust rounds) */
};

/* Presentation-only happenings (spawnBurst / deployScreen / igniteIllum /
   fireBeam / chargeWhine / unit-visual changes). Booked state never depends
   on these — the sink may drop them entirely (headless replay does). */
enum class PresentationKind {
  Burst,            /* :1162 spawnBurst(x, y, z) */
  SmokeScreen,      /* :1160 deployScreen visual (scored part booked first) */
  IllumFlare,       /* 08-burst-optics.js:514-526 — num0=burn seconds;
                       y = max(H,0)+330 */
  SunBeam,          /* :1161 fireBeam (sunlamp) */
  ChargeWhine,      /* :806/:829 chargeWhine(tof) — num0=tof */
  TroopsDown,       /* :1379 main-element figures fall */
  TwinTroopsDown,   /* :1372 twin-element figures (indices 4-7) fall */
  VehiclesTipped,   /* :1383 strongpoint vehicles at DESTROYED */
  BunkerScorched,   /* :1384 */
  KaijuScorched,    /* :1385 */
  DisperseTroops,   /* :1256-1262 — visual spread; the layout stream is
                       disperseOffsets() (mission_machine.h), drawn by the
                       presentation layer per VISIBLE figure in mesh order */
};

struct PresentationEvent {
  PresentationKind kind;
  double x = 0, y = 0, z = 0;
  double num0 = 0;
};

/* sendSpotReport reasons the mission machine raises (:1271). The spot-report
   content stream is 07-voice-spot-audio.js's own seeded stream (#8 in
   DETERMINISM_NOTE.md §c) — owned by its porter, not consumed here. */
enum class SpotReportReason { Displace };

class IMissionSink {
public:
  virtual ~IMissionSink() = default;
  /* FDC.say — returns the absolute sim time the line will air (see header
     comment for the verbatim timing contract). */
  virtual double sayLine(const FdcLine& line) = 0;
  virtual void sysLine(const SysLine& line) = 0;
  virtual void stateChanged(MissionPhase phase, double simNow) = 0;
  virtual void impact(const ImpactEvent& ev) = 0;
  virtual void presentation(const PresentationEvent& ev) = 0;
  virtual void spotReportRequested(SpotReportReason reason) = 0;
};

} }
