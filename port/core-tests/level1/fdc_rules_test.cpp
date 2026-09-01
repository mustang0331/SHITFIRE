#include <catch2/catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/doctrine/fdc_rules.h"
#include "shitfire/doctrine/fdc_say.h"
#include "shitfire/doctrine/fdc_sink.h"
#include "shitfire/doctrine/fdc_spot.h"
#include "shitfire/math/grid.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/sim/mission_machine.h"

/* [fdc_rules] — unit tests for the wave-2 FDC layer: the FDC.say timing
   chain (06-clock-comms.js:200-233), the NET18 backlog cap, the sanctioned-
   flavor gates, and the pure decision rules of 12-cff-handling.js (strict-net
   verdicts, deviation tiers, direction diagnosis, location validation, CFFQ
   staging, danger-close gating, unsafe-correction refusal/override), driven
   end to end through FdcEngine::submitMessage against the committed
   MissionMachine over a stub scenario view.

   What only the wave-3 Level-3 replay can prove: full decision SEQUENCING
   against recorded transcripts (parse -> rules -> machine -> sink over whole
   missions, with the recorded clock). These tests pin the individual rules
   and the plumbing; the replay pins their composition. */

using namespace shitfire;
using namespace shitfire::doctrine;

namespace {

/* Deterministic flavor RNG: cycles a fixed sequence. Default 0.99 fails
   every probability gate (all gates are <= 0.55... except readbackTail 0.45,
   ffeAck 0.4, completeTail 0.5, snideRound 0.55, corrSnark 0.5 — 0.99 fails
   all) and makes pick() choose the last pool line deterministically. */
struct SeqRng : IFlavorRng {
  std::vector<double> vals{0.99};
  std::size_t i = 0;
  int calls = 0;
  double next() override {
    calls++;
    const double v = vals[i % vals.size()];
    i++;
    return v;
  }
};

struct NetEvent {
  NetLineKind kind;
  std::string call, text;
};
struct RecNet : INetSink {
  std::vector<NetEvent> events;
  std::vector<TlogParseClass> parses;
  std::vector<std::pair<std::string, std::string>> spots;  /* loc, form */
  std::vector<std::pair<std::string, double>> syserrs;
  std::vector<std::pair<std::string, double>> posreps;
  void line(NetLineKind kind, const std::string& call,
            const std::string& text) override {
    events.push_back({kind, call, text});
  }
  void parseClass(const std::string&, const TlogParseClass& pc) override {
    parses.push_back(pc);
  }
  void spotMeta(const std::string& loc, const std::string& form) override {
    spots.emplace_back(loc, form);
  }
  void sysErr(const std::string& msg, double num) override {
    syserrs.emplace_back(msg, num);
  }
  void posRepMeta(const std::string& digits, double err) override {
    posreps.emplace_back(digits, err);
  }
  bool any(NetLineKind kind, const std::string& needle) const {
    for (const NetEvent& e : events)
      if (e.kind == kind && e.text.find(needle) != std::string::npos)
        return true;
    return false;
  }
  std::vector<std::string> fdcTexts() const {
    std::vector<std::string> out;
    for (const NetEvent& e : events)
      if (e.kind == NetLineKind::Fdc) out.push_back(e.text);
    return out;
  }
};

/* Configurable IScenarioView stub — land everywhere (H = 5), no friendlies
   or huts unless placed, troops scenario. */
struct StubView : sim::IScenarioView {
  sim::ScenarioType type_ = sim::ScenarioType::Troops;
  sim::Vec2 enemy_{0, 0}, twin_{0, 0};
  bool hasTwin_ = false;
  std::vector<sim::FriendlyPos> friendlies_;
  std::vector<sim::Vec2> huts_;
  double eff_ = 0;
  bool everSup_ = false;
  double alerted_ = 0;
  bool dispersed_ = false;
  sim::Posture posture_ = sim::Posture::Standing;
  bool enemyAlive_ = true, twinAlive_ = true;

  sim::ScenarioType type() const override { return type_; }
  double scenarioSeed() const override { return 42; }
  double scenarioT0() const override { return 0; }
  sim::Vec2 enemy() const override { return enemy_; }
  bool hasEnemy() const override { return true; }
  bool hasTwin() const override { return hasTwin_; }
  sim::Vec2 twin() const override { return twin_; }
  sim::TargetClass targetClass() const override {
    return sim::TargetClass::Personnel;
  }
  double effScale() const override { return 1; }
  double armor() const override { return 1; }
  sim::Posture posture() const override { return posture_; }
  bool dispersed() const override { return dispersed_; }
  int alerted() const override { return static_cast<int>(alerted_); }
  bool enemyAlive() const override { return enemyAlive_; }
  bool twinAlive() const override { return twinAlive_; }
  double eff() const override { return eff_; }
  double twinEff() const override { return 0; }
  bool everSuppressed() const override { return everSup_; }
  bool twinEverSuppressed() const override { return false; }
  std::vector<sim::FriendlyPos> friendlies() const override {
    return friendlies_;
  }
  std::vector<sim::Vec2> civilians() const override { return {}; }
  std::vector<sim::Vec2> huts() const override { return huts_; }
  double groundHeight(double, double) const override { return 5; }
  bool hasLOS(double, double, double, double, double, double) const override {
    return true;
  }
  sim::Vec3 observerEye() const override { return {0, 20, 0}; }
  std::size_t vehicleCount() const override { return 0; }
  sim::VehicleView vehicle(std::size_t) const override { return {}; }
  bool convoyHasPath() const override { return false; }
  sim::Dir2 pathDirAt(double) const override { return {}; }
  double convoyHeadD() const override { return 0; }
  void addEff(double c, bool) override { eff_ += c; }
  void setSuppressed(double, bool) override { everSup_ = true; }
  void raiseSuppressedTo(double) override { everSup_ = true; }
  void bumpAlerted() override { alerted_++; }
  void setDispersed() override { dispersed_ = true; }
  void setPostureProne() override { posture_ = sim::Posture::Prone; }
  void setEnemyAlive(bool a) override { enemyAlive_ = a; }
  void setTwinAlive(bool a) override { twinAlive_ = a; }
  void killVehicle(std::size_t) override {}
};

/* One fully wired net: clock, queue, speaker, sink, machine, spot, engine. */
struct Net {
  sim::SimClock clock;
  sim::EventQueue queue;
  FdcCallsigns calls;
  FdcChapterContext chapter;
  MapConfig map;                      /* originE 20000, originN 50000 */
  QuipSet quips = makeQuips();
  SeqRng flavor;
  RecNet net;
  StubView view;
  sim::SimTunables tun;
  scenario::OpPos op;
  sim::Vec2 battery;
  FdcSpeaker speaker;
  FdcMissionSink sink;
  sim::MissionMachine machine;
  FdcSpotReports spot;
  FdcEngine engine;

