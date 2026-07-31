/* ============================================================ MISSION STATE MACHINE */
let mission = null;
let state = 'OBSERVING';
let stateSince = 0, stateHinted = false;
const statusEl = document.getElementById('status');
function setState(s) {
  if (s !== state) { stateSince = sim.now; stateHinted = false; }
  state = s;
  refreshStatus();
}
// Status strip. Called on every state change AND on every optics change, so the
// vision mode is always visible in the HUD without setState being the only path.
function refreshStatus() {
  const rd = mission ? mission.rounds.length : 0;
  statusEl.textContent = `STATE ${state}  |  RDS ${rd}  |  ` +
    `OPTIC ${VISION.mode.toUpperCase()}  |  TOD ${TOD.toUpperCase()}  |  ` +
    (activeChapter ? `CH ${activeChapter.id}` : 'SKIRMISH') +
    `  |  MSN SEED ${CONFIG.SEED.mission}`;
}

function gauss(rng) {
  let u = 0, v = 0;
  while (u === 0) u = rng();
  while (v === 0) v = rng();
  return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
}
// Doctrinal coaching is a training aid: on for Easy/Normal, silent on Hard.
function coachOn() { return DIFFICULTY !== 'hard'; }

// --- MIL RELATION / OT FACTOR ------------------------------------------
// The observer MEASURES deviation with the reticle (mils) and converts to
// meters with the OT factor (OT distance / 1000, whole number per doctrine).
// Range error cannot be measured angularly — it is spotted over/short and
// bracketed. These helpers drive the lase readout, binos HUD, and spotting.
function otFactor(range) { return Math.max(1, Math.round(range / 1000)); }
function spotBurst(impact) {
  const S = Scenario;
  const azT = azTo(OP.x, OP.z, S.enemy.x, S.enemy.z);
  const azB = azTo(OP.x, OP.z, impact.x, impact.z);
  let d = azB - azT;
  while (d > Math.PI) d -= Math.PI * 2;
  while (d < -Math.PI) d += Math.PI * 2;
  const otRange = dist2(OP.x, OP.z, S.enemy.x, S.enemy.z);
  const otf = otFactor(otRange);
  const mils = d * MILS_PER_RAD;                       // + = burst right of target
  const overShort = dist2(OP.x, OP.z, impact.x, impact.z) - otRange;  // + = over
  // doctrinal deviation correction: mils x OT factor, nearest 10 m, opposite
  // the spotting; under 30 m for HE it is not called at all
  let devCorr = -Math.round(mils * otf / 10) * 10;
  if (Math.abs(devCorr) < 30) devCorr = 0;
  return { mils, otf, otRange, overShort, devCorr };
}

// 60mm mortars (Volume IV): much tighter dispersion than the 155 battery.
// 11c — the orbital lance barely disperses at all; it is a beam with a grid.
function assetScale() {
  const a = activeChapter && activeChapter.asset;
  return a === 'mortar60' ? 0.55 : a === 'sunlamp' ? 0.22 : 1;
}
// 11c — the Epilogue finale swaps the delivery prowords, never the doctrine:
// the six elements, the readback, the OT frame and the direct-impact model
// all stand; only what the net CALLS a round in flight changes.
const sunNet = () => activeChapter && activeChapter.asset === 'sunlamp';
const shotWord = () => sunNet() ? 'DISCHARGE, OVER.' : 'SHOT, OVER.';
const splashWord = () => sunNet() ? 'SOLAR EVENT, OVER.' : 'SPLASH, OVER.';
/* F2 — danger close is WEAPON-SPECIFIC, not one number (BALLISTICS_RESEARCH.md
   §6): JFIRE's real tables put a 60mm mortar's danger-close on the order of a
   third to half of the 155's. The old flat 600 m meant the mortar chapters
   demanded the proword at an ARTILLERY distance — backwards from doctrine.
   One helper, used by BOTH the call gate and the unsafe-correction check, so
   the two figures can never drift apart. The flat per-asset gate itself stays
   (the research argues the un-tiered simplification is correct). Inclusive
   <= per DOCTRINE's "within 600 m" (the F1b comparison fix, folded in here
   rather than a second pass over the same lines). */
function dangerCloseRadius() {
  const key = activeChapter && activeChapter.asset === 'mortar60' ? 'mortar60' : 'battery';
  return CONFIG.MISSION.dangerClose[key];
}
/* G3 — both error functions draw from the stream FIRST and only then decide
   whether to return the result. Bailing out early would leave the draws unspent,
   and `mission.rng` also feeds the FFE round stagger, so an early return would
   change the timing of the volley as a side effect of a switch that is only
   supposed to affect where rounds land. Draw, discard, return zero. */
function firstRoundError(rng) {
  const [lo, hi] = CONFIG.BALLISTICS.firstRound[DIFFICULTY];
  const mag = lerp(lo, hi, rng()) * assetScale(), az = rng() * Math.PI * 2;
  if (!CONFIG.BALLISTICS.dispersion) return { x: 0, z: 0 };
  return { x: Math.sin(az) * mag, z: -Math.cos(az) * mag };
}
function followUpError(rng, otAz) {
  const s = CONFIG.BALLISTICS.followUp[DIFFICULTY];
  const k = assetScale();
  const along = gauss(rng) * s.range * k, across = gauss(rng) * s.defl * k;
  if (!CONFIG.BALLISTICS.dispersion) return { x: 0, z: 0 };
  const fx = Math.sin(otAz), fz = -Math.cos(otAz);
  const rx = Math.cos(otAz), rz = Math.sin(otAz);
  return { x: fx * along + rx * across, z: fz * along + rz * across };
}
/* Sticky per-mission record. The switch can be thrown mid-mission, so asking
   "is dispersion on?" at the AAR would miss a mission that was flown with it off
   and flicked back on before the last round. Every round that leaves the tube
   while it is off marks the mission, permanently. */
function markDispersion() {
  if (mission && !CONFIG.BALLISTICS.dispersion) mission.noDisp = true;
}
function tofFor(aim) {
  const B = CONFIG.BALLISTICS;
  const range = dist2(BATTERY.x, BATTERY.z, aim.x, aim.z);
  return clamp(range / B.tofDivisor + B.tofBase, B.tofMin, B.tofMax);
}

// Public interface: start a mission at a world-space target location.
function fireMission(targetLocation, warno, meta) {
  noMissionStreak = 0;   // NET4 — a mission opening is the player getting unstuck
  mission = {
    aim: { x: targetLocation.x, z: targetLocation.z },
    // the location as transmitted, kept for the AAR shot plot
    aim0x: targetLocation.x, aim0z: targetLocation.z,
    rounds: [], ffeRounds: [], adjustRounds: 0,
    firstMiss: null, lastAdjDist: null, wrongWay: 0,
    // adjustment-doctrine tracking (feeds the live coach + AAR diagnosis)
    missTrace: [], sides: { long: false, short: false }, bracketed: false,
    coached: {}, wasted: 0, corrs: [],
    notes: (meta && meta.notes) || [],
    desc: (meta && meta.desc) || '', gridStr: (meta && meta.gridStr) || '',
    method: (meta && meta.method) || null,
    // initial target-location error: how far the transmitted location was
    // from truth at send time (JFO standard: within 200 m). Star metric.
    aimErr0: Scenario.enemy
      ? dist2(targetLocation.x, targetLocation.z, Scenario.enemy.x, Scenario.enemy.z) : null,
    // time-to-initiate: JFO standard is a complete CFF within 2 min of
    // target identification (scenario start stands in for target ID)
    tInit: sim.now - scenarioT0,
    tStart: sim.now, tEnd: null, done: false, failReason: null, hits: 0,
    /* G13 — what this mission is FOR. 'destroy' is the default; immediate
       suppression and suppress-target missions set 'suppress', where a
       suppressed-only outcome is the mission accomplished, not a shortfall. */
    intent: (meta && meta.intent) || 'destroy',
    imm: (meta && meta.imm) || null,   // TEMPO2 — 'suppress'/'smoke' when the call was an immediate
    usedIllum: false,   // TEMPO4 — set when an illumination round deploys
    bdaClaim: null,         // G13 — surveillance term the observer sent at EOM
    sheaf: (meta && meta.sheaf) || null,   // G15 — {kind, source, why}
    fuze:  (meta && meta.fuze)  || null,   // G16 — {kind, source, why}
    shell: (meta && meta.shell) || 'he',   // 12h — he | smoke | illum
    noDisp: false,          // G3 — set by markDispersion() if any round is fired
                            // with dispersion off; blocks star recording
    checkFire: false,       // G25 — safety hold active; blocks new firing
    usedCheckFire: false,   // G25 — sticky, for the AAR
    /* G24 — method of fire and control. `amc` persists for the whole mission once
       requested and until CANCEL AT MY COMMAND, per DOCTRINE.md §34; it is not a
       one-shot. `pendingFire` holds the firing call that is waiting on the
       observer's FIRE, and `released` is the one-shot latch that lets the retried
       call through without immediately re-arming the hold. */
    amc: !!(meta && meta.amc), pendingFire: null, released: false,
    mto: (meta && meta.mto) || null,   // G11 — MTO readback expectation, or null (immediate missions)
    cannotObserve: false,   // §34 — observer has declared he cannot spot
    tot: null,              // §34 — time on target, accepted and acknowledged
    rng: mulberry32((CONFIG.SEED.mission * 1046527 + 7) >>> 0),
    shotExtra: (meta && meta.shotExtra) || 0,   // SUGG2 — re-lay delay before SHOT
    fpf: !!(meta && meta.fpf),                  // SUGG1 — this FFE is the planned line
    series: (meta && meta.series) || null,      // SUGG4 — ordered recorded targets
  };
  if (warno === 'ffe') fireForEffect();
  else fireAdjustRound();
}

