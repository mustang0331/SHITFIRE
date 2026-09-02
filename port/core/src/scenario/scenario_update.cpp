#include "shitfire/scenario/scenario_state.h"
#include "shitfire/math/mils.h"
#include "shitfire/math/noise.h"
#include "shitfire/terrain/heightfield.h"
#include <algorithm>
#include <cmath>

/* port of the STATE-MUTATING slice of updateScenario (src/js/05-units-scenario.js:815,
   oracle-v1) and updateCivilians (04-world.js:1135). Mesh-driving branches
   (assault squad lerp, callin figures, flames, kp posts) are engine-side.

   wave-3 reconciliation: the wave-2 seams below are closed through the
   optional ScenarioTickIO (scenario_state.h) — live enemyAlive comes IN,
   latch/branch crossings go OUT, and the qfp H-hour S.rng draws are taken
   here at the exact source position (05:930-937). A null io preserves the
   committed generation-time behavior byte-for-byte (level-2 tests pass
   null): enemyAlive true, no draws, no flags.
   - battery: enemyVolley (05:1028) draws gauss() pairs from S.rng and books
     btyCas via the scheduler — mission-side; the caller acts on
     io->volleyFired. Here only bty.next advances.
   - suppression inputs (S.suppressedUntil, S.eff) are live scenario fields
     either way; enemyAlive is the mission-owned global (io->enemyAlive). */
namespace shitfire { namespace scenario {

void updateScenarioTick(ScenarioState& S, double simNow, double scenarioT0,
                        const GenContext& ctx, ScenarioTickIO* io) {
  const bool enemyAlive = io ? io->enemyAlive : true;   /* mission-owned */
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
      if (io) io->escapedNow = true;   /* wave-3: caller runs 05:858-865 */
    }
  } else if (S.type == "kaiju") {   /* 05:867 */
    if (enemyAlive) {
      const double d = clamp(*S.speed * (simNow - *S.t0), 0, S.path->len);
      const WorldXZ q = pathPoint(*S.path, d);
      S.enemy.x = q.x;
      S.enemy.z = q.z;
      if (d >= S.path->len && !*S.ashore) {
        S.ashore = true;
        if (io) io->ashoreNow = true;   /* wave-3: caller runs 05:883-889 */
      }
    }
  } else if (S.type == "battery" && S.bty) {   /* 05:891 */
    const double tS = simNow - scenarioT0;
    if (enemyAlive && tS >= S.bty->next) {
      if (simNow < S.suppressedUntil) {
        S.bty->next = (S.suppressedUntil - scenarioT0) + 6;
      } else {
        S.bty->next = tS + S.bty->period;
        /* wave-3: enemyVolley(S) (05:903) — gauss draws from S.rng +
           scheduled impacts stay mission-side; the caller fires them off
           this flag. Unreachable at generation (next >= 12 s). */
        if (io) io->volleyFired = true;
      }
    }
  } else if (S.type == "qfp" && !*S.overrun) {   /* 05:920 */
    if (!*S.hStarted) {
      if (simNow - scenarioT0 >= *S.hHour) {
        S.hStarted = true;
        /* wave-3 reconciliation: the H-hour install draws (05:930-937) —
           a = S.rng(), r = S.rng(), rotY = S.rng() per troop figure, all 8
           (units.troops.length is 8; every figure installs). The mesh
           positions are engine-side; the DRAWS happen here so every later
           S.rng consumer keeps its stream position. Null io = committed
           generation-time behavior (H-hour is unreachable there). */
        if (io) {
          for (int i = 0; i < 8; i++) {
            (void)S.rng.next();   /* a    (05:932) */
            (void)S.rng.next();   /* r    (05:932) */
            (void)S.rng.next();   /* rotY (05:936) */
          }
          io->hHourStarted = true;   /* caller logs 05:938-940 */
        }
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
      if (dFB <= 90 && !*S.overrun) {
        S.overrun = true;
        if (io) io->overrunNow = true;   /* wave-3: caller runs 05:973-982 */
      }
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
    if (dOP <= 90 && !*S.overrun) {
      S.overrun = true;
      if (io) io->overrunNow = true;   /* wave-3: caller runs 05:1014-1018 */
    }
  } else if (S.type == "callin" && S.caller) {   /* 05:916 */
    /* caller reactions latch off enemyAlive / S.everSuppressed; the voice
       lines (callerSay 05:918-919) are the caller's — flagged via io. */
    if (!S.caller->saidDead && !enemyAlive) {
      S.caller->saidDead = true;
      if (io) io->callerSaidDead = true;
    } else if (!S.caller->saidThanks && S.everSuppressed) {
      S.caller->saidThanks = true;
      if (io) io->callerSaidSup = true;
    }
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
