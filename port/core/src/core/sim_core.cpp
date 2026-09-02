#include "shitfire/sim_core.h"

#include <cmath>
#include <functional>

#include "scenario_view_adapter.h"
#include "shitfire/campaign/campaign_data.h"
#include "shitfire/campaign/campaign_state.h"
#include "shitfire/config/sim_config.h"
#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/doctrine/fdc_rules.h"
#include "shitfire/doctrine/fdc_say.h"
#include "shitfire/doctrine/fdc_sink.h"
#include "shitfire/doctrine/fdc_spot.h"
#include "shitfire/math/grid.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/prng.h"
#include "shitfire/review/aar_analysis.h"
#include "shitfire/review/tlog.h"
#include "shitfire/scenario/place_units.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_config.h"
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim/mission_ballistics.h"
#include "shitfire/sim/mission_clock.h"
#include "shitfire/sim/mission_effects.h"
#include "shitfire/sim/mission_machine.h"
#include "shitfire/sim/tunables.h"
#include "shitfire/terrain/heightfield.h"

/* SimCore facade bodies — thin assembly per the wave-3 brief. Everything
   with a rule in it is a call into a porter's unit; the code here is the
   newMission flow (15-campaign-menu.js:523-651), the frame-loop sim slice
   (17-loop-boot.js:67-186), enemyVolley (05-units-scenario.js:1023-1057 —
   the mission-side half of the battery seam), the scenario fail latches
   (05:857-865/:882-889/:972-982/:1012-1019) and the showAAR assembly
   (13-aar.js:239-403 via review::analyzeAar). Non-ASCII characters in net
   text are UTF-8 byte escapes (same convention as fdc_quips.cpp). */

#define EMD "\xE2\x80\x94"     /* U+2014 em dash */
#define MIDDOT "\xC2\xB7"      /* U+00B7 middle dot (KP list separator) */
#define DEG "\xC2\xB0"         /* U+00B0 degree sign */