/* ---- G11: the observer reads the MTO back (DOCTRINE.md §42) -----------------
   "The observer reads back the entire MTO." User's spec, verbatim: "word for
   word but if it gets the gist gtg." So the scorer is a GIST scorer, not a
   string comparison — it counts how many of the MTO's key elements appear in
   the transmission, with the TARGET NUMBER weighted as the one element that is
   not negotiable: reading back the wrong target number is precisely the error
   the readback exists to catch, so a readback with a wrong or missing number is
   challenged even when everything else matches.

   Threshold reasoning: 3+ keys means this is unmistakably a readback attempt
   and not a stray sentence — no plausible correction, description or question
   contains three of ["gun","battery","adjust","effect","155","he","fuze","pd"].
   Below 3 the transmission is NOT treated as a readback at all and falls
   through to the normal handlers, so nothing that used to work is captured. */
function scoreMTO(t, mto) {
  let hits = 0;
  for (const k of mto.keys) if (t.includes(k)) hits++;
  return { hits, tgtOk: t.includes(mto.tgt) };
}
function tryMTOReadback(p) {
  if (!mission || mission.done || !mission.mto || mission.mto.read) return false;
  const s = scoreMTO(p.raw || '', mission.mto);
  if (s.hits < 3 && !s.tgtOk) return false;          // not a readback attempt
  if (!s.tgtOk) {
    // right recitation, wrong (or no) target number — the one failure that matters
    FDC.say(`NEGATIVE, MUSTANG — the target number is ALPHA ALPHA ${mission.mto.tgt}. ` +
            `Read the whole message back, over.`, { delay: 1.0 });
    mission.notes.push(`MTO readback ${p.raw.match(/\b\d{4}\b/) ? 'carried the wrong target number' : 'omitted the target number'}.`);
    return true;
  }
  if (s.hits < 3) {
    // target number alone is not a readback of "the entire MTO"
    FDC.say(`That is the target number, not the message. Read back the whole MTO, over.`,
            { delay: 1.0 });
    return true;
  }
  mission.mto.read = true;
  const verbatim = s.hits >= mission.mto.keys.length - 1;
  FDC.say(verbatim ? 'READBACK CORRECT, OUT.'
                   : 'CLOSE ENOUGH, MUSTANG. READBACK CORRECT, OUT.', { delay: 0.9 });
  if (!verbatim && activeChapter && activeChapter.strict)
    mission.notes.push('MTO readback was gist, not verbatim — strict net wants it word for word.');
  return true;
}

/* ---- G9: POSITION REPORT, and why polar needs it -----------------------------
   User feedback: "Need to add a pos rep at the beginning if doing a polar
   mission." Doctrine agrees (DOCTRINE.md §27: "FDC must know observer's
   position") — a polar call is a direction and a distance FROM THE OBSERVER, so
   without knowing where the observer is, the FDC has one end of the vector and
   nothing to hang it on.

   The teaching decision that matters: the FDC resolves the polar mission from
   the REPORTED position, not from the observer's true position. It cannot do
   otherwise — it only knows what it was told. So an observer who mis-locates
   himself by 200 m puts every polar round 200 m off, and the trainer does not
   soften that: self-location by resection is the skill, and this is where it
   gets graded by the fall of shot. A 6-digit self-location carries up to ~70 m
   of grid-square rounding, which is real too (JFO standard: self-locate within
   100 m).

   Stored on Scenario (rebuilt fresh every mission, so it expires naturally and
   the "at the beginning" requirement is per-mission, as the user asked). */
function handlePosRep(p) {
  let px, pz;
  if (p.digits.length === 6) {
    const e3 = parseInt(p.digits.slice(0, 3)), n3 = parseInt(p.digits.slice(3));
    const w = enToWorld(e3 * 100 + 50, n3 * 100 + 50); px = w.x; pz = w.z;
  } else if (p.digits.length === 8) {
    const e4 = parseInt(p.digits.slice(0, 4)), n4 = parseInt(p.digits.slice(4));
    const w = enToWorld(e4 * 10 + 5, n4 * 10 + 5); px = w.x; pz = w.z;
  } else {
    FDC.say('MUSTANG 12, send your position as a 6 or 8 digit grid, over.', { delay: 0.9 });
    return;
  }
  Scenario.posRep = { x: px, z: pz, digits: p.digits };
  const err = dist2(px, pz, OP.x, OP.z);
  FDC.say(`POSITION GRID ${p.digits.length === 6
    ? p.digits.slice(0, 3) + ' ' + p.digits.slice(3) : p.digits}, OUT.`, { delay: 0.9 });
  /* No hint when the reported position is wrong — the FDC cannot know, and
     telling the observer would defeat the exercise. TLOG carries the truth so
     the AAR conversation can happen on paper. */
  TLOG.add('sys', '', `posrep ${p.digits}`, { selfLocErr: Math.round(err) });
}

/* G27 — recorded targets, by number. Survives across missions within a session
   (that is what "on file" means) but is not persisted, because a target number is
   only meaningful to the fire unit that assigned it. */
const RECTGT = {};
/* SUGG2 — the priority target (ATP 3-09.30 §1-29): AT MOST ONE recorded
   target the firing unit stays laid on. "FIRE TARGET <n>" on the priority
   target gets steel with almost no delay; on any other filed target the
   guns re-lay first and the observer feels the difference — that reaction
   gap is the trainable skill. Designating a new priority moves it (doctrine:
   priority shifts as the situation does). */
let PRIORITY = null;
function handlePriorityTarget(p) {
  const tgt = RECTGT[p.tgtNum];
  if (!tgt) {
    const known = Object.keys(RECTGT);
    FDC.say(`NEGATIVE — no target ${p.tgtNum} on file. ` +
      (known.length ? `I hold ${known.join(', ')}. ` : 'Record one at end of mission first. ') +
      'Over.', { delay: 1 });
    return;
  }
  const moved = PRIORITY && PRIORITY !== p.tgtNum;
  PRIORITY = p.tgtNum;
  FDC.say(`TARGET ${p.tgtNum} IS PRIORITY${moved ? ' — SHIFTING OFF THE OLD ONE' : ''}. ` +
          'GUNS ARE LAID AND STANDING BY, OUT.', { delay: 1 });
  log('', `Priority target set: ${p.tgtNum}. Re-engage it with "FIRE TARGET ${p.tgtNum}, OVER" ` +
    'and the rounds come fast — any other filed target waits for the re-lay.', 'sys');
}
function handleFireTarget(p) {
  if (mission && !mission.done) {
    FDC.say('MUSTANG 12, we are mid-mission. Finish this one, over.', { delay: 1 });
    return;
  }
  const tgt = RECTGT[p.tgtNum];
  if (!tgt) {
    const known = Object.keys(RECTGT);
    FDC.say(`NEGATIVE — no target ${p.tgtNum} on file. ` +
      (known.length ? `I hold ${known.join(', ')}. ` : 'Nothing is recorded on this net yet. ') +
      'Say again, over.', { delay: 1 });
    return;
  }
  const pri = PRIORITY === p.tgtNum;
  if (!pri)
    FDC.say(`STAND BY — RE-LAYING ONTO TARGET ${p.tgtNum}. That is what priority would have bought you, over.`, { delay: 0.8 });
  FDC.say(`FIRE TARGET ${p.tgtNum}, OUT.`, { delay: pri ? 0.6 : 1.4 });
  setState('MISSION SENT');
  fireMission({ x: tgt.x, z: tgt.z }, 'ffe',
    { desc: 'RECORDED TARGET', gridStr: `TARGET ${p.tgtNum}`, method: 'grid',
      intent: 'destroy', sheaf: inferSheaf('', p.raw), fuze: inferFuze('', p.raw),
      /* the felt difference: laid guns shoot now; anyone else re-lays first */
      shotExtra: pri ? 0 : 22 });
}
/* SUGG1 — the final protective fire (ATP 3-09.30 §1-30–1-32, §7-16–7-23).
   A PRE-PLANNED defensive line as close to friendlies as the observer dares:
   plan it quiet, adjust each piece individually ("NUMBER 1, RIGHT 20, DROP
   25" → "NUMBER 2, REPEAT"), then "FIRE THE FPF" brings the whole line down
   at max rate when the assault crosses it. Everything is the direct-impact
   model: four fixed gun points on a 180 m line, each round = its gun's point
   + error. Marking rounds during adjustment are single presentation rounds
   (spawnBurst — no mission machinery), but the fratricide invariant is NOT
   waived for them: a marking round inside a friendly radius is recorded and
   fails the FPF mission the moment it opens. The executed FPF itself fires
   through fireMission/resolveImpact, so effects, fratricide and collateral
   all run under the ordinary rules. */
