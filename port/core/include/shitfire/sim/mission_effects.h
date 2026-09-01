#pragma once
#include "shitfire/sim/mission_ballistics.h"
#include "shitfire/sim/mission_scenario_view.h"
#include "shitfire/sim/tunables.h"

/* The graded effect engine's pure lookups — port of src/js/10-mission.js
   :926-934 (effBands) and :995-1003 / :1027-1035 (sheaf and fuze
   multipliers) (oracle-v1). The accumulation itself (addRoundEffect) and the
   verdict (assessEffect) live on the mission machine, which owns the
   mission/scenario context they read. */
namespace shitfire { namespace sim {

/* mission.sheaf.kind: 'converged'|'circular'|'open'|'linear' — 'parallel'
   and 'special' are canonicalised to linear before storage
   (10-mission.js:983, :1083). None = no sheaf on the mission (mult 1). */
enum class SheafKind { None, Converged, Circular, Open, Linear };

/* mission.fuze.kind: 'pd'|'vt'|'time'|'delay'. None = no fuze set (mult 1). */
enum class FuzeKind { None, Pd, Vt, Time, Delay };

/* FM 6-30 §4-14 outcome ladder (10-mission.js:1102-1104). */
enum class EffectOutcome { None, Suppressed, Neutralized, Destroyed };

struct EffBandSet {
  double rFull, rHalf, rSupp, perRound, neutralizePct, destroyPct;
};

/* port of src/js/10-mission.js:926-934 (oracle-v1) — effBands(). The firing
   asset picks the band set (G18: mortar60 vs arty — sunlamp uses arty's,
   matching the JS `=== 'mortar60' ? 'mortar60' : 'arty'`), the target class
   the row, and S.effScale (JS falsy -> 1; here effScale <= 0 means unset)
   scales the three radii only. */
EffBandSet effBands(AssetKind asset, TargetClass cls, double effScale,
                    const EffectsTunables& E);

/* port of src/js/10-mission.js:995-1003 (oracle-v1) — sheafMult(). Reads the
   mission's sheaf kind plus S.tgtClass / S.dispersed. */
double sheafMult(SheafKind kind, TargetClass cls, bool dispersed);

/* port of src/js/10-mission.js:1027-1035 (oracle-v1) — fuzeMult(). Reads the
   mission's fuze kind plus S.tgtClass / S.posture. */
double fuzeMult(FuzeKind kind, TargetClass cls, Posture posture);

/* CONFIG.EFFECTS.posture[S.posture] || 1 (10-mission.js:961-962). */
double postureMult(Posture p, const EffectsTunables& E);

} }
