/* ============================================================ AAR SHOT PLOT */
// Bird's-eye 3D review of the engagement: the real terrain rendered from an
// oblique camera placed behind the OT line (so plot-left/right is deviation
// and plot-up/down is range, exactly as the observer called it), with the
// transmitted location, each adjusting round, and the FFE volley projected
// onto it through the same camera.
const _plotCam = new THREE.PerspectiveCamera(42, 16 / 10, 5, 30000);
const _pv = new THREE.Vector3();
function renderShotPlot(cv, m) {
  const S = Scenario;
  if (!S || !S.enemy) return false;
  const W = cv.width, Hh = cv.height;
  const ctx = cv.getContext('2d');
  const tgt = { x: S.enemy.x, z: S.enemy.z };
  const pts = [tgt, { x: m.aim0x, z: m.aim0z }, ...m.rounds];

  // frame everything, in OT-frame coordinates (u = deviation, v = range)
  const otAz = azTo(OP.x, OP.z, tgt.x, tgt.z);
  const fx = Math.sin(otAz), fz = -Math.cos(otAz);      // toward target (range)
  const rx = Math.cos(otAz), rz = Math.sin(otAz);       // observer's right
  let uMin = 0, uMax = 0, vMin = 0, vMax = 0;
  for (const p of pts) {
    if (!p || p.x === undefined) continue;
    const dx = p.x - tgt.x, dz = p.z - tgt.z;
    const u = dx * rx + dz * rz, v = dx * fx + dz * fz;
    uMin = Math.min(uMin, u); uMax = Math.max(uMax, u);
    vMin = Math.min(vMin, v); vMax = Math.max(vMax, v);
  }
  const pad = 120;
  const halfU = Math.max(220, (uMax - uMin) / 2 + pad);
  const halfV = Math.max(220, (vMax - vMin) / 2 + pad);
  const cU = (uMin + uMax) / 2, cV = (vMin + vMax) / 2;
  const cx = tgt.x + rx * cU + fx * cV, cz = tgt.z + rz * cU + fz * cV;
  // ~42° above horizontal: high enough to read the plot as a plan, oblique
  // enough that terrain relief still shows. Auto-fit so no marker is clipped —
  // perspective pushes near-edge points out, so widen until everything sits
  // inside the frame with margin.
  let span = Math.max(halfU * 1.6, halfV * 1.25);
  const groundY = Math.max(H(cx, cz), 0);
  _plotCam.aspect = W / Hh;
  _plotCam.up.set(0, 1, 0);
  const place = () => {
    _plotCam.position.set(cx - fx * span * 1.15, groundY + Math.max(260, span * 1.05),
                          cz - fz * span * 1.15);
    _plotCam.lookAt(cx, groundY, cz);
    _plotCam.updateProjectionMatrix();
    _plotCam.updateMatrixWorld(true);
  };
  place();
  for (let fit = 0; fit < 6; fit++) {
    let worst = 0;
    for (const p of pts) {
      if (!p || p.x === undefined) continue;
      _pv.set(p.x, Math.max(H(p.x, p.z), 0) + 1, p.z).project(_plotCam);
      worst = Math.max(worst, Math.abs(_pv.x), Math.abs(_pv.y));
    }
    if (worst <= 0.86) break;         // comfortably inside the frame
    span *= Math.min(1.5, worst / 0.8);
    place();
  }

  // render the live scene from that camera into the plot canvas
  const hidden = [];
  for (const b of bursts) if (b.group.visible) { b.group.visible = false; hidden.push(b.group); }
  const oldRatio = renderer.getPixelRatio();
  try {
    renderer.setPixelRatio(1);
    renderer.setSize(W, Hh, false);          // buffer only — CSS size untouched
    // size figures for the PLOT camera's range, not the observer's, or a
    // close bird's-eye frame shows 2.5x giants
    legibilityPass(_plotCam.position.x, _plotCam.position.y, _plotCam.position.z);
    // the shot plot is an after-action document, not a live sight picture — force
    // day materials/lighting for it and put the observer's optic back afterwards
    const _visWas = VISION.mode;
    if (_visWas !== 'day') applyVision('day');
    renderer.render(scene, _plotCam);
    if (_visWas !== 'day') applyVision();
    ctx.clearRect(0, 0, W, Hh);
    ctx.drawImage(renderer.domElement, 0, 0, W, Hh);
  } catch (e) {
    return false;
  } finally {
    // restore to the canonical viewport; nothing is composited mid-swap
    // because this all runs inside one task
    renderer.setPixelRatio(oldRatio);
    renderer.setSize(innerWidth, innerHeight, false);
    for (const g of hidden) g.visible = true;
  }

  // --- project world -> plot canvas
  const proj = (x, z, lift) => {
    _pv.set(x, Math.max(H(x, z), 0) + (lift || 0), z).project(_plotCam);
    return { x: (_pv.x * 0.5 + 0.5) * W, y: (-_pv.y * 0.5 + 0.5) * Hh, z: _pv.z };
  };
  const groundCircle = (wx, wz, r, stroke, dash) => {
    ctx.strokeStyle = stroke; ctx.lineWidth = 1.6;
    ctx.setLineDash(dash || []);
    ctx.beginPath();
    for (let i = 0; i <= 40; i++) {
      const a = i / 40 * Math.PI * 2;
      const p = proj(wx + Math.cos(a) * r, wz + Math.sin(a) * r, 0.5);
      i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y);
    }
    ctx.closePath(); ctx.stroke();
    ctx.setLineDash([]);
  };
  // the canvas is CSS-downscaled in the panel, so draw oversized
  const label = (txt, x, y, col) => {
    ctx.font = 'bold 16px Consolas, monospace';
    ctx.textAlign = 'left';
    ctx.lineWidth = 4; ctx.strokeStyle = 'rgba(8,10,6,.9)';
    ctx.strokeText(txt, x, y);
    ctx.fillStyle = col; ctx.fillText(txt, x, y);
  };

  // effect envelope (G13 — the outer band; rounds inside it contributed) + target
  groundCircle(tgt.x, tgt.z, S.type === 'convoy' ? 40 : effBands().rSupp,
               'rgba(255,120,90,.9)', [7, 5]);
  const tp = proj(tgt.x, tgt.z, 1);
  ctx.strokeStyle = '#ff7a5a'; ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.moveTo(tp.x - 13, tp.y); ctx.lineTo(tp.x + 13, tp.y);
  ctx.moveTo(tp.x, tp.y - 13); ctx.lineTo(tp.x, tp.y + 13);
  ctx.stroke();
  label('TARGET', tp.x + 16, tp.y + 5, '#ff9a7a');

  // no-fire elements in frame
  for (const f of friendlyPositions()) {
    const p = proj(f.x, f.z, 1);
    if (p.z > 1) continue;
    ctx.fillStyle = '#6aa7ff';
    ctx.fillRect(p.x - 4, p.y - 4, 8, 8);
  }
  if (S.village) {
    const p = proj(S.village.x, S.village.z, 1);
    if (p.z < 1) label('VILLAGE (NO-STRIKE)', p.x + 8, p.y + 4, '#e0c07a');
  }

  // transmitted location (what the observer sent) + error leg to truth
  if (m.aim0x !== undefined) {
    const ap = proj(m.aim0x, m.aim0z, 1);
    ctx.strokeStyle = 'rgba(255,233,168,.75)'; ctx.lineWidth = 1.4;
    ctx.setLineDash([4, 4]);
    ctx.beginPath(); ctx.moveTo(ap.x, ap.y); ctx.lineTo(tp.x, tp.y); ctx.stroke();
    ctx.setLineDash([]);
    ctx.strokeStyle = '#ffe9a8'; ctx.lineWidth = 2.6;
    ctx.strokeRect(ap.x - 9, ap.y - 9, 18, 18);
    label(`SENT${m.aimErr0 !== null ? ' (' + Math.round(m.aimErr0) + ' m off)' : ''}`,
          ap.x + 14, ap.y - 9, '#ffe9a8');
  }

  // adjusting rounds: numbered, joined in sequence
  const adj = m.rounds.filter(r => !m.ffeRounds.includes(r));
  ctx.strokeStyle = 'rgba(160,200,255,.85)'; ctx.lineWidth = 2.2;
  ctx.beginPath();
  adj.forEach((r, i) => {
    const p = proj(r.x, r.z, 1);
    i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y);
  });
  ctx.stroke();
  adj.forEach((r, i) => {
    const p = proj(r.x, r.z, 1);
    ctx.fillStyle = '#0e1a2a'; ctx.strokeStyle = '#8fc4ff'; ctx.lineWidth = 2.4;
    ctx.beginPath(); ctx.arc(p.x, p.y, 12, 0, Math.PI * 2);
    ctx.fill(); ctx.stroke();
    ctx.fillStyle = '#cfe4ff';
    ctx.font = 'bold 15px Consolas, monospace';
    ctx.textAlign = 'center';
    ctx.fillText(String(i + 1), p.x, p.y + 5.5);
  });

  // FFE volley: filled diamonds
  m.ffeRounds.forEach(r => {
    const p = proj(r.x, r.z, 1);
    ctx.fillStyle = '#ffcf5a'; ctx.strokeStyle = '#7a5a10'; ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(p.x, p.y - 9); ctx.lineTo(p.x + 9, p.y);
    ctx.lineTo(p.x, p.y + 9); ctx.lineTo(p.x - 9, p.y);
    ctx.closePath(); ctx.fill(); ctx.stroke();
  });

  // OT line arrow at the near edge — reminds which way "add" and "right" run
  ctx.strokeStyle = 'rgba(220,230,200,.55)'; ctx.lineWidth = 1.2;
  ctx.setLineDash([6, 5]);
  const l0 = proj(tgt.x - fx * span * 0.9, tgt.z - fz * span * 0.9, 1);
  ctx.beginPath(); ctx.moveTo(l0.x, l0.y); ctx.lineTo(tp.x, tp.y); ctx.stroke();
  ctx.setLineDash([]);
  label('OT LINE — ADD ↑ / DROP ↓', l0.x + 12, l0.y, 'rgba(220,230,200,.9)');

  // legend
  ctx.textAlign = 'left';
  const ly = Hh - 16;
  ctx.fillStyle = 'rgba(8,10,6,.78)';
  ctx.fillRect(8, Hh - 44, 470, 34);
  ctx.strokeStyle = '#8fc4ff'; ctx.lineWidth = 2.2;
  ctx.beginPath(); ctx.arc(26, ly - 5, 9, 0, Math.PI * 2); ctx.stroke();
  label('ADJUST (in order)', 42, ly, '#cfe4ff');
  ctx.fillStyle = '#ffcf5a';
  ctx.beginPath();
  ctx.moveTo(232, ly - 14); ctx.lineTo(241, ly - 5);
  ctx.lineTo(232, ly + 4); ctx.lineTo(223, ly - 5);
  ctx.closePath(); ctx.fill();
  label('FIRE FOR EFFECT', 250, ly, '#ffcf5a');
  return true;
}

