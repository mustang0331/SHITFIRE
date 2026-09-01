#include "shitfire/sim/mission_machine.h"

#include <cmath>

#include "shitfire/math/grid.h"
#include "shitfire/math/jsemantics.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/noise.h"
#include "shitfire/sim/spotburst_math.h"

/* Fire-mission state machine bodies — port of src/js/10-mission.js
   (oracle-v1) plus the two scored fragments of 08-burst-optics.js the
   mission path executes (illum burn draw :514-526, screen suppression
   :468-478). Speech is reduced to FdcLineId/SysLineId sink events; the JS
   sentence text lives with the wave-2 FDC porter. PRNG prime directive:
   every draw below sits at the exact source position, including
   draws-before-gates and rejection loops. */
namespace shitfire { namespace sim {

namespace {

constexpr double kPi = 3.141592653589793;   /* JS Math.PI */

/* UTF-8 em dash, byte-escaped so the literal is exact regardless of the
   compiler's source-charset guess (these strings land in mission.notes and
   are compared by the Level-3 AAR diff). */
const char* const kEmDash = "\xE2\x80\x94";

bool isWordChar(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') || c == '_';
}

/* JS /\b\d{4}\b/ over the raw transmission (10-mission.js:224): a run of
   exactly four digits whose neighbours are not word characters. */
bool hasFourDigitRun(const std::string& s) {
  const std::size_t n = s.size();
  std::size_t i = 0;
  while (i < n) {
    if (s[i] >= '0' && s[i] <= '9') {
      std::size_t j = i;
      while (j < n && s[j] >= '0' && s[j] <= '9') j++;
      const bool leftOk = (i == 0) || !isWordChar(s[i - 1]);
      const bool rightOk = (j == n) || !isWordChar(s[j]);
      if (j - i == 4 && leftOk && rightOk) return true;
      i = j;
    } else {
      i++;
    }
  }
  return false;
}

std::string toLowerAscii(const std::string& s) {
  std::string r = s;
  for (char& c : r) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  return r;
}

bool containsAny(const std::string& hay,
                 std::initializer_list<const char*> needles) {
  for (const char* n : needles)
    if (hay.find(n) != std::string::npos) return true;
  return false;
}

/* known.join(', ') (10-mission.js:298 etc.). */
std::string joinComma(const std::vector<std::string>& v) {
  std::string r;
  for (std::size_t i = 0; i < v.size(); i++) {
    if (i) r += ", ";
    r += v[i];
  }
  return r;
}

/* parseInt on a digit-only slice (10-mission.js:263 etc.). The parser layer
   guarantees digit strings; a sliced value with no digits (parseInt NaN) is
   unreachable through the shipped grammar, so it parses as 0 here. */
double parseIntDigits(const std::string& s) {
  double v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') break;
    v = v * 10.0 + (c - '0');
  }
  return v;
}

const char* shellName(ShellKind s) {
  return s == ShellKind::He ? "he" : s == ShellKind::Smoke ? "smoke" : "illum";
}

const char* fuzeName(FuzeKind k) {
  switch (k) {
    case FuzeKind::Vt: return "vt";
    case FuzeKind::Time: return "time";
    case FuzeKind::Delay: return "delay";
    case FuzeKind::Pd: return "pd";
    default: return "";
  }
}

const char* requestedSheafName(RequestedSheaf k) {
  switch (k) {
    case RequestedSheaf::Converged: return "converged";
    case RequestedSheaf::Circular: return "circular";
    case RequestedSheaf::Open: return "open";
    case RequestedSheaf::Parallel: return "parallel";
    case RequestedSheaf::Linear: return "linear";
    case RequestedSheaf::Special: return "special";
  }
  return "";
}

const char* sheafKindName(SheafKind k) {
  switch (k) {
    case SheafKind::Converged: return "converged";
    case SheafKind::Circular: return "circular";
    case SheafKind::Open: return "open";
    case SheafKind::Linear: return "linear";
    default: return "";
  }
}

std::string upperAscii(std::string s) {
  for (char& c : s) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
  return s;
}

}  // namespace

/* ---- Mission ---------------------------------------------------------- */

// port of src/js/10-mission.js:134-189 (oracle-v1) — the mission object
// literal, field for field. The rng seed is stream #9:
// mulberry32((CONFIG.SEED.mission * 1046527 + 7) >>> 0)  (:184).
Mission::Mission(Vec2 target, const MissionMeta& meta, double seedMission,
                 double simNow, double scenarioT0In,
                 std::optional<double> aimErr0In, TgtRef tgtRefIn,
                 MissionPhase phase0)
    : aim(target),
      aim0x(target.x), aim0z(target.z),
      notes(meta.notes),
      desc(meta.desc), gridStr(meta.gridStr),
      method(meta.method),
      aimErr0(aimErr0In),
      tInit(simNow - scenarioT0In),
      tStart(simNow),
      intent(meta.intent),
      imm(meta.imm),
      planned(meta.planned),
      tgtRef(tgtRefIn),
      sheaf(meta.sheaf),
      fuze(meta.fuze),
      shell(meta.shell),
      amc(meta.amc),
      pendingFire(nullptr),
      mto(meta.mto),
      rng(js::toUint32(seedMission * 1046527.0 + 7.0)),
      shotExtra(meta.shotExtra),
      fpf(meta.fpf),
      series(meta.series), group(meta.group),
      phase(phase0) {}

/* ---- stream #11: dispersal spread ------------------------------------- */

// port of src/js/10-mission.js:1255-1259 (oracle-v1) — the troop-dispersal
// layout stream, mulberry32((S.seed * 7919 + 13) >>> 0). Two draws per
// VISIBLE figure, in mesh order: angle then radius. The +cos(a) on z is the
// source's screen-space offset, not the azimuth frame.
std::vector<Vec2> disperseOffsets(double scenarioSeed,
                                  std::size_t visibleFigures) {
  Mulberry32 drng(js::toUint32(scenarioSeed * 7919.0 + 13.0));
  std::vector<Vec2> out;
  out.reserve(visibleFigures);
  for (std::size_t i = 0; i < visibleFigures; i++) {
    const double a = drng.next() * kPi * 2.0;
    const double r = 18.0 + drng.next() * 26.0;
    out.push_back(Vec2{std::sin(a) * r, std::cos(a) * r});
  }
  return out;
}

/* ---- MissionMachine --------------------------------------------------- */

MissionMachine::MissionMachine(SimClock& clock, EventQueue& queue,
                               IMissionSink& sink, IScenarioView& view,
                               const SimTunables& tunables,
                               const MapConfig& map, Difficulty difficulty,
                               AssetKind asset, Vec2 op, Vec2 battery,
                               bool chapterStrict)
    : clock_(clock), queue_(queue), sink_(sink), view_(view),
      tun_(tunables), map_(map), difficulty_(difficulty), asset_(asset),
      op_(op), battery_(battery), chapterStrict_(chapterStrict),
      seedMission_(tunables.seed.mission),
      ammoB155_(tunables.ammo.b155), ammoM60_(tunables.ammo.m60) {}

// port of src/js/10-mission.js:32 (oracle-v1) — coaching on Easy/Normal only.
bool MissionMachine::coachOn() const { return difficulty_ != Difficulty::Hard; }

// port of src/js/10-mission.js:6-14 (oracle-v1) — setState(s). refreshStatus
// (the HUD strip) becomes the sink's stateChanged notification.
void MissionMachine::setState(MissionPhase s) {
  if (s != state_) { stateSince_ = clock_.now; stateHinted_ = false; }
  state_ = s;
  if (active_) active_->phase = s;   /* SUGG5 */
  sink_.stateChanged(s, clock_.now);
}

// port of src/js/10-mission.js:125-131 (oracle-v1) — scheduleImpact: bind the
// firing mission for the callback's duration, restore after (try/finally in
// the source; core is exceptions-off so a plain restore is equivalent).
void MissionMachine::scheduleImpact(double t, Mission* m,
                                    std::function<void()> fn) {
  queue_.schedule(t, [this, m, fn = std::move(fn)]() {
    Mission* cur = active_;
    active_ = m;
    fn();
    active_ = cur;
  });
}

// port of src/js/10-mission.js:107-109 (oracle-v1) — markDispersion().
void MissionMachine::markDispersion() {
  if (active_ && !tun_.ballistics.dispersion) active_->noDisp = true;
}

/* ---- ammo (SUGG7, :607-630) ------------------------------------------- */

double& MissionMachine::stockField(AmmoTunables::Stock& s,
                                   ShellKind shell) const {
  return shell == ShellKind::He ? s.he
       : shell == ShellKind::Smoke ? s.smoke
       : s.illum;
}

// port of src/js/10-mission.js:609-611 (oracle-v1) — ammoPool(): the mortar
// chapters draw m60, everything else (sunlamp included) b155.
AmmoTunables::Stock& MissionMachine::ammoPool() {
  return asset_ == AssetKind::Mortar60 ? ammoM60_ : ammoB155_;
}
const AmmoTunables::Stock& MissionMachine::ammoPool() const {
  return asset_ == AssetKind::Mortar60 ? ammoM60_ : ammoB155_;
}
const AmmoTunables::Stock& MissionMachine::ammoFullStock() const {
  return asset_ == AssetKind::Mortar60 ? tun_.ammo.m60 : tun_.ammo.b155;
}

// port of src/js/10-mission.js:612-618 (oracle-v1) — ammoFull(): chapters
// replenish on launch; the amber latches clear with the stocks.
void MissionMachine::ammoFull() {
  ammoB155_ = tun_.ammo.b155;
  ammoM60_ = tun_.ammo.m60;
  ammoWarned_.clear();
}

// port of src/js/10-mission.js:619-629 (oracle-v1) — ammoSpend(shell): the
// one place a round leaves the books; one amber advisory per shell type per
// pool when it crosses 20% of the config stock.
void MissionMachine::ammoSpend(ShellKind shell) {
  AmmoTunables::Stock& pool = ammoPool();
  const AmmoTunables::Stock& full = ammoFullStock();
  double& cur = stockField(pool, shell);
  if (cur > 0) cur -= 1;
  const std::string key =
      std::string(shellName(shell)) +
      (asset_ == AssetKind::Mortar60 ? "60" : "155");
  bool warned = false;
  for (const std::string& w : ammoWarned_) if (w == key) { warned = true; break; }
  const double fullN = stockField(const_cast<AmmoTunables::Stock&>(full), shell);
  if (!warned && fullN > 0 && cur > 0 && cur <= fullN * 0.2) {
    ammoWarned_.push_back(key);
    FdcLine line{FdcLineId::AmmoAmber};
    line.delay = 1.5;
    line.num0 = cur;
    line.text = shellName(shell);
    sink_.sayLine(line);
  }
}

// port of src/js/10-mission.js:630 (oracle-v1) — ammoLeft(shell).
double MissionMachine::ammoLeft(ShellKind shell) const {
  const AmmoTunables::Stock& pool = ammoPool();
  return shell == ShellKind::He ? pool.he
       : shell == ShellKind::Smoke ? pool.smoke
       : pool.illum;
}

/* ---- registries ------------------------------------------------------- */

Mission* MissionMachine::findMission(const std::string& key) {
  for (auto& kv : missionIndex_)
    if (kv.first == key) return kv.second;
  return nullptr;
}

// port of src/js/10-mission.js:122 (oracle-v1) — openMissions():
// Object.values(MISSIONS).filter(m => !m.done), insertion order.
std::vector<Mission*> MissionMachine::openMissions() {
  std::vector<Mission*> out;
  for (auto& kv : missionIndex_)
    if (!kv.second->done) out.push_back(kv.second);
  return out;
}

RecTgt* MissionMachine::findRecTgt(const std::string& name) {
  for (auto& kv : recTgts_)
    if (kv.first == name) return &kv.second;
  return nullptr;
}

// RECTGT[name] = { x, z, t: sim.now } (:338, 12-cff:779) — assignment keeps
// an existing key's insertion position, as a JS object does.
void MissionMachine::putRecTgt(const std::string& name, double x, double z) {
  if (RecTgt* t = findRecTgt(name)) {
    t->x = x; t->z = z; t->t = clock_.now;
    return;
  }
  recTgts_.emplace_back(name, RecTgt{x, z, clock_.now});
}

std::vector<std::string> MissionMachine::recTgtKeys() const {
  std::vector<std::string> out;
  for (const auto& kv : recTgts_) out.push_back(kv.first);
  return out;
}

// port of src/js/10-mission.js:312-316 (oracle-v1) — nextFreeTgtNum():
// seed-anchored walk to the next free AB number.
std::string MissionMachine::nextFreeTgtNum() const {
  const int base = 7100 + static_cast<int>(std::fmod(seedMission_, 800.0));
  std::string tn = "AB" + std::to_string(base);
  auto taken = [this](const std::string& k) {
    for (const auto& kv : recTgts_) if (kv.first == k) return true;
    return false;
  };
  while (taken(tn)) {
    const int n = static_cast<int>(parseIntDigits(tn.substr(2)));
    tn = "AB" + std::to_string(7100 + ((n - 7100 + 1) % 800));
  }
  return tn;
}

// MISSIONS registry + active binding, wiped with every new scenario (:121).
// The caller clears the EventQueue with the scenario, so no scheduled
// callback can outlive the missions it captured.
void MissionMachine::resetMissions() {
  missionIndex_.clear();
  active_ = nullptr;
  missions_.clear();
}

// RECTGT / PRIORITY / SERIES / GROUPS / FPF — wiped on an island change
// (SUGG2/SUGG4 comments, :285-292, :486-487).
void MissionMachine::resetPlanning() {
  recTgts_.clear();
  priority_.clear();
  series_.clear();
  groups_.clear();
  fpf_.reset();
}

/* ---- lifecycle -------------------------------------------------------- */

// port of src/js/10-mission.js:132-195 (oracle-v1) — fireMission(). The
// NET4 noMissionStreak reset (:133) is cff-layer state: the cff layer calls
// this method and resets its own streak.
void MissionMachine::fireMission(Vec2 target, Warno warno,
                                 const MissionMeta& meta) {
  std::optional<double> aimErr0;
  if (view_.hasEnemy()) {
    const Vec2 e = view_.enemy();
    aimErr0 = dist2(target.x, target.z, e.x, e.z);
  }
  /* SUGG5 — nearer element owns the mission (:163-166). */
  TgtRef ref = TgtRef::Main;
  if (view_.hasTwin()) {
    const Vec2 tw = view_.twin(), e = view_.enemy();
    if (dist2(target.x, target.z, tw.x, tw.z) <
        dist2(target.x, target.z, e.x, e.z))
      ref = TgtRef::Twin;
  }
  missions_.push_back(std::make_unique<Mission>(
      target, meta, seedMission_, clock_.now, view_.scenarioT0(), aimErr0,
      ref, state_));
  Mission* m = missions_.back().get();
  active_ = m;
  /* :190-192 — register by target number; immediates stay unrouted. */
  if (m->mto) {
    const std::string key = "AA" + m->mto->tgt;
    bool replaced = false;
    for (auto& kv : missionIndex_)
      if (kv.first == key) { kv.second = m; replaced = true; break; }
    if (!replaced) missionIndex_.emplace_back(key, m);
  }
  if (warno == Warno::Ffe) fireForEffect();
  else fireAdjustRound();
}

// port of src/js/10-mission.js:765-773 (oracle-v1) — applyCorrection():
// OT-frame correction onto the walking aimpoint.
Vec2 MissionMachine::applyCorrection(double add, double right) {
  if (!active_) return Vec2{};   /* caller contract: mission on the net */
  Vec2& a = active_->aim;
  const double otAz = azTo(op_.x, op_.z, a.x, a.z);
  const double fx = std::sin(otAz), fz = -std::cos(otAz);  /* toward target */
  const double rx = std::cos(otAz), rz = std::sin(otAz);   /* observer's right */
  a.x += fx * add + rx * right;
  a.z += fz * add + rz * right;
  return a;
}

// port of src/js/10-mission.js:658-680 (oracle-v1) — armAtMyCommand(fn).
bool MissionMachine::armAtMyCommand(std::function<void()> fn) {
  if (!active_ || !active_->amc) return false;
  if (active_->released) { active_->released = false; return false; }
  active_->pendingFire = std::move(fn);
  setState(MissionPhase::AtMyCommand);
  FdcLine ready{FdcLineId::ReadyOver};
  ready.delay = 1.0;
  sink_.sayLine(ready);
  /* SUGG6 — the intercept drill coach, convoy + AMC only (:669-678). */
  if (view_.type() == ScenarioType::Convoy && coachOn() &&
      !active_->coached.intercept) {
    active_->coached.intercept = true;
    const double tof = js::round(tofFor(battery_.x, battery_.z,
                                        active_->aim.x, active_->aim.z,
                                        tun_.ballistics));
    const double sd = js::round(tun_.fdc.shotDelay);
    SysLine drill{SysLineId::InterceptDrill};
    drill.num0 = tof; drill.num1 = sd; drill.num2 = tof + sd;
    sink_.sysLine(drill);
  }
  return true;
}

// port of src/js/10-mission.js:775-808 (oracle-v1) — fireAdjustRound().
void MissionMachine::fireAdjustRound() {
  if (!active_) return;          /* only reachable with a mission (:194) */
  Mission& m = *active_;
  const BallisticsTunables& B = tun_.ballistics;
  /* G25 — last-gate hold enforcement (:780-784). */
  if (m.checkFire) {
    FdcLine l{FdcLineId::CheckFiringRefusal};
    l.delay = 0.8;
    sink_.sayLine(l);
    return;
  }
  /* SUGG7 — black is black (:788-791). */
  if (ammoLeft(m.shell) <= 0) {
    FdcLine l{FdcLineId::BlackOnShell};
    l.delay = 0.8;
    l.text = shellName(m.shell);
    sink_.sayLine(l);
    return;
  }
  /* G24 — lay and wait for FIRE (:794). */
  if (armAtMyCommand([this]() { fireAdjustRound(); })) return;
  markDispersion();
  m.adjustRounds++;
  const double otAz = azTo(op_.x, op_.z, m.aim.x, m.aim.z);
  const ErrXZ err = m.rounds.empty()
      ? firstRoundError(m.rng, difficulty_, asset_, B)
      : followUpError(m.rng, otAz, difficulty_, asset_, B);
  const Vec2 impact{m.aim.x + err.x, m.aim.z + err.z};
  const double tof = tofFor(battery_.x, battery_.z, m.aim.x, m.aim.z, B);
  setState(MissionPhase::Shot);
  FdcLine shot{FdcLineId::ShotOver};
  shot.delay = tun_.fdc.shotDelay + m.shotExtra;
  const double tShot = sink_.sayLine(shot);
  FdcLine splash{FdcLineId::SplashOver};
  splash.delay = tof - B.splashLead;
  sink_.sayLine(splash);
  if (asset_ == AssetKind::Sunlamp) {   /* 11c — chargeWhine(tof) (:806) */
    PresentationEvent w{PresentationKind::ChargeWhine};
    w.num0 = tof;
    sink_.presentation(w);
  }
  scheduleImpact(tShot + tof, &m,
                 [this, impact]() { resolveImpact(impact, false); });
}

// port of src/js/10-mission.js:810-916 (oracle-v1) — fireForEffect().
void MissionMachine::fireForEffect() {
  if (!active_) return;
  Mission& m = *active_;
  const BallisticsTunables& B = tun_.ballistics;
  if (m.checkFire) {                       /* G25 (:812-816) */
    FdcLine l{FdcLineId::CheckFiringRefusal};
    l.delay = 0.8;
    sink_.sayLine(l);
    return;
  }
  if (ammoLeft(m.shell) <= 0) {            /* SUGG7 (:817-820) */
    FdcLine l{FdcLineId::BlackOnShell};
    l.delay = 0.8;
    l.text = shellName(m.shell);
    sink_.sayLine(l);
    return;
  }
  if (armAtMyCommand([this]() { fireForEffect(); })) return;   /* G24 */
  markDispersion();
  setState(MissionPhase::FireForEffect);
  /* :824 — 40% ffeAck tail: sanctioned-flavor gate, the sink's coin. */
  FdcLine ack{FdcLineId::FfeAckFlavor};
  ack.delay = 0.8;
  ack.gateProbability = 0.4;
  sink_.sayLine(ack);
  const double otAz = azTo(op_.x, op_.z, m.aim.x, m.aim.z);
  const double tof = tofFor(battery_.x, battery_.z, m.aim.x, m.aim.z, B);
  FdcLine shot{FdcLineId::ShotOver};
  shot.delay = tun_.fdc.shotDelay + m.shotExtra;
  const double tShot = sink_.sayLine(shot);
  FdcLine splash{FdcLineId::SplashOver};
  splash.delay = tof - B.splashLead;
  sink_.sayLine(splash);
  if (asset_ == AssetKind::Sunlamp) {      /* 11c (:829) */
    PresentationEvent w{PresentationKind::ChargeWhine};
    w.num0 = tof;
    sink_.presentation(w);
  }
  /* :832-833 — a cold FFE carries the full first-round error as a common
     aim error for the whole volley. Draw order sacred. */
  const ErrXZ base = m.rounds.empty()
      ? firstRoundError(m.rng, difficulty_, asset_, B)
      : ErrXZ{0, 0};
  /* G15 — linear sheaf on a convoy spreads aimpoints along the column
     axis (:838-843). */
  const bool lin = m.sheaf.kind == SheafKind::Linear &&
                   view_.type() == ScenarioType::Convoy &&
                   view_.convoyHasPath();
  const Dir2 linDir = lin ? view_.pathDirAt(view_.convoyHeadD()) : Dir2{};
  /* SUGG1 — the FPF volley (:850-851). */
  const bool fpfRun = m.fpf && fpf_ != nullptr;
  if (fpfRun && fpf_->frat) m.failReason = FailReason::Fratricide;
  /* SUGG4/SUGG3 — series and group volleys over recorded points (:857-861). */
  auto allFiled = [this](const std::vector<std::string>& v) {
    if (v.empty()) return false;
    for (const std::string& n : v)
      if (!findRecTgt(n)) return false;
    return true;
  };
  const bool serRun = allFiled(m.series);
  const bool grpRun = !serRun && allFiled(m.group);
  int nRounds = fpfRun ? 8
              : serRun ? static_cast<int>(m.series.size()) * 4
              : grpRun ? static_cast<int>(m.group.size()) * 3
              : static_cast<int>(B.ffeRounds);
  /* SUGG7 — partial volley (:864-868). */
  const double avail = ammoLeft(m.shell);
  if (nRounds > avail) {
    nRounds = static_cast<int>(avail);
    FdcLine l{FdcLineId::PartialVolley};
    l.delay = 2.0;
    l.num0 = avail;
    sink_.sayLine(l);
  }
  double off = 0, tLast = 0;
  for (int i = 0; i < nRounds; i++) {
    const ErrXZ err = followUpError(m.rng, otAz, difficulty_, asset_, B);
    const double lo = lin ? (i - (B.ffeRounds - 1) / 2.0) * 35.0 : 0.0;
    Vec2 impact;
    if (fpfRun) {
      const Vec2& gun = fpf_->pts[i % 4];
      impact = Vec2{gun.x + err.x, gun.z + err.z};
    } else if (serRun) {
      const RecTgt* t = findRecTgt(m.series[static_cast<std::size_t>(i / 4)]);
      impact = Vec2{t->x + err.x, t->z + err.z};
    } else if (grpRun) {
      const RecTgt* t = findRecTgt(
          m.group[static_cast<std::size_t>(i) % m.group.size()]);
      impact = Vec2{t->x + err.x, t->z + err.z};
    } else {
      impact = Vec2{m.aim.x + base.x + err.x + (lin ? linDir.dx * lo : 0.0),
                    m.aim.z + base.z + err.z + (lin ? linDir.dz * lo : 0.0)};
    }
    tLast = tShot + tof + off;
    scheduleImpact(tLast, &m,
                   [this, impact]() { resolveImpact(impact, true); });
    /* series: re-lay pause between targets, max rate within one (:887). */
    const double gap = (serRun && i % 4 == 3) ? 9.0 : 0.0;
    off += lerp(B.ffeStagger0, B.ffeStagger1, m.rng.next()) *
               (fpfRun ? 0.33 : 1.0) +
           gap;
  }
  /* :890-915 — completion, bound to this mission (SUGG5). */
  scheduleImpact(tLast + 2.2, &m, [this]() {
    if (!active_ || active_->done) return;
    active_->tEnd = clock_.now;
    active_->ffeDone = true;
    assessFFE();
    FdcLine rc{FdcLineId::RoundsComplete};
    rc.delay = 0.4;
    sink_.sayLine(rc);
    /* G13 — post-volley traffic keyed on what the volley did (:900-913). */
    const EffectAssessment a = assessEffect();
    if (missionAccomplished()) {
      FdcLine tail{FdcLineId::CompleteTailFlavor};
      tail.delay = 1.2;
      tail.gateProbability = 0.5;
      sink_.sayLine(tail);
      FdcLine done{FdcLineId::CompletePrompt};
      done.delay = 1.6;
      sink_.sayLine(done);
    } else if (a.outcome == EffectOutcome::Suppressed) {
      FdcLine l{FdcLineId::SuppressedOnly};
      l.delay = 1.4;
      sink_.sayLine(l);
      sink_.sysLine(SysLine{SysLineId::SuppressedOnlyGuidance});
    } else {
      FdcLine l{FdcLineId::NoEffect};
      l.delay = 1.4;
      sink_.sayLine(l);
      sink_.sysLine(SysLine{SysLineId::NoEffectGuidance});
    }
    setState(MissionPhase::EomQuery);
  });
}

/* ---- the graded effect engine ---------------------------------------- */

double MissionMachine::missionSheafMult() const {
  /* :995-996 — if (!mission || !mission.sheaf) return 1. */
  if (!active_ || active_->sheaf.kind == SheafKind::None) return 1.0;
  return sheafMult(active_->sheaf.kind, view_.targetClass(),
                   view_.dispersed());
}

double MissionMachine::missionFuzeMult() const {
  /* :1027-1028 */
  if (!active_ || active_->fuze.kind == FuzeKind::None) return 1.0;
  return fuzeMult(active_->fuze.kind, view_.targetClass(), view_.posture());
}

// port of src/js/10-mission.js:935-970 (oracle-v1) — addRoundEffect().
void MissionMachine::addRoundEffect(Vec2 impact, bool isFFE) {
  if (view_.type() == ScenarioType::Convoy) return;   /* vehicle kills instead */
  const EffBandSet b = effBands(asset_, view_.targetClass(), view_.effScale(),
                                tun_.effects);
  /* SUGG5 — credit the nearer element; the twin keeps its own books. */
  bool tw = false;
  if (view_.hasTwin()) {
    const Vec2 e = view_.enemy(), t = view_.twin();
    const double dMain = dist2(impact.x, impact.z, e.x, e.z);
    const double dTwin = dist2(impact.x, impact.z, t.x, t.z);
    if (dTwin < dMain) tw = true;
  }
  const Vec2 c = tw ? view_.twin() : view_.enemy();
  const double d = dist2(impact.x, impact.z, c.x, c.z);
  if (d > b.rSupp) return;
  if (tw ? view_.twinAlive() : view_.enemyAlive())    /* the dead do not duck */
    view_.setSuppressed(clock_.now + tun_.effects.suppressSec, tw);
  const double band = d <= b.rFull ? 1.0 : d <= b.rHalf ? 0.5 : 0.25;
  const double post = view_.targetClass() == TargetClass::Personnel
      ? postureMult(view_.posture(), tun_.effects)
      : 1.0;
  /* G15/G16/11b — sheaf scales FFE rounds only, fuze every round, armor
     divides (:968). */
  const double armor = view_.armor() > 0 ? view_.armor() : 1.0;
  const double contrib = b.perRound * band * post *
                         (isFFE ? missionSheafMult() : 1.0) *
                         missionFuzeMult() / armor;
  view_.addEff(contrib, tw);
}

// port of src/js/10-mission.js:1089-1106 (oracle-v1) — assessEffect().
EffectAssessment MissionMachine::assessEffect() const {
  if (view_.type() == ScenarioType::Convoy) {
    int dead = 0;
    for (std::size_t i = 0; i < view_.vehicleCount(); i++)
      if (view_.vehicle(i).dead) dead++;
    return EffectAssessment{
        dead >= 3 ? EffectOutcome::Destroyed : EffectOutcome::None,
        dead * 25.0};
  }
  const EffBandSet b = effBands(asset_, view_.targetClass(), view_.effScale(),
                                tun_.effects);
  /* SUGG5 — a twin-element mission grades on the twin's own books. */
  const bool tw = active_ && active_->tgtRef == TgtRef::Twin && view_.hasTwin();
  const double eff = tw ? view_.twinEff() : view_.eff();
  const bool sup = tw ? view_.twinEverSuppressed() : view_.everSuppressed();
  /* threshold on the RAW figure; round only for display (:1101-1105). */
  const EffectOutcome outcome =
      eff >= b.destroyPct ? EffectOutcome::Destroyed
      : eff >= b.neutralizePct ? EffectOutcome::Neutralized
      : sup ? EffectOutcome::Suppressed
      : EffectOutcome::None;
  return EffectAssessment{outcome, js::round(eff)};
}

// port of src/js/10-mission.js:1110-1116 (oracle-v1) — missionAccomplished().
bool MissionMachine::missionAccomplished() const {
  if (active_ && active_->intent == IntentKind::Illum)
    return !active_->rounds.empty();
  const EffectAssessment a = assessEffect();
  if (view_.type() == ScenarioType::Kaiju)            /* 11b — destroy only */
    return a.outcome == EffectOutcome::Destroyed;
  return a.outcome == EffectOutcome::Destroyed ||
         a.outcome == EffectOutcome::Neutralized ||
         (a.outcome == EffectOutcome::Suppressed && active_ &&
          active_->intent == IntentKind::Suppress);
}

// port of src/js/10-mission.js:1118-1350 (oracle-v1) — resolveImpact().
// GFX2 contract: every consequence booked FIRST; presentation events go out
// after the books and cannot throw (sink contract).
void MissionMachine::resolveImpact(Vec2 impact, bool isFFE) {
  if (!active_ || active_->done) return;
  Mission& m = *active_;
  const double y = std::fmax(view_.groundHeight(impact.x, impact.z), 0.3);
  const ShellKind shell = m.shell;
  /* 12h — illum bursts high and touches nothing below (:1125-1135). The
     burn-time draw consumes ONE mission-stream value at exactly this point
     (08-burst-optics.js:522-524; DETERMINISM_NOTE.md §a). */
  if (shell == ShellKind::Illum) {
    const double iy = std::fmax(view_.groundHeight(impact.x, impact.z), 0.0)
                      + 330.0;                      /* 08-burst-optics.js:516 */
    const double u = m.rng.next();                  /* 08-burst-optics.js:523 */
    const double burn = tun_.effects.illumBurnLo +
        (tun_.effects.illumBurnHi - tun_.effects.illumBurnLo) * u;
    PresentationEvent flare{PresentationKind::IllumFlare};
    flare.x = impact.x; flare.y = iy; flare.z = impact.z;
    flare.num0 = burn;
    sink_.presentation(flare);
    m.usedIllum = true;                             /* TEMPO4 */
    m.rounds.push_back(impact);
    if (isFFE) m.ffeRounds.push_back(impact);
    ImpactEvent ev;
    ev.x = impact.x; ev.z = impact.z; ev.y = iy;
    ev.shell = ShellKind::Illum;
    ev.isFFE = isFFE;
    const Vec2 e = view_.enemy();
    ev.dTgtRounded = js::round(dist2(impact.x, impact.z, e.x, e.z));
    ev.adjustRound = m.adjustRounds;
    sink_.impact(ev);
    setState(isFFE ? MissionPhase::FireForEffect : MissionPhase::Adjusting);
    return;
  }
  /* GFX2 — book first (:1148-1156). */
  m.rounds.push_back(impact);
  if (isFFE) m.ffeRounds.push_back(impact);
  ammoSpend(shell);                                  /* SUGG7 */
  if (shell == ShellKind::He) addRoundEffect(impact, isFFE);   /* 12h */
  {
    ImpactEvent ev;
    ev.x = impact.x; ev.z = impact.z; ev.y = y;
    ev.shell = shell;
    ev.isFFE = isFFE;
    const Vec2 e = view_.enemy();
    ev.dTgtRounded = js::round(dist2(impact.x, impact.z, e.x, e.z));
    ev.adjustRound = m.adjustRounds;
    sink_.impact(ev);
  }
  /* 12h — the smoke screen's obscuration-suppression is MODEL, booked here
     (08-burst-optics.js:472-478); the wall itself is presentation. */
  if (shell == ShellKind::Smoke) {
    if (view_.hasEnemy() && view_.enemyAlive()) {
      const Vec2 e = view_.enemy();
      if (dist2(impact.x, impact.z, e.x, e.z) < tun_.effects.screenRadius)
        view_.raiseSuppressedTo(clock_.now + tun_.effects.screenSec);
    }
    PresentationEvent scr{PresentationKind::SmokeScreen};
    scr.x = impact.x; scr.y = y; scr.z = impact.z;
    sink_.presentation(scr);
  }
  if (asset_ == AssetKind::Sunlamp) {                /* 11c (:1161) */
    PresentationEvent beam{PresentationKind::SunBeam};
    beam.x = impact.x; beam.z = impact.z;
    sink_.presentation(beam);
  }
  {
    PresentationEvent burst{PresentationKind::Burst};
    burst.x = impact.x; burst.y = y; burst.z = impact.z;
    sink_.presentation(burst);
  }
  /* convoy attrition — any HE round can kill vehicles (:1167-1188). */
  if (view_.type() == ScenarioType::Convoy && shell == ShellKind::He) {
    /* SUGG6 — the intercept verdict on the FIRST effect round (:1173-1180). */
    if (isFFE && !m.convoyLead.has_value()) {
      for (std::size_t i = 0; i < view_.vehicleCount(); i++) {
        const VehicleView head = view_.vehicle(i);
        if (head.dead) continue;
        const Dir2 hd = view_.pathDirAt(head.d);
        m.convoyLead = js::round((impact.x - head.x) * hd.dx +
                                 (impact.z - head.z) * hd.dz);
        break;
      }
    }
    for (std::size_t i = 0; i < view_.vehicleCount(); i++) {
      const VehicleView v = view_.vehicle(i);
      if (!v.dead && dist2(impact.x, impact.z, v.x, v.z) < 40.0)
        view_.killVehicle(i);
    }
    int alive = 0;
    for (std::size_t i = 0; i < view_.vehicleCount(); i++)
      if (!view_.vehicle(i).dead) alive++;
    if (alive <= 1) view_.setEnemyAlive(false);
  }
  if (!m.firstMiss.has_value()) {
    const Vec2 e = view_.enemy();
    m.firstMiss = dist2(impact.x, impact.z, e.x, e.z);
  }
  /* G12 — fratricide/collateral FAIL the mission, never END it
     (:1191-1237). First cause stands; one full rant, then one grim line. */
  auto failHit = [this, &m](FailReason reason, FdcLineId rant,
                            FdcLineId repeatLine) {
    if (m.failReason == FailReason::None) m.failReason = reason;
    if (!m.failRanted) {
      m.failRanted = true;
      FdcLine r{rant};
      r.delay = 1.0;   /* the FDC layer chains the 3 rant lines 1.0/2.2/2.8 */
      sink_.sayLine(r);
      sink_.sysLine(SysLine{SysLineId::FailureRecordedGuidance});
    } else {
      FdcLine r{repeatLine};
      r.delay = 1.4;
      sink_.sayLine(r);
    }
  };
  for (const FriendlyPos& f : view_.friendlies()) {
    const double r = f.hasR ? f.r : tun_.mission.fratricideRadius;
    if (dist2(impact.x, impact.z, f.x, f.z) < r) {
      failHit(FailReason::Fratricide, FdcLineId::RantFratricide,
              FdcLineId::RantFratricideRepeat);
      break;
    }
  }
  {
    bool civHit = false;
    for (const Vec2& c : view_.civilians())
      if (dist2(impact.x, impact.z, c.x, c.z) < tun_.world.civCollateralR) {
        civHit = true;
        break;
      }
    if (!civHit)
      for (const Vec2& h : view_.huts())
        if (dist2(impact.x, impact.z, h.x, h.z) < tun_.world.hutCollateralR) {
          civHit = true;
          break;
        }
    if (civHit)
      failHit(FailReason::Collateral, FdcLineId::RantCollateral,
              FdcLineId::RantCollateralRepeat);
  }
  if (!isFFE) {
    const Vec2 e = view_.enemy();
    const double dTgt = dist2(impact.x, impact.z, e.x, e.z);
    /* wrong-way tirade after the second regression (:1239-1243). */
    if (m.lastAdjDist.has_value() && dTgt > *m.lastAdjDist + 40.0) {
      m.wrongWay++;
      if (m.wrongWay == 2) {
        FdcLine l{FdcLineId::WrongWayQuip};
        l.delay = 2.2;
        sink_.sayLine(l);
      }
    }
    /* troops react: alert, then disperse and go prone (:1248-1275). */
    const ScenarioType st = view_.type();
    if ((st == ScenarioType::Troops || st == ScenarioType::Raid ||
         st == ScenarioType::Chow) &&
        dTgt < 300.0 && !view_.dispersed()) {
      view_.bumpAlerted();
      if (view_.alerted() == 1) {
        sink_.sysLine(SysLine{SysLineId::AlertedMoving});
      } else if (view_.alerted() >= 2) {
        view_.setDispersed();
        /* stream #11 — the visual spread; the presentation layer draws
           disperseOffsets(view.scenarioSeed(), visibleFigures) (:1255-1262). */
        PresentationEvent dsp{PresentationKind::DisperseTroops};
        dsp.num0 = view_.scenarioSeed();
        sink_.presentation(dsp);
        /* G13 — dispersal is a POSTURE change (:1267). */
        view_.setPostureProne();
        sink_.sysLine(SysLine{SysLineId::DispersedProne});
        sink_.spotReportRequested(SpotReportReason::Displace);   /* :1271 */
        if (coachOn()) {
          FdcLine l{FdcLineId::DisperseAdvise};
          l.delay = 2.6;
          sink_.sayLine(l);
        }
      }
    }
    /* adjustment doctrine tracking (:1277-1303). */
    m.missTrace.push_back(js::round(dTgt));
    const double otAzT = azTo(op_.x, op_.z, e.x, e.z);
    const double along = (impact.x - e.x) * std::sin(otAzT) +
                         (impact.z - e.z) * -std::cos(otAzT);   /* + = over */
    if (along > 30.0) m.sides.longSide = true;
    else if (along < -30.0) m.sides.shortSide = true;
    const bool nowBracketed = m.sides.longSide && m.sides.shortSide;
    const std::optional<double> prev = m.lastAdjDist;
    if (prev.has_value() && std::fabs(dTgt - *prev) < *prev * 0.15 &&
        dTgt > 80.0)
      m.wasted++;
    if (coachOn()) {
      auto& C = m.coached;
      if (m.rounds.size() == 1 && dTgt > 250.0 && !C.loc) {
        C.loc = true;
        FdcLine l{FdcLineId::CoachLoc};
        l.delay = 2.4;
        l.num0 = js::round(dTgt);
        sink_.sayLine(l);
      } else if (nowBracketed && !m.bracketed && !C.bracketGood) {
        C.bracketGood = true;
        FdcLine l{FdcLineId::CoachBracketGood};
        l.delay = 2.4;
        sink_.sayLine(l);
      } else if (!nowBracketed && m.adjustRounds >= 3 && dTgt > 120.0 &&
                 !C.bracket) {
        C.bracket = true;
        FdcLine l{FdcLineId::CoachBracket};
        l.delay = 2.4;
        sink_.sayLine(l);
      } else if (prev.has_value() && std::fabs(dTgt - *prev) < *prev * 0.15 &&
                 dTgt > 80.0 && !C.stagnant) {
        C.stagnant = true;
        FdcLine l{FdcLineId::CoachStagnant};
        l.delay = 2.4;
        sink_.sayLine(l);
      }
    }
    m.bracketed = nowBracketed;
    m.lastAdjDist = dTgt;
  }
  /* spotting aid, tiered by difficulty (:1309-1347). The 1.5 s callback
     reads the ACTIVE mission at delivery time, exactly as the source's
     global reads do (plain schedule, not scheduleImpact). */
  if (!isFFE && difficulty_ != Difficulty::Hard) {
    const Vec2 e = view_.enemy();
    const SpotResult sp =
        spotBurst(op_.x, op_.z, e.x, e.z, impact.x, impact.z);
    const Vec3 eye = view_.observerEye();
    const bool seen = view_.hasLOS(
        eye.x, eye.y, eye.z, impact.x,
        std::fmax(view_.groundHeight(impact.x, impact.z), 0.0) + 2.0,
        impact.z);
    const Difficulty diff = difficulty_;
    queue_.schedule(clock_.now + 1.5, [this, sp, seen, diff]() {
      auto otDirReminderDue = [this]() {
        return active_ && !active_->otDirSent &&
               active_->method == MethodKind::Grid &&
               active_->adjustRounds <= 1;
      };
      if (!seen) {
        sink_.sysLine(SysLine{SysLineId::SpottingMasked});
        /* G8 — the masked path carries the direction reminder too (:1318). */
        if (otDirReminderDue())
          sink_.sysLine(SysLine{SysLineId::OtDirReminderMasked});
        return;
      }
      const bool over = sp.overShort >= 0;
      if (diff == Difficulty::Normal) {
        SysLine l{SysLineId::SpottingNormal};
        l.num0 = sp.mils; l.num1 = sp.otf; l.num2 = sp.otRange;
        l.text = over ? "over" : "short";
        sink_.sysLine(l);
      } else {
        /* :1332 — the source reads the global mission unguarded here; a null
           active mission at this instant is unreachable in play (the round
           just resolved), guarded anyway because core cannot throw. */
        const bool brk = active_ && active_->sides.longSide &&
                         active_->sides.shortSide;
        SysLine l{SysLineId::SpottingEasy};
        l.num0 = sp.mils; l.num1 = sp.otf; l.num2 = sp.devCorr;
        l.num3 = sp.otRange;
        l.text = brk ? "bracketed" : (over ? "over" : "short");
        sink_.sysLine(l);
      }
      if (otDirReminderDue())                          /* G8 (:1342-1345) */
        sink_.sysLine(SysLine{SysLineId::OtDirReminder});
    });
  }
  setState(isFFE ? MissionPhase::FireForEffect : MissionPhase::Adjusting);
}

// port of src/js/10-mission.js:1352-1387 (oracle-v1) — assessFFE().
void MissionMachine::assessFFE() {
  if (!active_) return;
  Mission& m = *active_;
  if (view_.type() == ScenarioType::Convoy) {
    int dead = 0;
    for (std::size_t i = 0; i < view_.vehicleCount(); i++)
      if (view_.vehicle(i).dead) dead++;
    m.hits = dead;
    if (m.hits >= 3) view_.setEnemyAlive(false);
    return;
  }
  /* G13 — hits is a diagnosis stat now (effect rounds inside rSupp). */
  const bool twRef = m.tgtRef == TgtRef::Twin && view_.hasTwin();
  const Vec2 el = twRef ? view_.twin() : view_.enemy();
  const double rSupp = effBands(asset_, view_.targetClass(), view_.effScale(),
                                tun_.effects).rSupp;
  int hits = 0;
  for (const Vec2& r : m.ffeRounds)
    if (dist2(r.x, r.z, el.x, el.z) < rSupp) hits++;
  m.hits = hits;
  const EffectAssessment a = assessEffect();
  if (view_.hasTwin() && m.tgtRef == TgtRef::Twin) {   /* SUGG5 (:1369-1373) */
    if (a.outcome == EffectOutcome::Destroyed ||
        a.outcome == EffectOutcome::Neutralized) {
      view_.setTwinAlive(false);
      sink_.presentation(PresentationEvent{PresentationKind::TwinTroopsDown});
    }
  } else if (a.outcome == EffectOutcome::Destroyed ||
             (a.outcome == EffectOutcome::Neutralized &&
              view_.type() != ScenarioType::Kaiju)) {  /* 11b (:1374) */
    view_.setEnemyAlive(false);
    sink_.presentation(PresentationEvent{PresentationKind::TroopsDown});
    if (view_.type() == ScenarioType::Strongpoint &&
        a.outcome == EffectOutcome::Destroyed)
      sink_.presentation(PresentationEvent{PresentationKind::VehiclesTipped});
    if (view_.type() == ScenarioType::Bunker &&
        a.outcome == EffectOutcome::Destroyed)
      sink_.presentation(PresentationEvent{PresentationKind::BunkerScorched});
    if (view_.type() == ScenarioType::Kaiju &&
        a.outcome == EffectOutcome::Destroyed)
      sink_.presentation(PresentationEvent{PresentationKind::KaijuScorched});
  }
}

/* ---- observer/net ops ------------------------------------------------- */

// port of src/js/10-mission.js:690-713 (oracle-v1) — handleFire(). Returns
// false when there is no live mission (caller speaks NET4 noMissionReply).
bool MissionMachine::observerFire() {
  if (!active_ || active_->done) return false;
  Mission& m = *active_;
  /* NET28 — refuse first, touch nothing (:697-700). */
  if (m.checkFire) {
    FdcLine l{FdcLineId::CheckFiringRefusalFire};
    l.delay = 0.9;
    sink_.sayLine(l);
    return true;
  }
  if (!m.pendingFire) {
    FdcLine l{m.amc ? FdcLineId::FireNothingLaid
                    : FdcLineId::FireNotAtMyCommand};
    l.delay = 0.9;
    sink_.sayLine(l);
    return true;
  }
  const std::function<void()> fn = m.pendingFire;
  m.pendingFire = nullptr;
  m.released = true;
  FdcLine l{FdcLineId::FireOut};
  l.delay = 0.5;
  sink_.sayLine(l);
  fn();
  return true;
}

// port of src/js/10-mission.js:714-733 (oracle-v1) — handleAtMyCommand(on).
void MissionMachine::setAtMyCommand(bool on) {
  if (!active_ || active_->done) {
    FdcLine l{on ? FdcLineId::AmcAdviceNoMission
                 : FdcLineId::AmcCancelNoMission};
    l.delay = 0.9;
    sink_.sayLine(l);
    return;
  }
  Mission& m = *active_;
  m.amc = on;
  FdcLine l{on ? FdcLineId::AmcOut : FdcLineId::CancelAmcOut};
  l.delay = 0.7;
  sink_.sayLine(l);
  if (!on && m.pendingFire) {
    /* cancel reverts to "when ready" — the laid round goes now (:724-732). */
    const std::function<void()> fn = m.pendingFire;
    m.pendingFire = nullptr;
    m.released = true;
    sink_.sysLine(SysLine{SysLineId::CancelAmcReverting});
    fn();
  }
}

// port of src/js/10-mission.js:734-743 (oracle-v1) — handleDoNotLoad().
void MissionMachine::doNotLoad() {
  if (!active_ || active_->done) {
    FdcLine l{FdcLineId::DoNotLoadIdle};
    l.delay = 0.7;
    sink_.sayLine(l);
    return;
  }
  active_->checkFire = true;      /* reuses G25's hold (:739-740) */
  active_->usedCheckFire = true;
  setState(MissionPhase::DoNotLoad);
  FdcLine l{FdcLineId::DoNotLoadOut};
  l.delay = 0.7;
  sink_.sayLine(l);
  sink_.sysLine(SysLine{SysLineId::DoNotLoadGuidance});
}

// port of src/js/10-mission.js:744-750 (oracle-v1) — handleCannotObserve().
bool MissionMachine::cannotObserve() {
  if (!active_ || active_->done) return false;   /* NET4 — caller replies */
  active_->cannotObserve = true;
  active_->notes.push_back(
      std::string("Declared CANNOT OBSERVE ") + kEmDash +
      " the FDC stops expecting spotting corrections.");
  FdcLine l{FdcLineId::CannotObserveOut};
  l.delay = 1.0;
  sink_.sayLine(l);
  return true;
}

// port of src/js/10-mission.js:751-762 (oracle-v1) — handleTimeOnTarget().
bool MissionMachine::timeOnTarget(double sec) {
  if (!active_ || active_->done) return false;   /* NET4 */
  active_->tot = sec;
  FdcLine l{FdcLineId::TimeOnTargetOut};
  l.delay = 0.8;
  l.num0 = sec;
  sink_.sayLine(l);
  SysLine s{SysLineId::TimeOnTargetFormatNote};
  s.num0 = sec;
  sink_.sysLine(s);
  return true;
}

// port of src/js/10-mission.js:683-689 (oracle-v1) — handleTofQuery().
void MissionMachine::tofQuery() {
  if (!active_ || active_->done) {
    FdcLine l{FdcLineId::TofQueryNoMission};
    l.delay = 0.9;
    sink_.sayLine(l);
    return;
  }
  FdcLine l{FdcLineId::TimeOfFlight};
  l.delay = 0.9;
  l.num0 = js::round(tofFor(battery_.x, battery_.z, active_->aim.x,
                            active_->aim.z, tun_.ballistics));
  sink_.sayLine(l);
}

// port of src/js/10-mission.js:631-645 (oracle-v1) — handleUnitStatus().
// The FDC layer composes the per-asset wording and the battery grid; the
// live books ride the payload.
void MissionMachine::unitStatusReport() {
  const bool repeat = unitStatusSent_;
  unitStatusSent_ = true;
  const AmmoTunables::Stock& a = ammoPool();
  FdcLine l{FdcLineId::UnitStatusReport};
  l.delay = 1.2;
  l.num0 = a.he; l.num1 = a.smoke; l.num2 = a.illum;
  if (repeat) l.text = "repeat";
  sink_.sayLine(l);
}

// port of src/js/10-mission.js:211-240 (oracle-v1) — scoreMTO +
// tryMTOReadback: the gist scorer over the observer's raw transmission.
bool MissionMachine::tryMtoReadback(const std::string& raw) {
  if (!active_ || active_->done || !active_->mto || active_->mto->read)
    return false;
  Mission& m = *active_;
  int hits = 0;
  for (const std::string& k : m.mto->keys)
    if (raw.find(k) != std::string::npos) hits++;
  const bool tgtOk = raw.find(m.mto->tgt) != std::string::npos;
  if (hits < 3 && !tgtOk) return false;      /* not a readback attempt */
  if (!tgtOk) {
    FdcLine l{FdcLineId::MtoWrongTargetNumber};
    l.delay = 1.0;
    l.text = m.mto->tgt;
    sink_.sayLine(l);
    m.notes.push_back(std::string("MTO readback ") +
                      (hasFourDigitRun(raw)
                           ? "carried the wrong target number"
                           : "omitted the target number") +
                      ".");
    return true;
  }
  if (hits < 3) {
    FdcLine l{FdcLineId::MtoNumberOnly};
    l.delay = 1.0;
    sink_.sayLine(l);
    return true;
  }
  m.mto->read = true;
  const bool verbatim =
      hits >= static_cast<int>(m.mto->keys.size()) - 1;
  FdcLine l{verbatim ? FdcLineId::MtoReadbackCorrect
                     : FdcLineId::MtoReadbackClose};
  l.delay = 0.9;
  sink_.sayLine(l);
  if (!verbatim && chapterStrict_)
    m.notes.push_back(std::string("MTO readback was gist, not verbatim ") +
                      kEmDash + " strict net wants it word for word.");
  return true;
}

// port of src/js/10-mission.js:1055-1070 (oracle-v1) — handleShell (12h +
// TEMPO4: intent follows the shell).
void MissionMachine::setShell(ShellKind kind) {
  if (!active_ || active_->done) {
    FdcLine l{FdcLineId::ShellNoMission};
    l.delay = 0.9;
    sink_.sayLine(l);
    return;
  }
  Mission& m = *active_;
  m.shell = kind;
  m.intent = kind == ShellKind::Illum ? IntentKind::Illum
           : kind == ShellKind::Smoke ? IntentKind::Suppress
           : (m.imm != ImmKind::None ? IntentKind::Suppress
                                     : IntentKind::Destroy);
  FdcLine l{FdcLineId::ShellOut};
  l.delay = 0.9;
  l.text = shellName(kind);
  sink_.sayLine(l);
}

// port of src/js/10-mission.js:1036-1050 (oracle-v1) — handleFuze (G16).
void MissionMachine::setFuze(FuzeKind kind) {
  if (!active_ || active_->done) {
    FdcLine l{FdcLineId::FuzeNoMission};
    l.delay = 0.9;
    sink_.sayLine(l);
    return;
  }
  Mission& m = *active_;
  if ((kind == FuzeKind::Vt || kind == FuzeKind::Time) &&
      m.gridStr.find("DANGER CLOSE") != std::string::npos) {
    m.fuze = FuzeSpec{FuzeKind::Pd, "fdc-override",
                      std::string("VT/time refused ") + kEmDash +
                          " airburst is not fired DANGER CLOSE; PD retained"};
    m.notes.push_back(
        std::string("Requested an airburst fuze on a danger-close mission ") +
        kEmDash + " VT frags over our own people. The FDC overrode to PD.");
    FdcLine l{FdcLineId::FuzeVtDangerCloseRefusal};
    l.delay = 1.1;
    sink_.sayLine(l);
    return;
  }
  m.fuze = FuzeSpec{kind, "requested",
                    "FUZE " + upperAscii(fuzeName(kind)) +
                        " requested mid-mission"};
  FdcLine l{FdcLineId::FuzeOut};
  l.delay = 0.9;
  l.text = fuzeName(kind);
  sink_.sayLine(l);
}

// port of src/js/10-mission.js:1072-1088 (oracle-v1) — handleSheaf (G15).
void MissionMachine::setSheaf(RequestedSheaf kind, bool cancel) {
  if (!active_ || active_->done) {
    FdcLine l{FdcLineId::SheafNoMission};
    l.delay = 0.9;
    sink_.sayLine(l);
    return;
  }
  Mission& m = *active_;
  if (cancel) {
    /* ATP 3-09.30 §5-30 — revert to what the FDC would have chosen (:1079). */
    m.sheaf = inferSheafFromDesc(m.desc);
    FdcLine l{FdcLineId::SheafCancelOut};
    l.delay = 1.0;
    l.text = std::string(requestedSheafName(kind)) + "|" +
             sheafKindName(m.sheaf.kind);
    sink_.sayLine(l);
    return;
  }
  const SheafKind k =
      (kind == RequestedSheaf::Parallel || kind == RequestedSheaf::Special)
          ? SheafKind::Linear
          : kind == RequestedSheaf::Converged ? SheafKind::Converged
          : kind == RequestedSheaf::Circular ? SheafKind::Circular
          : kind == RequestedSheaf::Open ? SheafKind::Open
          : SheafKind::Linear;
  m.sheaf = SheafSpec{k, "requested",
                      upperAscii(requestedSheafName(kind)) +
                          " SHEAF requested mid-mission"};
  if (kind == RequestedSheaf::Special)
    m.notes.push_back(
        std::string("A SPECIAL sheaf carries an attitude (long-axis azimuth) "
                    "when a length is given ") +
        kEmDash + " none was sent; fired as linear.");
  FdcLine l{FdcLineId::SheafOut};
  l.delay = 1.0;
  l.text = requestedSheafName(kind);
  sink_.sayLine(l);
}

/* ---- desc-based inference (:986-993 / :1020-1025) --------------------- */

// port of src/js/10-mission.js:986-993 (oracle-v1) — inferSheaf, desc and
// scenario branches only (the raw-request regex at :980-985 is the wave-2
// parser's; it hands a requested SheafSpec instead).
SheafSpec MissionMachine::inferSheafFromDesc(const std::string& desc) const {
  const std::string d = toLowerAscii(desc);
  if (view_.type() == ScenarioType::Convoy ||
      containsAny(d, {"convoy", "column", "vehicle", "truck"}))
    return SheafSpec{SheafKind::Linear, "inferred",
                     std::string("linear target (convoy/column) ") + kEmDash +
                         " bursts spread along its axis"};
  if (view_.targetClass() == TargetClass::Point ||
      containsAny(d, {"bunker", "pit", "emplacement", "gun", "structure",
                      "building", "craft", "wreck"}))
    return SheafSpec{SheafKind::Converged, "inferred",
                     std::string("small hard target ") + kEmDash +
                         " every piece on the same point"};
  if (containsAny(d, {"troops", "infantry", "personnel", "squad", "platoon",
                      "dismount"}))
    return SheafSpec{SheafKind::Open, "inferred",
                     std::string("personnel target ") + kEmDash +
                         " bursts one effective width apart"};
  return SheafSpec{SheafKind::Circular, "default",
                   std::string("nothing requested, nothing to infer ") +
                       kEmDash + " doctrine default (circular, 100 m)"};
}

// port of src/js/10-mission.js:1020-1025 (oracle-v1) — inferFuze, desc and
// scenario branches only (raw-request regex at :1013-1019 is wave-2).
FuzeSpec MissionMachine::inferFuzeFromDesc(const std::string& desc) const {
  const std::string d = toLowerAscii(desc);
  if (containsAny(d, {"trench", "foxhole", "dug in", "dug-in", "entrench"}))
    return FuzeSpec{FuzeKind::Vt, "inferred",
                    std::string("troops under cover ") + kEmDash +
                        " airburst reaches into the holes"};
  if (view_.targetClass() == TargetClass::Point ||
      containsAny(d, {"bunker", "structure", "building", "emplacement",
                      "woods", "earthwork"}))
    return FuzeSpec{FuzeKind::Delay, "inferred",
                    std::string("hard/overhead-cover target ") + kEmDash +
                        " penetrate, then burst"};
  return FuzeSpec{FuzeKind::Pd, "default",
                  std::string("standard method of engagement ") + kEmDash +
                      " point detonating (quick)"};
}

/* ---- planned fires ---------------------------------------------------- */

// The shared "we are mid-mission" refusal (:321-324 and siblings).
bool MissionMachine::midMissionRefuse(MissionOp op) {
  if (active_ && !active_->done) {
    FdcLine l{FdcLineId::MidMissionRefusal};
    l.delay = 1.0;
    l.num0 = static_cast<double>(op);
    sink_.sayLine(l);
    return true;
  }
  return false;
}

// port of src/js/10-mission.js:293-308 (oracle-v1) — handlePriorityTarget.
void MissionMachine::priorityTarget(const std::string& tgtNum) {
  if (!findRecTgt(tgtNum)) {
    FdcLine l{FdcLineId::NoSuchTargetPriority};
    l.delay = 1.0;
    l.text = joinComma(recTgtKeys());
    sink_.sayLine(l);
    return;
  }
  const bool moved = !priority_.empty() && priority_ != tgtNum;
  priority_ = tgtNum;
  FdcLine l{FdcLineId::PrioritySet};
  l.delay = 1.0;
  l.text = tgtNum;
  l.num0 = moved ? 1 : 0;
  sink_.sayLine(l);
  SysLine s{SysLineId::PriorityHint};
  s.text = tgtNum;
  sink_.sysLine(s);
}

// port of src/js/10-mission.js:320-342 (oracle-v1) — handlePlanTarget
// (SUGG8): file a target from a grid without firing.
void MissionMachine::planTarget(const std::string& digits) {
  if (midMissionRefuse(MissionOp::PlanTarget)) return;
  if (digits.size() != 6 && digits.size() != 8) {
    FdcLine l{FdcLineId::PlanNeedsGrid};
    l.delay = 1.0;
    sink_.sayLine(l);
    return;
  }
  const double e = digits.size() == 8
      ? parseIntDigits(digits.substr(0, 4)) * 10 + 5
      : parseIntDigits(digits.substr(0, 3)) * 100 + 50;
  const double n = digits.size() == 8
      ? parseIntDigits(digits.substr(4)) * 10 + 5
      : parseIntDigits(digits.substr(3)) * 100 + 50;
  const WorldXZ w = enToWorld(e, n, map_);
  if (std::fabs(w.x) > 5000.0 || std::fabs(w.z) > 5000.0) {
    FdcLine l{FdcLineId::OffMapSheet};
    l.delay = 1.0;
    sink_.sayLine(l);
    return;
  }
  const std::string tn = nextFreeTgtNum();
  putRecTgt(tn, w.x, w.z);
  FdcLine l{FdcLineId::TargetPlanned};
  l.delay = 1.0;
  l.text = tn;
  l.num0 = static_cast<double>(digits.size());
  sink_.sayLine(l);
  SysLine s{SysLineId::PlanTargetHint};
  s.text = tn;
  sink_.sysLine(s);
}

// port of src/js/10-mission.js:343-367 (oracle-v1) — handleFireTarget
// (SUGG2). The source infers sheaf/fuze from p.raw; the wave-2 caller
// resolves those and passes the specs.
void MissionMachine::fireTarget(const std::string& tgtNum,
                                const SheafSpec& sheaf, const FuzeSpec& fuze) {
  if (midMissionRefuse(MissionOp::FireTarget)) return;
  const RecTgt* tgt = findRecTgt(tgtNum);
  if (!tgt) {
    FdcLine l{FdcLineId::NoSuchTargetFire};
    l.delay = 1.0;
    l.text = joinComma(recTgtKeys());
    sink_.sayLine(l);
    return;
  }
  const bool pri = priority_ == tgtNum;
  if (!pri) {
    FdcLine l{FdcLineId::StandbyRelay};
    l.delay = 0.8;
    l.text = tgtNum;
    sink_.sayLine(l);
  }
  FdcLine l{FdcLineId::FireTargetOut};
  l.delay = pri ? 0.6 : 1.4;
  l.text = tgtNum;
  l.num0 = pri ? 1 : 0;
  sink_.sayLine(l);
  setState(MissionPhase::MissionSent);
  MissionMeta meta;
  meta.desc = "RECORDED TARGET";
  meta.gridStr = "TARGET " + tgtNum;
  meta.method = MethodKind::Grid;
  meta.intent = IntentKind::Destroy;
  meta.planned = true;                    /* SUGG4b */
  meta.sheaf = sheaf;
  meta.fuze = fuze;
  meta.shotExtra = pri ? 0 : 22;          /* laid guns shoot now (:366) */
  fireMission(Vec2{tgt->x, tgt->z}, Warno::Ffe, meta);
}

// port of src/js/10-mission.js:561-589 (oracle-v1) — handleSuppressTarget.
void MissionMachine::suppressTarget(const std::string& tgtNum, double minutes,
                                    const SheafSpec& sheaf,
                                    const FuzeSpec& fuze) {
  if (midMissionRefuse(MissionOp::SuppressTarget)) return;
  const RecTgt* tgt = findRecTgt(tgtNum);
  if (!tgt) {
    FdcLine l{FdcLineId::NoSuchTargetSuppress};
    l.delay = 1.0;
    l.text = joinComma(recTgtKeys());
    sink_.sayLine(l);
    return;
  }
  FdcLine l{FdcLineId::SuppressTargetOut};
  l.delay = 0.8;
  l.text = tgtNum;
  l.num0 = minutes;
  sink_.sayLine(l);
  if (minutes == 0) {
    SysLine s{SysLineId::SuppressNoDuration};
    s.text = tgtNum;
    sink_.sysLine(s);
  }
  setState(MissionPhase::MissionSent);
  MissionMeta meta;
  if (minutes == 0)
    meta.notes.push_back("Suppress mission sent without a duration.");
  meta.desc = "SUPPRESSION";
  meta.gridStr = "TARGET " + tgtNum;
  meta.method = MethodKind::Grid;
  meta.intent = IntentKind::Suppress;
  meta.planned = true;
  meta.sheaf = sheaf;
  meta.fuze = fuze;
  fireMission(Vec2{tgt->x, tgt->z}, Warno::Ffe, meta);
}

// port of src/js/10-mission.js:488-503 (oracle-v1) — handlePlanSeries (SUGG4).
void MissionMachine::planSeries(const std::string& name,
                                const std::vector<std::string>& tgts) {
  if (tgts.empty()) {
    FdcLine l{FdcLineId::SeriesNeedsTargets};
    l.delay = 1.0;
    sink_.sayLine(l);
    return;
  }
  std::vector<std::string> missing;
  for (const std::string& n : tgts)
    if (!findRecTgt(n)) missing.push_back(n);
  if (!missing.empty()) {
    FdcLine l{FdcLineId::PlanMissingTargets};
    l.delay = 1.0;
    l.text = joinComma(missing);
    sink_.sayLine(l);
    return;
  }
  for (auto& kv : series_)
    if (kv.first == name) { kv.second = tgts; goto established; }
  series_.emplace_back(name, tgts);
established:
  FdcLine l{FdcLineId::SeriesEstablished};
  l.delay = 1.0;
  l.text = name;
  l.num0 = static_cast<double>(tgts.size());
  sink_.sayLine(l);
}

// port of src/js/10-mission.js:504-522 (oracle-v1) — handleFireSeries.
void MissionMachine::fireSeries(const std::string& name) {
  const std::vector<std::string>* s = nullptr;
  for (const auto& kv : series_)
    if (kv.first == name) { s = &kv.second; break; }
  if (!s) {
    std::vector<std::string> known;
    for (const auto& kv : series_) known.push_back(kv.first);
    FdcLine l{FdcLineId::NoSuchSeries};
    l.delay = 1.0;
    l.text = joinComma(known);
    sink_.sayLine(l);
    return;
  }
  if (midMissionRefuse(MissionOp::FireSeries)) return;
  FdcLine l{FdcLineId::FiringSeries};
  l.delay = 0.7;
  l.text = name;
  l.num0 = static_cast<double>(s->size());
  sink_.sayLine(l);
  setState(MissionPhase::MissionSent);
  const RecTgt* first = findRecTgt((*s)[0]);
  MissionMeta meta;
  meta.desc = "SERIES " + name;
  meta.gridStr = "SERIES " + name;
  meta.method = MethodKind::Grid;
  meta.intent = IntentKind::Destroy;
  meta.planned = true;
  meta.series = *s;
  fireMission(Vec2{first->x, first->z}, Warno::Ffe, meta);
}

// port of src/js/10-mission.js:527-541 (oracle-v1) — handlePlanGroup (SUGG3).
void MissionMachine::planGroup(const std::string& name,
                               const std::vector<std::string>& tgts) {
  if (tgts.size() < 2) {
    FdcLine l{FdcLineId::GroupNeedsTwo};
    l.delay = 1.0;
    sink_.sayLine(l);
    return;
  }
  std::vector<std::string> missing;
  for (const std::string& n : tgts)
    if (!findRecTgt(n)) missing.push_back(n);
  if (!missing.empty()) {
    FdcLine l{FdcLineId::PlanMissingTargets};
    l.delay = 1.0;
    l.text = joinComma(missing);
    sink_.sayLine(l);
    return;
  }
  for (auto& kv : groups_)
    if (kv.first == name) { kv.second = tgts; goto established; }
  groups_.emplace_back(name, tgts);
established:
  FdcLine l{FdcLineId::GroupEstablished};
  l.delay = 1.0;
  l.text = name;
  l.num0 = static_cast<double>(tgts.size());
  sink_.sayLine(l);
}

// port of src/js/10-mission.js:542-560 (oracle-v1) — handleFireGroup.
void MissionMachine::fireGroup(const std::string& name) {
  const std::vector<std::string>* g = nullptr;
  for (const auto& kv : groups_)
    if (kv.first == name) { g = &kv.second; break; }
  if (!g) {
    std::vector<std::string> known;
    for (const auto& kv : groups_) known.push_back(kv.first);
    FdcLine l{FdcLineId::NoSuchGroup};
    l.delay = 1.0;
    l.text = joinComma(known);
    sink_.sayLine(l);
    return;
  }
  if (midMissionRefuse(MissionOp::FireGroup)) return;
  FdcLine l{FdcLineId::FiringGroup};
  l.delay = 0.7;
  l.text = name;
  sink_.sayLine(l);
  setState(MissionPhase::MissionSent);
  const RecTgt* first = findRecTgt((*g)[0]);
  MissionMeta meta;
  meta.desc = "GROUP " + name;
  meta.gridStr = "GROUP " + name;
  meta.method = MethodKind::Grid;
  meta.intent = IntentKind::Destroy;
  meta.planned = true;
  meta.group = *g;
  fireMission(Vec2{first->x, first->z}, Warno::Ffe, meta);
}

/* ---- FPF (SUGG1) ------------------------------------------------------ */

// port of src/js/10-mission.js:405-444 (oracle-v1) — handlePlanFPF. The
// grid/direction PRESENCE validation (:410-417) is the wave-2 parser's;
// this method receives a resolved center and attitude. saidDangerClose is
// the proword check's verdict (01-math-terrain.js:157-158, parser side).
void MissionMachine::planFpf(Vec2 center, double dirMils, bool saidDangerClose,
                             const std::string& gridDigits) {
  if (midMissionRefuse(MissionOp::PlanFpf)) return;
  double minF = 1e300;                       /* Infinity (:427) */
  for (const FriendlyPos& f : view_.friendlies())
    minF = std::fmin(minF, dist2(center.x, center.z, f.x, f.z));
  const double dcr = dangerCloseRadius(asset_, tun_.mission);
  if (minF <= dcr && !saidDangerClose) {
    FdcLine l{FdcLineId::FpfDangerCloseQuip};
    l.delay = 1.2;
    sink_.sayLine(l);
    return;
  }
  const double az = std::fmod(dirMils, 6400.0) / MILS_PER_RAD;
  const double lx = std::sin(az), lz = -std::cos(az);   /* along the attitude */
  fpf_ = std::make_unique<FpfState>(
      js::toUint32(seedMission_ * 2246822519.0 + 13.0));  /* stream #10 (:437) */
  const double offs[4] = {-60, -20, 20, 60};
  for (int i = 0; i < 4; i++)
    fpf_->pts[i] = Vec2{center.x + lx * offs[i], center.z + lz * offs[i]};
  fpf_->az = az;
  FdcLine l{FdcLineId::FpfEstablished};
  l.delay = 1.2;
  l.text = gridDigits;
  l.num0 = std::fmod(dirMils, 6400.0);
  l.num1 = (minF <= dcr) ? 1 : 0;
  sink_.sayLine(l);
  sink_.sysLine(SysLine{SysLineId::FpfInstructions});
  fpfMarkRound(0);
}

// port of src/js/10-mission.js:381-404 (oracle-v1) — fpfMarkRound(g):
// presentation round off the FPF's own stream, missionless (plain schedule),
// but the fratricide invariant is NOT waived (GFX9: booked first).
void MissionMachine::fpfMarkRound(int g) {
  const Vec2 p0 = fpf_->pts[g];
  const ErrXZ err = followUpError(fpf_->rng, azTo(op_.x, op_.z, p0.x, p0.z),
                                  difficulty_, asset_, tun_.ballistics);
  const double ix = p0.x + err.x, iz = p0.z + err.z;
  const double tof = tofFor(battery_.x, battery_.z, p0.x, p0.z,
                            tun_.ballistics);
  queue_.schedule(clock_.now + tun_.fdc.shotDelay + tof - 2.0,
                  [this, ix, iz]() {
    ammoSpend(ShellKind::He);        /* SUGG7 — marking rounds are real (:386) */
    for (const FriendlyPos& f : view_.friendlies()) {
      const double r = f.hasR ? f.r : tun_.mission.fratricideRadius;
      if (dist2(ix, iz, f.x, f.z) < r) {
        fpf_->frat = true;
        sink_.sysLine(SysLine{SysLineId::MarkingRoundCompromised});
        FdcLine l{FdcLineId::FpfMarkOnFriendlies};
        l.delay = 0.8;
        sink_.sayLine(l);
      }
    }
    PresentationEvent burst{PresentationKind::Burst};
    burst.x = ix;
    burst.y = std::fmax(view_.groundHeight(ix, iz), 0.0);
    burst.z = iz;
    sink_.presentation(burst);
  });
}

// port of src/js/10-mission.js:445-467 (oracle-v1) — handleFPFAdjust.
void MissionMachine::adjustFpf(int gun, double right, double add,
                               bool repeat) {
  if (!fpf_) {
    FdcLine l{FdcLineId::NoFpfPlanned};
    l.delay = 1.0;
    sink_.sayLine(l);
    return;
  }
  if (gun < 1 || gun > 4) {
    FdcLine l{FdcLineId::FourGunsOnly};
    l.delay = 1.0;
    l.num0 = gun;
    sink_.sayLine(l);
    return;
  }
  const int g = gun - 1;
  if (!repeat) {
    /* per-piece OT-frame move — the same frame every correction uses. */
    Vec2& pt = fpf_->pts[g];
    const double ot = azTo(op_.x, op_.z, pt.x, pt.z);
    const double fx = std::sin(ot), fz = -std::cos(ot);
    const double rx = std::cos(ot), rz = std::sin(ot);
    pt.x += fx * add + rx * right;
    pt.z += fz * add + rz * right;
    fpf_->adj[g] = true;
  }
  FdcLine l{FdcLineId::FpfNumberAck};
  l.delay = 0.8;
  l.num0 = gun; l.num1 = right; l.num2 = add;
  if (repeat) l.text = "repeat";
  sink_.sayLine(l);
  fpfMarkRound(g);
}

// port of src/js/10-mission.js:468-482 (oracle-v1) — handleFireFPF.
void MissionMachine::fireFpf() {
  if (!fpf_) {
    FdcLine l{FdcLineId::NoFpfPlanned};
    l.delay = 1.0;
    sink_.sayLine(l);
    return;
  }
  if (midMissionRefuse(MissionOp::FireFpf)) return;
  FdcLine l{FdcLineId::FiringFpf};
  l.delay = 0.5;
  sink_.sayLine(l);
  setState(MissionPhase::MissionSent);
  MissionMeta meta;
  meta.desc = "FINAL PROTECTIVE FIRE";
  meta.gridStr = "THE FPF";
  meta.method = MethodKind::Grid;
  meta.intent = IntentKind::Suppress;
  meta.planned = true;
  meta.fpf = true;
  fireMission(Vec2{(fpf_->pts[1].x + fpf_->pts[2].x) / 2.0,
                   (fpf_->pts[1].z + fpf_->pts[2].z) / 2.0},
              Warno::Ffe, meta);
}

} }
