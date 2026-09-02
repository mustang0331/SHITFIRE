#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "shitfire/config/sim_config.h"
#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/doctrine/fdc_say.h"
#include "shitfire/doctrine/fdc_sink.h"
#include "shitfire/doctrine/fdc_spot.h"
#include "shitfire/doctrine/parser_message.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim/mission_machine.h"

/* The FDC rule engine — port of src/js/12-cff-handling.js (oracle-v1),
   everything the parser porter did not take (they took parseMessage/
   tlogParseClass/formatNotes), plus the parser-facing 10-mission.js handlers
   the committed MissionMachine deliberately left to this layer:
   handlePosRep (:260-280, assigned here by mission_machine.h) and the
   raw-text halves of inferSheaf/inferFuze (:979-985 / :1012-1019).

   THE SINGLE DOOR (Level-3 replay contract): submitMessage(raw) is
   onPlayerMessage (12-cff:864) verbatim — note intercept, backlog cap,
   oversize refusal, parse, TLOG parse record, tutorial/caller intercepts,
   the CFFQ three-transmission staging, MTO readback claim, dispatch — and
   everything below it drives the committed sim:: machine through its public
   API and speaks through the shared FdcSpeaker chain. The wave-3 replay
   harness feeds recorded transmissions into this one method and drains the
   EventQueue against a synthetic clock; tick() is the CFFQ lapse watchdog
   (cffqTick, called once per frame in the browser's loop).

   Cross-layer reconciliation notes (facts, not edits — sim/ and scenario/
   are committed):
   - Scenario.posRep (written by handlePosRep, read only by this layer's
     polar-mission resolution) has no field in scenario::ScenarioState, so
     the engine OWNS it; bindScenario() clears it, reproducing "rebuilt
     fresh every mission, so it expires naturally" (10-mission.js:258).
   - The per-mission flags only this layer touches in the source
     (mto.nagged, dirNagged, creepKey/creepNoted, unsafeKey, coached.timid)
     are not in the committed sim::Mission/MtoSpec structs; they live in an
     engine-side table keyed by Mission*, wiped with bindScenario().
   - SUGG5 mission switches assign the JS state global directly
     (`state = other.phase`, 12-cff:796/:1061) without touching stateSince;
     the machine only exposes setState(), which refreshes stateSince on a
     change. stateSince feeds no ported decision (it paces the browser's UI
     hint timer), so setState() is used and the divergence is recorded here.
     A raw restoreState(MissionPhase) entry point would remove it.
   - handleEOM schedules the AAR at FDC.lastT + 1.6 (12-cff:801); the AAR
     itself is the wave-2 AAR porter's — hooks.showAar fills the slot.
   - handlePlanFPF's mid-mission refusal precedes its grid/direction checks
     in the source (:406 before :410); the machine's planFpf() carries that
     refusal, so on a mid-mission net the engine calls planFpf with dummy
     arguments — the machine refuses before reading them (verified:
     midMissionRefuse is its first statement) — and the local checks run
     only on a quiet net, preserving source order. */