  explicit Net(bool strict = false,
               sim::AssetKind asset = sim::AssetKind::Battery155,
               bool chapterActive = false)
      : op{opInit()}, battery{batteryInit()},
        speaker(clock, queue, net, chapter, calls),
        sink(speaker, quips, flavor, net, chapter, calls, map, battery),
        machine(clock, queue, sink, view, tun, map, sim::Difficulty::Normal,
                asset, sim::Vec2{op.x, op.z}, battery, strict),
        spot(clock, queue, speaker, net, quips, flavor, chapter, calls,
             SpotTunables{}, map),
        engine(clock, queue, machine, view, speaker, sink, &spot, quips,
               flavor, net, chapter, calls, map, op, battery) {
    chapter.active = chapterActive || strict ||
                     asset != sim::AssetKind::Battery155;
    chapter.strict = strict;
    chapter.asset = asset;
    sink.setMachine(&machine);
    sink.setSpot(&spot);
    view.enemy_ = target();
    engine.bindScenario(nullptr);
  }
  scenario::OpPos opInit() {
    const WorldXZ w = enToWorld(24000, 51800, map);
    return scenario::OpPos{w.x, w.z, 12};
  }
  sim::Vec2 batteryInit() {
    const WorldXZ w = enToWorld(23000, 51000, map);
    return sim::Vec2{w.x, w.z};
  }
  /* the test target: grid 245 523 -> its 100 m square center */
  sim::Vec2 target() const {
    const WorldXZ w = enToWorld(24550, 52350, map);
    return sim::Vec2{w.x, w.z};
  }
  void drain(double t) {
    clock.now = t;
    queue.run(clock);
  }
  void submit(const std::string& raw) { engine.submitMessage(raw); }
};

bool inPool(const QuipPool& pool, const std::string& text) {
  for (const std::string& l : pool.lines)
    if (l == text) return true;
  return false;
}

}  // namespace

/* ---------------- say-queue timing chain (06-clock-comms.js:200-233) ----- */

TEST_CASE("FDC.say chain: t = max(now, lastT) + delay, lastT advances, "
          "delivery in order", "[fdc_rules]") {
  Net h;
  h.clock.now = 10;
  const double t1 = h.speaker.say("ALPHA", 0.9);
  const double t2 = h.speaker.say("BRAVO", 1.5);
  CHECK(t1 == 10.9);
  CHECK(t2 == 12.4);
  CHECK(h.speaker.lastT() == 12.4);
  /* now beyond the chain: max(now, lastT) restarts from now */
  h.drain(20);
  const double t3 = h.speaker.say("CHARLIE", 1.0);
  CHECK(t3 == 21.0);
  h.drain(25);
  const auto fdc = h.net.fdcTexts();
  REQUIRE(fdc.size() == 3);
  CHECK(fdc[0] == "ALPHA");
  CHECK(fdc[1] == "BRAVO");
  CHECK(fdc[2] == "CHARLIE");
  CHECK(h.speaker.lastMsg() == "CHARLIE");
}

TEST_CASE("NET18 backlog cap: lastT = min(lastT, now + 5)", "[fdc_rules]") {
  Net h;
  h.clock.now = 100;
  h.speaker.setLastT(130);
  h.speaker.capBacklog();
  CHECK(h.speaker.lastT() == 105.0);   /* 12-cff-handling.js:875 */
  h.speaker.setLastT(102);
  h.speaker.capBacklog();
  CHECK(h.speaker.lastT() == 102.0);   /* cap never PUSHES the chain out */
}

TEST_CASE("backlog cap + busy-net line through submitMessage", "[fdc_rules]") {
  Net h;
  h.clock.now = 50;
  h.speaker.setLastT(80);   /* 30 s of stale backlog */
  h.submit("xyzzy plugh, over");
  CHECK(h.speaker.lastT() <= 50 + 5 + 1.0 + 1e-9);   /* cap, then one reply */
  /* NET25 — the synchronous stand-by line (after the cap, 5 > 2.5) */
  CHECK(h.net.any(NetLineKind::Sys, "Net is mid-transmission"));
}

/* ---------------- sanctioned-flavor gates (events.h sink contract) ------- */

TEST_CASE("gated line: failed coin leaves lastT untouched and draws no pick",
          "[fdc_rules]") {
  Net h;
  h.flavor.vals = {0.9};   /* >= 0.4 -> gate fails */
  sim::FdcLine l{sim::FdcLineId::FfeAckFlavor};
  l.delay = 0.8;
  l.gateProbability = 0.4;   /* 10-mission.js:824 */
  const double before = h.speaker.lastT();
  h.sink.sayLine(l);
  CHECK(h.speaker.lastT() == before);
  CHECK(h.flavor.calls == 1);   /* the coin only — pick() never ran */
  CHECK(h.queue.size() == 0);
}

TEST_CASE("gated line: passing coin airs and advances the chain",
          "[fdc_rules]") {
  Net h;
  h.flavor.vals = {0.1, 0.0};   /* coin passes, then pick draws */
  sim::FdcLine l{sim::FdcLineId::FfeAckFlavor};
  l.delay = 0.8;
  l.gateProbability = 0.4;
  const double t = h.sink.sayLine(l);
  CHECK(t == 0.8);
  CHECK(h.speaker.lastT() == 0.8);
  h.drain(1);
  REQUIRE(h.net.fdcTexts().size() == 1);
  CHECK(inPool(h.quips.ffeAck, h.net.fdcTexts()[0]));
}

