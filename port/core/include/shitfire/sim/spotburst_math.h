#pragma once

/* MIL RELATION / OT FACTOR spotting math — port of src/js/10-mission.js:34-56
   (oracle-v1). Pure position arithmetic: the observer measures burst
   deviation in mils off the reticle and converts to metres with the OT
   factor (OT distance / 1000, whole number per doctrine); range error is
   spotted over/short. Gated Level-1 by the spotburst/corrections fixtures. */
namespace shitfire { namespace sim {

/* port of src/js/10-mission.js:39 (oracle-v1) —
   Math.max(1, Math.round(range / 1000)); JS Math.round = js::round. */
double otFactor(double range);

/* port of src/js/10-mission.js:53-54 (oracle-v1) — the doctrinal deviation
   correction pulled out of spotBurst so the corrections gate exercises the
   exact shipped expression: -Math.round(mils * otf / 10) * 10, opposite the
   spotting, then zeroed under the 30 m HE minimum. */
double deviationCorrection(double mils, double otf);

struct SpotResult {
  double mils;       /* + = burst right of target */
  double otf;        /* OT factor (whole number, min 1) */
  double otRange;    /* OP -> enemy distance, m */
  double overShort;  /* + = over (OP->burst dist minus OT range) */
  double devCorr;    /* signed metres, nearest 10, 0 under the 30 m minimum */
};

/* port of src/js/10-mission.js:40-56 (oracle-v1) — spotBurst(impact),
   with the OP and S.enemy globals taken as parameters. */
SpotResult spotBurst(double opX, double opZ,
                     double enemyX, double enemyZ,
                     double impactX, double impactZ);

} }
