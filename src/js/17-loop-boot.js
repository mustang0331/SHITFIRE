/* ============================================================ MAIN LOOP */
const headingEl = document.getElementById('heading');
const msnclockEl = document.getElementById('msnclock');   // TEMPO3
let lastClockTxt = '';
const binoHdgEl = document.getElementById('binohdg');
/* Adaptive quality: a frame-time EMA steps pixel ratio down before anything else.
   The renderer is FILL-RATE bound — measured at `frame_ms = 5.57 * MPix + 1.46`
   (tools/perfsweep.js), i.e. 95% of the frame is resolution-dependent against only
   57 draw calls and 283k triangles. Pixel ratio is therefore the single effective
   performance lever in this app; geometry is not worth optimising.

   PERF1 — binoculars set a quality FLOOR, they no longer PIN to tier 0.

   13a/§G0.4 originally pinned tier 0 while glassing, and disabled the stepper
   outright, to stop a step-down making a target unresolvable. The reasoning was
   sound when it shipped and its premise has since been superseded: E1 added the
   angular-size floor (CONFIG.GFX.legFloorMil 1.8), which scales a figure until it
   subtends at least 1.8 mils AT ANY RANGE — about 12.2 CSS px at 1080p. Figure
   legibility no longer degrades with range the way 13a assumed, so it no longer
   needs the top tier to defend it; it needs a floor under it.

   What the pin cost: on a high-DPI display `Math.min(devicePixelRatio, 1.75)`
   resolves to 1.75, so raising binos took the drawing buffer from 4.06 to
   6.35 MPix (+56% fill) AND switched off the only relief valve — measured
   49 fps -> 26 fps, every frame over budget. That is the reported lag, and
   looking around is simply when a 38 ms frame becomes impossible to miss.

   binoFloor is a TIER INDEX, and lower index = higher quality, so the floor is a
   Math.min ceiling on the index. Tier 3 (0.85) stays unreachable while glassing —
   that is the tier 13a's own comment warned about. At the floor, tier 2 (1.1), a
   figure is ~13.4 device px tall: comfortably countable, versus the ~2 px that
   motivated the pin. On an ordinary devicePixelRatio-1 display the whole thing is
   inert, since min(1, anything >= 1) is 1 — this only ever engages where it is
   actually needed. The stepped tier is retained, not reset, and re-applies on exit. */
const QUALITY = { tiers: [1.75, 1.4, 1.1, 0.85], tier: 0, ema: 0.016, lastChange: 0,
                  binoFloor: 2, frozen: false };   // frozen: QA only, see window.SHITFIRE.perf
function qTierNow() { return binos ? Math.min(QUALITY.tier, QUALITY.binoFloor) : QUALITY.tier; }
/* The ratio a tier ACTUALLY renders at. The clamp against devicePixelRatio is
   what makes tiers collapse into each other — see qStep. */
function qRatio(tier) { return Math.min(devicePixelRatio, QUALITY.tiers[tier]); }
/* PERF1 — step to the next tier that CHANGES SOMETHING, skipping degenerate ones.
   The tier list is authored as absolute pixel ratios but every one is clamped by
   devicePixelRatio, so on real displays whole runs of tiers render identically:

     dPR 1.0   -> 1.00 1.00 1.00 0.85   tiers 0,1,2 identical: 2 real levels
     dPR 1.25  -> 1.25 1.25 1.10 0.85   tiers 0,1 identical:   3 real levels
     dPR 2.0   -> 1.75 1.40 1.10 0.85   all distinct:          4 real levels

   Without this, a step-down on a 1.25 display moves tier 0 -> 1 and changes the
   resolution by NOTHING, while still paying a full drawing-buffer reallocation
   (three's setPixelRatio calls setSize) plus another 2.5 s cooldown before the
   controller is allowed to try again. On a plain 1.0 display it burns two such
   no-op steps before reaching the only tier that does anything. That is the
   adaptive system failing to adapt on the two most common DPI settings there
   are, which is most of why it never visibly helped. */