TEST_CASE("pick(): no-immediate-repeat rotation; single-line pool draws "
          "nothing", "[fdc_rules]") {
  Net h;
  h.flavor.vals = {0.0};   /* always index 0 */
  QuipPool p;
  p.lines = {"a", "b", "c", "d"};
  CHECK(pick(p, h.flavor) == "a");
  CHECK(pick(p, h.flavor) == "b");   /* collision with _last steps forward */
  CHECK(pick(p, h.flavor) == "a");
  const int before = h.flavor.calls;
  CHECK(pick(h.quips.complete, h.flavor) ==
        h.quips.complete.lines[0]);   /* 07:292 — length 1, no draw */
  CHECK(h.flavor.calls == before);
}

TEST_CASE("rant lines chain 1.0/2.2/2.8 on the say clock", "[fdc_rules]") {
  Net h;
  sim::FdcLine l{sim::FdcLineId::RantFratricide};
  l.delay = 1.0;
  const double t = h.sink.sayLine(l);
  CHECK(t == 6.0);   /* 1.0 + 2.2 + 2.8 (10-mission.js:1213-1215) */
  h.drain(10);
  const auto fdc = h.net.fdcTexts();
  REQUIRE(fdc.size() == 3);
  CHECK(fdc[0].find("CHECK FIRE") != std::string::npos);
}

/* ---------------- G17 delivery-time callsign swap ------------------------ */

TEST_CASE("callsign swap happens at delivery time, per the chapter that owns "
          "the net when the line airs", "[fdc_rules]") {
  Net h;
  h.speaker.say("MUSTANG 12, HELLHOUND " "\xE2\x80\x94" " HELLHOUND FIRES "
                "standing by.", 1.0);
  /* the chapter flips to the mortar net BEFORE the line airs (06:217-225) */
  h.chapter.active = true;
  h.chapter.asset = sim::AssetKind::Mortar60;
  h.drain(2);
  REQUIRE(h.net.fdcTexts().size() == 1);
  const std::string out = h.net.fdcTexts()[0];
  CHECK(out.find("HACKSAW FIRES") != std::string::npos);
  CHECK(out.find("HACKSAW ") != std::string::npos);
  CHECK(out.find("HELLHOUND") == std::string::npos);
  /* delivered under the mortar callsign too */
  CHECK(h.net.events.back().call == "HACKSAW FIRES");
}

/* ---------------- strict-net format verdicts (12-cff:167-179) ------------ */

namespace {
ParsedMessage msg(const std::string& raw, const std::string& warno,
                  const std::string& desc, bool over) {
  ParsedMessage p;
  p.raw = raw;
  p.warno = warno;
  p.desc = desc;
  if (over) p.toks.push_back("over");
  return p;
}
}

TEST_CASE("strict-net challenges, in source order", "[fdc_rules]") {
  FdcCallsigns calls;
  FdcChapterContext ch;
  ch.active = true;
  ch.strict = true;

  /* no callsigns at all */
  CHECK(strictChallenge(msg("adjust fire grid 245 523", "adjust", "troops",
                            true), ch, calls)
            .find("this net runs callsigns") != std::string::npos);
  /* wrong unit on a mortar net */
  ch.asset = sim::AssetKind::Mortar60;
  CHECK(strictChallenge(msg("hellhound this is mustang 12 adjust fire",
                            "adjust", "troops", true), ch, calls)
            .find("HELLHOUND is the one-five-five battery") !=
        std::string::npos);
  ch.asset = sim::AssetKind::Battery155;
  /* callsigns fine, no warning order */
  CHECK(strictChallenge(msg("hellhound this is mustang 12 grid 245 523",
                            "none", "troops", true), ch, calls)
            .find("no warning order heard") != std::string::npos);
  /* no description */
  CHECK(strictChallenge(msg("hellhound mustang 12 adjust fire grid 245 523",
                            "adjust", "", true), ch, calls)
            .find("no target description") != std::string::npos);
  /* no OVER */
  CHECK(strictChallenge(msg("hellhound mustang 12 adjust fire grid 245 523",
                            "adjust", "troops", false), ch, calls)
            .find("transmissions end with OVER") != std::string::npos);
  /* clean call passes */
  CHECK(strictChallenge(msg("hellhound mustang 12 adjust fire grid 245 523",
                            "adjust", "troops", true), ch, calls).empty());
}

/* ---------------- deviation rounding tiers (12-cff:626/:714) ------------- */

TEST_CASE("deviation in tens, range in hundreds, fifties entering FFE",
          "[fdc_rules]") {
  CHECK_FALSE(strictRoundingViolation(50, 200, false));
  CHECK(strictRoundingViolation(55, 200, false));    /* deviation not in tens */
  CHECK(strictRoundingViolation(50, 250, false));    /* 50 m only entering FFE */
  CHECK_FALSE(strictRoundingViolation(50, 250, true));
  CHECK(strictRoundingViolation(0, 150, false));
  CHECK_FALSE(strictRoundingViolation(0, 150, true));
  CHECK_FALSE(strictRoundingViolation(-40, -300, false));   /* signs ignored */
}

/* ---------------- G2 direction diagnosis (12-cff:477-511) ---------------- */

TEST_CASE("direction diagnosis: magnetic, reversed, rough, honest",
          "[fdc_rules]") {
  const double trueAz = 1000;
  double err = 0;
  CHECK(diagnoseDirection(1000, trueAz, 7, &err) == DirDiagnosis::None);
  CHECK(err == 0.0);
  /* sent the raw magnetic reading: exactly one G-M angle low */
  CHECK(diagnoseDirection(gridToMag(trueAz, 7), trueAz, 7, &err) ==
        DirDiagnosis::Magnetic);
  /* applied the G-M angle backwards: one angle high */
  CHECK(diagnoseDirection(magToGrid(trueAz, 7), trueAz, 7, &err) ==
        DirDiagnosis::Reversed);
  /* the harness case from the source comment (12-cff:495-500): an honest
     90-mil-sloppy azimuth sits 34 off the reversed candidate but must NOT
     be accused — the candidate has to beat the raw error by 60. */
  CHECK(diagnoseDirection(1090, trueAz, 7, &err) == DirDiagnosis::Rough);
  CHECK(err == 90.0);
  /* 70 off: imprecise but under the 80-mil coach threshold */
  CHECK(diagnoseDirection(1070, trueAz, 7, &err) == DirDiagnosis::None);
  /* wrap-around: 6390 vs 10 is 20 mils, not 6380 */
  CHECK(diagnoseDirection(6390, 10, 7, &err) == DirDiagnosis::None);
  CHECK(err == 20.0);
}