namespace shitfire {

namespace {

/* setState vocabulary (events.h) -> the TLOG state string. */
const char* phaseStr(sim::MissionPhase p) {
  switch (p) {
    case sim::MissionPhase::Observing: return "OBSERVING";
    case sim::MissionPhase::MissionSent: return "MISSION SENT";
    case sim::MissionPhase::AtMyCommand: return "AT MY COMMAND";
    case sim::MissionPhase::Shot: return "SHOT";
    case sim::MissionPhase::Adjusting: return "ADJUSTING";
    case sim::MissionPhase::FireForEffect: return "FIRE FOR EFFECT";
    case sim::MissionPhase::EomQuery: return "EOM?";
    case sim::MissionPhase::DoNotLoad: return "DO NOT LOAD";
    case sim::MissionPhase::CheckFiring: return "CHECK FIRING";
    case sim::MissionPhase::CeaseLoading: return "CEASE LOADING";
    case sim::MissionPhase::Aar: return "AAR";
  }
  return "OBSERVING";
}

/* SCN_META names (00-config.js:323-346) — the skirmish sys line only. */
const char* scnMetaName(const std::string& t) {
  if (t == "strongpoint") return "POSITION UNDER ATTACK";
  if (t == "troops") return "TROOPS IN THE OPEN";
  if (t == "bunker") return "DUG-IN BUNKER";
  if (t == "convoy") return "CONVOY " EMD " MOVING TARGET";
  if (t == "assault") return "COMBINED ARMS ASSAULT " EMD " DANGER CLOSE";
  if (t == "raid") return "VILLAGE RAID " EMD " NO-STRIKE ADJACENT";
  if (t == "wreck") return "DERELICT WRECK (TRAINING)";
  if (t == "battery") return "COUNTERBATTERY " EMD " GUNS IN ACTION";
  if (t == "defense") return "DEFEND THE OP " EMD " DANGER CLOSE";
  if (t == "callin") return "CALL FOR SUPPORT " EMD " ROCK EATERS";
  if (t == "occupied") return "OCCUPIED SETTLEMENT " EMD " NO-STRIKE ADJACENT";
  if (t == "qfp") return "QUICK FIRE PLAN " EMD " H-HOUR";
  if (t == "twin") return "TWO TARGETS " EMD " TRAFFIC BY NUMBER";
  if (t == "chow") return "THE GREAT CHOW RAID " EMD " DANGER CLOSE";
  return "CLAWS OUT " EMD " MOVING HARD TARGET";
}

/* SPOT_SCN key set (07-voice-spot-audio.js:403-434). */
bool hasSpotCue(const std::string& t) {
  return t == "strongpoint" || t == "troops" || t == "bunker" ||
         t == "convoy" || t == "assault" || t == "wreck" || t == "raid" ||
         t == "battery" || t == "defense" || t == "occupied";
}

sim::Difficulty diffEnum(const std::string& d) {
  return d == "easy" ? sim::Difficulty::Easy
       : d == "hard" ? sim::Difficulty::Hard : sim::Difficulty::Normal;
}
sim::AssetKind assetEnum(const campaign::ChapterSpec* ch) {
  if (!ch) return sim::AssetKind::Battery155;
  if (ch->asset == "mortar60") return sim::AssetKind::Mortar60;
  if (ch->asset == "sunlamp") return sim::AssetKind::Sunlamp;
  return sim::AssetKind::Battery155;
}

std::string failWord(sim::FailReason r) {
  switch (r) {
    case sim::FailReason::Fratricide: return "fratricide";
    case sim::FailReason::Collateral: return "collateral";
    case sim::FailReason::Escaped: return "escaped";
    case sim::FailReason::Overrun: return "overrun";
    default: return "";
  }
}
std::string intentWord(sim::IntentKind k) {
  return k == sim::IntentKind::Suppress ? "suppress"
       : k == sim::IntentKind::Illum ? "illum" : "destroy";
}
std::string methodWord(sim::MethodKind k) {
  switch (k) {
    case sim::MethodKind::Grid: return "grid";
    case sim::MethodKind::Polar: return "polar";
    case sim::MethodKind::Shift: return "shift";
    default: return "";
  }
}
std::string sheafKindWord(sim::SheafKind k) {
  switch (k) {
    case sim::SheafKind::Converged: return "converged";
    case sim::SheafKind::Circular: return "circular";
    case sim::SheafKind::Open: return "open";
    case sim::SheafKind::Linear: return "linear";
    default: return "";
  }
}
std::string fuzeKindWordOf(sim::FuzeKind k) {
  switch (k) {
    case sim::FuzeKind::Pd: return "pd";
    case sim::FuzeKind::Vt: return "vt";
    case sim::FuzeKind::Time: return "time";
    case sim::FuzeKind::Delay: return "delay";
    default: return "";
  }
}

/* Built-in sanctioned-flavor RNG (quip pick + presence coins). Seed value
   is arbitrary by design — nothing scored reads this stream
   (DETERMINISM_NOTE.md §a); the capture harness's own seed is used as a
   stable default. */
class DefaultFlavor : public doctrine::IFlavorRng {
public:
  DefaultFlavor() : rng_(1592654542u) {}
  double next() override { return rng_.next(); }
private:
  Mulberry32 rng_;
};

}  // namespace

struct SimCore::Impl {
  /* fixed config blocks (00-config.js defaults, ported headers) */
  TerrainConfig terrainCfg{};
  MapConfig mapCfg{};
  scenario::WorldConfig worldCfg{};
  scenario::MissionConfig missionCfg{};
  scenario::GfxVegConfig vegCfg{};
  scenario::CameraConstants camCfg{};
  doctrine::SpotTunables spotCfg{};
  sim::SimTunables tun{};
  doctrine::FdcCallsigns calls{};
  doctrine::FdcChapterContext chapterCtx{};

  const campaign::ChapterSpec* chapter = nullptr;
  const campaign::VolumeSpec* vol = nullptr;
  std::string difficultyStr = "normal";
  double seedMission = 1;

  long long ms = 0;
  sim::SimClock clock{};
  sim::EventQueue queue;
  review::TranscriptLog tlog;
  doctrine::QuipSet quips = doctrine::makeQuips();
  DefaultFlavor defFlavor;
  doctrine::IFlavorRng* flavor = nullptr;
  std::vector<sim::PresentationEvent> pres;

  /* idle-coaching state (17-loop-boot.js:4, stateHinted global) */
  bool stateHinted = false;
  sim::MissionPhase prevPhase = sim::MissionPhase::Observing;
  double scenarioT0 = 0;

  std::unique_ptr<HeightField> hf;
  scenario::OpPos op{};
  scenario::BatteryPos battery{};
  scenario::EyePos eye{};
  scenario::WorldLayout world;
  scenario::GenContext genCtx{};
  scenario::ScenarioState scn;
  scenario::UnitLayout units{};

