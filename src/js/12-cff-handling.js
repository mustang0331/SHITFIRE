/* ================================================ G22: THE THREE-TRANSMISSION CFF
   DOCTRINE.md §15 and CLAUDE.md both say a call for fire is SIX ELEMENTS IN THREE
   TRANSMISSIONS, each read back by the FDC. Before this row there was no state
   between messages at all: parseMessage pulled warning order, location and
   description out of ONE string, so the doctrinal Transmission 1 fell through to
   `unknown` and got answered with the mockery reserved for gibberish. An observer
   who had learned the real format could not use it. Measured in the G19 audit: 16
   of 29 doctrinal transmissions classified as `unknown`.

   THE RULE THAT SHAPES THIS DESIGN. DOCTRINE.md §5 is explicit that the scripts
   are guidelines, not gates, and the forgiving one-shot call is what every
   existing chapter, tutorial and saved transcript uses. So this is PURELY
   ADDITIVE: a complete call in one message takes exactly the path it always did,
   byte for byte. The queue only engages when the observer sends a partial
   transmission, which previously did not work at all. Nothing that worked before
   can break, because nothing that worked before enters this code.

   Assembly, not reimplementation: partial transmissions accumulate here and the
   completed result is handed to the SAME handleCFF() the one-shot path uses. There
   is one code path for validating and firing a mission, so the two forms cannot
   drift into disagreeing about danger close, strict mode, grid bounds or anything
   else. `raw` and `toks` are concatenated across the transmissions, which is what
   lets callsigns sent in T1 satisfy strict mode and "danger close" sent in T3
   satisfy the danger-close gate. */
const CFFQ = { on: false, stage: 0, warno: 'none', loc: null,
               raw: '', toks: [], t0: 0 };
function cffqReset() {
  CFFQ.on = false; CFFQ.stage = 0; CFFQ.warno = 'none'; CFFQ.loc = null;
  CFFQ.raw = ''; CFFQ.toks = [];
}
function cffqAdd(p) {                       // fold a transmission's text in
  CFFQ.raw = (CFFQ.raw ? CFFQ.raw + ' ' : '') + (p.raw || '');
  CFFQ.toks = CFFQ.toks.concat(p.toks || []);
}
/* The observer can abandon a half-sent call. Left dangling, the next bare grid
   would be silently absorbed into a call started minutes ago, which is worse than
   not having a queue at all — so it lapses, and says so rather than going quiet. */
const CFFQ_LAPSE = 75;
function cffqTick() {
  if (!CFFQ.on) return;
  if (sim.now - CFFQ.t0 < CFFQ_LAPSE) return;
  cffqReset();
  FDC.say('MUSTANG 12, you started a call for fire and never finished it. ' +
          'Net is clear. Start again when you have all six elements, out.', { delay: 0.4 });
}
function cffqStart(warno, p) {
  cffqReset();
  CFFQ.on = true; CFFQ.stage = 1; CFFQ.warno = warno; CFFQ.t0 = sim.now;
  cffqAdd(p);
  // Transmission 1 read back, per CLAUDE.md: the FDC reads back EACH transmission
  FDC.say(`${CONFIG.FDC.obs}, ${fdcCall()}, ` +
          `${warno === 'ffe' ? 'FIRE FOR EFFECT' : 'ADJUST FIRE'}, OUT.`,
          { delay: CONFIG.FDC.readbackDelay });
  FDC.say('SEND YOUR TARGET LOCATION, OVER.', { delay: 1.5 });
}
/* Transmission 2 accepted. Read back the LOCATION only — not the whole call —
   because that is the transmission the observer just sent and reading back more
   than was said is how a readback stops being a check. */
function cffqLocation(p, locEcho) {
  CFFQ.stage = 2; CFFQ.loc = p; CFFQ.t0 = sim.now;
  cffqAdd(p);
  FDC.say(`${locEcho}, OUT.`, { delay: CONFIG.FDC.readbackDelay });
  FDC.say('SEND YOUR TARGET DESCRIPTION, OVER.', { delay: 1.5 });
}
/* Transmission 3 completes it. Build the object handleCFF expects by merging the
   queued location with the accumulated text, then hand it over. */
function cffqComplete(desc, p) {
  const loc = CFFQ.loc;
  cffqAdd(p);
  const merged = Object.assign({}, loc, {
    type: 'cff', warno: CFFQ.warno !== 'none' ? CFFQ.warno : loc.warno,
    desc, raw: CFFQ.raw, toks: CFFQ.toks, staged: true,
  });
  cffqReset();
  handleCFF(merged);
}
/* A short echo of just the location, for the T2 readback. Mirrors the strings
   handleCFF builds; kept deliberately terse because it is a readback, not a brief. */
function locEchoOf(p) {
  if (p.method === 'polar')
    return `DIRECTION ${fmtMils(p.dirMils % 6400)}, DISTANCE ${p.distM}`;
  if (p.method === 'shift')
    return `SHIFT KNOWN POINT ${p.kpId}` +
      (p.dirMils !== null ? `, DIRECTION ${fmtMils(p.dirMils % 6400)}` : '') +
      (p.sRight ? `, ${p.sRight > 0 ? 'RIGHT' : 'LEFT'} ${Math.abs(p.sRight)}` : '') +
      (p.sAdd ? `, ${p.sAdd > 0 ? 'ADD' : 'DROP'} ${Math.abs(p.sAdd)}` : '');
  if (p.digits && p.digits.length === 6)
    return `GRID ${p.digits.slice(0, 3)} ${p.digits.slice(3)}`;
  return `GRID ${p.digits}`;
}