/* ---------------- NET17 location validation (12-cff:104-130) ------------- */

TEST_CASE("locError: shift, polar, grid-length and map-sheet gates",
          "[fdc_rules]") {
  MapConfig map;
  QuipSet quips = makeQuips();
  SeqRng rng;
  std::vector<scenario::Kp> kps{{"1001", "BREWERY", -450, 2650}};

  ParsedMessage shift;
  shift.method = "shift";
  shift.kpId = "1002";
  CHECK(locError(shift, kps, false, false, map, quips, rng)
            .find("no known point 1002") != std::string::npos);
  shift.kpId = "1001";
  CHECK(locError(shift, kps, false, false, map, quips, rng).empty());

  ParsedMessage polar;
  polar.method = "polar";
  polar.dirMils = 4400;
  polar.distM = 2100;
  CHECK(locError(polar, kps, false, false, map, quips, rng)
            .find("POLAR") != std::string::npos);   /* G9 — no posrep */
  CHECK(locError(polar, kps, true, false, map, quips, rng).empty());
  polar.distM = 50;   /* under the 100 m floor */
  CHECK(inPool(quips.badGrid,
               locError(polar, kps, true, false, map, quips, rng)));

  ParsedMessage grid;
  grid.method = "grid";
  grid.digits = "245523";
  CHECK(locError(grid, kps, false, false, map, quips, rng).empty());
  grid.digits = "24555235";
  CHECK(locError(grid, kps, false, false, map, quips, rng).empty());
  grid.digits = "9999999";   /* 7 digits is no grid */
  CHECK(inPool(quips.badGrid,
               locError(grid, kps, false, false, map, quips, rng)));
  grid.digits = "999999";    /* off the map sheet */
  CHECK(inPool(quips.badGrid,
               locError(grid, kps, false, false, map, quips, rng)));
  grid.digits = "2455052350";   /* ten digits: sunlamp only (11c) */
  CHECK(inPool(quips.badGrid,
               locError(grid, kps, false, false, map, quips, rng)));
  CHECK(locError(grid, kps, false, true, map, quips, rng).empty());
}

TEST_CASE("locEchoOf keeps the observer's digit strings verbatim",
          "[fdc_rules]") {
  ParsedMessage g6;
  g6.method = "grid";
  g6.digits = "045123";
  CHECK(locEchoOf(g6) == "GRID 045 123");   /* string slices (12-cff:141) */
  ParsedMessage pol;
  pol.method = "polar";
  pol.dirMils = 6500;   /* wraps to 0100 */
  pol.distM = 2100;
  CHECK(locEchoOf(pol) == "DIRECTION 0100, DISTANCE 2100");
  ParsedMessage sh;
  sh.method = "shift";
  sh.kpId = "1001";
  sh.sRight = -110;
  sh.sAdd = 400;
  CHECK(locEchoOf(sh) == "SHIFT KNOWN POINT 1001, LEFT 110, ADD 400");
}

/* ---------------- raw-text sheaf/fuze requests (10:980/:1013) ------------ */

TEST_CASE("sheaf/fuze requests ride the raw text; parallel and special "
          "canonicalise to linear", "[fdc_rules]") {
  sim::SheafSpec s;
  CHECK_FALSE(sheafRequestFromRaw("troops in the open", s));
  REQUIRE(sheafRequestFromRaw("fire for effect converged sheaf over", s));
  CHECK(s.kind == sim::SheafKind::Converged);
  CHECK(s.source == "requested");
  REQUIRE(sheafRequestFromRaw("special sheaf over", s));
  CHECK(s.kind == sim::SheafKind::Linear);
  sim::FuzeSpec f;
  CHECK_FALSE(fuzeRequestFromRaw("troops dug in", f));
  REQUIRE(fuzeRequestFromRaw("fuze victor tango over", f));
  CHECK(f.kind == sim::FuzeKind::Vt);
  REQUIRE(fuzeRequestFromRaw("fuze quick over", f));
  CHECK(f.kind == sim::FuzeKind::Pd);
  REQUIRE(fuzeRequestFromRaw("fuze delay over", f));
  CHECK(f.kind == sim::FuzeKind::Delay);
}

/* ---------------- the single door: oversize, note, parse record ---------- */

TEST_CASE("oversize transmission is refused loudly without parsing "
          "(BUILD4/NET9)", "[fdc_rules]") {
  Net h;
  const std::string big(1001, 'a');
  h.submit(big);
  REQUIRE(h.net.syserrs.size() == 1);
  CHECK(h.net.syserrs[0].first == "oversize transmission refused");
  CHECK(h.net.syserrs[0].second == 1001.0);
  CHECK(h.net.parses.empty());   /* refused BEFORE the parser */
  h.drain(5);
  CHECK(h.net.any(NetLineKind::Fdc, "longer than my enlistment"));
  /* the obs echo is the 120-char head plus an ellipsis */
  REQUIRE(h.net.events[0].kind == NetLineKind::Obs);
  CHECK(h.net.events[0].text.size() == 120 + 1 + 3);
}

TEST_CASE("NET7 note channel: verbatim, no parse, no reply", "[fdc_rules]") {
  Net h;
  h.submit("note: reticle drifts left at 14x");
  REQUIRE(h.net.events.size() == 1);
  CHECK(h.net.events[0].kind == NetLineKind::Note);
  CHECK(h.net.events[0].text == "reticle drifts left at 14x");
  CHECK(h.net.parses.empty());
  CHECK(h.queue.size() == 0);
}

