#include "scenario_view_adapter.h"

#include <cmath>

#include "shitfire/terrain/los.h"

namespace shitfire { namespace core {

// SCN_TYPES string -> the mission machine's enum (00-config.js:323-346).
sim::ScenarioType ScenarioViewAdapter::type() const {
  const std::string& t = S_->type;
  if (t == "strongpoint") return sim::ScenarioType::Strongpoint;
  if (t == "troops") return sim::ScenarioType::Troops;
  if (t == "bunker") return sim::ScenarioType::Bunker;
  if (t == "convoy") return sim::ScenarioType::Convoy;
  if (t == "assault") return sim::ScenarioType::Assault;
  if (t == "raid") return sim::ScenarioType::Raid;
  if (t == "wreck") return sim::ScenarioType::Wreck;
  if (t == "battery") return sim::ScenarioType::Battery;
  if (t == "defense") return sim::ScenarioType::Defense;
  if (t == "callin") return sim::ScenarioType::Callin;
  if (t == "occupied") return sim::ScenarioType::Occupied;
  if (t == "qfp") return sim::ScenarioType::Qfp;
  if (t == "twin") return sim::ScenarioType::Twin;
  if (t == "chow") return sim::ScenarioType::Chow;
  return sim::ScenarioType::Kaiju;
}

// port of src/js/05-units-scenario.js:792-812 (oracle-v1) —
// friendlyPositions(). Static types come straight off S.friendlies; the
// assault line advances with the clock (the wave-3 seam this adapter closes).
std::vector<sim::FriendlyPos> ScenarioViewAdapter::friendlies() const {
  std::vector<sim::FriendlyPos> out;
  if (S_->type != "assault") {
    out.reserve(S_->friendlies.size());
    for (const scenario::FriendlyPos& f : S_->friendlies)
      out.push_back({f.x, f.z, f.r, true});   /* every generated r is set */
    return out;
  }
  const double adv =
      std::fmin(*S_->fSpeed * (clock_->now - *S_->ft0), *S_->fAdvMax);
  const double fx = std::sin(*S_->fAdvAz), fz = -std::cos(*S_->fAdvAz);
  const double rx = std::cos(*S_->fAdvAz), rz = std::sin(*S_->fAdvAz);
  out.reserve(6);
  for (int i = 0; i < 6; i++) {
    const double l = (i - 2.5) * 8;
    out.push_back({S_->fStart->x + fx * adv + rx * l,
                   S_->fStart->z + fz * adv + rz * l, 40, true});
  }
  return out;
}

// WORLD.civs in insertion order (10-mission.js:1233 reads c.x/c.z live).
std::vector<sim::Vec2> ScenarioViewAdapter::civilians() const {
  std::vector<sim::Vec2> out;
  out.reserve(world_->civs.size());
  for (const scenario::Civ& c : world_->civs) out.push_back({c.x, c.z});
  return out;
}

// villages[].huts, village order then hut order (10-mission.js:1234).
std::vector<sim::Vec2> ScenarioViewAdapter::huts() const {
  std::vector<sim::Vec2> out;
  for (const scenario::Village& v : world_->villages)
    for (const scenario::Hut& h : v.huts) out.push_back({h.x, h.z});
  return out;
}

// port of src/js/05-units-scenario.js:107-114 (oracle-v1) — convoyHeadD().
double ScenarioViewAdapter::convoyHeadD() const {
  const double raw = *S_->speed * (clock_->now - *S_->t0);
  const double len = S_->path->len;
  const auto clampLen = [len](double v) {
    return v < 0 ? 0 : (v > len ? len : v);
  };
  if (raw < S_->stop->d || !S_->stop->tArr.has_value()) return clampLen(raw);
  const double halted = clock_->now - *S_->stop->tArr;
  if (halted < S_->stop->dur) return S_->stop->d;
  return clampLen(S_->stop->d + *S_->speed * (halted - S_->stop->dur));
}

} }