  /* INetSink -> TLOG (the log()/TLOG.add sites, 06-clock-comms.js). */
  struct Net : doctrine::INetSink {
    Impl& o;
    explicit Net(Impl& i) : o(i) {}
    void line(doctrine::NetLineKind kind, const std::string& call,
              const std::string& text) override;
    void parseClass(const std::string& raw,
                    const doctrine::TlogParseClass& pc) override;
    void spotMeta(const std::string& loc, const std::string& form) override;
    void sysErr(const std::string& msg, double num) override;
    void posRepMeta(const std::string& digits, double selfLocErr) override;
  };
  /* IMissionSink tail — impacts to the TLOG (10-mission.js:1130/:1154),
     presentation collected for the bridge, state changes tracked for the
     idle hint (setState resets stateHinted, 10-mission.js:7). */
  struct Tail : sim::IMissionSink {
    Impl& o;
    explicit Tail(Impl& i) : o(i) {}
    double sayLine(const sim::FdcLine&) override { return 0; } /* not fanned */
    void sysLine(const sim::SysLine&) override {}              /* not fanned */
    void stateChanged(sim::MissionPhase p, double) override {
      if (p != o.prevPhase) {
        o.prevPhase = p;
        o.stateHinted = false;
      }
    }
    void impact(const sim::ImpactEvent& ev) override;
    void presentation(const sim::PresentationEvent& ev) override {
      o.pres.push_back(ev);
    }
    void spotReportRequested(sim::SpotReportReason) override {}
  };

  std::unique_ptr<Net> net;
  std::unique_ptr<Tail> tail;
  std::unique_ptr<doctrine::FdcSpeaker> speaker;
  std::unique_ptr<doctrine::FdcMissionSink> sink;
  std::unique_ptr<core::ScenarioViewAdapter> view;
  std::unique_ptr<sim::MissionMachine> machine;
  std::unique_ptr<doctrine::FdcSpotReports> spot;
  std::unique_ptr<doctrine::FdcEngine> engine;

  campaign::CampState camp;      /* fresh session (capture: fresh profile) */
  review::BestScores best;

  Impl(const MissionInit& mi, doctrine::IFlavorRng* flavorOverride);

