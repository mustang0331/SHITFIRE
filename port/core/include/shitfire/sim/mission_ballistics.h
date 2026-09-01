#pragma once
#include "shitfire/math/prng.h"
#include "shitfire/sim/tunables.h"

/* Ballistics of the fire mission — port of src/js/10-mission.js:25-30 and
   :58-114 (oracle-v1). PRNG prime directive applies throughout: gauss's
   while-zero rejection loops and the draw-before-gate order in both error
   functions are call-order sacred (G3 comment, 10-mission.js:83-87). */
namespace shitfire { namespace sim {

/* activeChapter.asset: null | 'mortar60' | 'sunlamp' (10-mission.js:60-63).
   Battery155 is the null/default case. */
enum class AssetKind { Battery155, Mortar60, Sunlamp };

/* port of src/js/10-mission.js:25-30 (oracle-v1) — Box-Muller off the given
   stream. Draw order sacred, INCLUDING the while-zero rejection loops. */
double gauss(Mulberry32& rng);

/* port of src/js/10-mission.js:60-63 (oracle-v1) —
   mortar60 -> 0.55, sunlamp -> 0.22, else 1. */
double assetScale(AssetKind a);

/* port of src/js/10-mission.js:79-82 (oracle-v1) — per-asset danger close;
   mortar60 keys 'mortar60', everything else 'battery'. */
double dangerCloseRadius(AssetKind a, const MissionTunables& m);

struct ErrXZ { double x = 0, z = 0; };

/* port of src/js/10-mission.js:88-93 (oracle-v1) — first-round error.
   G3: BOTH draws are spent BEFORE the dispersion gate can bail out, so the
   stream position never depends on the test-aid switch. */
ErrXZ firstRoundError(Mulberry32& rng, Difficulty d, AssetKind a,
                      const BallisticsTunables& B);

/* port of src/js/10-mission.js:94-102 (oracle-v1) — follow-up dispersion in
   the OT frame (along = range error, across = deflection), same
   draw-before-gate rule (two gauss() calls always). */
ErrXZ followUpError(Mulberry32& rng, double otAz, Difficulty d, AssetKind a,
                    const BallisticsTunables& B);

/* port of src/js/10-mission.js:110-114 (oracle-v1) — time of flight from the
   battery to the aimpoint, clamped. */
double tofFor(double batteryX, double batteryZ, double aimX, double aimZ,
              const BallisticsTunables& B);

/* Difficulty band selectors (data lookups over tunables; the JS indexes
   CONFIG.BALLISTICS.firstRound[DIFFICULTY] / .followUp[DIFFICULTY]). */
BallisticsTunables::Band firstRoundBand(Difficulty d, const BallisticsTunables& B);
BallisticsTunables::FollowUp followUpBand(Difficulty d, const BallisticsTunables& B);

} }