let FPF = null;
function fpfMarkRound(g) {
  const p0 = FPF.pts[g];
  const err = followUpError(FPF.rng, azTo(OP.x, OP.z, p0.x, p0.z));
  const ix = p0.x + err.x, iz = p0.z + err.z;
  schedule(sim.now + CONFIG.FDC.shotDelay + tofFor(p0) - 2, () => {
    spawnBurst(ix, Math.max(H(ix, iz), 0), iz);
    for (const f of friendlyPositions())
      if (dist2(ix, iz, f.x, f.z) < (f.r || CONFIG.MISSION.fratricideRadius)) {
        FPF.frat = true;
        log('', 'That marking round landed INSIDE the friendly position. The FPF is compromised — the failure is recorded and rides the mission.', 'sys');
        // not the rant pool: those entries feed failHit's multi-part playback
        FDC.say('CHECK FIRE — THAT MARKING ROUND WAS ON THE FRIENDLIES. The FPF you just adjusted onto our own people is the FPF you will answer for when it fires, over.', { delay: 0.8 });
      }
  });
}
function handlePlanFPF(p) {
  if (mission && !mission.done) {
    FDC.say('MUSTANG 12, we are mid-mission. Plan your FPF when the net is quiet, over.', { delay: 1 });
    return;
  }
  if (!p.digits || (p.digits.length !== 6 && p.digits.length !== 8)) {
    FDC.say('SAY AGAIN THE FPF GRID — six or eight digits, over.', { delay: 1 });
    return;
  }
  if (p.dirMils === null) {
    FDC.say('AN FPF HAS AN ATTITUDE — SEND DIRECTION WITH IT ("PLAN FPF, GRID, DIRECTION"), OVER.', { delay: 1 });
    return;
  }
  let w;
  if (p.digits.length === 6) {
    w = enToWorld(parseInt(p.digits.slice(0, 3), 10) * 100 + 50,
                  parseInt(p.digits.slice(3), 10) * 100 + 50);
  } else {
    w = enToWorld(parseInt(p.digits.slice(0, 4), 10) * 10 + 5,
                  parseInt(p.digits.slice(4), 10) * 10 + 5);
  }
  // an FPF exists to be danger close — the proword discipline holds while planning
  let minF = Infinity;
  for (const f of friendlyPositions()) minF = Math.min(minF, dist2(w.x, w.z, f.x, f.z));
  if (minF <= dangerCloseRadius() && !saidDangerClose(p.raw)) {
    FDC.say(pick(QUIPS.dangerClose), { delay: 1.2 });
    return;
  }
  const az = (p.dirMils % 6400) / MILS_PER_RAD;
  const lx = Math.sin(az), lz = -Math.cos(az);        // along the attitude
  FPF = { pts: [-60, -20, 20, 60].map(o => ({ x: w.x + lx * o, z: w.z + lz * o })),
          az, adj: [false, false, false, false], frat: false,
          rng: mulberry32((CONFIG.SEED.mission * 2246822519 + 13) >>> 0) };
  FDC.say(`FPF ESTABLISHED — FOUR GUNS ON A 180 METER LINE, ATTITUDE ${fmtMils(p.dirMils % 6400)}, ` +
          `CENTER GRID ${p.digits}${minF <= dangerCloseRadius() ? ', DANGER CLOSE' : ''}. ` +
          'ADJUST EACH PIECE — NUMBER 1 IS UP, OVER.', { delay: 1.2 });
  log('', 'Adjust each gun onto the line: "NUMBER 1, RIGHT 20, DROP 25" moves it (a marking round follows); ' +
    '"NUMBER 2, REPEAT" re-fires a piece unmoved. When the assault crosses the line: "FIRE THE FPF".', 'sys');
  fpfMarkRound(0);
}
function handleFPFAdjust(p) {
  if (!FPF) {
    FDC.say('NEGATIVE — no FPF is planned on this net. "PLAN FPF, GRID, DIRECTION" first, over.', { delay: 1 });
    return;
  }
  if (p.gun < 1 || p.gun > 4) {
    FDC.say(`I HAVE FOUR GUNS, MUSTANG, NOT ${p.gun}. NUMBER 1 THROUGH 4, OVER.`, { delay: 1 });
    return;
  }
  const g = p.gun - 1;
  if (!p.repeat) {
    // per-piece OT-frame move — the same frame every correction uses
    const pt = FPF.pts[g];
    const ot = azTo(OP.x, OP.z, pt.x, pt.z);
    const fx = Math.sin(ot), fz = -Math.cos(ot), rx = Math.cos(ot), rz = Math.sin(ot);
    pt.x += fx * p.add + rx * p.right;
    pt.z += fz * p.add + rz * p.right;
    FPF.adj[g] = true;
  }
  FDC.say(`NUMBER ${p.gun}${p.repeat ? ', REPEAT' : (p.right ? `, ${p.right > 0 ? 'RIGHT' : 'LEFT'} ${Math.abs(p.right)}` : '') +
          (p.add ? `, ${p.add > 0 ? 'ADD' : 'DROP'} ${Math.abs(p.add)}` : '')}, OUT.`, { delay: 0.8 });
  fpfMarkRound(g);
}
function handleFireFPF(p) {
  if (!FPF) {
    FDC.say('NEGATIVE — no FPF is planned on this net. Plan it before you need it, over.', { delay: 1 });
    return;
  }
  if (mission && !mission.done) {
    FDC.say('MUSTANG 12, we are mid-mission. End it or let it ride — the FPF fires clean, over.', { delay: 1 });
    return;
  }
  FDC.say('FIRING THE FPF — ALL GUNS, MAXIMUM RATE, OUT.', { delay: 0.5 });
  setState('MISSION SENT');
  fireMission({ x: (FPF.pts[1].x + FPF.pts[2].x) / 2, z: (FPF.pts[1].z + FPF.pts[2].z) / 2 },
    'ffe', { desc: 'FINAL PROTECTIVE FIRE', gridStr: 'THE FPF', method: 'grid',
             intent: 'suppress', fpf: true });
}
/* SUGG4 — series of targets (ATP 3-09.30 §1-42): a named, ordered sequence
   of RECORDED targets, fired one after another — pre-mission target planning
   as a first-class skill instead of reactive adjust-fire. Rides RECTGT the
   same way the priority target does; wiped with it on an island change. */