namespace shitfire { namespace doctrine {

/* ---- pure rule seams (exposed for [fdc_rules] unit tests) -------------- */

/* Declination — port of src/js/01-math-terrain.js:184-186 (oracle-v1).
   declEastDeg is CONFIG.NAV.declEastDeg (7 in every chapter, 00-config:162). */
double declMilsFor(double declEastDeg);
double gridToMag(double mils, double declEastDeg);
double magToGrid(double mils, double declEastDeg);

/* G2 diagnosis — port of src/js/12-cff-handling.js:477-511 (oracle-v1).
   off(t) = |((sent - t + 3200) % 6400) - 3200| (JS %, sign-preserving);
   a candidate fits when it is < 45 AND beats the raw error by > 60; the
   generic coach threshold is err > 80. errOut receives off(trueAz). */
enum class DirDiagnosis { None, Magnetic, Reversed, Rough };
DirDiagnosis diagnoseDirection(double sentMils, double trueAzMils,
                               double declEastDeg, double* errOut);

/* Correction rounding doctrine — 12-cff-handling.js:626 / :714: deviation
   in tens, range in hundreds, fifties only entering fire for effect. */
bool strictRoundingViolation(double right, double add, bool ffe);

/* Strict-net format verdict — 12-cff-handling.js:167-179. Returns the
   challenge text ("" = call passes). Pure given the chapter context. */
std::string strictChallenge(const ParsedMessage& p,
                            const FdcChapterContext& chapter,
                            const FdcCallsigns& calls);

/* NET17 location validation for transmission 2 — 12-cff-handling.js:104-130.
   Returns the refusal string, "" when the location is workable. Draws a
   badGrid quip (flavor rng) only on the refusing branches, like the source. */
std::string locError(const ParsedMessage& p,
                     const std::vector<scenario::Kp>& kps, bool havePosRep,
                     bool sunlampNet, const MapConfig& map, QuipSet& quips,
                     IFlavorRng& flavor);

/* T2 location echo — 12-cff-handling.js:133-144. */
std::string locEchoOf(const ParsedMessage& p);

/* Raw-text sheaf/fuze requests — port of src/js/10-mission.js:980-985 /
   :1013-1019 (oracle-v1), the observer-transmission halves the machine left
   to this layer; the desc/default halves are the machine's
   inferSheafFromDesc/inferFuzeFromDesc. matched=false means no request in
   the raw text (fall through to the desc inference). */
bool sheafRequestFromRaw(const std::string& raw, sim::SheafSpec& out);
bool fuzeRequestFromRaw(const std::string& raw, sim::FuzeSpec& out);

/* G9 position report — engine-owned (see reconciliation note above). */
struct PosRep {
  double x = 0, z = 0;
  std::string digits;
};

/* ---- the engine -------------------------------------------------------- */

struct FdcHooks {
  /* TUT.steps / tutEvent('msg', p) — 12-cff:911-912. Null = no tutorial.
     Return true to consume the transmission. */
  std::function<bool(const ParsedMessage&)> tutorialMsg;
  /* TUT.noMission — 12-cff:913-916 (guns-cold tutorials). */
  bool tutorialNoMission = false;
  /* showAAR — 12-cff:801; the AAR layer's entry. Null = the schedule slot
     still burns (state goes Aar) but nothing runs. */
  std::function<void()> showAar;
};

class FdcEngine {
public:
  FdcEngine(sim::SimClock& clock, sim::EventQueue& queue,
            sim::MissionMachine& machine, sim::IScenarioView& view,
            FdcSpeaker& speaker, FdcMissionSink& sink, FdcSpotReports* spot,
            QuipSet& quips, IFlavorRng& flavor, INetSink& net,
            const FdcChapterContext& chapter, const FdcCallsigns& calls,
            const MapConfig& map, const scenario::OpPos& op,
            sim::Vec2 battery, double declEastDeg = 7.0);

  /* onPlayerMessage — 12-cff-handling.js:864-1016. THE door. */
  void submitMessage(const std::string& raw);

  /* cffqTick — 12-cff-handling.js:39-45 (the 75 s lapse watchdog). */
  void tick();

  /* wave-3 reconciliation: public because the frame loop's idle coaching
     calls it directly (17-loop-boot.js:108-116) — the SimCore facade drives
     that check; the internal call sites (handleAdjust/noMissionReply paths)
     are unchanged. */
  void sysHint();                                           /* :830 */

  /* Rebind on scenario (re)build: clears posRep (expiry), the per-mission
     side table, and the caller/KP source. Null = no scenario. */
  void bindScenario(const scenario::ScenarioState* scn);

  FdcHooks hooks;

