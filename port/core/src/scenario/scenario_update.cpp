#include "shitfire/scenario/scenario_state.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/noise.h"
#include "shitfire/terrain/heightfield.h"
#include <algorithm>
#include <cmath>

/* port of the STATE-MUTATING slice of updateScenario (src/js/05-units-scenario.js:815,
   oracle-v1) and updateCivilians (04-world.js:1135). Mesh-driving branches
   (assault squad lerp, callin figures, flames, kp posts) are engine-side.

   SEAMS left for wave 2 / the MISSION porter, called out inline:
   - battery: enemyVolley (05:1028) draws gauss() pairs from S.rng and books
     btyCas via the scheduler — mission-owned. Here only bty.next advances.
   - qfp H-hour install (05:930-937) draws 3 values per visible figure from
     S.rng — the troop meshes are engine-side, but the DRAWS must be taken
     when wave 2 crosses H-hour or every later S.rng consumer shifts.
   - suppression inputs (S.suppressedUntil, S.eff, enemyAlive) are mission
     state; at generation time they hold their initial values, which is all
     the level-2 fixtures exercise. enemyAlive is true at generation. */
namespace shitfire { namespace scenario {

void updateScenarioTick(ScenarioState& S, double simNow, double scenarioT0,
                        const GenContext& ctx) {
  const bool enemyAlive = true;   /* mission-owned; true at generation */
  if (S.type == "convoy") {   /* 05:818 */
    const double raw = *S.speed * (simNow - *S.t0);
    double dHead;
    if (raw < S.stop->d) {
      dHead = raw;
    } else {
      if (!S.stop->tArr) S.stop->tArr = simNow;   /* log() is UI-side */
      const double halted = simNow - *S.stop->tArr;
      if (halted < S.stop->dur) {
        dHead = S.stop->d;
      } else {
        if (!S.stop->resumed) S.stop->resumed = true;
        dHead = S.stop->d + *S.speed * (halted - S.stop->dur);
      }
    }
    dHead = clamp(dHead, 0, S.path->len);
    double cxSum = 0, czSum = 0;
    int alive = 0;
    for (auto& v : *S.veh) {
      if (!v.dead) {
        const double d = clamp(dHead - v.off, 0, S.path->len);
        const WorldXZ q = pathPoint(*S.path, d);
        v.x = q.x;
        v.z = q.z;
        v.d = d;
        cxSum += v.x;
        czSum += v.z;
        alive++;
      }
      /* vehicle mesh + flame positioning skipped (engine-side) */
    }
    if (alive) {
      S.enemy.x = cxSum / alive;
      S.enemy.z = czSum / alive;
    }
    if (dHead >= S.path->len && !*S.escaped) {
      S.escaped = true;   /* mission-fail bookkeeping is mission-owned */
    }
  } else if (S.type == "kaiju") {   /* 05:867 */
    if (enemyAlive) {
      const double d = clamp(*S.speed * (simNow - *S.t0), 0, S.path->len);
      const WorldXZ q = pathPoint(*S.path, d);
      S.enemy.x = q.x;
      S.enemy.z = q.z;
      if (d >= S.path->len && !*S.ashore) S.ashore = true;
    }
  } else if (S.type == "battery" && S.bty) {   /* 05:891 */
    const double tS = simNow - scenarioT0;
    if (enemyAlive && tS >= S.bty->next) {
      if (simNow < S.suppressedUntil) {
        S.bty->next = (S.suppressedUntil - scenarioT0) + 6;
      } else {
        S.bty->next = tS + S.bty->period;
        /* SEAM: enemyVolley(S) — gauss draws from S.rng + scheduled impacts;
           mission module (wave 2). Unreachable at generation (next >= 12 s). */
      }
    }
  } else if (S.type == "qfp" && !*S.overrun) {   /* 05:920 */
    if (!*S.hStarted) {
      if (simNow - scenarioT0 >= *S.hHour) {
        S.hStarted = true;
        /* SEAM: the H-hour install (05:930-937) draws
           a = S.rng(), r = S.rng(), rotY = S.rng() for each of the 8 troop
           figures — wave 2 must take those draws here. */
      }
    } else {
      const double dt2 =
          !S.lastT ? 0 : std::min(0.05, simNow - *S.lastT);   /* ENEMY3 clamp */
      S.lastT = simNow;
      const WorldXZ fb{ S.fireBase->x, S.fireBase->z };
      const double dFB = dist2(S.enemy.x, S.enemy.z, fb.x, fb.z);
      if (enemyAlive && simNow >= S.suppressedUntil && dt2 > 0) {
        const double pace = *S.aSpeed * std::max(0.25, 1 - std::min(S.eff, 1.0));
        const double step = std::min(pace * dt2, std::max(dFB - 60, 0.0));
        if (dFB > 60) {
          S.enemy.x += (fb.x - S.enemy.x) / dFB * step;
          S.enemy.z += (fb.z - S.enemy.z) / dFB * step;
        }
      }
      if (dFB <= 90 && !*S.overrun) S.overrun = true;
    }
  } else if (S.type == "defense" && !*S.overrun) {   /* 05:984 */
    const double dt2 = !S.lastT ? 0 : std::min(0.05, simNow - *S.lastT);
    S.lastT = simNow;
    const double dOP = dist2(S.enemy.x, S.enemy.z, ctx.op.x, ctx.op.z);
    if (enemyAlive && simNow >= S.suppressedUntil && dt2 > 0) {
      const double pace = *S.aSpeed * std::max(0.25, 1 - std::min(S.eff, 1.0));
      const double step = std::min(pace * dt2, std::max(dOP - 60, 0.0));
      if (dOP > 60) {
        S.enemy.x += (ctx.op.x - S.enemy.x) / dOP * step;
        S.enemy.z += (ctx.op.z - S.enemy.z) / dOP * step;
      }
    }
    if (dOP <= 90 && !*S.overrun) S.overrun = true;
  } else if (S.type == "callin" && S.caller) {   /* 05:916 */
    /* caller reactions latch off enemyAlive / S.everSuppressed — both at
       their initial values at generation; voice lines are UI-side. */
    if (!S.caller->saidDead && !enemyAlive) S.caller->saidDead = true;
    else if (!S.caller->saidThanks && S.everSuppressed)
      S.caller->saidThanks = true;
  }
  /* assault (05:906): squad mesh only — no scenario state mutates. */
}

/* 04-world.js:1135 — updateCivilians, position math only. t = sim.now. */
void updateCivilians(std::vector<Civ>& civs, double t) {
  for (auto& c : civs) {
    c.x = c.bx + std::sin(t * 0.14 + c.phase) * c.wr;
    c.z = c.bz + std::cos(t * 0.11 + c.phase * 1.7) * c.wr;
  }
}

} }