/* ============================================================ MESSAGE HANDLING */
function handleCFF(p) {
  if (mission && !mission.done) {
    FDC.say('MUSTANG 12, we are mid-mission. Adjust, fire for effect, or send end of mission, over.', { delay: 1 });
    return;
  }
  // 60mm precision shoots demand the full 8-digit grid
  if (activeChapter && activeChapter.asset === 'mortar60' &&
      p.method === 'grid' && p.digits && p.digits.length === 6) {
    FDC.say('MUSTANG 12, HELLHOUND — this is a 60 MIKE MIKE precision shoot. Six digits is a neighborhood; I need an ADDRESS. Send 8 digits, ten-meter precision, over.', { delay: 1.1 });
    return;
  }
  // strict net (Volume IV): full format or the call is rejected
  if (activeChapter && activeChapter.strict) {
    let challenge = null;
    // G17 — the RIGHT callsign: on a mortar net, addressing HELLHOUND is a miss
    if (!p.raw.includes(fdcShort().toLowerCase()) || !p.raw.includes('mustang'))
      challenge = p.raw.includes('hellhound') && fdcShort() !== 'HELLHOUND'
        ? `HELLHOUND is the one-five-five battery and they are not on this net. You are talking to ${fdcCall()}. Say again your last, addressed to the right people, over.`
        : 'this net runs callsigns. Identify yourself and your addressee and say again your last, over.';
    else if (p.warno === 'none')
      challenge = 'no warning order heard. ADJUST FIRE or FIRE FOR EFFECT, then location. Say again, over.';
    else if (!p.desc)
      challenge = 'no target description. I do not shoot at adjectives. Say again with description, over.';
    else if (!p.toks.includes('over'))
      challenge = 'transmissions end with OVER on this net. The book is not a suggestion tonight. Say again, over.';
    if (challenge) {
      FDC.say('MUSTANG 12, HELLHOUND — STRICT NET: ' + challenge, { delay: 1.1 });
      return;
    }
  }
  // target location -> world point, any method
  let cx, cz, locStr;
  if (p.method === 'shift') {
    const kp = (Scenario.kps || []).find(k => k.id === p.kpId);
    if (!kp) {
      FDC.say(`NEGATIVE. There is no known point ${p.kpId} registered on this net. Say again, over.`, { delay: 1 });
      return;
    }
    const az = p.dirMils !== null
      ? (p.dirMils % 6400) / MILS_PER_RAD
      : azTo(OP.x, OP.z, kp.x, kp.z);
    const fx = Math.sin(az), fz = -Math.cos(az);
    const rx = Math.cos(az), rz = Math.sin(az);
    cx = kp.x + fx * p.sAdd + rx * p.sRight;
    cz = kp.z + fz * p.sAdd + rz * p.sRight;
    locStr = `SHIFT KNOWN POINT ${p.kpId}` +
      (p.dirMils !== null ? `, DIRECTION ${fmtMils(p.dirMils % 6400)}` : '') +
      (p.sRight ? `, ${p.sRight > 0 ? 'RIGHT' : 'LEFT'} ${Math.abs(p.sRight)}` : '') +
      (p.sAdd ? `, ${p.sAdd > 0 ? 'ADD' : 'DROP'} ${Math.abs(p.sAdd)}` : '') +
      (p.vertM ? `, ${p.vertM > 0 ? 'UP' : 'DOWN'} ${Math.abs(p.vertM)}` : '');
  } else if (p.method === 'polar') {
    /* G9 — no position report, no polar mission. The FDC has a direction and a
       distance and nothing to hang them on. */
    if (!Scenario.posRep) {
      FDC.say('MUSTANG 12, HELLHOUND — I cannot resolve a POLAR mission. I do not hold your ' +
              'position. Send a position report first — "POSITION GRID 245 523" — then your ' +
              'call, over.', { delay: 1.1 });
      return;
    }
    if (p.distM < 100 || p.distM > 12000) {
      FDC.say(pick(QUIPS.badGrid), { delay: 1 });
      return;
    }
    /* Resolved from the REPORTED position, not the true one. The FDC only knows
       what it was told; an observer who mis-located himself finds out from the
       fall of shot, which is the entire point of requiring the report. */
    const az = (p.dirMils % 6400) / MILS_PER_RAD;
    cx = Scenario.posRep.x + Math.sin(az) * p.distM;
    cz = Scenario.posRep.z - Math.cos(az) * p.distM;
    locStr = `DIRECTION ${fmtMils(p.dirMils % 6400)}, DISTANCE ${p.distM}` +
             (p.vertM ? `, ${p.vertM > 0 ? 'UP' : 'DOWN'} ${Math.abs(p.vertM)}` : '');
  } else if (p.digits.length === 6) {
    const e3 = parseInt(p.digits.slice(0, 3)), n3 = parseInt(p.digits.slice(3));
    const w = enToWorld(e3 * 100 + 50, n3 * 100 + 50); cx = w.x; cz = w.z;
    locStr = `GRID ${e3} ${n3}`;
  } else if (p.digits.length === 8) {
    const e4 = parseInt(p.digits.slice(0, 4)), n4 = parseInt(p.digits.slice(4));
    const w = enToWorld(e4 * 10 + 5, n4 * 10 + 5); cx = w.x; cz = w.z;
    locStr = `GRID ${p.digits}`;
  } else {
    FDC.say(pick(QUIPS.badGrid), { delay: 1 });
    return;
  }
  const en = worldToEN(cx, cz);
  if (en.e < CONFIG.MAP.originE || en.e >= CONFIG.MAP.originE + 10000 ||
      en.n < CONFIG.MAP.originN || en.n >= CONFIG.MAP.originN + 10000) {
    FDC.say(pick(QUIPS.badGrid), { delay: 1 });
    return;
  }
  // danger-close gate: friendlies inside 600 m demand the proword
  let minF = Infinity;
  for (const f of friendlyPositions())
    minF = Math.min(minF, dist2(cx, cz, f.x, f.z));
  const dcSent = saidDangerClose(p.raw);          // F6 — tolerant of "danger clothes"
  if (minF < 600 && !dcSent) {
    FDC.say(pick(QUIPS.dangerClose), { delay: 1.2 });
    return;
  }
  if (dcSent) locStr += ', DANGER CLOSE';
  const notes = formatNotes(p);
  /* G23 — the vertical is doctrinally conditional, not optional: DOCTRINE.md §27
     and §28 send up/down ONLY when the height difference from the reference point
     to the target is 35 m or more. The heightfield knows that difference exactly,
     so the trainer can check both directions of the mistake instead of ignoring
     the element. Reference is the OBSERVER for polar and the KNOWN POINT for a
     shift, because that is what each method measures from. */
  if (p.method === 'polar' || p.method === 'shift') {
    let refH = OP.h;
    if (p.method === 'shift') {
      const rkp = (Scenario.kps || []).find(k => k.id === p.kpId);
      if (rkp) refH = H(rkp.x, rkp.z);
    }
    const dH = H(cx, cz) - refH;
    const needVert = Math.abs(dH) >= 35;
    const sentVert = p.vertM !== null && p.vertM !== undefined;
    if (needVert && !sentVert)
      notes.push(`Height difference to the target is ${Math.round(dH)} m — at 35 m or more the ` +
                 `vertical shift is part of the call. Send ${dH > 0 ? 'UP' : 'DOWN'} ` +
                 `${Math.round(Math.abs(dH) / 5) * 5}.`);
    else if (!needVert && sentVert)
      notes.push(`You sent a vertical shift, but the height difference is only ${Math.round(dH)} m. ` +
                 `Below 35 m it is omitted.`);
    else if (sentVert && Math.abs(p.vertM) % 5)
      notes.push('Vertical shift is sent to the nearest 5 m.');
  }
  if (activeChapter && activeChapter.method && p.method !== activeChapter.method)
    notes.push(`This chapter required a ${activeChapter.method.toUpperCase()} target location — you sent ${p.method.toUpperCase()} (chapter capped at 2★).`);
  const warno = p.warno === 'none' ? 'adjust' : p.warno;
  const warnoText = warno === 'ffe' ? 'FIRE FOR EFFECT' : 'ADJUST FIRE';
  /* G22 — read back what was actually just said. On the staged path the warning
     order and the location were each already read back when they were sent, so
     repeating the whole call here would be a third readback of two things the
     observer has already confirmed. Read back the third transmission only. The
     one-shot path is unchanged: everything arrived at once, so all of it is read
     back at once. */
  /* G14/G26 — an immediate mission gets its own short readback and NO MTO, per
     DOCTRINE.md §42 ("No MTO is sent for immediate suppression"). The whole point
     of the mission type is that it is fast: friendlies are being shot at, so the
     FDC does not stop to recite units, rounds and a target number. */
  const immText = p.imm === 'smoke' ? 'IMMEDIATE SMOKE' : 'IMMEDIATE SUPPRESSION';
  FDC.say(p.imm
    ? `${immText}, ${locStr}, OUT.`
    : p.staged
      ? `${p.desc ? p.desc.toUpperCase() : 'NO DESCRIPTION'}` +
        (saidDangerClose(p.raw) ? ', DANGER CLOSE' : '') + ', OUT.'
      : `${CONFIG.FDC.obs}, ${fdcCall()}, ${warnoText}, ${locStr}` +
        (p.desc ? `, ${p.desc.toUpperCase()}` : '') + ', OUT.',
          { delay: p.imm ? 0.7 : CONFIG.FDC.readbackDelay });
  /* G11 — the MTO expectation. Built LOCALLY and handed to fireMission via meta,
     never written onto `mission` here: fireMission rebuilds the mission object
     after this block runs, so anything set on the old one is silently wiped —
     measured the trap before shipping into it. Text for the say and keys + target
     number for the gist-scorer come from one place so they cannot disagree.
     DOCTRINE.md §42: "The observer reads back the entire MTO." */
  let mtoSpec = null;
  if (!p.imm) {
    const m60 = activeChapter && activeChapter.asset === 'mortar60';
    mtoSpec = m60
      ? { keys: ['tube', 'section', 'adjust', 'effect', '60', 'mike', 'he'],
          tgt: '7002', read: false, nagged: false }
      : { keys: ['gun', 'battery', 'adjust', 'effect', '155', 'he', 'fuze', 'pd'],
          tgt: '7001', read: false, nagged: false };
    FDC.say(m60
      ? 'MESSAGE TO OBSERVER: ONE TUBE IN ADJUST, SECTION IN EFFECT, 60 MIKE MIKE HE, TARGET NUMBER ALPHA ALPHA 7002, OVER.'
      : 'MESSAGE TO OBSERVER: ONE GUN IN ADJUST, BATTERY IN EFFECT, 155 HE, FUZE PD, TARGET NUMBER ALPHA ALPHA 7001, OVER.',
            { delay: 1.4 });
  }
  // deviation policy: sloppy-but-safe call format earns a snide remark and
  // proceeds; a clean call gets the usual readback color.
  if (notes.length >= 2) FDC.say(pick(QUIPS.snide), { delay: 1.1 });
  else if (Math.random() < 0.45) FDC.say(pick(QUIPS.readbackTail), { delay: 1.1 });
  // advisory when the aimpoint is near a civilian village (non-blocking)
  let minCiv = Infinity;
  for (const v of WORLD.villages)
    for (const hh of v.huts) minCiv = Math.min(minCiv, dist2(cx, cz, hh.x, hh.z));
  if (minCiv < 250) FDC.say(pick(QUIPS.nearCiv), { delay: 1.2 });
  if (H(cx, cz) < 0) FDC.say(pick(QUIPS.water), { delay: 1 });
  setState('MISSION SENT');
  // G24 — "at my command" is an element of transmission 3, so it arrives inside the
  // call's own text rather than as a separate proword. CFFQ concatenates raw across
  // transmissions, so this works whether the call came one-shot or staged.
  fireMission({ x: cx, z: cz }, warno, { notes, desc: p.desc, gridStr: locStr,
                                         method: p.method, mto: mtoSpec,
                                         amc: p.raw.includes('at my command') });
}