  /* state peeks (tests / harness) */
  const std::optional<PosRep>& posRep() const { return posRep_; }
  bool cffqOpen() const { return cffq_.on; }
  int cffqStage() const { return cffq_.stage; }

private:
  /* CFFQ — 12-cff-handling.js:25-96 (G22 three-transmission staging). */
  struct CffQueue {
    bool on = false;
    int stage = 0;
    std::string warno = "none";
    std::unique_ptr<ParsedMessage> loc;   /* null until T2 */
    std::string raw;
    std::vector<std::string> toks;
    double t0 = 0;
    std::string kpId;                     /* NET15; "" = null */
  };
  /* Per-mission flags only this layer touches (see header note). */
  struct MissionExtra {
    bool mtoNagged = false;                    /* mission.mto.nagged */
    bool dirNagged = false;                    /* mission.dirNagged (G8) */
    bool creepNoted = false;                   /* mission.creepNoted (NET23) */
    bool coachedTimid = false;                 /* mission.coached.timid */
    std::optional<std::string> creepKey;       /* mission.creepKey */
    std::optional<std::string> unsafeKey;      /* mission.unsafeKey */
  };

  void cffqReset();                                       /* :27 */
  void cffqAdd(const ParsedMessage& p);                   /* :31 */
  void cffqStart(const std::string& warno,
                 const ParsedMessage& p);                 /* :46 */
  void cffqLocation(const ParsedMessage& p,
                    const std::string& locEcho);          /* :59 */
  void cffqComplete(const std::string& desc,
                    const ParsedMessage& p);              /* :67 */

  void dispatchParsed(const ParsedMessage& p, bool routed); /* :1025 */
  bool handleCFF(const ParsedMessage& p, bool staged);      /* :147 */
  void handleDirection(const ParsedMessage& p);             /* :470 */
  void handleAdjust(const ParsedMessage& p);                /* :515 */
  void handleEOM(const ParsedMessage& p);                   /* :724 */
  void handleCheckFiring();                                 /* :430 */
  void handleCeaseLoading();                                /* :446 */
  bool liftCheckFire();                                     /* :460 */
  void handleRepeat(const ParsedMessage& p);                /* :1131 */
  void handlePosRep(const ParsedMessage& p);   /* 10-mission.js:260-280 */
  void noMissionReply();                                    /* :840 */
  std::string expectedHint() const;                         /* :804 */
  void addNote(const std::string& text);                    /* :854 (net side) */

  sim::SheafSpec inferSheaf(const std::string& desc,
                            const std::string& raw) const; /* 10:979 */
  sim::FuzeSpec inferFuze(const std::string& desc,
                          const std::string& raw) const;   /* 10:1012 */

  MissionExtra& extraFor(const sim::Mission* m);
  ParserContext parserContext() const;
  std::vector<std::string> openMissionKeys() const;  /* Object.keys(MISSIONS) live */
  void sayNet(const std::string& msg, double delay); /* FDC.say via speaker */
  void sysLog(const std::string& msg);               /* log('', msg, 'sys') */
  double dangerCloseR() const;                       /* 10:79 via tunables */
  static std::unique_ptr<ParsedMessage> cloneNoInner(const ParsedMessage& p);

  sim::SimClock& clock_;
  sim::EventQueue& queue_;
  sim::MissionMachine& machine_;
  sim::IScenarioView& view_;
  FdcSpeaker& speaker_;
  FdcMissionSink& sink_;
  FdcSpotReports* spot_;
  QuipSet& quips_;
  IFlavorRng& flavor_;
  INetSink& net_;
  const FdcChapterContext& chapter_;
  FdcCallsigns calls_;
  MapConfig map_;
  scenario::OpPos op_;
  sim::Vec2 battery_;
  double declEastDeg_;
  const scenario::ScenarioState* scn_ = nullptr;

  CffQueue cffq_;
  std::optional<PosRep> posRep_;
  int unknownStreak_ = 0;     /* :832 */
  int noMissionStreak_ = 0;   /* :839 (NET4) */
  std::vector<std::pair<const sim::Mission*, MissionExtra>> extras_;
};

} }
