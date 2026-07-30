/* ============================================================ CAMERA CONTROL + TOOLS */
let yaw = 0, pitch = 0, binos = false;
const _dir = new THREE.Vector3(), _tgt = new THREE.Vector3();
canvas.addEventListener('click', () => { if (!aarEl.classList.contains('on')) canvas.requestPointerLock(); });
document.addEventListener('mousemove', e => {
  if (document.pointerLockElement !== canvas) return;
  const s = CONFIG.CAMERA.mouseSens * (binos ? binoFovNow() / CONFIG.CAMERA.fov : 1);
  yaw += e.movementX * s;
  pitch = clamp(pitch - e.movementY * s, -CONFIG.CAMERA.pitchClamp, CONFIG.CAMERA.pitchClamp);
});
/* 13i — hold sway. A hand-held optic is never perfectly still: a small damped
   two-frequency wobble rides the view while glassing. Raising the binos adds a
   settling transient (~1 s); a lase steadies the hold to near zero for a
   moment (the squeeze). The sway is CONSTANT-ANGLE, so higher magnification
   makes it look bigger all by itself — which is exactly what real glass does.
   It lives here in lookDir so the view, the reticle measurements and the laser
   ray all agree about where the optic is actually pointing. Never applied to
   the naked eye, and yaw/pitch state is untouched — sway is presentation. */
const SWAY = { t0: -9e9, steadyUntil: -9e9 };
function lookDir(out) {
  let y = yaw, p = pitch;
  if (binos && CONFIG.CAMERA.swayMil > 0) {
    const t = sim.now;
    const settle = 1 + 2.4 * Math.exp(-(t - SWAY.t0) / 0.45);
    const steady = t < SWAY.steadyUntil ? 0.08 : 1;
    const A = CONFIG.CAMERA.swayMil * 0.00098 * settle * steady;   // mils → rad
    y += A * (Math.sin(t * 1.1) + 0.45 * Math.sin(t * 2.7 + 1.3));
    p += A * (Math.sin(t * 0.83 + 0.7) + 0.45 * Math.sin(t * 2.1 + 4.2));
  }
  const cp = Math.cos(p);
  out.set(Math.sin(y) * cp, Math.sin(p), -Math.cos(y) * cp);
  return out;
}