TEST_CASE("every parsed transmission writes the TLOG parse record",
          "[fdc_rules]") {
  Net h;
  h.submit("adjust fire, grid 245 523, troops in the open, over");
  REQUIRE(h.net.parses.size() == 1);
  CHECK(h.net.parses[0].ptype == "cff");
  CHECK(h.net.parses[0].method == "grid");
  CHECK(h.net.parses[0].warno == "adjust");
}

/* ---------------- one-shot CFF -> mission (the forgiving path) ----------- */

TEST_CASE("one-shot grid CFF opens the mission: readback, MTO, state",
          "[fdc_rules]") {
  Net h;
  h.submit("adjust fire, grid 245 523, troops in the open, over");
  REQUIRE(h.machine.activeMission() != nullptr);
  const sim::Mission* m = h.machine.activeMission();
  REQUIRE(m->mto.has_value());
  CHECK(m->mto->tgt == "7001");
  CHECK(m->method == sim::MethodKind::Grid);
  h.drain(10);
  CHECK(h.net.any(NetLineKind::Fdc, "ADJUST FIRE, GRID 245 523"));
  CHECK(h.net.any(NetLineKind::Fdc,
                  "TARGET NUMBER ALPHA ALPHA 7001"));
  CHECK(h.net.any(NetLineKind::Sys, "SHEAF:"));
  CHECK(h.net.any(NetLineKind::Sys, "FUZE:"));
}

TEST_CASE("MTO readback: gist scoring through the unknown-claim path (G11)",
          "[fdc_rules]") {
  Net h;
  h.submit("adjust fire, grid 245 523, troops in the open, over");
  h.submit("one gun in adjust battery in effect one round 155 he fuze pd "
           "target number alpha alpha 7001, over");
  CHECK(h.machine.activeMission()->mto->read);
  h.drain(30);
  CHECK(h.net.any(NetLineKind::Fdc, "READBACK CORRECT, OUT."));
}

/* ---------------- danger close (F2/F6) ----------------------------------- */

TEST_CASE("danger-close gate: friendlies inside the radius demand the "
          "proword; radius is per asset", "[fdc_rules]") {
  {
    Net h;   /* battery: 600 m radius, friendly at 500 m */
    const sim::Vec2 tgt = h.target();
    h.view.friendlies_ = {{tgt.x + 500, tgt.z, 0, false}};
    h.submit("adjust fire, grid 245 523, troops in the open, over");
    CHECK(h.machine.activeMission() == nullptr);   /* refused */
    h.drain(5);
    bool sawDc = false;
    for (const auto& t : h.net.fdcTexts())
      if (inPool(h.quips.dangerClose, t)) sawDc = true;
    CHECK(sawDc);
    /* the compliant resend goes through */
    h.submit("adjust fire, grid 245 523, danger close, troops in the open, "
             "over");
    REQUIRE(h.machine.activeMission() != nullptr);
    CHECK(h.machine.activeMission()->gridStr.find("DANGER CLOSE") !=
          std::string::npos);
  }
  {
    /* mortar60: 250 m radius — 500 m out needs no proword (F2); the sixty
       demands an 8-digit grid (:161-165). */
    Net h(false, sim::AssetKind::Mortar60);
    const WorldXZ w = enToWorld(24555, 52355, h.map);
    h.view.enemy_ = {w.x, w.z};
    h.view.friendlies_ = {{w.x + 500, w.z, 0, false}};
    h.submit("adjust fire, grid 2455 5235, troops in the open, over");
    CHECK(h.machine.activeMission() != nullptr);
  }
}

TEST_CASE("60 mike mike refuses a six-digit grid", "[fdc_rules]") {
  Net h(false, sim::AssetKind::Mortar60);
  h.submit("adjust fire, grid 245 523, troops in the open, over");
  CHECK(h.machine.activeMission() == nullptr);
  h.drain(5);
  CHECK(h.net.any(NetLineKind::Fdc, "I need an ADDRESS"));
}

/* ---------------- G9 posrep + polar -------------------------------------- */

TEST_CASE("polar needs a position report first; then resolves from the "
          "REPORTED position", "[fdc_rules]") {
  Net h;
  h.submit("adjust fire, direction 4400, distance 2100, troops in the open, "
           "over");
  CHECK(h.machine.activeMission() == nullptr);
  h.drain(5);
  CHECK(h.net.any(NetLineKind::Fdc, "I cannot resolve a POLAR mission"));
  h.submit("position grid 240 518, over");
  REQUIRE(h.engine.posRep().has_value());
  CHECK(h.engine.posRep()->digits == "240518");
  REQUIRE(h.net.posreps.size() == 1);
  h.submit("adjust fire, direction 4400, distance 2100, troops in the open, "
           "over");
  REQUIRE(h.machine.activeMission() != nullptr);
  /* aim = reported position + polar vector (12-cff:221-223) */
  const WorldXZ rep = enToWorld(24050, 51850, h.map);
  const double az = std::fmod(4400.0, 6400.0) / MILS_PER_RAD;
  const sim::Mission* m = h.machine.activeMission();
  CHECK(std::fabs(m->aim.x - (rep.x + std::sin(az) * 2100)) < 1e-9);
  CHECK(std::fabs(m->aim.z - (rep.z - std::cos(az) * 2100)) < 1e-9);
}

/* ---------------- CFFQ: the three-transmission call (G22) ---------------- */

TEST_CASE("CFFQ: T1 warno, T2 location, T3 description assemble one mission",
          "[fdc_rules]") {
  Net h;
  h.submit("adjust fire, over");                       /* T1 — bare warno */
  CHECK(h.engine.cffqOpen());
  CHECK(h.engine.cffqStage() == 1);
  h.submit("grid 245 523, over");                      /* T2 — location */
  CHECK(h.engine.cffqStage() == 2);
  h.submit("troops in the open, over");                /* T3 — description */
  CHECK_FALSE(h.engine.cffqOpen());
  REQUIRE(h.machine.activeMission() != nullptr);
  CHECK(h.machine.activeMission()->desc == "troops in the open");
  h.drain(15);
  CHECK(h.net.any(NetLineKind::Fdc, "SEND YOUR TARGET LOCATION, OVER."));
  CHECK(h.net.any(NetLineKind::Fdc, "GRID 245 523, OUT."));
  CHECK(h.net.any(NetLineKind::Fdc, "SEND YOUR TARGET DESCRIPTION, OVER."));
  /* the staged readback reads back T3 only (G22) */
  CHECK(h.net.any(NetLineKind::Fdc, "TROOPS IN THE OPEN, OUT."));
}