/* ---- G25: the safety stops (DOCTRINE.md §67) --------------------------------
   CHECK FIRING is an emergency stop on the guns. CEASE LOADING is the softer
   one: stop feeding the tubes, but rounds already loaded may be fired.

   What deliberately does NOT happen: rounds already in the air are not deleted.
   A check-firing call cannot recall a shell in flight, and pretending otherwise
   would teach the observer that the call is a bigger safety net than it is —
   which is exactly the wrong lesson to take out of a fires trainer. Scheduled
   impacts resolve normally; only NEW firing is blocked.

   Either call is always answered, mission or no mission. An observer who says
   CHECK FIRING and hears nothing back has every reason to think the net is dead.
   Resuming is implicit: the next correction or fire-for-effect lifts the hold,
   and the FDC says so, per DOCTRINE §5 (scripts are guidelines, not gates). */
function handleCheckFiring() {
  FDC.say('CHECK FIRING, CHECK FIRING — ALL GUNS SAFE, OUT.', { delay: 0.5 });
  if (!mission || mission.done) {
    log('', 'CHECK FIRING acknowledged. No mission in progress — nothing was in the air.', 'sys');
    return;
  }
  mission.checkFire = true;
  mission.usedCheckFire = true;
  setState('CHECK FIRING');
  const inFlight = state === 'SHOT' || state === 'FIRE FOR EFFECT';
  log('', 'CHECK FIRING — the battery will fire nothing further until you resume. ' +
    (inFlight
      ? 'Rounds already in the air will still impact; a check-firing call cannot recall a shell in flight. '
      : '') +
    'Send a correction or FIRE FOR EFFECT when you are ready to continue.', 'sys');
}
function handleCeaseLoading() {
  FDC.say('CEASE LOADING, OUT.', { delay: 0.5 });
  if (!mission || mission.done) {
    log('', 'CEASE LOADING acknowledged. No mission in progress.', 'sys');
    return;
  }
  mission.checkFire = true;
  mission.usedCheckFire = true;
  setState('CEASE LOADING');
  log('', 'CEASE LOADING — no further rounds will be loaded. Anything already loaded or in ' +
    'the air still goes downrange. Send a correction or FIRE FOR EFFECT to resume.', 'sys');
}
/* Called by handleAdjust before it fires anything. Returns true if the hold was
   lifted by this transmission, so the caller can carry on. */