const binoEl = document.getElementById('bino');
const reticleCv = document.getElementById('reticle');
const crosshairEl = document.getElementById('crosshair');
const toolEl = document.getElementById('tool');
function drawReticle() {
  reticleCv.width = innerWidth; reticleCv.height = innerHeight;
  const ctx = reticleCv.getContext('2d');
  ctx.clearRect(0, 0, innerWidth, innerHeight);
  const cx = innerWidth / 2, cy = innerHeight / 2;
  const pxPerMil = innerHeight / (binoFovNow() * 17.7778);
  /* G4 — the graduated arms have to fit the FIELD, not a fixed mil count. They
     used to run to a hard-coded +/-42 mils, which was fine at one fixed fov and
     breaks the moment zoom exists: measured at 14X on a 1080p screen, a 42-mil
     arm is 595 px against a 540 px half-screen, so the arm, its 40-mil labels and
     the stadia block all ran off the picture. Derive the extent from what is
     actually visible, snapped down to a multiple of 5 so the labelled 10-mil
     graduations always land on a real tick.

     px-per-mil is UNCHANGED by this — the scale stays true, only how far the
     ruler is drawn changes. A short honest ruler beats a long one off-screen. */
  const ext = Math.max(10, Math.floor(Math.min(42,
    (Math.min(innerHeight, innerWidth) / 2 - 30) / pxPerMil) / 5) * 5);
  ctx.strokeStyle = 'rgba(15,20,12,0.9)';
  ctx.fillStyle = 'rgba(15,20,12,0.9)';
  ctx.lineWidth = 1.4;
  ctx.font = '11px Consolas, monospace';
  ctx.textAlign = 'center';
  // axes
  ctx.beginPath();
  ctx.moveTo(cx - ext * pxPerMil, cy); ctx.lineTo(cx - 3 * pxPerMil, cy);
  ctx.moveTo(cx + 3 * pxPerMil, cy); ctx.lineTo(cx + ext * pxPerMil, cy);
  ctx.moveTo(cx, cy - ext * pxPerMil); ctx.lineTo(cx, cy - 3 * pxPerMil);
  ctx.moveTo(cx, cy + 3 * pxPerMil); ctx.lineTo(cx, cy + ext * pxPerMil);
  ctx.stroke();
  for (let m = -ext; m <= ext; m += 5) {
    if (m === 0) continue;
    const L = (m % 10 === 0) ? 9 : 5;
    const px = cx + m * pxPerMil, py = cy + m * pxPerMil;
    ctx.beginPath();
    ctx.moveTo(px, cy - L); ctx.lineTo(px, cy + L);
    ctx.moveTo(cx - L, py); ctx.lineTo(cx + L, py);
    ctx.stroke();
    if (m % 10 === 0) {
      ctx.fillText(String(Math.abs(m)), px, cy + 24);
      ctx.fillText(String(Math.abs(m)), cx + 26, py + 4);
    }
  }
  // fine 1-mil ticks near center
  ctx.lineWidth = 1;
  for (let m = -10; m <= 10; m++) {
    if (m % 5 === 0) continue;
    const px = cx + m * pxPerMil, py = cy + m * pxPerMil;
    ctx.beginPath();
    ctx.moveTo(px, cy - 2.5); ctx.lineTo(px, cy + 2.5);
    ctx.moveTo(cx - 2.5, py); ctx.lineTo(cx + 2.5, py);
    ctx.stroke();
  }
  /* Stadia bars. These were a 2 m MAN at 500/1000/2000 m, which directly
     contradicted the mil card's own DO NOT MIL A MAN warning (E1 draws figures
     enlarged at range so they stay visible, so ranging off one is guaranteed
     wrong). Referenced to the 5 m truck instead — a real millable hard target,
     the same 5 m the card lists, and the thing an observer actually ranges on a
     convoy mission. 5 m at 1000/2000/3000 m = 5 / 2.5 / 1.67 mils. */
  ctx.lineWidth = 1.6;
  ctx.textAlign = 'left';
  const stX = cx - (ext - 4) * pxPerMil, stY = cy + ext * 0.58 * pxPerMil;
  [['1000', 5], ['2000', 2.5], ['3000', 5 / 3]].forEach((s, i) => {
    const y = stY + i * 14;
    ctx.beginPath();
    ctx.moveTo(stX, y); ctx.lineTo(stX + s[1] * pxPerMil, y);
    ctx.stroke();
    ctx.fillText(s[0] + ' M', stX + 6 * pxPerMil + 4, y + 4);
  });
  ctx.font = '10px Consolas, monospace';
  ctx.fillText('5M TRUCK', stX, stY - 12);
  // current optical power, so the observer is never unsure which he is on
  ctx.textAlign = 'right';
  ctx.font = '11px Consolas, monospace';
  ctx.fillText(`${binoPower()}X`, cx + (ext - 2) * pxPerMil, cy - ext * 0.62 * pxPerMil);
  // center dot
  ctx.beginPath(); ctx.arc(cx, cy, 1.6, 0, Math.PI * 2); ctx.fill();
}
function setBinos(on) {
  binos = on;
  if (on) { tutEvent('binos'); SWAY.t0 = sim.now; }   // 13i — raise transient
  camera.fov = on ? binoFovNow() : CONFIG.CAMERA.fov;
  camera.updateProjectionMatrix();
  binoEl.classList.toggle('on', on);
  crosshairEl.style.display = on ? 'none' : 'block';
  refreshTool();
  if (on) drawReticle();
  if (!on) { laseEl.textContent = ''; milcardEl.classList.remove('on'); }
  // pin to tier 0 while glassing, restore the stepped tier on exit. Also restarts the step
  // cooldown, so the higher cost of rendering at tier 0 can't slam the tier down the instant
  // binos come back down.
  applyQuality(sim.now);
}
/* G4 — step the optical power. Wraps, redraws the reticle at the new scale, and
   re-applies the camera fov only if the binos are actually up, so cycling with
   them down just pre-selects a power. */