TEST_CASE("CFFQ lapse: 75 s of silence clears the queue and says so",
          "[fdc_rules]") {
  Net h;
  h.submit("adjust fire, over");
  CHECK(h.engine.cffqOpen());
  h.clock.now = 76;
  h.engine.tick();
  CHECK_FALSE(h.engine.cffqOpen());
  h.drain(80);
  CHECK(h.net.any(NetLineKind::Fdc, "never finished it"));
}

TEST_CASE("NET14: a refused T3 restores the queue at stage 2 for the "
          "compliant resend", "[fdc_rules]") {
  Net h;
  const sim::Vec2 tgt = h.target();
  h.view.friendlies_ = {{tgt.x + 500, tgt.z, 0, false}};   /* danger close */
  h.submit("adjust fire, over");
  h.submit("grid 245 523, over");
  h.submit("troops in the open, over");   /* refused: no DANGER CLOSE */
  CHECK(h.machine.activeMission() == nullptr);
  CHECK(h.engine.cffqOpen());             /* restored, not destroyed */
  CHECK(h.engine.cffqStage() == 2);
  h.submit("danger close, troops in the open, over");
  REQUIRE(h.machine.activeMission() != nullptr);
  CHECK(h.machine.activeMission()->gridStr.find("DANGER CLOSE") !=
        std::string::npos);
}

TEST_CASE("NET17: an unworkable T2 location is refused at the readback and "
          "the queue stays at stage 1", "[fdc_rules]") {
  Net h;
  h.submit("adjust fire, over");
  h.submit("grid 999 999, over");   /* off the sheet */
  CHECK(h.engine.cffqOpen());
  CHECK(h.engine.cffqStage() == 1);
  h.submit("grid 245 523, over");
  CHECK(h.engine.cffqStage() == 2);
}

/* ---------------- adjust flow: G8, unsafe corrections, creeping fire ----- */

namespace {
/* Open a grid mission and run it to ADJUSTING (first round impacts). */
void toAdjusting(Net& h, const std::string& call =
                     "adjust fire, grid 245 523, troops in the open, over") {
  h.submit(call);
  REQUIRE(h.machine.activeMission() != nullptr);
  h.drain(h.clock.now + 120);
  REQUIRE(h.machine.state() == sim::MissionPhase::Adjusting);
}
std::string dirTo(Net& h) {
  const sim::Vec2 aim = h.machine.activeMission()->aim;
  const double mils = radToMils(azTo(h.op.x, h.op.z, aim.x, aim.z));
  return "direction " + jsNum(js::round(mils / 10) * 10) + ", over";
}
}

TEST_CASE("G8: a correction before OT DIRECTION on a grid mission is refused, "
          "twice, differently", "[fdc_rules]") {
  Net h;
  toAdjusting(h);
  const std::size_t rounds = h.machine.activeMission()->rounds.size();
  h.submit("add 200, over");
  CHECK(h.machine.activeMission()->rounds.size() == rounds);   /* not fired */
  h.submit("add 200, over");
  h.drain(h.clock.now + 20);
  CHECK(h.net.any(NetLineKind::Fdc, "I have no OT DIRECTION"));
  CHECK(h.net.any(NetLineKind::Fdc, "STILL no OT direction"));
  /* DIRECTION then the correction goes through */
  h.submit(dirTo(h));
  h.submit("add 200, over");
  CHECK(h.machine.state() == sim::MissionPhase::Shot);
}

TEST_CASE("unsafe correction: refused once, identical resend overrides "
          "(his funeral)", "[fdc_rules]") {
  Net h;
  toAdjusting(h);
  h.submit(dirTo(h));
  /* place a friendly exactly where ADD 700 walks the aimpoint */
  const sim::Vec2 aim = h.machine.activeMission()->aim;
  const double otAz = azTo(h.op.x, h.op.z, aim.x, aim.z);
  h.view.friendlies_ = {{aim.x + std::sin(otAz) * 700,
                         aim.z - std::cos(otAz) * 700, 0, false}};
  const std::size_t rounds = h.machine.activeMission()->rounds.size();
  h.submit("add 700, over");
  CHECK(h.machine.activeMission()->rounds.size() == rounds);   /* refused */
  CHECK(h.machine.state() == sim::MissionPhase::Adjusting);
  h.drain(h.clock.now + 5);
  bool refusal = false, insist = false;
  for (const auto& t : h.net.fdcTexts()) {
    if (t.find("Say again, over.") != std::string::npos &&
        t.find("the friendly position") != std::string::npos)
      refusal = true;
  }
  CHECK(refusal);
  h.submit("add 700, over");   /* the identical resend overrides */
  CHECK(h.machine.state() == sim::MissionPhase::Shot);
  h.drain(h.clock.now + 5);
  for (const auto& t : h.net.fdcTexts())
    if (inPool(h.quips.unsafeInsist, t)) insist = true;
  CHECK(insist);
}

TEST_CASE("NET23: danger close demands creeping fire; the resend overrides "
          "and is noted once", "[fdc_rules]") {
  Net h;
  const sim::Vec2 tgt = h.target();
  h.view.friendlies_ = {{tgt.x, tgt.z + 500, 0, false}};
  toAdjusting(h, "adjust fire, grid 245 523, danger close, troops in the "
                 "open, over");
  h.submit(dirTo(h));
  h.submit("add 940, over");   /* > 100 m on a DC mission: challenged */
  CHECK(h.machine.state() == sim::MissionPhase::Adjusting);
  h.drain(h.clock.now + 5);
  CHECK(h.net.any(NetLineKind::Fdc, "we CREEP"));
  const sim::Mission* m = h.machine.activeMission();
  bool noted = false;
  for (const auto& n : m->notes)
    if (n.find("creeping fire") != std::string::npos) noted = true;
  CHECK(noted);
  h.submit("add 940, over");   /* identical resend: sent as-is */
  CHECK(h.machine.state() == sim::MissionPhase::Shot);
  h.drain(h.clock.now + 5);
  CHECK(h.net.any(NetLineKind::Fdc, "On your head"));
}

