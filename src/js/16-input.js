/* ============================================================ PUSH-TO-TALK (SpeechRecognition) */
const SR = window.SpeechRecognition || window.webkitSpeechRecognition;
const pttEl = document.getElementById('ptt');
const pttLiveEl = document.getElementById('pttlive');
/* G6 — restore the comms panel's saved geometry. Deliberately called HERE and not
   next to its own code: applyComms() positions the PTT banner too, and pttEl is a
   `const` declared on the line above, so calling it any earlier would hit the
   temporal dead zone and throw. */
loadComms();
let rec = null, recActive = false, recFinal = '', recInterim = '';
let srWarned = false;
/* Chrome cannot PERSIST microphone permission for a file:// page — there is no
   origin to remember the grant against — and SpeechRecognition re-prompts on
   every start() unless the tab already holds a live audio stream. So on the
   first PTT we also acquire ONE getUserMedia stream and hold it for the life
   of the tab: the permission prompt covers both (same mic permission), and
   every later transmission starts clean. The held stream is never recorded,
   routed, or read — it exists purely so the grant stays live. Failure is
   silent and changes nothing: PTT still works, it just prompts as before.
   (For a grant that survives ACROSS sessions, serve over localhost — see
   tools/serve.cmd — where Chrome has a real origin to remember.) */
let _micHeld = null, micTried = false;   // _ prefix: assigned-never-read is the point
function micHold() {
  if (micTried || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) return;
  micTried = true;
  navigator.mediaDevices.getUserMedia({ audio: true })
    .then(s => { _micHeld = s; })
    .catch(() => { _micHeld = null; });   // denied/unavailable: degrade to per-use prompts
}
function startPTT() {
  if (recActive) return;
  if (!SR) {
    if (!srWarned) {
      srWarned = true;
      log('', 'Voice input is not supported in this browser (Chrome/Edge desktop only) — use the typed box.', 'sys');
    }
    return;
  }
  micHold();
  try {
    rec = new SR();
  } catch (e) { return; }
  rec.lang = CONFIG.VOICE.lang;
  rec.continuous = true;
  rec.interimResults = true;
  recFinal = ''; recInterim = '';
  rec.onresult = ev => {
    recInterim = '';
    for (let i = ev.resultIndex; i < ev.results.length; i++) {
      const r = ev.results[i];
      if (r.isFinal) recFinal += r[0].transcript + ' ';
      else recInterim += r[0].transcript;
    }
    pttLiveEl.textContent = (recFinal + recInterim).trim();
  };
  rec.onerror = ev => {
    if (ev.error === 'not-allowed' || ev.error === 'service-not-allowed')
      log('', 'Microphone permission denied — voice PTT unavailable; use the typed box.', 'sys');
    else if (ev.error !== 'no-speech' && ev.error !== 'aborted')
      log('', `Voice input error (${ev.error}) — use the typed box.`, 'sys');
  };
  rec.onend = () => {
    recActive = false;
    pttEl.classList.remove('on');
    const txt = (recFinal + ' ' + recInterim).trim();
    if (txt) onPlayerMessage(txt);
  };
  recActive = true;
  pttLiveEl.textContent = '';
  pttEl.classList.add('on');
  // half-duplex net: keying the mic steps on the FDC
  if ('speechSynthesis' in window) speechSynthesis.cancel();
  try { rec.start(); } catch (e) { recActive = false; pttEl.classList.remove('on'); }
}
function endPTT() {
  if (rec && recActive) {
    try { rec.stop(); } catch (e) { /* onend still fires */ }
  }
}

/* ============================================================ INPUT WIRING */
const txForm = document.getElementById('txform');
const txInput = document.getElementById('txinput');
txForm.addEventListener('submit', e => {
  e.preventDefault();
  const v = txInput.value.trim();
  txInput.value = '';
  if (v) onPlayerMessage(v);
});
document.addEventListener('keydown', e => {
  if (e.target === txInput) {
    if (e.key === 'Escape') txInput.blur();
    return;
  }
  const k = e.key.toLowerCase();
  if (k === 'escape') {
    cheatEl.classList.remove('on');   // F3
    aarEl.classList.remove('on');
    mapEl.classList.remove('on');
    menuEl.classList.remove('on');
    libEl.classList.remove('on');
    if (briefEl.classList.contains('on')) {
      briefEl.classList.remove('on');
      if ('speechSynthesis' in window) speechSynthesis.cancel();
    }
    return;
  }
  if (e.code === 'Space') {
    e.preventDefault();
    if (!e.repeat) startPTT();
    return;
  }
  if (k === 'b') setBinos(!binos);
  else if (k === 'z') cycleBinoZoom(e.shiftKey ? -1 : 1);   // G4 — optical power
  else if (k === 'h') cheatEl.classList.toggle('on');       // F3 — cheat sheet
  else if (k === 'r') toggleMilCard();
  else if (k === 'o') cycleVision();
  else if (k === 'l') doLase();
  else if (k === 'm') toggleMap();
  else if (k === 'k') toggleMenu();
  else if (k === 'p') toggleLibrary();
  else if (k === 'v') {
    CONFIG.VOICE.ttsEnabled = !CONFIG.VOICE.ttsEnabled;
    if (!CONFIG.VOICE.ttsEnabled && 'speechSynthesis' in window) speechSynthesis.cancel();
    log('', `FDC voice ${CONFIG.VOICE.ttsEnabled ? 'ON' : 'OFF'}.`, 'sys');
  }
  else if (k === 'n') newMission(false);
  /* G3 — SHIFT+D, not plain D. This is a test aid that removes the difficulty
     from the trainer, so it should take deliberate intent to hit and should never
     be reachable by a stray keypress while flying a graded chapter. */
  else if (k === 'd' && e.shiftKey) {
    const B = CONFIG.BALLISTICS;
    B.dispersion = !B.dispersion;
    refreshTool();
    if (mission && !mission.done) markDispersion();
    log('', B.dispersion
      ? 'Round dispersion ON — rounds fall with normal first-round and follow-up error.'
      : 'Round dispersion OFF (test mode) — every round lands exactly on the aimpoint. ' +
        'This mission will NOT be graded and no stars will be recorded.', 'sys');
    TLOG.add('sys', '', `dispersion ${B.dispersion ? 'on' : 'off'}`, { noDisp: !B.dispersion });
  }
  else if (k === 'enter') { e.preventDefault(); txInput.focus(); }
});
document.addEventListener('keyup', e => {
  if (e.target === txInput) return;
  if (e.code === 'Space') { e.preventDefault(); endPTT(); }
});
/* G4 — the wheel is what a hand reaches for to zoom, so bind it too. Only while
   glassing: with binos down the wheel is left alone for the page. */