const SERIES = {};
function handlePlanSeries(p) {
  if (!p.tgts.length) {
    FDC.say('A SERIES IS TARGETS, MUSTANG — "PLAN SERIES MAX, TARGETS AB7101, AB7102", OVER.', { delay: 1 });
    return;
  }
  const missing = p.tgts.filter(n => !RECTGT[n]);
  if (missing.length) {
    const known = Object.keys(RECTGT);
    FDC.say(`NEGATIVE — ${missing.join(', ')} ${missing.length === 1 ? 'is' : 'are'} not on file. ` +
      (known.length ? `I hold ${known.join(', ')}. ` : 'Record targets before you plan with them. ') + 'Over.', { delay: 1 });
    return;
  }
  SERIES[p.name] = p.tgts.slice();
  FDC.say(`SERIES ${p.name} ESTABLISHED — ${p.tgts.length} TARGETS, IN ORDER: ${p.tgts.join(', ')}. ` +
          `CALL FOR IT BY NAME, OUT.`, { delay: 1 });
}
function handleFireSeries(p) {
  const s = SERIES[p.name];
  if (!s) {
    const known = Object.keys(SERIES);
    FDC.say(`NEGATIVE — no series ${p.name} on this net. ` +
      (known.length ? `I hold ${known.join(', ')}. ` : 'Plan one first. ') + 'Over.', { delay: 1 });
    return;
  }
  if (mission && !mission.done) {
    FDC.say('MUSTANG 12, we are mid-mission. Finish this one, over.', { delay: 1 });
    return;
  }
  FDC.say(`FIRING SERIES ${p.name} — ${s.length} TARGETS IN SEQUENCE, OUT.`, { delay: 0.7 });
  setState('MISSION SENT');
  const first = RECTGT[s[0]];
  fireMission({ x: first.x, z: first.z }, 'ffe',
    { desc: `SERIES ${p.name}`, gridStr: `SERIES ${p.name}`, method: 'grid',
      intent: 'destroy', series: s.slice() });
}
function handleSuppressTarget(p) {
  if (mission && !mission.done) {
    FDC.say('MUSTANG 12, we are mid-mission. Finish this one, over.', { delay: 1 });
    return;
  }
  const tgt = RECTGT[p.tgtNum];
  if (!tgt) {
    const known = Object.keys(RECTGT);
    FDC.say(`NEGATIVE — I have no target ${p.tgtNum} on file. ` +
      (known.length ? `I hold ${known.join(', ')}. ` : 'Nothing is recorded on this net yet. ') +
      'Say again, over.', { delay: 1 });
    return;
  }
  /* Doctrine sends duration with a suppress mission. This model has no sustained
     fire, so the duration is read back and graded as format rather than simulated —
     the same honesty applied to sheaf, volume of fire and time on target. */
  const mins = p.minutes;
  FDC.say(`SUPPRESS TARGET ${p.tgtNum}${mins ? ', ' + mins + ' MINUTES' : ''}, OUT.`,
          { delay: 0.8 });
  if (!mins)
    log('', 'A suppress mission carries a duration — "SUPPRESS TARGET ' + p.tgtNum +
      ', 10 MINUTES, OVER". Firing anyway.', 'sys');
  setState('MISSION SENT');
  fireMission({ x: tgt.x, z: tgt.z }, 'ffe',
    { notes: mins ? [] : ['Suppress mission sent without a duration.'],
      desc: 'SUPPRESSION', gridStr: `TARGET ${p.tgtNum}`, method: 'grid',
      intent: 'suppress', sheaf: inferSheaf('', p.raw), fuze: inferFuze('', p.raw) });
}

/* ---- G10: FIRE UNIT STATUS (FM 6-30 §8-12) ----------------------------------
   No land field-artillery or mortar doctrine names a pre-mission status
   exchange at all (DOCTRINE.md §Pre-mission and effects data) — the only
   sourced shape is naval gunfire's FIRE UNIT STATUS, so the trainer borrows
   it: observer-REQUESTED, one FDC reply, no readback, and not repeated —
   round count, unit location, tubes, munitions, closed with the on-station
   report. The in-mission MTO is untouched (the user's explicit constraint).
   Round counts are flavor state, not an ammo model: nothing decrements them,
   the same honesty rule as TOT and suppress durations. The callsign is
   written HELLHOUND and G17's delivery-time swap renames it on a mortar
   chapter; the CONTENT differs per asset here because the unit itself does. */
let unitStatusSent = false;
function handleUnitStatus() {
  if (unitStatusSent) {
    FDC.say('MUSTANG 12, my status has not changed since my last, and it will not change until you send me some work, over.', { delay: 1.0 });
    return;
  }
  unitStatusSent = true;
  const m60 = activeChapter && activeChapter.asset === 'mortar60';
  const gs = gridOf(BATTERY.x, BATTERY.z);
  FDC.say(m60
    ? `MUSTANG 12, HELLHOUND FIRES — FIRE UNIT STATUS: TWO TUBES, 60 MIKE MIKE, GRID ${gs}. HE 96 ROUNDS, WHISKEY PAPA 24, MULTI-OPTION FUZE ON HAND. ON STATION AND READY FOR CALL FOR FIRE, OVER.`
    : `MUSTANG 12, HELLHOUND FIRES — FIRE UNIT STATUS: SIX GUNS, 155, GRID ${gs}. HE 240 ROUNDS, SMOKE 48, ILLUM 36; FUZE PD, VT AND DELAY ON HAND. ON STATION AND READY FOR CALL FOR FIRE, OVER.`,
    { delay: 1.2 });
}

/* ---- G24: AT MY COMMAND (DOCTRINE.md §34) -----------------------------------
   Method of fire and control. The default is "when ready" — the FDC fires as soon
   as it has data. Under AT MY COMMAND it lays the guns, reports READY, and waits
   for the observer to say FIRE. It persists for the whole mission until CANCEL AT
   MY COMMAND; it is not a one-shot, which is the detail most easily got wrong.

   Absorbs the old row 12f, which had been blocked behind the parser work.

   `released` is a one-shot latch rather than a second flag on the call: the held
   firing function is re-invoked verbatim when FIRE arrives, so without the latch
   it would arm the hold again on the way in and never fire. */
function armAtMyCommand(fn) {
  if (!mission || !mission.amc) return false;
  if (mission.released) { mission.released = false; return false; }
  mission.pendingFire = fn;
  setState('AT MY COMMAND');
  FDC.say('READY, OVER.', { delay: 1.0 });
  /* SUGG6 — the intercept drill (ATP 3-09.30 §5-115–5-136), coached exactly
     where doctrine uses it: guns laid AT MY COMMAND on a moving target. The
     coach teaches the METHOD with the real TOF; the speed estimate stays the
     observer's job (the band is what a map-recon would give, not the answer).
     Easy/Normal only, once per mission, like every other coach line. */
  if (Scenario && Scenario.type === 'convoy' && coachOn() && !mission.coached.intercept) {
    mission.coached.intercept = true;
    const tof = Math.round(tofFor(mission.aim));
    const lead = tof + Math.round(CONFIG.FDC.shotDelay);
    log('', `Intercept drill: guns are laid on your point and waiting. Time of flight ~${tof} s, ` +
      `FIRE-to-shot ~${Math.round(CONFIG.FDC.shotDelay)} s — call it ${lead} s from your FIRE to splash. ` +
      `Estimate the column's speed (trucks on this road make roughly 3-4 m/s), multiply by ${lead}, ` +
      `and mark the TRIGGER POINT that far up the road BEFORE your intercept point. ` +
      `The lead vehicle touches it — you say FIRE. Not before, not after.`, 'sys');
  }
  return true;
}
/* SUGG6 — the observer may request time of flight; the intercept sequence
   needs it and doctrine provides it on request. */
function handleTofQuery() {
  if (!mission || mission.done) {
    FDC.say('NO MISSION ON THE NET — time of flight to where, exactly? Over.', { delay: 0.9 });
    return;
  }
  FDC.say(`TIME OF FLIGHT ${Math.round(tofFor(mission.aim))} SECONDS, OVER.`, { delay: 0.9 });
}
function handleFire() {
  if (!mission || mission.done) { noMissionReply(); return; }   // NET4
  if (!mission.pendingFire) {
    FDC.say(mission.amc
      ? 'NOTHING IS LAID AND WAITING, MUSTANG. Send your correction first, over.'
      : 'You are not AT MY COMMAND, MUSTANG — I fire when ready. Nothing is waiting on you, over.',
      { delay: 0.9 });
    return;
  }
  const fn = mission.pendingFire;
  mission.pendingFire = null;
  mission.released = true;
  FDC.say('FIRE, OUT.', { delay: 0.5 });
  fn();
}
function handleAtMyCommand(on) {
  if (!mission || mission.done) {
    // Legal in a call for fire, so say so rather than mocking a correct proword.
    FDC.say(on
      ? 'AT MY COMMAND is method of fire and control — send it with your call for fire, over.'
      : 'No mission running, nothing to cancel, out.', { delay: 0.9 });
    return;
  }
  mission.amc = on;
  FDC.say(on ? 'AT MY COMMAND, OUT.' : 'CANCEL AT MY COMMAND, OUT.', { delay: 0.7 });
  if (!on && mission.pendingFire) {
    // Cancelling reverts to "when ready", so anything already laid goes now —
    // leaving it held would strand the mission on a proword that no longer applies.
    const fn = mission.pendingFire;
    mission.pendingFire = null;
    mission.released = true;
    log('', 'CANCEL AT MY COMMAND — reverting to WHEN READY. The round already laid goes now.', 'sys');
    fn();
  }
}
function handleDoNotLoad() {
  if (!mission || mission.done) { FDC.say('DO NOT LOAD, OUT. Nothing is loaded.', { delay: 0.7 }); return; }
  // Doctrinally distinct from CEASE LOADING but has the same effect on this model:
  // nothing new goes up. Reuses G25's hold rather than inventing a second one.
  mission.checkFire = true;
  mission.usedCheckFire = true;
  setState('DO NOT LOAD');
  FDC.say('DO NOT LOAD, OUT.', { delay: 0.7 });
  log('', 'DO NOT LOAD — the guns stay empty. Send a correction or FIRE FOR EFFECT to resume.', 'sys');
}
function handleCannotObserve() {
  if (!mission || mission.done) { noMissionReply(); return; }   // NET4
  mission.cannotObserve = true;
  mission.notes.push('Declared CANNOT OBSERVE — the FDC stops expecting spotting corrections.');
  FDC.say('CANNOT OBSERVE, OUT. We will fire the mission blind on your last data. ' +
          'Send END OF MISSION when you are done, over.', { delay: 1.0 });
}
function handleTimeOnTarget(sec) {
  if (!mission || mission.done) { noMissionReply(); return; }   // NET4
  mission.tot = sec;
  FDC.say(`TIME ON TARGET ${sec}, OUT.`, { delay: 0.8 });
  /* Honest about the model, in the same spirit as DOCTRINE.md §76's note on sheaf
     and volume of fire: the proword is understood and read back, but this sim has
     no multi-battery synchronisation for it to coordinate, so it does not change
     when rounds land. Recorded so it is gradeable as format. */
  log('', `TIME ON TARGET ${sec} acknowledged. Note: this trainer models a single ` +
    `firing unit, so TOT is accepted and graded as format but does not change ` +
    `impact timing.`, 'sys');
}