TEST_CASE("F5: a stray number refuses the half-correction", "[fdc_rules]") {
  Net h;
  toAdjusting(h);
  h.submit(dirTo(h));
  h.submit("left 50, 200, over");   /* 200 attaches to nothing */
  CHECK(h.machine.state() == sim::MissionPhase::Adjusting);
  h.drain(h.clock.now + 5);
  CHECK(h.net.any(NetLineKind::Fdc, "I cannot attach to anything"));
}

/* ---------------- strict-net gates through the full door ----------------- */

TEST_CASE("strict net: unrounded correction is refused; MTO readback is "
          "demanded once", "[fdc_rules]") {
  Net h(true);
  toAdjusting(h, "hellhound, this is mustang 12, adjust fire, grid 245 523, "
                 "troops in the open, over");
  h.submit(dirTo(h));
  /* first correction: the strict net demands the skipped MTO readback (G11) */
  h.submit("left 50, add 200, over");
  CHECK(h.machine.state() == sim::MissionPhase::Adjusting);
  h.drain(h.clock.now + 5);
  CHECK(h.net.any(NetLineKind::Fdc,
                  "you did not read back my message to observer"));
  /* nagged once — now the rounding gate speaks */
  h.submit("left 55, add 200, over");
  CHECK(h.machine.state() == sim::MissionPhase::Adjusting);
  h.drain(h.clock.now + 5);
  CHECK(h.net.any(NetLineKind::Fdc, "deviation in TENS, range in HUNDREDS"));
  h.submit("left 50, add 200, over");
  CHECK(h.machine.state() == sim::MissionPhase::Shot);
}

/* ---------------- EOM: RREMS, overclaim, AAR handoff --------------------- */

TEST_CASE("end of mission: BDA logged as sent, overclaim noted, AAR "
          "scheduled at lastT + 1.6", "[fdc_rules]") {
  Net h;
  bool aarRan = false;
  h.engine.hooks.showAar = [&aarRan]() { aarRan = true; };
  toAdjusting(h);
  h.submit("end of mission, target destroyed, record as target, over");
  const sim::Mission* m = h.machine.activeMission();
  REQUIRE(m != nullptr);
  CHECK(m->done);
  CHECK(m->recorded);
  CHECK(m->bdaClaim == "destroyed");
  bool overclaim = false;
  for (const auto& n : m->notes)
    if (n.find("Surveillance overclaimed") != std::string::npos)
      overclaim = true;
  CHECK(overclaim);   /* one adjust round did not destroy anything */
  CHECK(h.machine.state() == sim::MissionPhase::Aar);
  CHECK(h.machine.recTgtKeys().size() == 1);
  h.drain(h.clock.now + 60);
  CHECK(aarRan);
  CHECK(h.net.any(NetLineKind::Fdc, "END OF MISSION, TARGET DESTROYED, OUT."));
  CHECK(h.net.any(NetLineKind::Fdc, "RECORDED AS TARGET "));
  /* NET26 — a second EOM acknowledges and stops */
  const std::size_t before = h.net.parses.size();
  h.submit("end of mission, over");
  CHECK(h.net.parses.size() == before + 1);
  h.drain(h.clock.now + 10);
  CHECK(h.net.any(NetLineKind::Fdc, "that mission is already closed"));
}

/* ---------------- SUGG5: two missions, routed traffic -------------------- */

TEST_CASE("two open missions: unprefixed traffic is challenged, target-number "
          "routing switches the active mission", "[fdc_rules]") {
  Net h;
  h.view.hasTwin_ = true;
  const WorldXZ tw = enToWorld(25500, 52800, h.map);
  h.view.twin_ = {tw.x, tw.z};
  toAdjusting(h);
  h.submit("adjust fire, grid 255 528, second squad in the open, over");
  REQUIRE(h.machine.openMissions().size() == 2);
  CHECK(h.machine.activeMission()->mto->tgt == "7011");   /* shifted by ten */
  h.drain(h.clock.now + 120);
  h.submit("add 50, over");   /* ambiguous with two on the net */
  h.drain(h.clock.now + 5);
  CHECK(h.net.any(NetLineKind::Fdc, "TWO MISSIONS ON THIS NET"));
  h.submit("target aa 7001, end of mission, target suppressed, over");
  CHECK(h.machine.openMissions().size() == 1);   /* routed EOM closed 7001 */
  h.drain(h.clock.now + 5);
  CHECK(h.net.any(NetLineKind::Fdc, "IS STILL YOURS"));
}

/* ---------------- NET4 streaks ------------------------------------------- */

TEST_CASE("no-mission streak escalates to the rant on the third strike",
          "[fdc_rules]") {
  Net h;
  h.submit("add 50, over");
  h.submit("add 50, over");
  h.submit("add 50, over");
  h.drain(20);
  int rants = 0, plain = 0;
  for (const auto& t : h.net.fdcTexts()) {
    if (inPool(h.quips.rantLost, t)) rants++;
    if (inPool(h.quips.noMission, t)) plain++;
  }
  CHECK(plain == 2);
  CHECK(rants == 1);
}

/* ---------------- sink text composition spot-checks ---------------------- */

