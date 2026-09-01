#include "shitfire/sim/mission_ballistics.h"

#include <cmath>

#include "shitfire/math/mils.h"
#include "shitfire/math/noise.h"

namespace shitfire { namespace sim {

namespace {
constexpr double kPi = 3.141592653589793;   /* JS Math.PI */
}

// port of src/js/10-mission.js:25-30 (oracle-v1) —
// function gauss(rng) {
//   let u = 0, v = 0;
//   while (u === 0) u = rng();
//   while (v === 0) v = rng();
//   return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
// }
double gauss(Mulberry32& rng) {
  double u = 0, v = 0;
  while (u == 0) u = rng.next();
  while (v == 0) v = rng.next();
  return std::sqrt(-2.0 * std::log(u)) * std::cos(2.0 * kPi * v);
}

// port of src/js/10-mission.js:60-63 (oracle-v1) — assetScale().
double assetScale(AssetKind a) {
  return a == AssetKind::Mortar60 ? 0.55
       : a == AssetKind::Sunlamp  ? 0.22
       : 1.0;
}

// port of src/js/10-mission.js:79-82 (oracle-v1) — dangerCloseRadius().
double dangerCloseRadius(AssetKind a, const MissionTunables& m) {
  return a == AssetKind::Mortar60 ? m.dangerCloseMortar60
                                  : m.dangerCloseBattery;
}

// CONFIG.BALLISTICS.firstRound[DIFFICULTY] (00-config.js:53).
BallisticsTunables::Band firstRoundBand(Difficulty d,
                                        const BallisticsTunables& B) {
  switch (d) {
    case Difficulty::Easy: return B.firstRoundEasy;
    case Difficulty::Hard: return B.firstRoundHard;
    default:               return B.firstRoundNormal;
  }
}

// CONFIG.BALLISTICS.followUp[DIFFICULTY] (00-config.js:54-55).
BallisticsTunables::FollowUp followUpBand(Difficulty d,
                                          const BallisticsTunables& B) {
  switch (d) {
    case Difficulty::Easy: return B.followUpEasy;
    case Difficulty::Hard: return B.followUpHard;
    default:               return B.followUpNormal;
  }
}

// port of src/js/10-mission.js:88-93 (oracle-v1) — firstRoundError(rng).
// Draw order: (1) the magnitude draw feeding lerp, (2) the azimuth draw;
// only THEN the dispersion gate (G3). C++ argument evaluation order is
// unspecified, so the draws are sequenced as named locals.
ErrXZ firstRoundError(Mulberry32& rng, Difficulty d, AssetKind a,
                      const BallisticsTunables& B) {
  const BallisticsTunables::Band band = firstRoundBand(d, B);
  const double t = rng.next();
  const double mag = lerp(band.lo, band.hi, t) * assetScale(a);
  const double az = rng.next() * kPi * 2.0;
  if (!B.dispersion) return ErrXZ{0, 0};
  return ErrXZ{std::sin(az) * mag, -std::cos(az) * mag};
}

// port of src/js/10-mission.js:94-102 (oracle-v1) — followUpError(rng, otAz).
// along (range) then across (deflection), each one gauss() = two-plus draws;
// gate AFTER both, then the OT frame projection.
ErrXZ followUpError(Mulberry32& rng, double otAz, Difficulty d, AssetKind a,
                    const BallisticsTunables& B) {
  const BallisticsTunables::FollowUp s = followUpBand(d, B);
  const double k = assetScale(a);
  const double along = gauss(rng) * s.range * k;
  const double across = gauss(rng) * s.defl * k;
  if (!B.dispersion) return ErrXZ{0, 0};
  const double fx = std::sin(otAz), fz = -std::cos(otAz);
  const double rx = std::cos(otAz), rz = std::sin(otAz);
  return ErrXZ{fx * along + rx * across, fz * along + rz * across};
}

// port of src/js/10-mission.js:110-114 (oracle-v1) — tofFor(aim).
double tofFor(double batteryX, double batteryZ, double aimX, double aimZ,
              const BallisticsTunables& B) {
  const double range = dist2(batteryX, batteryZ, aimX, aimZ);
  return clamp(range / B.tofDivisor + B.tofBase, B.tofMin, B.tofMax);
}

} }