function liftCheckFire() {
  if (!mission || !mission.checkFire) return false;
  mission.checkFire = false;
  FDC.say('CANCEL CHECK FIRING — BATTERY IS HOT, OUT.', { delay: 0.6 });
  setState('ADJUSTING');
  return true;
}

// OT direction: doctrine requires it before or with the first correction on
// a grid mission, so the FDC can orient the observer's left/right.
function handleDirection(p) {
  if (!mission || mission.done) {
    FDC.say(pick(QUIPS.noMission), { delay: 1 });
    return;
  }
  mission.otDirSent = true;
  const trueAz = radToMils(azTo(OP.x, OP.z, mission.aim.x, mission.aim.z));
  const off = t => Math.abs(((p.dirMils - t + 3200) % 6400) - 3200);
  let err = off(trueAz);
  FDC.say(`DIRECTION ${fmtMils(p.dirMils % 6400)}, OUT.`, { delay: 1 });
  /* G2 — there are now TWO specific, nameable declination mistakes, and both land
     exactly one G-M angle off the truth:

       magErr  the observer read MAG off the HUD and sent it raw. Needs to add 124.
       revErr  the observer knew about the G-M angle and applied it backwards.

     Both were previously invisible. The generic coach only spoke above 200 mils
     and a 7-degree declination error is 124, so the trainer accepted a rotated OT
     frame in silence — measured, and the reason the generic threshold below is now
     100 rather than 200. OT direction is typed, not sighted through a wobbling
     compass, so 100 mils is a real mistake and not honest sighting scatter. */
  const magErr = off(gridToMag(trueAz));      // sent the raw magnetic reading
  const revErr = off(magToGrid(trueAz));      // applied the G-M angle the wrong way
  const D = Math.round(declMils());
  /* A specific diagnosis has to fit DECISIVELY better than the plain answer, not
     merely fit. A first attempt used `candidateErr < 40` alone, and the harness
     caught it accusing an honest 90-mil-sloppy grid azimuth of having the G-M
     angle backwards — 90 sits only 34 off the reversed candidate. Requiring the
     candidate to beat the raw error by 60 mils separates "you made THIS mistake"
     from "you were imprecise", which is the difference between coaching and
     misinforming. */
  const fits = candErr => candErr < 45 && err - candErr > 60;
  if (fits(magErr)) {
    if (coachOn())
      FDC.say(`MUSTANG, that is your MAGNETIC reading. The net runs on grid. Add your G-M angle — ${D} mils — and send it again, over.`, { delay: 1.4 });
    mission.notes.push(`Sent OT direction as a magnetic azimuth. The HUD reads MAG; the net wants GRID. Add ${D} mils (declination ${CONFIG.NAV.declEastDeg}° E).`);
  } else if (fits(revErr)) {
    if (coachOn())
      FDC.say(`MUSTANG, you have your G-M angle the wrong way round — that is ${D} mils PAST the line, not short of it. Grid is magnetic PLUS the angle. Send it again, over.`, { delay: 1.4 });
    mission.notes.push(`Applied the G-M angle in the wrong direction (subtracted instead of added). GRID = MAG + ${D} mils.`);
  } else if (err > 80 && coachOn()) {
    FDC.say(`MUSTANG, that direction is off your own line of sight by roughly ${Math.round(err)} mils. Your left and right corrections are computed against it — check your compass, over.`, { delay: 1.4 });
  }
}

