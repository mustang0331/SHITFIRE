#pragma once
#include <vector>

#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim/mission_clock.h"
#include "shitfire/sim/mission_scenario_view.h"
#include "shitfire/terrain/heightfield.h"
#include "shitfire/terrain/los.h"

/* wave-3 reconciliation: the adapter the mission porter's seam design named
   (mission_scenario_view.h header note) — sim::IScenarioView implemented
   over the scenario porter's ScenarioState plus the world/terrain state the
   browser read through globals (WORLD.civs, villages, H, hasLOS, eye).
   Internal to the SimCore facade — not part of the public surface.

   Ownership notes:
   - enemyAlive is the module-level global of 05-units-scenario.js:78; it
     lives HERE (reset true per placeUnits, 05:1080; written by the machine
     via setEnemyAlive).
   - the assault friendlies advancing line is a mission-time function
     (friendlyPositions, 05:792-812) computed off the clock, closing the
     flagged seam; every other type reads S.friendlies.
   - convoyHeadD is the read-only twin of updateScenario's piecewise
     time->distance (05:107-114). */
namespace shitfire { namespace core {

class ScenarioViewAdapter : public sim::IScenarioView {
public:
  ScenarioViewAdapter(scenario::ScenarioState& S,
                      scenario::WorldLayout& world, const HeightField& hf,
                      const scenario::EyePos& eye, const sim::SimClock& clock,
                      double scenarioT0)
      : S_(&S), world_(&world), hf_(&hf), eye_(eye), clock_(&clock),
        scenarioT0_(scenarioT0) {}

  void resetEnemyAlive() { enemyAlive_ = true; }   /* 05:1080 */

  /* ---- reads ---- */
  sim::ScenarioType type() const override;
  double scenarioSeed() const override { return S_->seed; }
  double scenarioT0() const override { return scenarioT0_; }
  sim::Vec2 enemy() const override { return {S_->enemy.x, S_->enemy.z}; }
  bool hasEnemy() const override { return true; }   /* set for every type */
  bool hasTwin() const override { return S_->twinTgt.has_value(); }
  sim::Vec2 twin() const override {
    return {S_->twinTgt->x, S_->twinTgt->z};
  }
  sim::TargetClass targetClass() const override {
    return S_->tgtClass == "point" ? sim::TargetClass::Point
                                   : sim::TargetClass::Personnel;
  }
  double effScale() const override { return S_->effScale; }
  double armor() const override { return S_->armor.value_or(0); }
  /* 'dug-in' (battery/occupied, 05:464/:696) maps to Standing: every JS read
     is either `=== 'prone'` (10:1032) or a posture-table lookup whose miss
     falls back to 1 = the standing figure (10:961-962). */
  sim::Posture posture() const override {
    return S_->posture == "prone" ? sim::Posture::Prone
                                  : sim::Posture::Standing;
  }
  bool dispersed() const override { return S_->dispersed != 0; }
  int alerted() const override { return static_cast<int>(S_->alerted); }
  bool enemyAlive() const override { return enemyAlive_; }
  bool twinAlive() const override {
    return S_->twinTgt ? S_->twinTgt->alive : true;
  }
  double eff() const override { return S_->eff; }
  double twinEff() const override { return S_->twinTgt ? S_->twinTgt->eff : 0; }
  bool everSuppressed() const override { return S_->everSuppressed; }
  bool twinEverSuppressed() const override {
    return S_->twinTgt ? S_->twinTgt->everSuppressed : false;
  }
  std::vector<sim::FriendlyPos> friendlies() const override;
  std::vector<sim::Vec2> civilians() const override;
  std::vector<sim::Vec2> huts() const override;
  double groundHeight(double x, double z) const override {
    return hf_->H(x, z);
  }
  bool hasLOS(double ax, double ay, double az, double bx, double by,
              double bz) const override {
    return ::shitfire::hasLOS(*hf_, ax, ay, az, bx, by, bz);
  }
  sim::Vec3 observerEye() const override {
    return {eye_.x, eye_.y, eye_.z};
  }

  /* ---- convoy ---- */
  std::size_t vehicleCount() const override {
    return S_->veh ? S_->veh->size() : 0;
  }
  sim::VehicleView vehicle(std::size_t i) const override {
    const scenario::Veh& v = (*S_->veh)[i];
    return {v.x, v.z, v.d, v.dead};
  }
  bool convoyHasPath() const override { return S_->path.has_value(); }
  sim::Dir2 pathDirAt(double d) const override {
    const WorldXZ q = scenario::pathDir(*S_->path, d);
    return {q.x, q.z};
  }
  double convoyHeadD() const override;

  /* ---- writes ---- */
  void addEff(double contrib, bool twinBooks) override {
    if (twinBooks) S_->twinTgt->eff += contrib;   /* 10:969 */
    else S_->eff += contrib;
  }
  void setSuppressed(double until, bool twinBooks) override {   /* 10:953-958 */
    if (twinBooks) {
      S_->twinTgt->everSuppressed = true;
      S_->twinTgt->suppressedUntil = until;
    } else {
      S_->everSuppressed = true;
      S_->suppressedUntil = until;
    }
  }
  void raiseSuppressedTo(double until) override {   /* 08-burst:476-477 */
    S_->everSuppressed = true;
    if (until > S_->suppressedUntil) S_->suppressedUntil = until;
  }
  void bumpAlerted() override { S_->alerted += 1; }        /* 10:1249 */
  void setDispersed() override { S_->dispersed = 1; }      /* 10:1254 */
  void setPostureProne() override { S_->posture = "prone"; } /* 10:1267 */
  void setEnemyAlive(bool alive) override { enemyAlive_ = alive; }
  void setTwinAlive(bool alive) override { S_->twinTgt->alive = alive; }
  void killVehicle(std::size_t i) override { (*S_->veh)[i].dead = true; }

private:
  scenario::ScenarioState* S_;
  scenario::WorldLayout* world_;
  const HeightField* hf_;
  scenario::EyePos eye_;
  const sim::SimClock* clock_;
  double scenarioT0_;
  bool enemyAlive_ = true;   /* 05:78 module global */
};

} }