// Public interface: OT-frame correction -> world delta on the aimpoint.
function applyCorrection(otFrameDelta) {
  const a = mission.aim;
  const otAz = azTo(OP.x, OP.z, a.x, a.z);
  const fx = Math.sin(otAz), fz = -Math.cos(otAz);   // toward target
  const rx = Math.cos(otAz), rz = Math.sin(otAz);    // observer's right
  a.x += fx * otFrameDelta.add + rx * otFrameDelta.right;
  a.z += fz * otFrameDelta.add + rz * otFrameDelta.right;
  return a;
}

function fireAdjustRound() {
  const B = CONFIG.BALLISTICS;
  /* G25 — the hold is enforced HERE as well as at the handler, on purpose. This is
     the last gate before rounds leave the tube, and a safety stop that depends on
     every future caller remembering to check a flag is not a safety stop. */
  if (mission && mission.checkFire) {
    FDC.say('NEGATIVE — WE ARE CHECK FIRING. CANCEL CHECK FIRING BEFORE I PUT ANYTHING ELSE UP, OVER.',
            { delay: 0.8 });
    return;
  }
  // G24 — lay the gun and wait for FIRE. Before markDispersion and the round
  // counter, so a held round is not counted or logged until it actually goes.
  if (armAtMyCommand(fireAdjustRound)) return;
  markDispersion();
  mission.adjustRounds++;
  const otAz = azTo(OP.x, OP.z, mission.aim.x, mission.aim.z);
  const err = mission.rounds.length === 0
    ? firstRoundError(mission.rng)
    : followUpError(mission.rng, otAz);
  const impact = { x: mission.aim.x + err.x, z: mission.aim.z + err.z };
  const tof = tofFor(mission.aim);
  setState('SHOT');
  const tShot = FDC.say(shotWord(), { delay: CONFIG.FDC.shotDelay + (mission.shotExtra || 0) });
  FDC.say(splashWord(), { delay: tof - B.splashLead });
  if (sunNet()) chargeWhine(tof);   // 11c — TOF is a charging whine from everywhere at once
  schedule(tShot + tof, () => resolveImpact(impact, false));
}

function fireForEffect() {
  const B = CONFIG.BALLISTICS;
  if (mission && mission.checkFire) {          // G25 — same last-gate enforcement
    FDC.say('NEGATIVE — WE ARE CHECK FIRING. CANCEL CHECK FIRING BEFORE I PUT ANYTHING ELSE UP, OVER.',
            { delay: 0.8 });
    return;
  }
  if (armAtMyCommand(fireForEffect)) return;   // G24
  markDispersion();
  setState('FIRE FOR EFFECT');
  if (Math.random() < 0.4) FDC.say(pick(QUIPS.ffeAck), { delay: 0.8 });
  const otAz = azTo(OP.x, OP.z, mission.aim.x, mission.aim.z);
  const tof = tofFor(mission.aim);
  const tShot = FDC.say(shotWord(), { delay: CONFIG.FDC.shotDelay + (mission.shotExtra || 0) });
  FDC.say(splashWord(), { delay: tof - B.splashLead });
  if (sunNet()) chargeWhine(tof);   // 11c
  // FFE with no prior adjustment still carries the full first-round spotting
  // error, applied to the whole volley as a common aim error.
  const base = mission.rounds.length === 0
    ? firstRoundError(mission.rng) : { x: 0, z: 0 };
  /* G15 — a LINEAR sheaf on a convoy spreads the volley's aimpoints along the
     column axis (35 m apart, centered on the aimpoint), so leading the column
     with the right sheaf genuinely catches more of it. This is aimpoint
     geometry, not a trajectory — each round is still impact = aimpoint + error. */
  const lin = mission.sheaf && mission.sheaf.kind === 'linear' &&
              Scenario.type === 'convoy' && Scenario.path;
  /* WORLD3 — on a road polyline the column axis is the road's direction where
     the column currently is, not a constant; line paths return the same
     dx/dz they always did. */
  const linDir = lin ? pathDir(Scenario.path, convoyHeadD()) : null;
  /* SUGG1 — the FPF volley is the four ADJUSTED gun points, each fired twice,
     at maximum rate (a third of the normal stagger). Each round is still
     impact = its gun's point + error — the line is aimpoint geometry, exactly
     like the linear sheaf above. A marking-round fratricide recorded during
     adjustment fails this mission on execution: the round that landed on
     friendlies was this mission's round, fired early. */
  const fpfRun = mission.fpf && FPF;
  if (fpfRun && FPF.frat) mission.failReason = 'fratricide';
  /* SUGG4 — a series volley is 4 rounds per RECORDED target, targets taken in
     their planned ORDER with a shift gap between them (the guns re-lay from
     one filed point to the next). Recorded points carry no transmit error —
     that is what recording bought — so rounds are point + follow-up error,
     the same rule the FPF's adjusted line uses. */
  const serRun = mission.series && mission.series.every(n => RECTGT[n]) ? mission.series : null;
  const nRounds = fpfRun ? 8 : serRun ? serRun.length * 4 : B.ffeRounds;
  let off = 0, tLast = 0;
  for (let i = 0; i < nRounds; i++) {
    const err = followUpError(mission.rng, otAz);
    const lo = lin ? (i - (B.ffeRounds - 1) / 2) * 35 : 0;
    const gun = fpfRun ? FPF.pts[i % 4] : null;
    const ser = serRun ? RECTGT[serRun[(i / 4) | 0]] : null;
    const impact = fpfRun
      ? { x: gun.x + err.x, z: gun.z + err.z }
      : ser
      ? { x: ser.x + err.x, z: ser.z + err.z }
      : { x: mission.aim.x + base.x + err.x + (lin ? linDir.dx * lo : 0),
          z: mission.aim.z + base.z + err.z + (lin ? linDir.dz * lo : 0) };
    tLast = tShot + tof + off;
    schedule(tLast, () => resolveImpact(impact, true));
    // series: a re-lay pause between targets, max rate within one
    const gap = serRun && i % 4 === 3 ? 9 : 0;
    off += lerp(B.ffeStagger[0], B.ffeStagger[1], mission.rng()) * (fpfRun ? 0.33 : 1) + gap;
  }
  schedule(tLast + 2.2, () => {
    if (mission.done) return;
    mission.tEnd = sim.now;
    assessFFE();
    FDC.say('ROUNDS COMPLETE, OVER.', { delay: 0.4 });
    /* G13 — the post-volley traffic now depends on what the volley actually
       did. Accomplished: close it out. Suppressed only: the observer gets the
       continue-or-end decision — suppression is temporary and the FDC says so.
       No effect: the mission is not over just because the tubes went quiet. */
    const a = assessEffect();
    if (missionAccomplished()) {
      if (Math.random() < 0.5) FDC.say(pick(QUIPS.completeTail), { delay: 1.2 });
      FDC.say(pick(QUIPS.complete), { delay: 1.6 });
    } else if (a.outcome === 'suppressed') {
      FDC.say(pick(QUIPS.suppressedOnly), { delay: 1.4 });
      log('', 'Target is SUPPRESSED — heads down, not casualties, and it wears off. Your call: ' +
        'continue the mission (send a correction, or REPEAT for another volley) or close with ' +
        '"END OF MISSION, TARGET SUPPRESSED" if suppression is all you needed.', 'sys');
    } else {
      FDC.say(pick(QUIPS.noEffect), { delay: 1.4 });
      log('', 'No appreciable effect on the target. Refine your correction and REPEAT ' +
        '("right 50, repeat"), or close with END OF MISSION and own the result.', 'sys');
    }
    setState('EOM?');
  });
}