function qStep(from, dir, worst) {
  const r0 = qRatio(from);
  for (let n = from + dir; n >= 0 && n <= worst; n += dir)
    if (dir > 0 ? qRatio(n) < r0 : qRatio(n) > r0) return n;
  return from;   // nothing distinct in that direction — leave the tier alone
}
function applyQuality(t) {
  QUALITY.lastChange = t;
  renderer.setPixelRatio(qRatio(qTierNow()));
}
let lastFrame = 0, lastHdg = -1;
function animate(tMs) {
  requestAnimationFrame(animate);
  const t = tMs / 1000;
  const dt = Math.min(0.05, t - lastFrame);
  lastFrame = t;
  sim.now = t;
  // 13g — three scalar writes into preallocated uniforms; no allocation
  if (CONFIG.GFX.water) {
    WATER_U.uTime.value = t;
    WATER_U.uSun.value.copy(sun.position).normalize();
    WATER_U.uSunI.value = sun.intensity;
  }
  runEvents();
  pollGamepad(dt);
  QUALITY.ema = QUALITY.ema * 0.94 + dt * 0.06;
  /* PERF1 — the stepper now runs while glassing too, bounded by binoFloor. It
     used to be gated on `!binos`, which meant the observer spent the most
     expensive state in the app with no way out of it.

     It steps from qTierNow(), the tier actually being RENDERED, not from the
     stored one. Those two diverge while glassing (stored 0, rendered 2), and
     stepping the stored tier off a frame time earned at the rendered tier makes
     the controller oscillate: good frame time -> raise stored tier -> the floor
     stops hiding it -> bad frame time -> lower it -> repeat. Measured, that cost
     a 117 ms hitch every cycle, because three's setPixelRatio() calls setSize()
     and reallocates the drawing buffer. Deriving from the rendered tier keeps
     the controller measuring and adjusting the same quantity. */
  if (!QUALITY.frozen && t - QUALITY.lastChange > 2.5) {
    const worst = binos ? QUALITY.binoFloor : QUALITY.tiers.length - 1;
    const cur = qTierNow();
    const next = QUALITY.ema > 0.024 ? qStep(cur, 1, worst)
               : QUALITY.ema < 0.013 ? qStep(cur, -1, worst) : cur;
    if (next !== cur) { QUALITY.tier = next; applyQuality(t); }
  }
  // idle coaching: nudge once if the net has gone quiet mid-mission
  if (!stateHinted) {
    const idle = sim.now - stateSince;
    if ((state === 'ADJUSTING' && idle > 25) ||
        (state === 'EOM?' && idle > 15) ||
        (state === 'OBSERVING' && idle > 60)) {
      stateHinted = true;
      sysHint();
    }
  }
  // camera
  lookDir(_dir);
  _tgt.set(eye.x + _dir.x, eye.y + _dir.y, eye.z + _dir.z);
  camera.lookAt(_tgt);
  // heading readout — MAGNETIC (G2). This is an instrument, and instruments read
  // magnetic. Labelled MAG so it can never be confused with the grid azimuths on
  // the map sheet, which is the whole reason the label changed from a bare AZ.
  const hdg = Math.round(radToMils(yaw));
  if (hdg !== lastHdg) {
    lastHdg = hdg;
    const s = `MAG ${fmtMils(gridToMag(hdg))}`;
    headingEl.textContent = s;
    binoHdgEl.textContent = s;
  }
  // TEMPO3 — mission clock: only scenarios with a real coded deadline return
  // one (convoy escape, E.2 landfall); everything else keeps an empty slot.
  // Same change-gated write pattern as the heading readout above.
  {
    const dl = scenarioDeadline();
    const s = dl ? Math.max(0, Math.ceil(dl.t)) : 0;
    const txt = dl ? `${dl.label} ${(s / 60) | 0}:${String(s % 60).padStart(2, '0')}` : '';
    if (txt !== lastClockTxt) {
      lastClockTxt = txt;
      msnclockEl.textContent = txt;
      msnclockEl.classList.toggle('warn', !!dl && s <= 60);
    }
  }
  // tutorial look-around tracking
  if (TUT.steps) {
    if (TUT.lastYaw !== null) TUT.yawAcc += Math.abs(yaw - TUT.lastYaw);
    TUT.lastYaw = yaw;
    const ts = TUT.steps[TUT.i];
    if (ts && ts.on === 'look' && TUT.yawAcc > (ts.amt || 5)) tutEvent('look');
  }
  // scenario dynamics + battle ambience
  updateScenario();
  cffqTick();             // G22 — lapse a call for fire the observer never finished
  updateCivilians(t);
  legibilityPass(eye.x, eye.y, eye.z);
  // G13 — a suppressed enemy stops shooting for the suppression window and
  // resumes when it lapses: the observer can SEE suppression working and wearing off
  const flashOn = enemyAlive && Scenario && sim.now >= (Scenario.suppressedUntil || 0) &&
    (Scenario.type === 'strongpoint' || Scenario.type === 'assault' ||
     Scenario.type === 'bunker' || Scenario.type === 'raid' ||
     Scenario.type === 'defense' ||  // ENEMY2 — they are shooting at YOU
     Scenario.type === 'callin' ||   // NET1 — they are shooting at the team
     Scenario.type === 'occupied');  // WORLD2 — holding fire off the perimeter
  for (let i = 0; i < units.flashes.length; i++) {
    const f = units.flashes[i];
    f.s.visible = flashOn && (Scenario.type !== 'bunker' || i === 0) &&
      (((t * 0.53 + f.phase) % 1.9) < 0.06);
  }
  for (const p of units.smokePuffs) {
    if (!p.on) { p.m.visible = false; continue; }
    p.m.visible = true;
    const ct = ((t + p.off) % 10) / 10;
    p.m.position.set(p.bx + ct * 26, p.by + 2 + ct * 78, p.bz + ct * 6);
    p.m.scale.setScalar(4 + ct * 17);
    p.m.material.opacity = 0.42 * (1 - ct);
  }
  updateBursts(dt);
  uasUpdate(dt);   // NET2 — orbit + steering + readout
  renderFrame();   // 11d — direct render, or composer+bloom on the SUNLAMP net at tier 0
  renderUAS();     // NET2 — scissored second pass; ~0.1 MPix, PERF1-cheap
  // observer optics overlay: 2D canvas only, throttled to CONFIG.OPTICS.hz and
  // drawn entirely from layers prebuilt at boot. No allocation, no composer.
  if (VISION.mode !== 'day' && t - VISION.last > 1 / CONFIG.OPTICS.hz) {
    VISION.last = t;
    drawOptics();
  }
}

