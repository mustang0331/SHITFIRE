#include "shitfire/sim/mission_effects.h"

namespace shitfire { namespace sim {

// port of src/js/10-mission.js:926-934 (oracle-v1) — effBands().
// const c = E[asset][S.tgtClass] || E[asset].personnel;  (enum is total here)
// const k = S.effScale || 1;
EffBandSet effBands(AssetKind asset, TargetClass cls, double effScale,
                    const EffectsTunables& E) {
  const bool m60 = (asset == AssetKind::Mortar60);
  const EffectsTunables::BandSet& c =
      m60 ? (cls == TargetClass::Point ? E.mortar60Point : E.mortar60Personnel)
          : (cls == TargetClass::Point ? E.artyPoint : E.artyPersonnel);
  const double k = effScale > 0 ? effScale : 1.0;   /* JS `S.effScale || 1` */
  return EffBandSet{c.rFull * k, c.rHalf * k, c.rSupp * k,
                    c.perRound, c.neutralizePct, c.destroyPct};
}

// port of src/js/10-mission.js:995-1003 (oracle-v1) — sheafMult().
double sheafMult(SheafKind kind, TargetClass cls, bool dispersed) {
  if (kind == SheafKind::None) return 1.0;          /* no mission.sheaf */
  if (cls == TargetClass::Point) {                  /* spreading off a point costs */
    switch (kind) {
      case SheafKind::Converged: return 1.25;
      case SheafKind::Circular:  return 1.0;
      case SheafKind::Open:      return 0.6;
      case SheafKind::Linear:    return 0.6;
      default:                   return 1.0;        /* JS `|| 1` */
    }
  }
  if (dispersed) {                                  /* spread men want spread bursts */
    switch (kind) {
      case SheafKind::Open:      return 1.2;
      case SheafKind::Circular:  return 1.0;
      case SheafKind::Linear:    return 0.9;
      case SheafKind::Converged: return 0.6;
      default:                   return 1.0;
    }
  }
  switch (kind) {
    case SheafKind::Converged: return 1.1;
    case SheafKind::Circular:  return 1.0;
    case SheafKind::Open:      return 0.9;
    case SheafKind::Linear:    return 0.8;
    default:                   return 1.0;
  }
}

// port of src/js/10-mission.js:1027-1035 (oracle-v1) — fuzeMult().
double fuzeMult(FuzeKind kind, TargetClass cls, Posture posture) {
  if (kind == FuzeKind::None) return 1.0;           /* no mission.fuze */
  if (cls == TargetClass::Point) {                  /* airburst does nothing to a roof */
    switch (kind) {
      case FuzeKind::Delay: return 1.3;
      case FuzeKind::Pd:    return 1.0;
      case FuzeKind::Vt:    return 0.35;
      case FuzeKind::Time:  return 0.35;
      default:              return 1.0;
    }
  }
  if (posture == Posture::Prone) {                  /* airburst defeats going flat */
    switch (kind) {
      case FuzeKind::Vt:    return 1.5;
      case FuzeKind::Time:  return 1.3;
      case FuzeKind::Pd:    return 1.0;
      case FuzeKind::Delay: return 0.4;
      default:              return 1.0;
    }
  }
  switch (kind) {
    case FuzeKind::Vt:    return 1.15;
    case FuzeKind::Time:  return 1.05;
    case FuzeKind::Pd:    return 1.0;
    case FuzeKind::Delay: return 0.45;
    default:              return 1.0;
  }
}

// CONFIG.EFFECTS.posture[S.posture] || 1 (10-mission.js:961-962,
// 00-config.js:118 — standing 1.0, prone 0.4).
double postureMult(Posture p, const EffectsTunables& E) {
  return p == Posture::Prone ? E.postureProne : E.postureStanding;
}

} }