/* ---- G13: the graded effect engine (DOCTRINE.md §Pre-mission and effects data)
   Terminal effect only — WHERE a round lands stays impact = aimpoint + error.
   Every HE round contributes to Scenario.eff (casualty % for personnel,
   structural damage for point targets) banded by miss distance and scaled by
   posture; the accumulated figure grades into SUPPRESSED / NEUTRALIZED /
   DESTROYED per FM 6-30 §4-14. Suppression is the temporary one: any round in
   the outer band forces heads down for EFFECTS.suppressSec, and the verdict
   remembers it via everSuppressed even after the enemy is back up. */
function effBands() {
  const S = Scenario, E = CONFIG.EFFECTS;
  // G18 — the firing asset picks the band set; the chapter says which asset fires
  const asset = (activeChapter && activeChapter.asset === 'mortar60') ? 'mortar60' : 'arty';
  const c = E[asset][S.tgtClass] || E[asset].personnel;
  const k = S.effScale || 1;
  return { rFull: c.rFull * k, rHalf: c.rHalf * k, rSupp: c.rSupp * k,
           perRound: c.perRound, neutralizePct: c.neutralizePct, destroyPct: c.destroyPct };
}
function addRoundEffect(impact, isFFE) {
  const S = Scenario;
  if (S.type === 'convoy') return;              // convoys are assessed by vehicle kills
  const b = effBands();
  const d = dist2(impact.x, impact.z, S.enemy.x, S.enemy.z);
  if (d > b.rSupp) return;
  if (enemyAlive) {                              // the dead do not duck
    S.everSuppressed = true;
    S.suppressedUntil = sim.now + CONFIG.EFFECTS.suppressSec;
  }
  const band = d <= b.rFull ? 1 : d <= b.rHalf ? 0.5 : 0.25;
  const post = S.tgtClass === 'personnel'
    ? (CONFIG.EFFECTS.posture[S.posture] || 1) : 1;
  // G15 — sheaf shapes the VOLLEY, so it scales FFE rounds only; a single
  // adjusting round has no sheaf to speak of. G16 — the fuze is on EVERY round.
  // 11b — S.armor divides the contribution: big enough to hit easily can still
  // be hard to hurt (chitin the size of a church).
  S.eff += b.perRound * band * post * (isFFE ? sheafMult() : 1) * fuzeMult() / (S.armor || 1);
}

/* ---- G15: sheaf (FM 6-30 §4-6.f / ATP 3-09.30 §4-45) -----------------------
   Accepted by name in the call or mid-mission; inferred from the target
   description when unrequested; doctrine default otherwise (circular, 100 m).
   The choice is a statistical effect scale (per CLAUDE.md's ballistics rule),
   EXCEPT on a convoy, where a linear sheaf genuinely spreads the volley's
   aimpoints along the column axis — geometry, not a trajectory. The inference
   is recorded with its reason so the AAR can explain itself (the row's gate). */
function inferSheaf(desc, raw) {
  const m = (raw || '').toLowerCase()
    .match(/\b(converged|open|parallel|linear|circular|special)\s+sheaf\b/);
  if (m) {
    const kind = m[1] === 'parallel' || m[1] === 'special' ? 'linear' : m[1];
    return { kind, source: 'requested', why: `${m[1].toUpperCase()} SHEAF requested in the call` };
  }
  const d = (desc || '').toLowerCase();
  if (Scenario.type === 'convoy' || /convoy|column|vehicle|truck/.test(d))
    return { kind: 'linear', source: 'inferred', why: 'linear target (convoy/column) — bursts spread along its axis' };
  if (Scenario.tgtClass === 'point' || /bunker|pit|emplacement|gun|structure|building|craft|wreck/.test(d))
    return { kind: 'converged', source: 'inferred', why: 'small hard target — every piece on the same point' };
  if (/troops|infantry|personnel|squad|platoon|dismount/.test(d))
    return { kind: 'open', source: 'inferred', why: 'personnel target — bursts one effective width apart' };
  return { kind: 'circular', source: 'default', why: 'nothing requested, nothing to infer — doctrine default (circular, 100 m)' };
}
function sheafMult() {
  if (!mission || !mission.sheaf) return 1;
  const k = mission.sheaf.kind, S = Scenario;
  if (S.tgtClass === 'point')                     // spreading fire off a point costs
    return { converged: 1.25, circular: 1.0, open: 0.6, linear: 0.6 }[k] || 1;
  return S.dispersed                              // spread men want spread bursts
    ? { open: 1.2, circular: 1.0, linear: 0.9, converged: 0.6 }[k] || 1
    : { converged: 1.1, circular: 1.0, open: 0.9, linear: 0.8 }[k] || 1;
}
/* ---- G16: fuze (ATP 3-09.30 §4-43 / FM 6-30 §4-15) --------------------------
   PD (quick) is the standard method; VT/time airburst defeats troops who have
   gone flat or dug in; delay penetrates structures and buries itself against
   men in the open. Requested by name, inferred from the description otherwise,
   and the choice SCALES THE GRADED EFFECT on every round — statistical terminal
   effect per CLAUDE.md's ballistics rule, never a trajectory. VT is never
   fired danger close (airburst frag over friendlies): the FDC overrides to PD
   and says so. */
function inferFuze(desc, raw) {
  const m = (raw || '').toLowerCase()
    .match(/\bfuze\s+(vt|victor tango|variable time|time|delay|quick|pd|point detonating)\b/);
  if (m) {
    const kind = (m[1] === 'victor tango' || m[1] === 'variable time') ? 'vt'
               : (m[1] === 'point detonating' || m[1] === 'quick') ? 'pd' : m[1];
    return { kind, source: 'requested', why: `FUZE ${m[1].toUpperCase()} requested in the call` };
  }
  const d = (desc || '').toLowerCase();
  if (/trench|foxhole|dug in|dug-in|entrench/.test(d))
    return { kind: 'vt', source: 'inferred', why: 'troops under cover — airburst reaches into the holes' };
  if (Scenario.tgtClass === 'point' || /bunker|structure|building|emplacement|woods|earthwork/.test(d))
    return { kind: 'delay', source: 'inferred', why: 'hard/overhead-cover target — penetrate, then burst' };
  return { kind: 'pd', source: 'default', why: 'standard method of engagement — point detonating (quick)' };
}
function fuzeMult() {
  if (!mission || !mission.fuze) return 1;
  const k = mission.fuze.kind, S = Scenario;
  if (S.tgtClass === 'point')                     // an airburst does nothing to a roof
    return { delay: 1.3, pd: 1.0, vt: 0.35, time: 0.35 }[k] || 1;
  return S.posture === 'prone'                    // airburst defeats going flat
    ? { vt: 1.5, time: 1.3, pd: 1.0, delay: 0.4 }[k] || 1
    : { vt: 1.15, time: 1.05, pd: 1.0, delay: 0.45 }[k] || 1;
}
function handleFuze(p) {
  if (!mission || mission.done) {
    FDC.say('No mission on the net — fuzes go on rounds, not on air, over.', { delay: 0.9 });
    return;
  }
  if ((p.kind === 'vt' || p.kind === 'time') && mission.gridStr.includes('DANGER CLOSE')) {
    mission.fuze = { kind: 'pd', source: 'fdc-override',
                     why: 'VT/time refused — airburst is not fired DANGER CLOSE; PD retained' };
    mission.notes.push('Requested an airburst fuze on a danger-close mission — VT frags over our own people. The FDC overrode to PD.');
    FDC.say('NEGATIVE ON VT, MUSTANG — I am not putting airburst over friendlies at danger close. FUZE PD stays, over.', { delay: 1.1 });
    return;
  }
  mission.fuze = { kind: p.kind, source: 'requested', why: `FUZE ${p.kind.toUpperCase()} requested mid-mission` };
  FDC.say(`FUZE ${p.kind.toUpperCase()}, OUT.`, { delay: 0.9 });
}
/* 12h — shell selection mid-mission: the next rounds up carry the new nature.
   Switching to smoke on a destroy mission does not soften the verdict — the
   graded effect simply stops growing until HE comes back, which is its own
   honest lesson about what smoke does to a fire mission. */