function handleAdjust(p) {
  if (!mission || mission.done) {
    FDC.say(pick(QUIPS.noMission), { delay: 1 });
    sysHint();
    return;
  }
  /* G25 — lift a check-firing hold FIRST. It has to happen before the
     `state !== 'ADJUSTING'` gate below, or the very transmission meant to resume
     the mission would be rejected by the hold it is resuming from. */
  liftCheckFire();
  /* G11 — the MTO readback was skipped and the observer is already correcting.
     Same shape as the OT-direction gate below, deliberately: strict blocks ONCE
     (nagged flag set first, so the retry passes — scripts are guidelines even
     when the net is strict about saying so), forgiving coaches once and moves on.
     The round is already on the way either way; DOCTRINE §5 grades, not gates. */
  if (mission.mto && !mission.mto.read && !mission.mto.nagged) {
    mission.mto.nagged = true;
    mission.notes.push('Never read back the message to observer — the readback is your only check that the FDC has the right target number.');
    if (activeChapter && activeChapter.strict) {
      FDC.say('MUSTANG 12, HELLHOUND — STRICT NET: you did not read back my message to observer. ' +
              `Read it back — target number ALPHA ALPHA ${mission.mto.tgt} — then send your correction, over.`,
              { delay: 1.1 });
      return;
    }
    if (coachOn())
      FDC.say('Copy — but you never read back my MTO, MUSTANG. That readback is how we both know ' +
              'you are shooting the target I think you are shooting. Continuing, out.', { delay: 1.4 });
  }
  /* G8 — no OT direction, no correction. This used to be a coach-once nag in
     forgiving mode ("sending it anyway"), and that was WRONG, not lenient: the
     user's correction is that the battery is physically unable to convert
     "left 50" or "add 200" into a gun order without knowing the observer-target
     line — left of WHAT? add along WHAT? — so executing the correction anyway
     was the trainer quietly doing impossible math on the observer's behalf.
     DOCTRINE §5's grade-don't-gate applies to format, not to physics. The
     refusal stands in every mode, repeats until DIRECTION arrives, and names
     the fix. Grid missions only: polar and shift carry their direction in the
     call itself, which is why they are exempt (G7). */
  if (p.any && !mission.otDirSent && mission.method === 'grid') {
    if (!mission.dirNagged) {
      mission.dirNagged = true;
      mission.notes.push('Sent a correction before OT DIRECTION on a grid mission — the FDC cannot orient left/right or add/drop without the observer-target line.');
      FDC.say('MUSTANG 12, HELLHOUND — I cannot execute that. I have no OT DIRECTION for this ' +
              'mission: without your observer-target line I do not know which way your LEFT is or ' +
              'along what line to ADD. Send DIRECTION — grid azimuth to the target, nearest 10 mils — ' +
              'then your correction, over.', { delay: 1.1 });
    } else {
      FDC.say('STILL no OT direction, MUSTANG. DIRECTION first, then the correction, over.',
              { delay: 1.0 });
    }
    return;
  }
  if (state === 'EOM?') {
    FDC.say('MUSTANG 12, rounds are complete. Send end of mission, over.', { delay: 1 });
    return;
  }
  /* F5 — an unclaimed number in the transmission. Say so and refuse, rather than
     acting on half a correction. The observer usually cannot tell that the net
     heard something different from what he said, so this is the only place the
     mismatch can surface. */
  if (p.stray > 0) {
    FDC.say(`MUSTANG 12, I have ${p.stray === 1 ? 'a number' : p.stray + ' numbers'} in that ` +
            `correction I cannot attach to anything. Say again with the proword — ` +
            `LEFT or RIGHT, then ADD or DROP, over.`, { delay: 1.0 });
    mission.notes.push('A correction contained a number with no proword attached. ' +
      'Deviation first ("LEFT/RIGHT nnn"), then range ("ADD/DROP nnn").');
    return;
  }
  if (state !== 'ADJUSTING') { FDC.say(pick(QUIPS.inFlight), { delay: 1 }); return; }
  /* G23 — height of burst. The observer can now send it and be understood instead
     of being told to say again in English. It deliberately does NOT move the
     aimpoint: the model is `impact = aimpoint + error` in the horizontal plane
     (CLAUDE.md forbids a trajectory sim), and height of burst is a fuze-setting
     correction, not a location correction. So it is acknowledged honestly rather
     than faked — and the standard method of engagement here is fuze quick, where
     HOB genuinely does nothing, which is what the FDC says. Sending one is a
     format note, not an error.
     ⚠ When G16 lands (fuze selection) this is where a VT/time fuze would make the
     correction meaningful; leave the hook, do not fake the effect before then. */
  if (p.corr.vert) {
    const v = p.corr.vert;
    FDC.say(`${v > 0 ? 'UP' : 'DOWN'} ${Math.abs(v)} — NEGATIVE, MUSTANG, we are firing FUZE QUICK. ` +
            `Height of burst does nothing on a point-detonating round. ` +
            `Deviation and range only, over.`, { delay: 1.1 });
    mission.notes.push('Sent a height-of-burst correction against fuze quick. HOB applies to ' +
                       'airburst (VT/time) fuzes; with PD it has no effect.');
    if (Math.abs(v) % 5)
      mission.notes.push('Height of burst is sent in multiples of 5 m.');
    // a vertical-only transmission is fully handled; nothing left to apply
    if (!p.corr.right && !p.corr.add && !p.ffe) return;
  }
  // strict net: corrections come rounded — deviation in tens, range in hundreds.
  // 50 m is legal ONLY on the correction that enters fire for effect (DOCTRINE.md,
  // "Corrections"), so the range modulus is gated on p.ffe rather than always 50.
  if (activeChapter && activeChapter.strict && p.any &&
      (Math.abs(p.corr.right) % 10 || Math.abs(p.corr.add) % (p.ffe ? 50 : 100))) {
    FDC.say(p.ffe
      ? 'MUSTANG 12, HELLHOUND — STRICT NET: deviation in TENS, range in FIFTIES entering fire for effect. Recompute and say again, over.'
      : 'MUSTANG 12, HELLHOUND — STRICT NET: deviation in TENS, range in HUNDREDS. Fifties are for the correction that takes us into fire for effect, not this one. Recompute and say again, over.',
      { delay: 1 });
    return;
  }
  // dangerous deviation gate: a correction that walks the aimpoint onto
  // friendlies or a village is refused with a rant. Resending the identical
  // correction overrides it — scripts are guidelines, and it is his funeral.
  if (p.any) {
    const a = mission.aim;
    const otAz = azTo(OP.x, OP.z, a.x, a.z);
    const nx = a.x + Math.sin(otAz) * p.corr.add + Math.cos(otAz) * p.corr.right;
    const nz = a.z - Math.cos(otAz) * p.corr.add + Math.sin(otAz) * p.corr.right;
    let danger = null;
    const declaredDC = mission.gridStr.includes('DANGER CLOSE');
    if (!declaredDC) {
      for (const f of friendlyPositions())
        if (dist2(nx, nz, f.x, f.z) < (f.r || CONFIG.MISSION.fratricideRadius) + 50) {
          danger = 'the friendly position'; break;
        }
    }
    if (!danger) {
      for (const v of WORLD.villages)
        if (v.huts.some(hh => dist2(nx, nz, hh.x, hh.z) < 70)) { danger = 'the village'; break; }
    }
    const key = p.corr.right + ',' + p.corr.add;
    if (danger && mission.unsafeKey !== key) {
      mission.unsafeKey = key;
      FDC.say(pick(QUIPS.unsafeCorr).replace('{WHO}', danger), { delay: 1 });
      return;
    }
    if (danger) FDC.say(pick(QUIPS.unsafeInsist), { delay: 1 });
    mission.unsafeKey = null;
  }
  // timid correction: doctrine wants a bold shift that crosses the target,
  // not a fraction of the observed miss. Coach at decision time.
  if (p.any && coachOn() && !mission.coached.timid && mission.lastAdjDist !== null) {
    const corrMag = Math.hypot(p.corr.right, p.corr.add);
    if (mission.lastAdjDist > 150 && corrMag < mission.lastAdjDist * 0.45) {
      mission.coached.timid = true;
      FDC.say(pick(QUIPS.coachTimid).replace('{MISS}', Math.round(mission.lastAdjDist)),
              { delay: 1.6 });
    }
  }
  const parts = [];
  if (p.any) {
    mission.corrs.push(Math.round(Math.hypot(p.corr.right, p.corr.add)));
    if (p.corr.right) parts.push(`${p.corr.right > 0 ? 'RIGHT' : 'LEFT'} ${Math.abs(p.corr.right)}`);
    if (p.corr.add) parts.push(`${p.corr.add > 0 ? 'ADD' : 'DROP'} ${Math.abs(p.corr.add)}`);
    applyCorrection({ right: p.corr.right, add: p.corr.add });
  }
  if (p.ffe) parts.push('FIRE FOR EFFECT');
  FDC.say(parts.join(', ') + ', OUT.', { delay: 1 });
  // stupid-but-safe: un-doctrinal rounding gets a snide remark, then we fire.
  // Same doctrinal rule as the strict gate above — 50 m only entering fire for effect.
  if (p.any && (Math.abs(p.corr.right) % 10 || Math.abs(p.corr.add) % (p.ffe ? 50 : 100)) && Math.random() < 0.55)
    FDC.say(pick(QUIPS.snideRound), { delay: 1.3 });
  else if (Math.abs(p.corr.right) + Math.abs(p.corr.add) > 150 && Math.random() < 0.5)
    FDC.say(pick(QUIPS.corrSnark), { delay: 1 });
  if (p.ffe) fireForEffect();
  else fireAdjustRound();
}