addEventListener('wheel', e => {
  if (!binos) return;
  e.preventDefault();
  setBinoZoom(CONFIG.CAMERA.binoZoom + (e.deltaY < 0 ? 1 : -1));
}, { passive: false });
addEventListener('resize', () => {
  renderer.setSize(innerWidth, innerHeight);
  composerDrop();   // 11d — stale render targets die here; rebuilt next frame if wanted
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  if (binos) drawReticle();
  // G6 — re-clamp the comms panel. Shrinking the window could otherwise leave a
  // panel that was dragged to the right edge entirely outside the viewport, with
  // its header — the only way to drag it back — unreachable.
  if (commsGeom) applyComms(commsGeom);
});

/* ============================================================ GAMEPAD */
let padIndex = null;
const padPrev = new Array(17).fill(false);
window.addEventListener('gamepadconnected', e => {
  padIndex = e.gamepad.index;
  log('', `Controller connected (${e.gamepad.id}). Right stick — look · hold RB — talk · Y — binos · A — lase · LB — map · START — menu · BACK — map library · B — close · D-pad up — new mission.`, 'sys');
});
window.addEventListener('gamepaddisconnected', e => {
  if (padIndex === e.gamepad.index) padIndex = null;
});
function pollGamepad(dt) {
  if (padIndex === null || !navigator.getGamepads) return;
  const gp = navigator.getGamepads()[padIndex];
  if (!gp) return;
  const dz = CONFIG.GAMEPAD.deadzone;
  let rx = gp.axes[2] || 0, ry = gp.axes[3] || 0;
  rx = Math.abs(rx) > dz ? rx * Math.abs(rx) : 0;
  ry = Math.abs(ry) > dz ? ry * Math.abs(ry) : 0;
  const s = CONFIG.GAMEPAD.lookSpeed * (binos ? binoFovNow() / CONFIG.CAMERA.fov : 1);
  yaw += rx * s * dt;
  pitch = clamp(pitch - ry * s * dt, -CONFIG.CAMERA.pitchClamp, CONFIG.CAMERA.pitchClamp);
  const press = i => { const b = gp.buttons[i]; return !!(b && b.pressed); };
  const edge = i => press(i) && !padPrev[i];
  if (edge(3)) setBinos(!binos);      // Y / triangle
  if (edge(0)) doLase();              // A / cross
  if (edge(4)) toggleMap();           // LB
  if (edge(9)) toggleMenu();          // START
  if (edge(8)) toggleLibrary();       // BACK / select
  if (edge(12)) newMission(false);    // D-pad up
  if (edge(1)) {                      // B / circle — close overlays
    aarEl.classList.remove('on');
    mapEl.classList.remove('on');
    menuEl.classList.remove('on');
    libEl.classList.remove('on');
  }
  if (edge(5)) startPTT();            // RB hold-to-talk
  if (!press(5) && padPrev[5]) endPTT();
  for (let i = 0; i < padPrev.length; i++) padPrev[i] = press(i);
}

/* ============================================================ TOUCH */
const IS_TOUCH = matchMedia('(pointer: coarse)').matches;
if (IS_TOUCH) document.body.classList.add('touch');
document.querySelectorAll('#touchbar [data-act]').forEach(b => {
  b.addEventListener('click', () => {
    const a = b.dataset.act;
    if (a === 'binos') setBinos(!binos);
    else if (a === 'lase') doLase();
    else if (a === 'optics') cycleVision();
    else if (a === 'map') toggleMap();
    else if (a === 'menu') toggleMenu();
    else if (a === 'new') newMission(false);
  });
});
{
  const tbTalk = document.getElementById('tb-talk');
  tbTalk.addEventListener('touchstart', e => { e.preventDefault(); startPTT(); });
  tbTalk.addEventListener('touchend', e => { e.preventDefault(); endPTT(); });
}
let lastTouch = null;
canvas.addEventListener('touchstart', e => {
  if (e.touches.length === 1)
    lastTouch = { x: e.touches[0].clientX, y: e.touches[0].clientY };
});
canvas.addEventListener('touchmove', e => {
  e.preventDefault();
  if (!lastTouch || e.touches.length !== 1) return;
  const tx = e.touches[0].clientX, ty = e.touches[0].clientY;
  const s = 0.0045 * (binos ? binoFovNow() / CONFIG.CAMERA.fov : 1);
  yaw += (tx - lastTouch.x) * s;
  pitch = clamp(pitch - (ty - lastTouch.y) * s, -CONFIG.CAMERA.pitchClamp, CONFIG.CAMERA.pitchClamp);
  lastTouch.x = tx; lastTouch.y = ty;
}, { passive: false });
canvas.addEventListener('touchend', () => { lastTouch = null; });