function handleShell(p) {
  if (!mission || mission.done) {
    FDC.say('No mission on the net. Name the shell in your call for fire, over.', { delay: 0.9 });
    return;
  }
  mission.shell = p.kind;
  /* TEMPO4 — the mission's PURPOSE follows the switch, the same rule 12h
     applied at call time (intent follows the shell). Before this, a mission
     opened as ILLUMINATION and switched to HE mid-mission still graded on
     "light provided" — the coordinated-illumination sequence (illum up, then
     HE under the flare) would have passed without a single effective round. */
  mission.intent = p.kind === 'illum' ? 'illum'
                 : p.kind === 'smoke' ? 'suppress'
                 : (mission.imm ? 'suppress' : 'destroy');
  FDC.say(`SHELL ${p.kind === 'illum' ? 'ILLUMINATION' : p.kind.toUpperCase()}, OUT.`, { delay: 0.9 });
}

function handleSheaf(p) {
  if (!mission || mission.done) {
    FDC.say('No mission on the net to shape a sheaf for, over.', { delay: 0.9 });
    return;
  }
  if (p.cancel) {
    // ATP 3-09.30 §5-30 — cancelling reverts to what the FDC would have chosen
    mission.sheaf = inferSheaf(mission.desc, '');
    FDC.say(`CANCEL ${p.kind.toUpperCase()} SHEAF — ${mission.sheaf.kind.toUpperCase()} SHEAF, OUT.`, { delay: 1.0 });
    return;
  }
  const kind = p.kind === 'parallel' || p.kind === 'special' ? 'linear' : p.kind;
  mission.sheaf = { kind, source: 'requested', why: `${p.kind.toUpperCase()} SHEAF requested mid-mission` };
  if (p.kind === 'special')
    mission.notes.push('A SPECIAL sheaf carries an attitude (long-axis azimuth) when a length is given — none was sent; fired as linear.');
  FDC.say(`${p.kind.toUpperCase()} SHEAF, OUT.`, { delay: 1.0 });
}
function assessEffect() {
  const S = Scenario;
  if (S.type === 'convoy') {
    const dead = S.veh.filter(v => v.dead).length;
    return { outcome: dead >= 3 ? 'destroyed' : 'none', pct: dead * 25 };
  }
  const b = effBands();
  // threshold on the RAW figure — rounding first would gift a 9.6% volley the
  // 10% neutralization it did not earn; round only for display
  const outcome = S.eff >= b.destroyPct ? 'destroyed'
                : S.eff >= b.neutralizePct ? 'neutralized'
                : S.everSuppressed ? 'suppressed' : 'none';
  return { outcome, pct: Math.round(S.eff) };
}
// The mission is accomplished at NEUTRALIZED or better — or at SUPPRESSED when
// suppression was the stated intent (immediate suppression, suppress-target,
// a smoke screen) — or, for an illumination mission, once light is provided.
function missionAccomplished() {
  if (mission && mission.intent === 'illum') return mission.rounds.length > 0;
  const a = assessEffect();
  if (Scenario && Scenario.type === 'kaiju') return a.outcome === 'destroyed';   // 11b
  return a.outcome === 'destroyed' || a.outcome === 'neutralized' ||
         (a.outcome === 'suppressed' && mission && mission.intent === 'suppress');
}

