#pragma once
#include <string>
#include <vector>

#include "shitfire/config/sim_config.h"
#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/doctrine/fdc_say.h"
#include "shitfire/sim/events.h"
#include "shitfire/sim/mission_machine.h"

/* The wave-2 FDC side of the events.h seam: composes the actual radio
   sentence for every FdcLineId / SysLineId the committed mission machine
   emits (each composition cites its 10-mission.js call site) and speaks it
   through the shared FdcSpeaker chain.

   Sink contract honored (events.h header):
   - sayLine returns the absolute air time from the FDC.say chain.
   - gateProbability in (0,1) is decided HERE with the sink's own flavor
     coin (harness-seeded, never a mission stream); when the coin fails the
     lastT chain is left untouched and no quip pick() is drawn (in the source
     the gated FDC.say — and its pick() — is simply never called,
     10-mission.js:824/:902).
   - RantFratricide / RantCollateral play the 3-line rant at delays
     1.0/2.2/2.8 (10-mission.js:1213-1215).
   - Nothing here throws.

   Payload gaps found while composing (machine header is committed, so noted
   rather than fixed):
   - SysLineId::SpottingEasy loses the over/short spotting word when a
     bracket is held (text=="bracketed", 10-mission.js:1335 prints BOTH).
     Composed here with the word BRACKETED in the header slot; text-only,
     never diffed. A num field carrying overShort's sign would restore it.
   - FdcLineId::NoFpfPlanned covers two texts (:447 adjust vs :470 fire);
     FdcLineId::TargetPlanned / SeriesEstablished / GroupEstablished / the
     NoSuchTarget* family need words from the observer transmission that are
     not in the payload (target number, grid digits, planned target list).
     Both are resolved via the compose context below, set by the FdcEngine
     for the duration of each dispatched transmission. */
namespace shitfire { namespace doctrine {

class FdcSpotReports;   /* fdc_spot.h */

/* Fields of the observer transmission currently being handled, for the
   compositions whose payload does not carry every word the JS template
   printed. Set/cleared by FdcEngine::submitMessage around dispatch; null
   whenever the machine speaks outside a player transmission (timer-driven
   impacts, ammo amber, ...), where none of the ids below can occur. */
struct FdcComposeContext {
  std::string ptype;               /* p.type — NoFpfPlanned variant pick */
  std::string tgtNum;              /* p.tgtNum */
  std::string digits;              /* p.digits — TargetPlanned grid echo */
  std::string name;                /* p.name */
  std::vector<std::string> tgts;   /* p.tgts — series/group readback list */
};

class FdcMissionSink : public sim::IMissionSink {
public:
  FdcMissionSink(FdcSpeaker& speaker, QuipSet& quips, IFlavorRng& flavor,
                 INetSink& net, const FdcChapterContext& chapter,
                 const FdcCallsigns& calls, const MapConfig& map,
                 sim::Vec2 battery);

  /* The machine is constructed WITH this sink, so the back-pointer (used by
     compositions that read live state: PartialVolley's shell name,
     PlanMissingTargets' known-target list) is wired afterwards. */
  void setMachine(sim::MissionMachine* machine) { machine_ = machine; }
  void setSpot(FdcSpotReports* spot) { spot_ = spot; }
  /* Optional fan-out for the non-speech events (impact -> TLOG,
     presentation -> renderer, stateChanged -> HUD). Null = dropped, which is
     the headless-replay mode events.h names. */
  void setTail(sim::IMissionSink* tail) { tail_ = tail; }
  void setContext(const FdcComposeContext* ctx) { ctx_ = ctx; }

  double sayLine(const sim::FdcLine& line) override;
  void sysLine(const sim::SysLine& line) override;
  void stateChanged(sim::MissionPhase phase, double simNow) override;
  void impact(const sim::ImpactEvent& ev) override;
  void presentation(const sim::PresentationEvent& ev) override;
  void spotReportRequested(sim::SpotReportReason reason) override;

private:
  std::string composeFdc(const sim::FdcLine& line);
  std::string composeSys(const sim::SysLine& line) const;

  FdcSpeaker& speaker_;
  QuipSet& quips_;
  IFlavorRng& flavor_;
  INetSink& net_;
  const FdcChapterContext& chapter_;
  FdcCallsigns calls_;
  MapConfig map_;
  sim::Vec2 battery_;
  sim::MissionMachine* machine_ = nullptr;
  FdcSpotReports* spot_ = nullptr;
  sim::IMissionSink* tail_ = nullptr;
  const FdcComposeContext* ctx_ = nullptr;
};

/* Shell / fuze display names shared with fdc_rules.cpp
   ('he'|'smoke'|'illum', 'pd'|'vt'|'time'|'delay'). */
std::string shellWord(sim::ShellKind k);
std::string fuzeWord(sim::FuzeKind k);

} }