function setBinoZoom(i) {
  const Z = CONFIG.CAMERA.binoZooms;
  CONFIG.CAMERA.binoZoom = ((i % Z.length) + Z.length) % Z.length;
  if (binos) {
    camera.fov = binoFovNow();
    camera.updateProjectionMatrix();
    drawReticle();
  }
  refreshTool();
}
function cycleBinoZoom(dir) {
  if (!binos) { log('', 'Zoom requires binoculars up — press [B] first.', 'sys'); return; }
  setBinoZoom(CONFIG.CAMERA.binoZoom + (dir || 1));
  log('', `Optics ${binoPower()}X — field of view ${binoFovNow().toFixed(1)}° ` +
    `(${Math.round(binoFovNow() * 17.7778)} mils across the vertical). ` +
    `Reticle graduations are true mils at every power.`, 'sys');
}
function refreshTool() {
  toolEl.textContent = (binos ? `BINOS ${binoPower()}X` : 'EYES') +
    (VISION.mode === 'day' ? '' : ' · ' + VIS_LABEL[VISION.mode]) +
    // G3 — permanently on screen while dispersion is off. A test aid that can be
    // forgotten about is a test aid that eventually gets mistaken for the real
    // thing, and this one makes the trainer flatter every shot.
    (CONFIG.BALLISTICS.dispersion ? '' : ' · NO DISP');
}

const laseEl = document.getElementById('lase');
let laseToken = 0;
let lastLase = null;   // most recent laser return — source of the OT factor
const milcardEl = document.getElementById('milcard');
const cheatEl = document.getElementById('cheat');   // F3
function toggleMilCard(force) {
  const on = force !== undefined ? force : !milcardEl.classList.contains('on');
  milcardEl.classList.toggle('on', on && binos);
}
function doLase() {
  if (!binos) { log('', 'Lasing requires binoculars up — press [B] first.', 'sys'); return; }
  // degraded-optics drill: some chapters kill the laser so the observer must
  // estimate range by mil relation off known object sizes
  if (activeChapter && activeChapter.noLaser) {
    laseEl.textContent = 'LRF FAULT — NO RETURN';
    log('', 'LASER RANGEFINDER IS DEAD (battery). Estimate range by MIL RELATION: press [R] for the card — measure a known object in the reticle, then SIZE ÷ MILS × 1000 = range.', 'sys');
    toggleMilCard(true);
    return;
  }
  const tok = ++laseToken;
  // 13i — the squeeze: the hold steadies through the lase and its readout,
  // set BEFORE the scheduled raycast so the ray itself is fired steady
  SWAY.steadyUntil = sim.now + 1.4;
  laseTick();
  laseEl.textContent = 'LASING…';
  schedule(sim.now + 0.55, () => {
    if (tok !== laseToken) return;
    lookDir(_dir);
    const hit = groundHit(eye.x, eye.y, eye.z, _dir.x, _dir.y, _dir.z, 14000);
    laseEl.textContent = hit
      ? `RNG ${Math.round(hit.dist)} M   GRID ${gridOf(hit.x, hit.z)}`
      : 'NO RETURN';
    if (hit) {
      // the lase also gives the observer his OT factor for mil relation
      lastLase = { range: hit.dist, t: sim.now };
      laseEl.textContent += `   OT FACTOR ${otFactor(hit.dist)}`;
      tutEvent('lase', hit);
    }
    schedule(sim.now + CONFIG.UI.laseHold, () => { if (tok === laseToken) laseEl.textContent = ''; });
  });
}