/* ============================================================ AAR */
const aarEl = document.getElementById('aar');
const aarBox = document.getElementById('aarbox');
// best scores per scenario+difficulty; localStorage when available (in-memory otherwise)
const BEST = {
  data: {},
  load() {
    try { this.data = JSON.parse(localStorage.getItem('shitfire_best') || '{}'); }
    catch (e) { this.data = {}; }
  },
  save() {
    try { localStorage.setItem('shitfire_best', JSON.stringify(this.data)); }
    catch (e) { /* in-memory only (e.g. artifact sandbox) */ }
  },
};
BEST.load();
function fmtTime(s) {
  const m = Math.floor(s / 60), r = Math.floor(s % 60);
  return `${m}:${String(r).padStart(2, '0')}`;
}
function showAAR() {
  const m = mission;
  if (!m) return;
  const M = CONFIG.MISSION;
  const S = Scenario;
  /* TEMPO4 — the reqIllum shortfall can only be known at the end (the illum
     might legally have come at any point in the mission), so the note lands
     here, once, before grading reads m.notes. */
  if (activeChapter && activeChapter.reqIllum && !m.usedIllum && !m.reqIllumNoted) {
    m.reqIllumNoted = true;
    m.notes.push('This chapter demanded COORDINATED ILLUMINATION — you fought it dark. Effective, maybe, but the skill under test was the light (chapter capped at 2★).');
  }
  /* ENEMY1 — same shape as the reqIllum note: casualties are only countable at
     the end, land once, before grading reads them. Cost, never a fail. */
  if (S.btyCas && !m.btyCasNoted) {
    m.btyCasNoted = true;
    m.notes.push(`The battery put ${S.btyCas} round${S.btyCas === 1 ? '' : 's'} inside the friendly position while you worked. ` +
      'Friendly casualties taken under enemy fire cost you a star per two rounds — silence the gun faster.');
  }
  const isConvoy = S.type === 'convoy';
  if (isConvoy) m.hits = S.veh.filter(v => v.dead).length;
  /* G13 — the verdict comes off the graded effect, and it distinguishes the
     three doctrinal outcomes. SUPPRESSED is a pass only when suppression was
     the mission's intent; on a destroy mission it is a marginal — real effect,
     wrong amount, and it wears off. */
  const a = assessEffect();
  const accomplished = (m.intent === 'illum' && m.rounds.length > 0) ||   // 12h
                       a.outcome === 'destroyed' || a.outcome === 'neutralized' ||
                       (a.outcome === 'suppressed' && m.intent === 'suppress');
  const bands = isConvoy ? null : effBands();
  const frat = m.failReason === 'fratricide';
  const collat = m.failReason === 'collateral';
  const escaped = m.failReason === 'escaped';
  const overrun = m.failReason === 'overrun';   // ENEMY2
  const pass = !frat && !collat && !escaped && !overrun && accomplished && m.adjustRounds <= M.passMaxAdjustRounds;
  const grade = ['A', 'B', 'C', 'D'][Math.min(m.notes.length, 3)];
  const dur = (m.tEnd || sim.now) - m.tStart;
  let verdict, why = '';
  if (frat) { verdict = 'FAIL — FRATRICIDE'; why = 'Rounds impacted a friendly element. Automatic failure.'; }
  else if (collat) { verdict = 'FAIL — CIVILIAN CASUALTIES'; why = 'Rounds impacted a civilian village. Collateral damage is an automatic failure, same as fratricide.'; }
  else if (escaped) {
    verdict = S.type === 'kaiju' ? 'FAIL — LANDFALL' : 'FAIL — CONVOY ESCAPED';
    why = S.type === 'kaiju'
      ? 'The crab reached the village. The report will describe it as "weather".'
      : 'Fewer than three vehicles destroyed before the column ran off the map.';
  }
  else if (overrun) {   // ENEMY2 — the objective was you
    verdict = 'FAIL — POSITION OVERRUN';
    why = 'The assault reached the tower before you broke it. The last correction you never sent was the one that mattered.';
  }
  else if (S.type === 'kaiju' && a.outcome !== 'destroyed') {   // 11b — walking or not
    verdict = 'FAIL — IT IS STILL WALKING';
    why = `Assessed effect ${a.pct}% of the ${bands.destroyPct}% that stops it. It does not do "combat-ineffective". It is a crab.`;
  }
  else if (m.intent === 'illum') {   // 12h — an illumination mission is graded on light, not casualties
    verdict = m.rounds.length ? 'PASS — ILLUMINATION PROVIDED' : 'FAIL — NO ILLUMINATION FIRED';
    why = m.rounds.length ? `${m.rounds.length} illumination round${m.rounds.length === 1 ? '' : 's'} over the area.`
                          : 'Mission closed without a round in the air.';
  }
  else if (a.outcome === 'none') {
    verdict = isConvoy ? 'FAIL — CONVOY STILL EFFECTIVE' : 'FAIL — TARGET STILL EFFECTIVE';
    why = isConvoy
      ? `Only ${m.hits}/4 vehicles destroyed (need 3).`
      : (m.rounds.length
          ? `Assessed effect ${a.pct}% — below the ${bands.neutralizePct}% that renders a unit combat-ineffective (FM 6-30). The fires never touched them.`
          : 'Mission ended without a round fired.');
  }
  else if (a.outcome === 'suppressed' && !accomplished) {
    verdict = 'MARGINAL — TARGET SUPPRESSED ONLY';
    why = `Heads went down while the rounds fell (${a.pct}% casualties — neutralization needs ${bands.neutralizePct}%), and suppression wears off. They are back on their guns. You had the option to continue: correction, REPEAT.`;
  }
  else if (a.outcome === 'suppressed') { verdict = 'PASS — TARGET SUPPRESSED'; why = 'Suppression was the mission. They stopped shooting while it mattered.'; }
  else if (!pass) { verdict = 'MARGINAL — TOO MANY ADJUSTING ROUNDS'; why = `Target ${a.outcome}, but ${m.adjustRounds} adjusting rounds used (pass ≤ ${M.passMaxAdjustRounds}).`; }
  else if (a.outcome === 'destroyed') { verdict = isConvoy ? 'PASS — CONVOY DESTROYED' : 'PASS — TARGET DESTROYED'; why = isConvoy ? '' : `Assessed ${a.pct}% casualties — past the ${bands.destroyPct}% that takes a unit off the books permanently.`; }
  else { verdict = 'PASS — TARGET NEUTRALIZED'; why = `Assessed ${a.pct}% casualties — combat-ineffective (≥${bands.neutralizePct}%), short of destruction (${bands.destroyPct}%).`; }
  if (pass) {
    const key = S.type + ':' + S.difficulty;
    const prev = BEST.data[key];
    if (!prev || m.adjustRounds < prev.rounds ||
        (m.adjustRounds === prev.rounds && dur < prev.time)) {
      BEST.data[key] = { rounds: m.adjustRounds, time: Math.round(dur) };
      BEST.save();
    }
  }
  // adjustment-doctrine diagnosis: what the round trace says about technique
  const tr = m.missTrace;
  const corrEff = correctionEfficiency(m);
  const diag = [];
  if (m.tInit > 120)
    diag.push(`Call for fire took ${fmtTime(m.tInit)} to initiate (standard: within 2:00 of identifying the target). Locate faster — the target is not waiting for you.`);
  if (m.aimErr0 !== null && m.aimErr0 > 200)
    diag.push(`Initial target location was ${Math.round(m.aimErr0)} m off (standard: ≤200 m). Everything after that was recovery — work the map, or lase and read the grid, before you transmit.`);
  if (corrEff !== null && corrEff < 0.3 && tr.length >= 3)
    diag.push(`Correction efficiency ${Math.round(corrEff * 100)}% per round — doctrine's successive bracketing roughly halves the miss each time (~50%). Your corrections were too small for the error you were seeing.`);
  if (tr.length >= 3 && !(m.sides.long && m.sides.short))
    diag.push('You never bracketed the target — every adjusting round landed on the same side of it. Fire a correction big enough to cross the target, then split the difference.');
  if (m.wasted >= 2)
    diag.push(`${m.wasted} adjusting rounds changed the miss by less than 15% — rounds and time spent for nothing.`);
  if (!diag.length && pass && m.adjustRounds <= 3)
    diag.push('Clean adjustment: bold first correction, bracket, split, effect. That is the method.');
  // career record (feeds HELLHOUND's continuity quips)
  const car = CAMP.data._career || (CAMP.data._career = { frat: 0, collat: 0, missions: 0 });
  car.missions++;
  if (frat) car.frat++;
  if (collat) car.collat++;
  CAMP.save();
  // commendations: funny medals for feats worth bragging about
  const parC = (activeChapter && activeChapter.par) || 300;
  const medals = [];
  if (pass) {
    if (m.adjustRounds <= 1) medals.push('HELL OF A HOLE IN ONE — effect after a single adjusting round');
    if (m.aimErr0 !== null && m.aimErr0 <= 50) medals.push('EAGLE EYE — initial location within 50 m');
    if (m.notes.length === 0) medals.push('ACTUALLY READ THE MANUAL — clean call format, no coaching');
    if (dur <= parC * 0.6) medals.push('FAST MOVER — done in well under par');
    if (activeChapter && activeChapter.strict) medals.push('IRON NET — passed under strict doctrine');
    if (m.refined || m.recorded) medals.push('BY THE BOOK — closed the mission with refinement / record as target (RREMS)');
    if (m.otDirSent) medals.push('ORIENTED — OT direction passed to the FDC');
    if (activeChapter && activeChapter.asset === 'mortar60') medals.push('TEN-METER MAN — precision work with the sixty');
  }
  TLOG.add('aar', '', verdict, { rounds: m.adjustRounds, dur: Math.round(dur),
    // G13 — graded outcome fields (additive; existing entry fields unchanged)
    outcome: a.outcome, effPct: a.pct, intent: m.intent,
    ...(m.sheaf ? { sheaf: m.sheaf.kind, sheafSrc: m.sheaf.source } : {}),
    ...(m.fuze ? { fuze: m.fuze.kind, fuzeSrc: m.fuze.source } : {}),
    ...(m.shell && m.shell !== 'he' ? { shell: m.shell } : {}),
    ...(m.bdaClaim ? { bdaClaim: m.bdaClaim } : {}),
    firstMiss: m.firstMiss === null ? null : Math.round(m.firstMiss),
    aimErr0: m.aimErr0 === null ? null : Math.round(m.aimErr0),
    tInit: Math.round(m.tInit),
    corrEff: corrEff === null ? null : Math.round(corrEff * 100),
    bracketed: !!(m.sides.long && m.sides.short),
    trace: m.missTrace.slice(),
    notes: m.notes.length,
    ...(m.noDisp ? { noDisp: true } : {}),
    ...(activeChapter && !m.noDisp ? { stars: gradeMission(m) } : {}) });
  // campaign star grading (MW2 style): grade, persist best, show in AAR
  let starsHtml = '';
  if (activeChapter && m.noDisp) {
    /* G3 — a mission flown without dispersion is not a measurement of anything,
       so it does not touch the campaign record. Deliberately does NOT record zero
       stars either: that would destroy a previous honest result on this chapter,
       punishing the user for using a test aid the trainer offers them. */
    starsHtml = '<div style="margin-bottom:8px;color:#ffb3a0">NOT GRADED — round dispersion was ' +
      'disabled (SHIFT+D) during this mission. No stars recorded; your previous best for this ' +
      'chapter is untouched.</div>';
  } else if (activeChapter) {
    const s = gradeMission(m);
    CAMP.record(activeChapter.id, S.difficulty, s);
    const cap = { easy: 3, normal: 4, hard: 5 }[S.difficulty];
    starsHtml = `<div class="aarstars">${starStr(s)}</div>` +
      (s > 0 && s === cap && cap < 5
        ? `<div style="margin-bottom:8px;color:#9fb08c">Capped at ${cap}★ on ${S.difficulty.toUpperCase()} — full five need HARD.</div>` : '');
  }
  aarBox.innerHTML =
    `<h2>AAR — ${activeChapter
      ? `CHAPTER ${activeChapter.id}: ${activeChapter.title}`
      : SCN_META[S.type].name} (${S.difficulty.toUpperCase()})</h2>` +
    starsHtml +
    `<div class="verdict ${pass ? 'pass' : 'fail'}">${verdict}</div>` +
    (why ? `<div style="margin-bottom:10px">${why}</div>` : '') +
    `<canvas id="aarplot" width="880" height="550"></canvas>` +
    `<div class="plotcap">Bird's-eye review, oriented along the observer-target line — plot left/right is deviation, up/down is range.</div>` +
    `<table>` +
    `<tr><td>Adjusting rounds</td><td>${m.adjustRounds}</td></tr>` +
    `<tr><td>First-round miss</td><td>${m.firstMiss === null ? '—' : Math.round(m.firstMiss) + ' m'}</td></tr>` +
    `<tr><td>Initial location error</td><td>${m.aimErr0 === null ? '—' : Math.round(m.aimErr0) + ' m (JFO standard ≤ 200 m)'}</td></tr>` +
    (isConvoy
      ? `<tr><td>Vehicles destroyed</td><td>${m.hits}/4</td></tr>` +
        /* SUGG6 — the intercept verdict: where the first effect round landed
           relative to the live column head, signed along the road. */
        (m.convoyLead !== undefined
          ? `<tr><td>Intercept lead</td><td>${m.convoyLead >= 0
              ? m.convoyLead + ' m AHEAD of the lead vehicle — ' + (m.convoyLead <= 120 ? 'a working trigger point' : 'over-led; the column drove through the smoke, not the steel')
              : Math.abs(m.convoyLead) + ' m BEHIND the lead vehicle — fired late; work the trigger-point math (speed × time of flight)'}</td></tr>`
          : '')
      : `<tr><td>Assessed effect</td><td>${a.pct}% ${S.tgtClass === 'point' ? 'structural damage' : 'casualties'} — ${a.outcome === 'none' ? 'NO EFFECT' : a.outcome.toUpperCase()} (neutralize ≥${bands.neutralizePct}%, destroy ≥${bands.destroyPct}%${S.posture === 'prone' ? '; target went prone — per-round effect ×' + CONFIG.EFFECTS.posture.prone : ''})</td></tr>` +
        `<tr><td>Effect rounds in the outer band</td><td>${m.hits}/${m.ffeRounds.length} within ${Math.round(bands.rSupp)} m</td></tr>`) +
    (m.bdaClaim ? `<tr><td>Surveillance sent</td><td>${m.bdaClaim.toUpperCase()}${m.bdaClaim === a.outcome ? ' — matches the assessment' : ` — assessed: ${a.outcome === 'none' ? 'NO EFFECT' : a.outcome.toUpperCase()}`}</td></tr>` : '') +
    (m.sheaf ? `<tr><td>Sheaf</td><td>${m.sheaf.kind.toUpperCase()} (${m.sheaf.source}) — ${m.sheaf.why}</td></tr>` : '') +
    (m.fuze ? `<tr><td>Fuze</td><td>${m.fuze.kind.toUpperCase()} (${m.fuze.source}) — ${m.fuze.why}</td></tr>` : '') +
    (m.shell && m.shell !== 'he' ? `<tr><td>Shell</td><td>${m.shell === 'illum' ? 'ILLUMINATION — light, not fires' : 'SMOKE — a screen, not casualties'}</td></tr>` : '') +
    `<tr><td>Time to initiate CFF</td><td>${fmtTime(m.tInit)} (standard ≤ 2:00)</td></tr>` +
    `<tr><td>Mission time</td><td>${fmtTime(dur)}</td></tr>` +
    `<tr><td>Call format grade</td><td>${grade}</td></tr>` +
    (m.noDisp ? `<tr><td>Round dispersion</td><td>DISABLED — test mode, results not representative</td></tr>` : '') +
    (m.usedCheckFire ? `<tr><td>Safety stop</td><td>CHECK FIRING / CEASE LOADING called during this mission</td></tr>` : '') +
    (m.mto ? `<tr><td>MTO readback</td><td>${m.mto.read ? 'READ BACK' : 'NOT read back — the FDC never got confirmation you copied the target number'}</td></tr>` : '') +
    (tr.length ? `<tr><td>Adjusting round trace</td><td>${tr.join(' → ')} m</td></tr>` : '') +
    (corrEff !== null ? `<tr><td>Correction efficiency</td><td>${Math.round(corrEff * 100)}% miss reduction/round (doctrine ≈50%)</td></tr>` : '') +
    (tr.length >= 2 ? `<tr><td>Bracket established</td><td>${m.sides.long && m.sides.short ? 'YES' : 'NO — all rounds one side'}</td></tr>` : '') +
    `</table>` +
    (diag.length ? `<div class="mlabel" style="margin-top:8px">ADJUSTMENT DIAGNOSIS</div><ul>${diag.map(n => `<li>${n}</li>`).join('')}</ul>` : '') +
    (m.notes.length ? `<div>Format coaching:</div><ul>${m.notes.map(n => `<li>${n}</li>`).join('')}</ul>` : '') +
    (medals.length ? `<div class="mlabel" style="margin-top:8px">COMMENDATIONS</div><ul>${medals.map(n => `<li>${n}</li>`).join('')}</ul>` : '') +
    (activeChapter && activeChapter.outro && pass
      ? `<div style="font-style:italic;color:#9fb08c;margin:8px 0">${activeChapter.outro}</div>` : '') +
    (activeChapter ? `<div style="margin-bottom:6px;color:#9fb08c">Replay from the mission menu [K] to improve your stars.</div>` : '') +
    `<div class="foot">[N] — skirmish mission &middot; [K] — mission menu &middot; [ESC] — back to the OP &middot; seed ${CONFIG.SEED.mission}</div>`;
  if (document.exitPointerLock) document.exitPointerLock();
  aarEl.classList.add('on');
  // draw the shot plot into the freshly written canvas; drop it if the
  // engagement has nothing to show or the render fails
  const plotCv = document.getElementById('aarplot');
  if (plotCv) {
    let ok = false;
    try { ok = m.rounds.length > 0 && renderShotPlot(plotCv, m); }
    catch (e) { ok = false; }
    if (!ok) {
      plotCv.remove();
      const cap = aarBox.querySelector('.plotcap');
      if (cap) cap.remove();
    }
  }
  setState('AAR');
}

