#pragma once
#include <cstddef>
#include <string>

#include "shitfire/config/sim_config.h"
#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/doctrine/fdc_say.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_layout.h"

/* SALUTE spot reports + the scripted NET1 callers — port of
   src/js/07-voice-spot-audio.js:299-561 (oracle-v1), the transferable half
   of that file. Everything SpeechSynthesis/WebAudio in it stays behind.

   Determinism: the spot-report CONTENT draws come from PRNG stream #8
   (DETERMINISM_NOTE.md §c), seeded
     (S.seed * 30011 + (reason === 'displace' ? 977 : 7)) >>> 0   (07:449)
   — two sub-streams by reason. Draw order is source order: src, minutes,
   salute-coin, [unit] (07:460-467). Only the spotTail quip (07:475) and the
   caller ack line (07:556) use the injected flavor RNG. */
namespace shitfire { namespace doctrine {

/* port of src/js/00-config.js:179 (oracle-v1) — CONFIG.SPOT. */
struct SpotTunables {
  bool enabled = true;
  double lead = 2.4;
  double gap = 2.9;
  double saluteChance = 0.7;
  double landmarkMax = 2600;
};

enum class SpotReason { Start, Displace };   /* 07:442 'start' | 'displace' */
enum class CallerCue { Sup, Dead };          /* callerSay('sup'/'dead') 07:558 */

class FdcSpotReports {
public:
  FdcSpotReports(sim::SimClock& clock, sim::EventQueue& queue,
                 FdcSpeaker& speaker, INetSink& net, QuipSet& quips,
                 IFlavorRng& flavor, const FdcChapterContext& chapter,
                 const FdcCallsigns& calls, const SpotTunables& spot,
                 const MapConfig& map);

  /* Scenario/world binding (the browser reads Scenario/WORLD/OP globals).
     Rebind on every new scenario; null scn disables reports. */
  void bind(const scenario::ScenarioState* scn,
            const scenario::WorldLayout* world, const scenario::OpPos& op);

  /* 07:443-478 — sendSpotReport(reason). Start comes from the campaign layer
     at mission start; Displace arrives via the mission machine's
     spotReportRequested (10-mission.js:1271 -> FdcMissionSink). */
  void sendSpotReport(SpotReason reason);

  /* NET1 rock-eater callers (07:484-561). */
  void callerKickoff();                        /* 07:541 */
  void callerReply(const std::string& raw);    /* 07:551 — original raw text */
  void callerSay(CallerCue cue);               /* 07:558 */
  /* First word of CALLERS[i].cs, lowercased — the onPlayerMessage intercept
     test (12-cff-handling.js:927). */
  std::string callerFirstWordLower(std::size_t i) const;

  /* 07:376-402 — spotLocation(x, z, diff), exposed for unit tests (pure
     given the bound world). */
  std::string spotLocation(double x, double z,
                           const std::string& diff) const;

private:
  struct Landmark { std::string name; double x, z, d; };
  bool nearestLandmark(double x, double z, Landmark& out) const; /* 07:351 */
  void callerLine(const std::string& cs, const std::string& msg); /* 07:526 */
  std::string callerLocLine() const;                              /* 07:533 */

  sim::SimClock& clock_;
  sim::EventQueue& queue_;
  FdcSpeaker& speaker_;
  INetSink& net_;
  QuipSet& quips_;
  IFlavorRng& flavor_;
  const FdcChapterContext& chapter_;
  FdcCallsigns calls_;
  SpotTunables spot_;
  MapConfig map_;
  const scenario::ScenarioState* scn_ = nullptr;
  const scenario::WorldLayout* world_ = nullptr;
  scenario::OpPos op_;
};

} }