/* ============================================================ BOOT */
refreshNoteBtn();   // NET7 — show the NOTE button if dev mode persisted
newMission(true);
requestAnimationFrame(animate);

// Debug / stable interface exposure
window.SHITFIRE = { CONFIG, H, fireMission, applyCorrection, FDC, WORLD,
  gradeMission, CAMPAIGN, CAMP, TLOG,
  get Scenario() { return Scenario; }, get mission() { return mission; }, OP, BATTERY,
  // QA hooks for tools/shots.js and tools/replay.js (additive — the stable
  // interface above is unchanged): the screenshot rig drives TOD and optics
  // states no keybind reaches; the replay rig re-classifies recorded traffic
  // through parseMessage without touching mission state.
  applyTOD, setVision, onPlayerMessage, parseMessage,
  /* WORLD1 — placement QA. genScenario lets a harness sample target positions
     across many seeds without driving the UI; read-only against world state
     (it builds and returns an S, it does not install it). hasLOS rides along
     so a harness can check sight lines the way the scenario generator does. */
  genScenario, hasLOS,
  /* NET2 — UAS QA: state readout + a setter so a harness can place the
     sensor deterministically. */
  uas: {
    info: () => ({ on: UAS.on, az: +UAS.az.toFixed(4),
                   fx: +UAS.focus.x.toFixed(1), fz: +UAS.focus.z.toFixed(1),
                   crs: Math.round(radToMils(Math.atan2(Math.cos(UAS.az), -Math.sin(UAS.az)))) }),
    set: (x, z, az) => { UAS.focus.x = x; UAS.focus.z = z; if (az !== undefined) UAS.az = az; },
  },
  /* TEMPO1 — TOD legibility QA: point the view at a world point so a harness
     can screenshot the target area at any time of day. Sets the same yaw/pitch
     state the mouse input does; presentation (sway) rides on top as always. */
  /* TEMPO5 — flare QA: burn time and liveness, so a harness can verify the
     per-flare draw without guessing from pixels. Read-only. */
  illumInfo: () => ({ T: ILLUM.T || null, age: +(sim.now - ILLUM.t0).toFixed(1),
                      lit: ILLUM.light ? ILLUM.light.intensity > 0 : false }),
  // GFX2 - arm a one-shot presentation fault so the guard can be tested
  qaBreakBurst: () => { QA_BREAK_BURST = true; },
  qaLookAt(x, z) {
    yaw = azTo(OP.x, OP.z, x, z);
    const d = dist2(OP.x, OP.z, x, z);
    pitch = clamp(Math.atan2(H(x, z) + 2 - eye.y, d),
                  -CONFIG.CAMERA.pitchClamp, CONFIG.CAMERA.pitchClamp);
  },
  /* PERF1 — frame-budget QA. tools/perfprobe.js needs the renderer's own
     accounting (draw calls, triangles) and the live quality state to tell a
     fill-rate problem from a geometry one; neither is reachable from outside
     the module. Read-only facts plus one setter the probe uses to sweep the
     tier curve — nothing here is wired to gameplay. */
  perf: {
    info: () => ({ calls: renderer.info.render.calls,
                   tris: renderer.info.render.triangles,
                   ratio: renderer.getPixelRatio(),
                   tier: QUALITY.tier, binos, ema: +QUALITY.ema.toFixed(4) }),
    /* Freeze the adaptive stepper. Required for any sweep: since PERF1 the
       controller runs while glassing, so it will happily re-tier the renderer
       in the middle of a measurement window and the sample comes back attached
       to the wrong resolution. (It did — the first post-fix sweep reported
       ratio 1.4 at 4.06 MPix and ratio 1.75 at 2.51, which is impossible, and
       that impossibility is the only reason the bad data was caught.) */
    freeze: on => { QUALITY.frozen = !!on; },
    // forces a ratio for measurement only; pointless unless frozen first
    setRatio: r => renderer.setPixelRatio(r),
  },
  // 13e — terrain QA: patch/base geometry facts + a way to time a re-focus
  setTerrainFocus, terrainInfo: () => ({
    baseIdx: terrainMesh.geometry.getIndex().count,
    patchIdx: terrainPatchMesh ? terrainPatchMesh.geometry.getIndex().count : 0,
    patchGeo: terrainPatchMesh ? terrainPatchMesh.geometry : null,
    sharedMat: !!terrainPatchMesh && terrainPatchMesh.material === terrainMesh.material,
    patchCx, patchCz }) };