function resolveImpact(impact, isFFE) {
  if (mission.done) return;
  const y = Math.max(H(impact.x, impact.z), 0.3);
  const shell = mission.shell || 'he';   // 12h
  /* 12h — an illumination round never touches the ground: it bursts high,
     hangs a flare, and everything below (crater, casualties, fratricide,
     alerts, convoy attrition) simply does not happen. It is light, not fires. */
  if (shell === 'illum') {
    igniteIllum(impact.x, impact.z);
    mission.usedIllum = true;   // TEMPO4 — reqIllum chapters grade on this
    mission.rounds.push(impact);
    if (isFFE) mission.ffeRounds.push(impact);
    TLOG.add('impact', '', 'illumination round', { shell: 'illum' });
    if (!isFFE) setState('ADJUSTING'); else setState('FIRE FOR EFFECT');
    return;
  }
  if (sunNet()) fireBeam(impact.x, impact.z);   // 11c — a column of noon; the burst below is real
  spawnBurst(impact.x, y, impact.z);
  if (shell === 'smoke') deployScreen(impact.x, y, impact.z);   // 12h
  mission.rounds.push(impact);
  if (isFFE) mission.ffeRounds.push(impact);
  const S = Scenario;
  // 12h — smoke builds a screen, not a casualty count; only HE feeds the model
  if (shell === 'he') addRoundEffect(impact, isFFE);
  TLOG.add('impact', '', isFFE ? 'FFE round' : `adjust round ${mission.adjustRounds}`,
    { dTgt: Math.round(dist2(impact.x, impact.z, S.enemy.x, S.enemy.z)),
      ...(shell !== 'he' ? { shell } : {}) });
  // convoy attrition — any HE round can kill vehicles (smoke cannot; 12h)
  if (S.type === 'convoy' && shell === 'he') {
    /* SUGG6 — the intercept verdict, measured at the moment it can be: the
       FIRST effect round against the live column head. Positive = the volley
       arrived up the road AHEAD of the column (a lead), negative = behind it
       (the classic miss the trigger-point math exists to prevent). */
    if (isFFE && mission.convoyLead === undefined) {
      const head = S.veh.find(v => !v.dead);
      if (head) {
        const hd = pathDir(S.path, head.d || 0);
        mission.convoyLead = Math.round(
          (impact.x - head.x) * hd.dx + (impact.z - head.z) * hd.dz);
      }
    }
    S.veh.forEach((v, i) => {
      if (!v.dead && dist2(impact.x, impact.z, v.x, v.z) < 40) {
        v.dead = true;
        visSetMat(units.vehicles[i], units.vehMatDead);
      }
    });
    if (S.veh.filter(v => !v.dead).length <= 1) enemyAlive = false;
  }
  if (mission.firstMiss === null)
    mission.firstMiss = dist2(impact.x, impact.z, S.enemy.x, S.enemy.z);
  /* G12 — fratricide and collateral damage FAIL the mission; they do not END it.
     User's correction, and it changes the lesson being taught: the old code cut
     to the AAR sixteen seconds after a friendly hit, which quietly relieved the
     observer of the worst part of the real consequence — the target is STILL
     SHOOTING at the people you just hit, and the fire mission still has to be
     finished by the same observer with the same shaking hands. Now the failure
     is recorded permanently (0★, the verdict never softens), the rant fires
     once in full, and the mission runs on to a real conclusion: effect on
     target, then RREMS. gradeMission, the AAR verdict and the EOM traffic all
     key on failReason, not on how the mission ended, so they need no change.

     The first cause stands: a fratricide followed by a collateral hit stays
     FAIL — FRATRICIDE. Repeat hits get one grim line, not the full rant again —
     the rant is a consequence, not a soundboard. No early return: the round
     still counts against the enemy if it also landed near them (physics does
     not care whose side the shrapnel finds), and execution must reach the
     state handling at the bottom or the net would jam in SHOT. */
  const failHit = (reason, pool, repeatLine) => {
    if (!mission.failReason) mission.failReason = reason;
    if (!mission.failRanted) {
      mission.failRanted = true;
      const rant = pick(pool);
      FDC.say(rant[0], { delay: 1.0 });
      FDC.say(rant[1], { delay: 2.2 });
      FDC.say(rant[2], { delay: 2.8 });
      log('', 'This mission is now a FAILURE — permanently, no stars, and the AAR will say why. ' +
        'But the target is still in the fight and the mission is still yours: adjust, get effect, ' +
        'and close it out with END OF MISSION.', 'sys');
    } else {
      FDC.say(repeatLine, { delay: 1.4 });
    }
  };
  for (const f of friendlyPositions()) {
    if (dist2(impact.x, impact.z, f.x, f.z) < (f.r || CONFIG.MISSION.fratricideRadius)) {
      failHit('fratricide', QUIPS.rantFrat,
        'MORE rounds on the friendlies, MUSTANG. Every one of these is a name I have to write down. FIX YOUR CORRECTION.');
      break;
    }
  }
  // Collateral damage vs civilians/villages — identical automatic fail.
  const WC = CONFIG.WORLD;
  const civHit =
    WORLD.civs.some(c => dist2(impact.x, impact.z, c.x, c.z) < WC.civCollateralR) ||
    WORLD.villages.some(v => v.huts.some(hh => dist2(impact.x, impact.z, hh.x, hh.z) < WC.hutCollateralR));
  if (civHit)
    failHit('collateral', QUIPS.rantCiv,
      'That one hit the village TOO. Walk it OUT of there — now, MUSTANG.');
  // walking rounds the WRONG direction twice earns a tirade
  if (!isFFE) {
    const dTgt = dist2(impact.x, impact.z, S.enemy.x, S.enemy.z);
    if (mission.lastAdjDist !== null && dTgt > mission.lastAdjDist + 40) {
      mission.wrongWay++;
      if (mission.wrongWay === 2) FDC.say(pick(QUIPS.wrongWay), { delay: 2.2 });
    }
    // Troops in the open react to fire: a near miss alerts them, and after
    // the second one they go to ground and spread out — the effect radius
    // stops being generous. Slow adjustment costs effect, not just stars.
    if ((S.type === 'troops' || S.type === 'raid' || S.type === 'chow') && dTgt < 300 && !S.dispersed) {
      S.alerted++;
      if (S.alerted === 1) {
        log('', 'The infantry heard that one — they are moving and going flat.', 'sys');
      } else if (S.alerted >= 2) {
        S.dispersed = 1;
        // spread the visible cluster and drop them prone
        const drng = mulberry32((S.seed * 7919 + 13) >>> 0);
        units.troops.forEach(m => {
          if (!m.visible) return;
          const a = drng() * Math.PI * 2, r = 18 + drng() * 26;
          const nx = m.position.x + Math.sin(a) * r, nz = m.position.z + Math.cos(a) * r;
          m.position.set(nx, H(nx, nz) + 0.3, nz);
          m.rotation.z = 1.2;                       // prone, pivoting on the boots
        });
        /* G13 — dispersal is now a POSTURE change, which is what it physically
           is: FM 7-90 App. B has the same volley clearing neutralization on a
           standing platoon and failing it prone. Casualty effect per round
           drops to EFFECTS.posture.prone of the standing figure. */
        S.posture = 'prone';
        log('', 'They have dispersed and gone prone. Casualty effect per round is now less than ' +
          'half — a volley that would have destroyed them standing may only neutralize, or worse. ' +
          'Adjust faster next time.', 'sys');
        sendSpotReport('displace');   // target displaced — re-cue the observer
        if (coachOn())
          FDC.say('Be advised, MUSTANG — they are dispersing. Every round you spend walking is a man of theirs finding a hole. Get on with it.', { delay: 2.6 });
      }
    }
    // --- adjustment doctrine: track the spotting, then coach the pattern
    mission.missTrace.push(Math.round(dTgt));
    const otAzT = azTo(OP.x, OP.z, S.enemy.x, S.enemy.z);
    const along = (impact.x - S.enemy.x) * Math.sin(otAzT) +
                  (impact.z - S.enemy.z) * -Math.cos(otAzT);   // + = over, - = short
    if (along > 30) mission.sides.long = true;
    else if (along < -30) mission.sides.short = true;
    const nowBracketed = mission.sides.long && mission.sides.short;
    const prev = mission.lastAdjDist;
    if (prev !== null && Math.abs(dTgt - prev) < prev * 0.15 && dTgt > 80) mission.wasted++;
    if (coachOn()) {
      const C = mission.coached;
      if (mission.rounds.length === 1 && dTgt > 250 && !C.loc) {
        C.loc = true;
        FDC.say(pick(QUIPS.coachLoc).replace('{MISS}', Math.round(dTgt)), { delay: 2.4 });
      } else if (nowBracketed && !mission.bracketed && !C.bracketGood) {
        C.bracketGood = true;
        FDC.say(pick(QUIPS.coachBracketGood), { delay: 2.4 });
      } else if (!nowBracketed && mission.adjustRounds >= 3 && dTgt > 120 && !C.bracket) {
        C.bracket = true;
        FDC.say(pick(QUIPS.coachBracket), { delay: 2.4 });
      } else if (prev !== null && Math.abs(dTgt - prev) < prev * 0.15 && dTgt > 80 && !C.stagnant) {
        C.stagnant = true;
        FDC.say(pick(QUIPS.coachStagnant), { delay: 2.4 });
      }
    }
    mission.bracketed = nowBracketed;
    mission.lastAdjDist = dTgt;
  }
  // Spotting aid, tiered so the observer climbs the skill ladder:
  //   EASY   — the measurement AND the worked mil-relation arithmetic
  //   NORMAL — the measurement only; the observer does the math
  //   HARD   — nothing; measure it off your own reticle
  if (!isFFE && DIFFICULTY !== 'hard') {
    const sp = spotBurst(impact);
    const seen = hasLOS(eye.x, eye.y, eye.z, impact.x, Math.max(H(impact.x, impact.z), 0) + 2, impact.z);
    schedule(sim.now + 1.5, () => {
      if (!seen) {
        log('', 'SPOTTING: burst not observed — masked by terrain. Adjust off the map and the sound.', 'sys');
        // G8 — the masked path must carry the direction reminder too: a defilade
        // chapter is a grid mission fought off the map, and the requirement to
        // send DIRECTION before correcting does not care whether the burst was seen.
        if (mission && !mission.otDirSent && mission.method === 'grid' && mission.adjustRounds <= 1)
          log('', 'Before your first correction: send OT DIRECTION ("DIRECTION 5920, OVER"). ' +
            'The battery cannot orient LEFT/RIGHT or ADD/DROP without your observer-target line.', 'sys');
        return;
      }
      const lr = sp.mils >= 0 ? 'RIGHT' : 'LEFT';
      const os = sp.overShort >= 0 ? 'OVER' : 'SHORT';
      if (DIFFICULTY === 'normal') {
        log('', `SPOTTING: burst ${Math.abs(sp.mils).toFixed(0)} mils ${lr} of target, spotting ${os}. ` +
          `OT range ${Math.round(sp.otRange)} m → OT factor ${sp.otf}. Deviation = mils × OT factor, to the nearest 10. Range: bracket it.`, 'sys');
      } else {
        const devTxt = sp.devCorr === 0
          ? `${Math.abs(sp.mils).toFixed(0)} mils × ${sp.otf} is under the 30 m minimum — deviation is good, do not call it`
          : `${Math.abs(sp.mils).toFixed(0)} mils × ${sp.otf} = ${Math.abs(Math.round(sp.mils * sp.otf))} → call ${sp.devCorr > 0 ? 'RIGHT' : 'LEFT'} ${Math.abs(sp.devCorr)}`;
        const brk = mission.sides.long && mission.sides.short
          ? 'You have a bracket — halve your last range correction'
          : `Spotting ${os} — bracket it: ${os === 'SHORT' ? 'ADD' : 'DROP'} enough to land on the far side`;
        log('', `SPOTTING (EASY): burst ${Math.abs(sp.mils).toFixed(0)} mils ${lr}, ${os}. ` +
          `OT factor ${sp.otf} (${Math.round(sp.otRange)} m ÷ 1000). Deviation: ${devTxt}. Range: ${brk}.`, 'sys');
      }
      /* G8 — teach the requirement at the exact moment it becomes binding: the
         observer is looking at his first burst and composing his first
         correction. Grid missions only; the FDC cannot orient a correction
         without the OT line, and will refuse until DIRECTION is sent. */
      if (mission && !mission.otDirSent && mission.method === 'grid' && mission.adjustRounds <= 1)
        log('', 'Before your first correction: send OT DIRECTION ("DIRECTION 5920, OVER" — grid ' +
          'azimuth to the target, nearest 10 mils). The battery cannot compute LEFT/RIGHT or ' +
          'ADD/DROP without your observer-target line, and will refuse the correction.', 'sys');
    });
  }
  if (!isFFE) setState('ADJUSTING');
  else setState('FIRE FOR EFFECT');
}

function assessFFE() {
  const S = Scenario;
  if (S.type === 'convoy') {
    mission.hits = S.veh.filter(v => v.dead).length;
    if (mission.hits >= 3) enemyAlive = false;
    return;
  }
  // G13 — `hits` is now "effect rounds inside the outer band": a display and
  // diagnosis stat, not the pass criterion. The criterion is assessEffect().
  mission.hits = mission.ffeRounds.filter(r =>
    dist2(r.x, r.z, S.enemy.x, S.enemy.z) < effBands().rSupp).length;
  const a = assessEffect();
  // 11b — a kaiju does not do "combat-ineffective": it is walking or it is
  // not. Only DESTROYED fells it; anything less and the landfall clock runs.
  if (a.outcome === 'destroyed' || (a.outcome === 'neutralized' && S.type !== 'kaiju')) {
    enemyAlive = false;
    // figure origin is at the feet, so this pivots the body over without sinking
    // it — and, unlike the old `position.y -= 0.55`, it is idempotent if the
    // effect check re-runs.
    units.troops.forEach(m => { if (m.visible) m.rotation.z = 1.35; });
    // the heavier materiel damage renders only at DESTROYED — a neutralized
    // position has broken men, not burning trucks
    if (S.type === 'strongpoint' && a.outcome === 'destroyed')
      units.vehicles.forEach(m => { if (m.visible) m.rotation.z = 0.25; });
    if (S.type === 'bunker' && a.outcome === 'destroyed') visSetColor(units.bunker, 0x3A352F);
    if (S.type === 'kaiju' && a.outcome === 'destroyed') visSetColor(units.bunker, 0x4A2B24);
  }
}

