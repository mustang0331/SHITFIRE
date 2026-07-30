/* ============================================================ MAIN LOOP */
const headingEl = document.getElementById('heading');
const binoHdgEl = document.getElementById('binohdg');
// adaptive quality: frame-time EMA steps pixel ratio down before anything else
const QUALITY = { tiers: [1.75, 1.4, 1.1, 0.85], tier: 0, ema: 0.016, lastChange: 0 };
// Binoculars pin the renderer to tier 0. A 2 m figure at 3000 m subtends ~0.67 mrad, which
// through the 9° bino FOV is ~4 px tall at full resolution and ~2 px at the bottom tier — so a
// step-down taken while glassing can make a target unresolvable exactly when the observer is
// trying to resolve it. That is a training-fidelity bug, not a performance trade. The stepped
// tier is retained, not reset, and re-applies on exit.
function applyQuality(t) {
  QUALITY.lastChange = t;
  const tier = binos ? 0 : QUALITY.tier;
  renderer.setPixelRatio(Math.min(devicePixelRatio, QUALITY.tiers[tier]));
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
  // frozen while glassing — see applyQuality; the tier resumes stepping once binos come down
  if (!binos && t - QUALITY.lastChange > 2.5) {
    if (QUALITY.ema > 0.024 && QUALITY.tier < QUALITY.tiers.length - 1) {
      QUALITY.tier++; applyQuality(t);
    } else if (QUALITY.ema < 0.013 && QUALITY.tier > 0) {
      QUALITY.tier--; applyQuality(t);
    }
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
     Scenario.type === 'bunker' || Scenario.type === 'raid');
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
  renderer.render(scene, camera);
  // observer optics overlay: 2D canvas only, throttled to CONFIG.OPTICS.hz and
  // drawn entirely from layers prebuilt at boot. No allocation, no composer.
  if (VISION.mode !== 'day' && t - VISION.last > 1 / CONFIG.OPTICS.hz) {
    VISION.last = t;
    drawOptics();
  }
}

/* ============================================================ BOOT */
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
  // 13e — terrain QA: patch/base geometry facts + a way to time a re-focus
  setTerrainFocus, terrainInfo: () => ({
    baseIdx: terrainMesh.geometry.getIndex().count,
    patchIdx: terrainPatchMesh ? terrainPatchMesh.geometry.getIndex().count : 0,
    patchGeo: terrainPatchMesh ? terrainPatchMesh.geometry : null,
    sharedMat: !!terrainPatchMesh && terrainPatchMesh.material === terrainMesh.material,
    patchCx, patchCz }) };