function handleEOM(p) {
  if (!mission) { FDC.say(pick(QUIPS.noMission), { delay: 1 }); return; }
  // strict net: end of mission carries surveillance (RREMS terms)
  if (activeChapter && activeChapter.strict && !mission.done && !mission.eomChallenged &&
      !/(neutralized|destroyed|suppressed)/.test(p.bda || '')) {
    mission.eomChallenged = true;
    FDC.say('MUSTANG 12, HELLHOUND — STRICT NET: end of mission carries surveillance. NEUTRALIZED, DESTROYED, or SUPPRESSED. Say again with your assessment, over.', { delay: 1.1 });
    return;
  }
  if (!mission.done) { mission.done = true; if (!mission.tEnd) mission.tEnd = sim.now; }
  // RREMS: refinement moves the recorded mean point of impact onto the
  // adjusting point; "record as target" files it for future use
  if (p.refine && p.refine.any) {
    applyCorrection({ right: p.refine.right, add: p.refine.add });
    mission.refined = true;
    FDC.say(`REFINEMENT ${p.refine.right ? (p.refine.right > 0 ? 'RIGHT ' : 'LEFT ') + Math.abs(p.refine.right) : ''}` +
            `${p.refine.add ? (p.refine.right ? ', ' : '') + (p.refine.add > 0 ? 'ADD ' : 'DROP ') + Math.abs(p.refine.add) : ''}, OUT.`,
            { delay: 1.0 });
  }
  if (p.record) {
    mission.recorded = true;
    const tn = 'AB' + (7100 + (CONFIG.SEED.mission % 800));
    /* G27 — the target number now means something. It used to be announced and
       thrown away, so "suppress target AB7101" had nothing to look up and the
       whole suppress-by-number mission type was unreachable. Stored with the
       aimpoint as it stands at end of mission, which is after any refinement —
       which is exactly what recording a target is for. */
    RECTGT[tn] = { x: mission.aim.x, z: mission.aim.z, t: sim.now };
    FDC.say(`RECORDED AS TARGET ${tn}, OUT.`, { delay: 1.0 });
    log('', `Target ${tn} is on file at ${gridOf(mission.aim.x, mission.aim.z)}. ` +
      `You can re-engage it with "SUPPRESS TARGET ${tn}, 5 MINUTES, OVER".`, 'sys');
  }
  FDC.say(`END OF MISSION${p.bda ? ', ' + p.bda.toUpperCase() : ''}, OUT.`, { delay: 1.2 });
  if (mission.failReason !== 'fratricide' && mission.failReason !== 'collateral') {
    const good = Scenario.type === 'convoy'
      ? Scenario.veh.filter(v => v.dead).length >= 3
      : (mission.ffeRounds.length > 0 && mission.hits >= Scenario.hitsNeed);
    FDC.say(pick(good ? QUIPS.eomGood : QUIPS.eomBad), { delay: 1.2 });
  }
  setState('AAR');
  schedule(FDC.lastT + 1.6, showAAR);
}

