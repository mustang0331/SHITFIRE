#pragma once
#include <memory>
#include <string>
#include <vector>

#include "shitfire/sim/events.h"

/* SimCore — the wave-3 facade that assembles the ported units (terrain,
   world, scenario, mission machine, FDC engine, spot reports, campaign
   grading, TLOG) into the one object the P3 Unreal bridge consumes. Thin
   assembly only: every rule lives where its porter put it; this class owns
   wiring, the frame loop slice of 17-loop-boot.js:67-186, and the
   newMission flow of 15-campaign-menu.js:523-651.

   Engine-free: standard library only, no exceptions, no I/O. The clock is
   integer milliseconds advanced in 16 ms frames — the browser's rAF grid
   under the capture harness (ENGINE_PORT/fixtures/MANIFEST.json clock
   note); sim.now = ms / 1000.0 exactly as tMs / 1000 (17-loop-boot.js:69). */
namespace shitfire {

class HeightField;
namespace review { class TranscriptLog; }
namespace doctrine {
class IFlavorRng;
class FdcEngine;
class FdcSpeaker;
struct QuipSet;
}
namespace sim { class MissionMachine; struct SimClock; class EventQueue; }
namespace scenario {
struct ScenarioState;
struct WorldLayout;
struct UnitLayout;
struct OpPos;
struct BatteryPos;
}

/* One mission launch (newMission, 15-campaign-menu.js:523). Exactly the
   metadata the oracle transcripts record (tools/oracle-capture.js:1005). */
struct MissionInit {
  std::string chapterId;      /* "" = skirmish */
  std::string skirmishType;   /* skirmish: the recorded scenario type (the
                                 browser's draw at 15:569 is harness
                                 Math.random — DETERMINISM_NOTE.md §b1) */
  std::string difficulty = "normal";
  double missionSeed = 1;     /* CONFIG.SEED.mission at launch (15:539/:567) */
  double terrainSeed = 1337;  /* skirmish island; chapters use their volume's
                                 tseed (15:548-553) and this must agree */
  long long startMs = 0;      /* sim clock at newMission, integer ms,
                                 multiple of 16 */
};

class SimCore {
public:
  SimCore();
  ~SimCore();
  SimCore(const SimCore&) = delete;
  SimCore& operator=(const SimCore&) = delete;

  /* Optional: replace the sanctioned-flavor RNG (quip pick + gated-presence
     coins — fdc_quips.h). Non-owning; must outlive this object. Call BEFORE
     init(); null restores the built-in default. */
  void setFlavorRng(doctrine::IFlavorRng* rng);

  /* newMission (15-campaign-menu.js:523-651): island + world for the
     terrain seed, genScenario, placeUnits, OP/battery, chapter context,
     opening traffic (brief, greet, spot report / caller kickoff). */
  void init(const MissionInit& mi);

  /* One 16 ms frame — the sim slice of animate() (17-loop-boot.js:67-186):
     runEvents, idle coaching, updateScenario (+ the mission-side latch
     consequences), cffqTick, updateCivilians. */
  void advance();
  /* Frames until the clock reaches ms (inclusive). */
  void advanceTo(long long ms);

  /* THE door — onPlayerMessage (12-cff-handling.js:864) at the current
     sim time. */
  void submitMessage(const std::string& raw);

  /* lase() is deliberately absent for now: nothing scored reads the laser
     (the parser resolves polar missions from the REPORTED position,
     10-mission.js:242-258; G9). The P3 bridge adds it UI-side. */

  double now() const;
  long long nowMs() const;

  review::TranscriptLog& tlog();

  /* Presentation events booked since the last drain (bursts, screens,
     flares, troop visuals). The headless replay drops them; the bridge
     renders them. */
  std::vector<sim::PresentationEvent> drainPresentation();

  /* Bridge/test access to the assembled parts (non-owning). */
  struct Parts {
    sim::SimClock* clock = nullptr;
    sim::EventQueue* queue = nullptr;
    sim::MissionMachine* machine = nullptr;
    doctrine::FdcEngine* engine = nullptr;
    doctrine::FdcSpeaker* speaker = nullptr;
    doctrine::QuipSet* quips = nullptr;
    scenario::ScenarioState* scenario = nullptr;
    scenario::WorldLayout* world = nullptr;
    const scenario::UnitLayout* units = nullptr;
    const HeightField* heightfield = nullptr;
    const scenario::OpPos* op = nullptr;
    const scenario::BatteryPos* battery = nullptr;
  };
  Parts parts();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  doctrine::IFlavorRng* flavorOverride_ = nullptr;
};

}