  review::TlogContext ctx() const {
    review::TlogContext c;
    c.simNow = clock.now;
    c.state = machine ? phaseStr(machine->state()) : "OBSERVING";
    c.seedMission = seedMission;
    if (chapter) c.chapterId = chapter->id;
    return c;
  }
  void sys(const std::string& msg) {
    net->line(doctrine::NetLineKind::Sys, "", msg);
  }
  void openingTraffic();
  void advanceOneFrame();
  void enemyVolley();
  void failMission(sim::FailReason r);
  void showAar();
};

/* ---- Net sink ---------------------------------------------------------- */

void SimCore::Impl::Net::line(doctrine::NetLineKind kind,
                              const std::string& call,
                              const std::string& text) {
  const char* k = "sys";
  switch (kind) {
    case doctrine::NetLineKind::Fdc: k = "fdc"; break;
    case doctrine::NetLineKind::Sys: k = "sys"; break;
    case doctrine::NetLineKind::Obs: k = "obs"; break;
    case doctrine::NetLineKind::Caller: k = "caller"; break;
    case doctrine::NetLineKind::Note: k = "note"; break;
  }
  o.tlog.add(o.ctx(), k, call, text, review::TlogEntry{});
}

void SimCore::Impl::Net::parseClass(const std::string& raw,
                                    const doctrine::TlogParseClass& pc) {
  review::TlogEntry ex;
  ex.ptype = pc.ptype;
  ex.method.present = true;
  if (!pc.method.empty()) ex.method.value = pc.method;
  ex.warno.present = true;
  if (!pc.warno.empty()) ex.warno.value = pc.warno;
  ex.ffe = pc.ffe;
  o.tlog.add(o.ctx(), "parse", "", raw, ex);
}

void SimCore::Impl::Net::spotMeta(const std::string& loc,
                                  const std::string& form) {
  review::TlogEntry ex;
  ex.form = form;
  ex.diff = o.scn.difficulty;   /* 07:456/:476 — scenario state */
  ex.scn = o.scn.type;
  o.tlog.add(o.ctx(), "spot", "", loc, ex);
}

void SimCore::Impl::Net::sysErr(const std::string& msg, double num) {
  review::TlogEntry ex;
  ex.len = num;   /* 12-cff-handling.js:892 (oversize refusal) */
  o.tlog.add(o.ctx(), "syserr", "", msg, ex);
}

void SimCore::Impl::Net::posRepMeta(const std::string& digits,
                                    double selfLocErr) {
  review::TlogEntry ex;
  ex.selfLocErr = selfLocErr;   /* 10-mission.js:279 */
  o.tlog.add(o.ctx(), "sys", "", "posrep " + digits, ex);
}

/* ---- Tail sink --------------------------------------------------------- */

void SimCore::Impl::Tail::impact(const sim::ImpactEvent& ev) {
  review::TlogEntry ex;
  ex.dTgt = ev.dTgtRounded;
  std::string msg;
  if (ev.shell == sim::ShellKind::Illum) {
    msg = "illumination round";               /* 10:1130 */
    ex.shell = "illum";
  } else {
    msg = ev.isFFE ? "FFE round"
                   : "adjust round " + doctrine::jsNum(ev.adjustRound);
    if (ev.shell == sim::ShellKind::Smoke) ex.shell = "smoke"; /* 10:1156 */
  }
  o.tlog.add(o.ctx(), "impact", "", msg, ex);
}

/* ---- construction / newMission ----------------------------------------- */

SimCore::Impl::Impl(const MissionInit& mi,
                    doctrine::IFlavorRng* flavorOverride) {
  chapter = mi.chapterId.empty() ? nullptr : campaign::findChapter(mi.chapterId);
  vol = chapter ? campaign::volumeOf(chapter->id) : nullptr;
  const double tseed = (chapter && vol && vol->tseed.has_value())
                           ? *vol->tseed
                           : mi.terrainSeed;
  difficultyStr = mi.difficulty;
  seedMission = mi.missionSeed;
  ms = mi.startMs;
  clock.now = static_cast<double>(ms) / 1000.0;   /* 17-loop-boot.js:69/73 */
  tun.seed.mission = mi.missionSeed;
  tun.seed.terrain = tseed;
  flavor = flavorOverride ? flavorOverride : &defFlavor;
  chapterCtx.active = chapter != nullptr;
  chapterCtx.asset = assetEnum(chapter);
  chapterCtx.strict = chapter && chapter->strict;
  chapterCtx.requiredMethod = chapter ? chapter->method : "";
  chapterCtx.reqImmediate = chapter && chapter->reqImmediate;

  /* per-volume island (15:546-557; boot builds the 1337 island the same way). */
  hf = std::make_unique<HeightField>(terrainCfg, tseed);
  op = scenario::findOP(*hf);
  battery = scenario::findBattery(*hf, op);
  eye = scenario::eyeFor(op, camCfg);
  world = scenario::buildWorldFeatures(*hf, op, battery, worldCfg, vegCfg,
                                       tseed, nullptr,
                                       vol && vol->palette == "black");

  genCtx.hf = hf.get();
  genCtx.world = &world;
  genCtx.op = op;
  genCtx.eye = eye;
  genCtx.mission = missionCfg;
  genCtx.worldCfg = worldCfg;
  genCtx.map = mapCfg;
  genCtx.islandRadius = terrainCfg.islandRadius;
  genCtx.terrainSeed = tseed;
  genCtx.difficulty = difficultyStr;
  genCtx.chapterActive = chapter != nullptr;
  if (chapter) {
    genCtx.opts.mask = chapter->scn.mask;
    genCtx.opts.fClose = chapter->scn.fClose;
    genCtx.opts.lanes = chapter->scn.lanes;
    genCtx.opts.effR = chapter->scn.effR;
  }
  genCtx.now = clock.now;

  /* Scenario = genScenario(currentType, CONFIG.SEED.mission) (15:573) */
  scn = scenario::genScenario(chapter ? chapter->type : mi.skirmishType,
                              mi.missionSeed, genCtx);
  scenarioT0 = clock.now;                       /* 15:574 */
  units = scenario::placeUnits(scn, world.civs); /* 15:575 */

  net = std::make_unique<Net>(*this);
  tail = std::make_unique<Tail>(*this);
  speaker = std::make_unique<doctrine::FdcSpeaker>(clock, queue, *net,
                                                   chapterCtx, calls);
  sink = std::make_unique<doctrine::FdcMissionSink>(
      *speaker, quips, *flavor, *net, chapterCtx, calls, mapCfg,
      sim::Vec2{battery.x, battery.z});
  view = std::make_unique<core::ScenarioViewAdapter>(scn, world, *hf, eye,
                                                     clock, scenarioT0);
  view->resetEnemyAlive();                      /* 05:1080 */
  machine = std::make_unique<sim::MissionMachine>(
      clock, queue, *sink, *view, tun, mapCfg, diffEnum(difficultyStr),
      assetEnum(chapter), sim::Vec2{op.x, op.z},
      sim::Vec2{battery.x, battery.z}, chapterCtx.strict);
  machine->setMissionSeed(mi.missionSeed);      /* 15:539/:567 */
  sink->setMachine(machine.get());
  sink->setTail(tail.get());
  spot = std::make_unique<doctrine::FdcSpotReports>(
      clock, queue, *speaker, *net, quips, *flavor, chapterCtx, calls,
      spotCfg, mapCfg);
  spot->bind(&scn, &world, op);
  sink->setSpot(spot.get());
  engine = std::make_unique<doctrine::FdcEngine>(
      clock, queue, *machine, *view, *speaker, *sink, spot.get(), quips,
      *flavor, *net, chapterCtx, calls, mapCfg, op,
      sim::Vec2{battery.x, battery.z}, 7.0);
  engine->bindScenario(&scn);
  engine->hooks.showAar = [this]() { showAar(); };
  if (chapter && chapter->tut.has_value()) {
    /* F.1/F.2 guns-cold tutorials (12-cff:911-916). The step engine itself
       is presentation (campaign_data.h note); parse classification is
       logged BEFORE the intercept (12-cff:907), which is all Level 3
       compares on these transcripts. */
    engine->hooks.tutorialMsg = [](const doctrine::ParsedMessage&) {
      return false;
    };
    engine->hooks.tutorialNoMission = chapter->tut->noMission;
  }
  openingTraffic();
}

void SimCore::Impl::openingTraffic() {
  using doctrine::pick;
  if (chapter) {
    /* per-volume island announcement (15:556) — boot terrain is 1337. */
    if (vol && vol->tseed.has_value() && *vol->tseed != 1337)
      sys("Deploying to a new island " EMD " new map sheets are in the "
          "library [P].");
    const std::string wantTod =
        !chapter->tod.empty() ? chapter->tod : (vol ? vol->tod : "");
    if (!wantTod.empty() && wantTod != "day")
      sys("Light: " + doctrine::upperAscii(wantTod) + ".");   /* 15:564 */
    sys("CHAPTER " + chapter->id + " " EMD " " + chapter->title + " (" +
        doctrine::upperAscii(difficultyStr) + "). " + scn.brief); /* 15:583 */
    if (!chapter->method.empty())   /* 15:585-588 */
      sys("Chapter requirement: locate the target by " +
          std::string(chapter->method == "polar"
                          ? "POLAR (direction + distance)"
                          : "SHIFT FROM A KNOWN POINT") +
          " " EMD " a grid call caps this chapter at 2\xE2\x98\x85." +
          std::string(chapter->method == "polar"
                          ? " POLAR needs your position first: fix yourself "
                            "on the sheet and send \"POSITION GRID \xE2\x80"
                            "\xA6\" before your call " EMD " the FDC resolves "
                            "the mission from the position you REPORT."
                          : ""));
    if (chapter->strict)   /* 15:590 */
      sys("STRICT NET: full format enforced " EMD " callsigns, warning "
          "order, location, description, OVER; corrections rounded "
          "(deviation in tens, range in hundreds " EMD " fifties only "
          "entering fire for effect); surveillance term at end of mission.");
    if (chapter->asset == "mortar60")   /* 15:592 */
      sys("ASSET: 60mm mortar section, callsign " + calls.fdc60 + " " EMD
          " address THEM, not HELLHOUND; strict net will bounce the wrong "
          "callsign. 8-digit grids (10 m precision) required. Tighter "
          "dispersion, tighter target.");
    if (chapter->par.has_value())   /* 15:594 */
      sys("Par time for full stars: " + review::fmtTime(*chapter->par) + ".");
    if (chapter->tut.has_value() && !chapter->tut->steps.empty()) {
      /* tutStart (15:339-343) — the first prompt airs at +2.5; later steps
         key on look/binos/lase events the replay does not drive. */
      const std::string say0 = chapter->tut->steps[0].say;
      queue.schedule(clock.now + 2.5, [this, say0]() {
        sys("GUNNY BOTTLECAP: " + say0);
      });
    }
    for (std::size_t i = 0; i < chapter->coach.size(); i++) {   /* 15:597 */
      const std::string c = chapter->coach[i];
      queue.schedule(clock.now + 4 + static_cast<double>(i) * 7,
                     [this, c]() { sys("GUNNY BOTTLECAP: " + c); });
    }
  } else {
    sys("SKIRMISH: " + std::string(scnMetaName(scn.type)) + " (" +
        doctrine::upperAscii(difficultyStr) + "). " + scn.brief); /* 15:601 */
  }
  /* 15:606 — the MET line. */
  sys("MET: wind from " + fmtMils(world.wind.fromMils) + " mils at " +
      doctrine::jsNum(js::round(world.wind.speed)) + " m/s " EMD " read "
      "your smoke; it will drift downwind. Rounds are not wind-corrected on "
      "this net (the FDC applies met).");
  /* 15:607-609 — the OP line. */
  sys("Your OP: watchtower at grid " + gridOf(op.x, op.z, mapCfg) +
      ", ground elev " + doctrine::jsNum(js::round(op.h)) + " m, deck +" +
      doctrine::jsNum(js::round(camCfg.towerHeight)) + " m. Use [B] binos "
      "and [L] laser to fix the target ([M] map, [K] mission menu), then "
      "send your call for fire " EMD " grid, polar, or shift from a known "
      "point. Hold [SPACE] to transmit by voice, or type below.");
  /* 15:613-615 — the G2 declination line. */
  sys("NAVIGATION: your heading readout is MAGNETIC. The grid, the map "
      "sheet and the fire net are GRID. Declination is 7" DEG " EAST, so "
      "GRID = MAG + " +
      doctrine::jsNum(js::round(doctrine::declMilsFor(7))) + " mils. Send "
      "OT DIRECTION in GRID " EMD " [R] has the conversion on the mil card, "
      "and the sheet carries the declination diagram.");
  /* 15:616-619 — known points. */
  if (!scn.kps.empty()) {
    std::string list;
    for (std::size_t i = 0; i < scn.kps.size(); i++) {
      const scenario::Kp& kp = scn.kps[i];
      if (i) list += " " MIDDOT " ";
      list += "AB" + kp.id + " \"" + kp.name + "\" grid " +
              gridOf(kp.x, kp.z, mapCfg) + " (grid brg " +
              fmtMils(radToMils(azTo(op.x, op.z, kp.x, kp.z))) + ")";
    }
    sys("Known points registered: " + list +
        ". Shift example: \"adjust fire, shift known point 1001, right 200, "
        "add 400, over\" " EMD " the KP name works too (\"shift "
        "BREWERY\").");
  }
  /* 15:620-624 — the no-strike line. */
  if (!world.villages.empty()) {
    std::string names;
    for (std::size_t i = 0; i < world.villages.size(); i++) {
      if (i) names += " and ";
      names += world.villages[i].name;
    }
    sys("NO-STRIKE: civilian village" +
        std::string(world.villages.size() > 1 ? "s" : "") + " " + names +
        " " EMD " plotted on your map sheet. Collateral damage fails the "
        "mission, same as fratricide. Permanent structures and roads are on "
        "the sheet too " EMD " use them to orient.");
  }
  if (!scn.brief.empty()) sys(">>> THE JOB: " + scn.brief);   /* 15:630 */
  speaker->say(pick(quips.greet, *flavor), 1.5);              /* 15:632 */
  /* 15:634-639 — career callback: CAMP.data._career is absent on a fresh
     session, so the JS never draws the coin; same here (camp.career unset). */
  if (camp.career.has_value()) {
    const double air = std::fmax(clock.now, speaker->lastT()) + 1.6;
    if (flavor->coin(0.4, doctrine::FlavorGate::CareerCallback, air)) {
      if (camp.career->frat > 0)
        speaker->say(pick(quips.careerFrat, *flavor), 1.6);
      else if (camp.career->collat > 0)
        speaker->say(pick(quips.careerCollat, *flavor), 1.6);
      else if (camp.career->missions >= 5)
        speaker->say(pick(quips.careerVet, *flavor), 1.6);
    }
  }
  /* 15:641-643 — the cue. */
  if (scn.type == "callin") spot->callerKickoff();
  else if (hasSpotCue(scn.type)) spot->sendSpotReport(doctrine::SpotReason::Start);
  else speaker->say(pick(quips.spotNone, *flavor), spotCfg.lead);
  machine->setState(sim::MissionPhase::Observing);   /* 15:650 */
}

/* ---- frame loop --------------------------------------------------------- */

void SimCore::Impl::advanceOneFrame() {
  ms += 16;
  clock.now = static_cast<double>(ms) / 1000.0;   /* 17-loop-boot.js:73 */
  queue.run(clock);                               /* :85 runEvents() */
  /* :108-116 — idle coaching, once per state. */
  if (!stateHinted) {
    const double idle = clock.now - machine->stateSince();
    const sim::MissionPhase st = machine->state();
    if ((st == sim::MissionPhase::Adjusting && idle > 25) ||
        (st == sim::MissionPhase::EomQuery && idle > 15) ||
        (st == sim::MissionPhase::Observing && idle > 60)) {
      stateHinted = true;
      engine->sysHint();
    }
  }
  /* :152 — updateScenario() with the wave-3 IO seam. */
  scenario::ScenarioTickIO io;
  io.enemyAlive = view->enemyAlive();
  scenario::updateScenarioTick(scn, clock.now, scenarioT0, genCtx, &io);
  if (io.volleyFired) enemyVolley();   /* 05:903 */
  if (io.escapedNow) {                 /* 05:857-865 */
    int alive = 0;
    if (scn.veh)
      for (const scenario::Veh& v : *scn.veh)
        if (!v.dead) alive++;
    if (alive > 1) {
      sys("The convoy has reached the end of the road and dispersed. "
          "Mission window closed.");
      failMission(sim::FailReason::Escaped);
    }
  }
  if (io.ashoreNow) {                  /* 05:882-889 */
    sys("LANDFALL. The crab is in the village. There is no doctrinal term "
        "for what happens next, and the mission is over.");
    failMission(sim::FailReason::Escaped);
  }
  if (io.overrunNow) {                 /* 05:972-982 / 05:1012-1019 */
    sim::Mission* m = machine->activeMission();
    if (m && !m->done) {
      sys(scn.type == "qfp"
              ? "They are inside the wire. The position is overrun " EMD
                " the plan came too late. Mission over."
              : "They are through the wire and on the tower stairs. The OP "
                "is overrun " EMD " mission over.");
      failMission(sim::FailReason::Overrun);
    }
  }
  if (io.hHourStarted) {               /* 05:938-940 */
    sys(scn.axisAz.has_value()
            ? "H-HOUR. The counterattack is in the draw " EMD " grid " +
                  gridOf(scn.enemy.x, scn.enemy.z, mapCfg) +
                  ", walking your phase lines toward the position. Fire the "
                  "plan."
            : "H-HOUR. Assault force on avenue " +
                  std::string(scn.assaultAv.has_value() &&
                                      *scn.assaultAv < 3
                                  ? (*scn.assaultAv == 0   ? "ALPHA"
                                     : *scn.assaultAv == 1 ? "BRAVO"
                                                           : "CHARLIE")
                                  : "ALPHA") +
                  " " EMD " grid " + gridOf(scn.enemy.x, scn.enemy.z, mapCfg) +
                  " " EMD " advancing on the position. If you planned it, "
                  "two words bring it down.");
  }
  if (io.callerSaidDead) spot->callerSay(doctrine::CallerCue::Dead); /* 05:918 */
  if (io.callerSaidSup) spot->callerSay(doctrine::CallerCue::Sup);   /* 05:919 */
  engine->tick();                                     /* :153 cffqTick() */
  scenario::updateCivilians(world.civs, clock.now);   /* :154 */
}

/* ---- enemyVolley (05-units-scenario.js:1023-1057) ----------------------- */

void SimCore::Impl::enemyVolley() {
  const double fbx = scn.fireBase->x, fbz = scn.fireBase->z;
  /* muzzle signature (05:1036) — presentation only. */
  sim::PresentationEvent sig{sim::PresentationKind::Burst};
  sig.x = scn.enemy.x;
  sig.y = hf->H(scn.enemy.x, scn.enemy.z);
  sig.z = scn.enemy.z;
  pres.push_back(sig);
  for (int i = 0; i < 3; i++) {
    /* draws from the SCENARIO stream, in loop order (05:1042). */
    const double ix = fbx + sim::gauss(scn.rng) * 55;
    const double iz = fbz + sim::gauss(scn.rng) * 55;
    queue.schedule(clock.now + scn.bty->tof + i * 0.8,
                   [this, ix, iz, fbx, fbz]() {
      /* the `Scenario !== S` guard (05:1044) is structural here: one
         scenario per SimCore lifetime. Book first (GFX9). */
      if (dist2(ix, iz, fbx, fbz) < scn.friendlies[0].r) {
        scn.btyCas = scn.btyCas.value_or(0) + 1;
        if (*scn.btyCas == 1)
          sys("Rounds inside the friendly position. They are taking "
              "casualties down there " EMD " kill that gun.");
      }
      sim::PresentationEvent b{sim::PresentationKind::Burst};
      b.x = ix;
      b.y = std::fmax(hf->H(ix, iz), 0.0);
      b.z = iz;
      pres.push_back(b);
    });
  }
}

/* ---- scenario fail bookkeeping (05:861-864 et al.) ---------------------- */

void SimCore::Impl::failMission(sim::FailReason r) {
  sim::Mission* m = machine->activeMission();
  if (!m || m->done) return;
  m->done = true;
  m->failReason = r;
  m->tEnd = clock.now;
  queue.schedule(clock.now + 2, [this]() { showAar(); });
}

/* ---- showAAR (13-aar.js:239-403 via review::analyzeAar) ----------------- */

void SimCore::Impl::showAar() {
  sim::Mission* m = machine->activeMission();
  if (!m) return;   /* 13-aar.js:241 */
  review::AarMissionView av;
  av.g.failReason = failWord(m->failReason);
  av.g.intent = intentWord(m->intent);
  av.g.roundsFired = static_cast<double>(m->rounds.size());
  av.g.adjustRounds = m->adjustRounds;
  av.g.notesCount = static_cast<double>(m->notes.size());
  av.g.aimErr0 = m->aimErr0;
  av.g.tStart = m->tStart;
  av.g.tEnd = m->tEnd;
  av.g.tInit = m->tInit;
  av.g.wasted = m->wasted;
  av.g.missTrace = m->missTrace;
  av.g.method = methodWord(m->method);
  av.g.imm = m->imm != sim::ImmKind::None;
  av.g.usedIllum = m->usedIllum;
  av.g.planned = m->planned;
  av.sidesLong = m->sides.longSide;
  av.sidesShort = m->sides.shortSide;
  av.firstMiss = m->firstMiss;
  av.bdaClaim = m->bdaClaim;
  av.sheafKind = sheafKindWord(m->sheaf.kind);
  av.sheafSource = m->sheaf.source;
  av.fuzeKind = fuzeKindWordOf(m->fuze.kind);
  av.fuzeSource = m->fuze.source;
  av.shell = m->shell == sim::ShellKind::Smoke ? "smoke"
           : m->shell == sim::ShellKind::Illum ? "illum" : "he";
  av.noDisp = m->noDisp;
  av.refined = m->refined;
  av.recorded = m->recorded;
  av.otDirSent = m->otDirSent;
  av.hits = m->hits;
  av.notes = m->notes;
  review::AarScenarioView sv;
  sv.type = scn.type;
  sv.difficulty = scn.difficulty;
  sv.btyCas = scn.btyCas.value_or(0);
  double dead = 0;
  if (scn.veh)
    for (const scenario::Veh& v : *scn.veh)
      if (v.dead) dead += 1;
  sv.vehDeadCount = dead;
  const sim::EffectAssessment a = machine->assessEffect();
  const bool isConvoy = scn.type == "convoy";
  sim::EffBandSet bands{};
  if (!isConvoy)
    bands = sim::effBands(assetEnum(chapter), view->targetClass(),
                          scn.effScale, tun.effects);
  const review::AarReport rep = review::analyzeAar(
      av, sv, chapter, a, isConvoy ? nullptr : &bands, clock.now,
      tun.mission.passMaxAdjustRounds, best, camp);
  tlog.add(ctx(), "aar", "", rep.verdict, rep.tlogAar);   /* 13-aar:364 */
}

/* ---- public surface ----------------------------------------------------- */

SimCore::SimCore() = default;
SimCore::~SimCore() = default;

void SimCore::setFlavorRng(doctrine::IFlavorRng* rng) { flavorOverride_ = rng; }

void SimCore::init(const MissionInit& mi) {
  impl_ = std::make_unique<Impl>(mi, flavorOverride_);
}

void SimCore::advance() { impl_->advanceOneFrame(); }

void SimCore::advanceTo(long long targetMs) {
  while (impl_->ms < targetMs) impl_->advanceOneFrame();
}

void SimCore::submitMessage(const std::string& raw) {
  impl_->engine->submitMessage(raw);
}

double SimCore::now() const { return impl_->clock.now; }
long long SimCore::nowMs() const { return impl_->ms; }

review::TranscriptLog& SimCore::tlog() { return impl_->tlog; }

std::vector<sim::PresentationEvent> SimCore::drainPresentation() {
  std::vector<sim::PresentationEvent> out;
  out.swap(impl_->pres);
  return out;
}

SimCore::Parts SimCore::parts() {
  Parts p;
  p.clock = &impl_->clock;
  p.queue = &impl_->queue;
  p.machine = impl_->machine.get();
  p.engine = impl_->engine.get();
  p.speaker = impl_->speaker.get();
  p.quips = &impl_->quips;
  p.scenario = &impl_->scn;
  p.world = &impl_->world;
  p.units = &impl_->units;
  p.heightfield = impl_->hf.get();
  p.op = &impl_->op;
  p.battery = &impl_->battery;
  return p;
}

}