function expectedHint() {
  switch (state) {
    case 'ADJUSTING':
      return (mission && !mission.otDirSent && mission.method === 'grid'
        ? 'Send OT DIRECTION first ("direction 5920, over") — then an OT-line correction, '
        : 'Send an OT-line correction, ') +
        'e.g. "right 50, add 100, over" — or "fire for effect, over" once rounds are on target. [R] in binos for the mil-relation card.';
    case 'EOM?':
      return 'Close with RREMS: "end of mission, target neutralized, over" — you may add a refinement ("left 10, drop 50") and "record as target" first.';
    case 'SHOT': case 'MISSION SENT': case 'FIRE FOR EFFECT':
      return 'Rounds are in progress — wait for SPLASH, observe the burst, then correct.';
    default:
      return 'Send a call for fire — grid: "adjust fire, grid 245523, infantry in the open, over" — or polar: "adjust fire, direction 4400, distance 2100, troops in the open, over".';
  }
}
function sysHint() { log('', 'HINT: ' + expectedHint(), 'sys'); }

let unknownStreak = 0;
function onPlayerMessage(raw) {
  squelch();
  log(CONFIG.FDC.obs, raw.toUpperCase(), 'obs');
  const p = parseMessage(raw);
  TLOG.add('parse', '', raw, { ptype: p.type, method: p.method || null,
    warno: p.warno || null, ffe: !!p.ffe });
  // tutorial intercept: steps may consume the message; noMission tutorials
  // keep the guns cold entirely
  if (TUT.steps) {
    if (tutEvent('msg', p)) return;
    if (TUT.noMission && (p.type === 'cff' || p.type === 'adjust' || p.type === 'repeat')) {
      log('', 'GUNNY BOTTLECAP: Guns are cold today, killer. Follow the drill.', 'sys');
      return;
    }
  }
  if (p.type !== 'unknown' && p.type !== 'empty') unknownStreak = 0;
  /* G22 — the three-transmission CFF. Every branch below is reached only by a
     PARTIAL transmission, which did not work at all before this row; a complete
     one-shot call skips all of it and takes its original path. Ordered before the
     switch because a queued call changes what a bare grid or a bare description
     MEANS, and that decision cannot be made inside handlers that never see the
     queue. */
  if (!(mission && !mission.done)) {
    // T1: a bare warning order opens a call.
    if (p.type === 'warno') { cffqStart(p.warno, p); return; }
    // T1 with FIRE FOR EFFECT. Bare "fire for effect" parses as `adjust` because
    // mid-mission it is the command to shift into effect; with no mission running
    // it can only be a warning order, and only here is that knowable.
    if (p.type === 'adjust' && p.ffe && !p.any) { cffqStart('ffe', p); return; }
    if (CFFQ.on) {
      // T2: a location with no warning order of its own, while a call is open.
      if (p.type === 'cff' && CFFQ.stage === 1 && p.warno === 'none') {
        if (p.desc) { cffqComplete(p.desc, p); return; }   // T2 and T3 in one, fine
        cffqLocation(p, locEchoOf(p));
        return;
      }
      // T3: the description. Anything unparseable becomes the target description
      // ONLY while a call is open and waiting for one — outside that it stays
      // `unknown`, so the forgiving-but-mocking default is untouched.
      if (CFFQ.stage === 2 && (p.type === 'unknown' || p.type === 'warno')) {
        // F7 — same cleaning as the one-shot paths: transmission 3 legitimately
        // carries DANGER CLOSE and fire-control prowords alongside the description,
        // and they must not end up read back twice.
        const d = descClean((p.raw || '').replace(/\b(over|out|break)\b/g, ''));
        // it parsed as `unknown` but it was a valid transmission, so it must not
        // leave a strike on the gibberish counter that feeds the FDC's rantLost
        if (d) { unknownStreak = 0; cffqComplete(d, p); return; }
      }
      // A complete call, or end of mission, or anything else deliberate abandons
      // the queue rather than merging into it.
      if (p.type === 'cff' || p.type === 'eom') cffqReset();
    }
  }
  /* G11 — a pending MTO readback claims otherwise-unhandled transmissions FIRST.
     Tested only for types that would otherwise be mocked or misread: `unknown`
     (the usual shape of a readback — "one gun in adjust, battery in effect, 155,
     target alpha alpha 7001" parses as nothing else) and `warno` (a verbatim
     readback contains the word "adjust", which G22's bare-warning-order branch
     would otherwise capture and misread as opening a NEW call for fire). Every
     other type keeps its meaning: a correction sent instead of a readback is
     still a correction — handleAdjust coaches the skipped readback there. */
  if ((p.type === 'unknown' || p.type === 'warno') && tryMTOReadback(p)) {
    unknownStreak = 0;
    return;
  }
  switch (p.type) {
    case 'empty': break;
    case 'sayagain':
      if (FDC.lastMsg) FDC.say(`I SAY AGAIN: ${FDC.lastMsg}`, { delay: 1 });
      break;
    case 'cff': handleCFF(p); break;
    case 'direction': handleDirection(p); break;
    case 'adjust': handleAdjust(p); break;
    case 'checkfire': handleCheckFiring(); break;
    case 'ceaseload': handleCeaseLoading(); break;
    case 'amc': handleAtMyCommand(p.on); break;
    case 'fire': handleFire(); break;
    case 'donotload': handleDoNotLoad(); break;
    case 'cannotobserve': handleCannotObserve(); break;
    case 'tot': handleTimeOnTarget(p.sec); break;
    case 'suppresstgt': handleSuppressTarget(p); break;
    case 'posrep': handlePosRep(p); break;
    case 'otfactor':
      // G7 — snide, not a rant: it is a harmless misunderstanding, not a hazard
      FDC.say('MUSTANG, the OT factor is YOUR arithmetic — it turns your mils into meters. ' +
              'I do not want it. Send me the RESULT: left or right, add or drop. ' +
              'What I do need on a grid mission is your OT DIRECTION, over.', { delay: 1.1 });
      if (mission && !mission.done)
        mission.notes.push('Transmitted the OT factor. It is observer-side arithmetic; only OT DIRECTION goes to the FDC (grid missions).');
      break;
    case 'repeat':
      if (mission && !mission.done && state === 'ADJUSTING') {
        FDC.say('REPEAT, OUT.', { delay: 1 }); fireAdjustRound();
      } else { FDC.say(pick(QUIPS.noMission), { delay: 1 }); sysHint(); }
      break;
    case 'eom': handleEOM(p); break;
    default:
      unknownStreak++;
      if (unknownStreak >= 3) {
        unknownStreak = 0;
        FDC.say(pick(QUIPS.rantLost), { delay: 1 });
      } else {
        FDC.say(pick(QUIPS.unknown), { delay: 1 });
      }
      sysHint();
  }
}