TEST_CASE("sink composes machine payloads into the source sentences",
          "[fdc_rules]") {
  Net h;
  {
    sim::FdcLine l{sim::FdcLineId::TimeOfFlight};
    l.delay = 0.9;
    l.num0 = 27;
    h.sink.sayLine(l);
  }
  {
    sim::FdcLine l{sim::FdcLineId::UnitStatusReport};
    l.delay = 1.2;
    l.num0 = 240; l.num1 = 48; l.num2 = 36;
    h.sink.sayLine(l);
  }
  {
    sim::FdcLine l{sim::FdcLineId::FpfNumberAck};
    l.delay = 0.8;
    l.num0 = 2; l.num1 = 20; l.num2 = -25;
    h.sink.sayLine(l);
  }
  h.drain(10);
  const auto fdc = h.net.fdcTexts();
  REQUIRE(fdc.size() == 3);
  CHECK(fdc[0] == "TIME OF FLIGHT 27 SECONDS, OVER.");
  CHECK(fdc[1].find("FIRE UNIT STATUS: SIX GUNS, 155, GRID ") !=
        std::string::npos);
  CHECK(fdc[1].find("HE 240 ROUNDS, SMOKE 48, ILLUM 36") != std::string::npos);
  CHECK(fdc[2] == "NUMBER 2, RIGHT 20, DROP 25, OUT.");
}

/* ---------------- spot reports (stream #8) + callers (NET1) -------------- */

TEST_CASE("spot report content comes from the seeded stream, not the flavor "
          "rng; forms and metadata match", "[fdc_rules]") {
  scenario::ScenarioState scn;
  scn.type = "troops";
  scn.difficulty = "normal";
  scn.seed = 42;
  scenario::WorldLayout world;
  MapConfig map;
  const WorldXZ e = enToWorld(24550, 52350, map);
  scn.enemy = {e.x, e.z};
  scenario::Facility af;
  af.kind = "airfield";
  af.name = "AIRFIELD";
  const WorldXZ afw = enToWorld(24000, 52000, map);
  af.x = afw.x;
  af.z = afw.z;
  world.facilities.push_back(af);

  auto runOnce = [&](std::vector<double> flavorVals) {
    auto h = std::make_unique<Net>();
    h->flavor.vals = std::move(flavorVals);
    const WorldXZ opw = enToWorld(24000, 51800, map);
    h->spot.bind(&scn, &world, scenario::OpPos{opw.x, opw.z, 12});
    h->spot.sendSpotReport(SpotReason::Start);
    h->drain(60);
    return std::make_pair(h->net.fdcTexts(), h->net.spots);
  };
  const auto a = runOnce({0.99});
  const auto b = runOnce({0.11, 0.23, 0.37});   /* different flavor stream */
  REQUIRE(a.first.size() >= 3);
  REQUIRE(a.first.size() == b.first.size());
  /* every line but the flavor spotTail is identical: seeded stream #8 */
  for (std::size_t i = 0; i + 1 < a.first.size(); i++)
    CHECK(a.first[i] == b.first[i]);
  CHECK(a.second == b.second);   /* same loc + form metadata */
  REQUIRE(a.second.size() == 1);
  CHECK((a.second[0].second == "salute" || a.second[0].second == "loose"));
  /* the cue orients off the landmark, in the LOCATION metadata */
  CHECK(a.second[0].first.find("OF THE AIRFIELD") != std::string::npos);
  /* the displace re-cue */
  auto h = std::make_unique<Net>();
  const WorldXZ opw = enToWorld(24000, 51800, map);
  h->spot.bind(&scn, &world, scenario::OpPos{opw.x, opw.z, 12});
  h->spot.sendSpotReport(SpotReason::Displace);
  h->drain(60);
  REQUIRE(h->net.spots.size() == 1);
  CHECK(h->net.spots[0].second == "update");
  CHECK(h->net.any(NetLineKind::Fdc, "UPDATED SPOT REPORT"));
}

TEST_CASE("spot location ladder: easy names the grid square, normal/hard "
          "never state a distance to the target", "[fdc_rules]") {
  scenario::ScenarioState scn;
  scn.type = "troops";
  scn.seed = 7;
  scenario::WorldLayout world;   /* no landmarks -> tower bearings */
  MapConfig map;
  Net h;
  const WorldXZ opw = enToWorld(24000, 51800, map);
  h.spot.bind(&scn, &world, scenario::OpPos{opw.x, opw.z, 12});
  const WorldXZ e = enToWorld(24550, 52350, map);
  CHECK(h.spot.spotLocation(e.x, e.z, "easy") ==
        "GRID SQUARE 2452");   /* the 1 km box, nothing finer (07:381) */
  const std::string norm = h.spot.spotLocation(e.x, e.z, "normal");
  CHECK(norm.find("BEARING ") != std::string::npos);
  CHECK(norm.find("THOUSAND OUT") != std::string::npos);
  const std::string hard = h.spot.spotLocation(e.x, e.z, "hard");
  CHECK(hard.find("QUADRANT FROM YOUR TOWER") != std::string::npos);
}

TEST_CASE("NET1 caller net: unparseable traffic naming the caller goes to "
          "the rock eater, never the FDC", "[fdc_rules]") {
  Net h;
  scenario::ScenarioState scn;
  scn.type = "callin";
  scn.seed = 9;
  scenario::CallerState c;
  c.i = 3;   /* SPADE 4 */
  c.repAz = 1.0;
  c.repDist = 400;
  scn.caller = c;
  const WorldXZ tw = enToWorld(24400, 52100, h.map);
  scn.team = WorldXZ{tw.x, tw.z};
  scenario::WorldLayout world;
  h.engine.bindScenario(&scn);
  h.spot.bind(&scn, &world, h.op);
  CHECK(h.spot.callerFirstWordLower(3) == "spade");
  h.submit("spade 4, uh, hang tight, over");   /* unknown + callsign */
  h.drain(10);
  bool callerLine = false, fdcReply = false;
  for (const auto& e : h.net.events) {
    if (e.kind == NetLineKind::Caller && e.call == "SPADE 4")
      callerLine = true;
    if (e.kind == NetLineKind::Fdc) fdcReply = true;
  }
  CHECK(callerLine);
  CHECK_FALSE(fdcReply);   /* the FDC never sees caller-net traffic */
  /* "say again" repeats the caller's own location report */
  h.submit("spade 4, say again, over");
  h.drain(h.clock.now + 5);
  bool locLine = false;
  for (const auto& e : h.net.events)
    if (e.kind == NetLineKind::Caller &&
        e.text.find("grid square") != std::string::npos)
      locLine = true;
  CHECK(locLine);
}
