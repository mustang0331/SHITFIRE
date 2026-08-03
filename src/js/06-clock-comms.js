/* ============================================================ CLOCK + SCHEDULER */
const sim = { now: 0 };
const events = [];
function schedule(t, fn) {
  const e = { t, fn };
  let i = events.length;
  while (i > 0 && events[i - 1].t > t) i--;
  events.splice(i, 0, e);
}
function runEvents() {
  while (events.length && events[0].t <= sim.now) events.shift().fn();
}
function clearEvents() { events.length = 0; }

/* ============================================================ TRANSCRIPT LOG */
// Structured session transcript: every net transmission (observer / FDC /
// system) plus parse results, impacts, and AAR outcomes, with sim-time and
// mission context. Reviewable after the fact to correct dialogue, parser
// handling, and FDC responses. Persisted (last 800 entries) and exportable
// from the mission menu as JSON or readable text.
const TLOG = {
  entries: [],
  max: 2000,
  _pending: false,
  add(kind, call, msg, extra) {
    const e = {
      t: Math.round(sim.now * 10) / 10, wall: Date.now(),
      state, seed: CONFIG.SEED.mission,
      ch: activeChapter ? activeChapter.id : null,
      kind, call: call || '', msg,
    };
    if (extra) Object.assign(e, extra);
    this.entries.push(e);
    if (this.entries.length > this.max)
      this.entries.splice(0, this.entries.length - this.max);
    this.save();
  },
  save() {   // debounced; in-memory only where localStorage is unavailable
    if (this._pending) return;
    this._pending = true;
    setTimeout(() => {
      this._pending = false;
      try { localStorage.setItem('shitfire_transcript', JSON.stringify(this.entries.slice(-800))); }
      catch (e) { /* in-memory only */ }
    }, 3000);
  },
  load() {
    try { this.entries = JSON.parse(localStorage.getItem('shitfire_transcript') || '[]'); }
    catch (e) { this.entries = []; }
  },
  clear() {
    this.entries.length = 0;
    try { localStorage.removeItem('shitfire_transcript'); } catch (e) {}
  },
  text() {
    return this.entries.map(e => {
      const tag = `[${fmtTime(e.t)} ${e.state}${e.ch ? ' ch' + e.ch : ''} s${e.seed}]`;
      if (e.kind === 'parse')
        return `${tag} · parsed as ${e.ptype}${e.method ? '/' + e.method : ''}${e.warno ? '/' + e.warno : ''}`;
      if (e.kind === 'impact')   /* NET8a — legacy illum entries had no dTgt
        and printed "undefined m from target"; render distance only when held */
        return `${tag} * ${e.msg}${e.dTgt !== undefined ? ` — ${e.dTgt} m from target` : ''}`;
      if (e.kind === 'spot')     /* NET8b — the spot entry is the cue's METADATA,
        logged when the report is scheduled; unmarked it read as a bare answer
        leaked before its own SALUTE in every export */
        return `${tag} ~ spot-report cue (${e.form || 'report'}): ${e.msg}`;
      if (e.kind === 'aar')
        return `${tag} == AAR: ${e.msg}${e.stars !== undefined ? ` (${e.stars}★)` : ''} — ${e.rounds} adj rds, ${fmtTime(e.dur)}`;
      return `${tag} ${e.call ? e.call + ': ' : ''}${e.msg}`;
    }).join('\n');
  },
};
TLOG.load();
function downloadText(name, text, mime) {
  const a = document.createElement('a');
  a.download = name;
  a.href = URL.createObjectURL(new Blob([text], { type: mime }));
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

/* ============================================================ COMMS LOG + FDC */
const logEl = document.getElementById('log');

/* --- G6: the comms panel moves and resizes -----------------------------------
   User feedback: "Chat terminal should be draggable, resizable, taking up to
   much space." Resizing is the native CSS `resize` handle — no pointer maths to
   get wrong — and the panel is a flex column so the transcript absorbs the spare
   height rather than being clipped. Dragging is by the header only, so a click
   anywhere in the transcript still selects text.

   Geometry is persisted, and CLAUDE.md is explicit that localStorage may be
   unavailable (blocked in a claude.ai Artifact), so every access is guarded and
   the panel simply reverts to its default position when it is. Same try/catch
   pattern as TLOG and CAMP. */
const commsEl = document.getElementById('comms');
const commsHeadEl = document.getElementById('commshead');
const COMMS_KEY = 'shitfire_comms_geom';
let commsGeom = null;                    // in-memory copy; the fallback when storage fails

function commsDefaults() {
  // matches the CSS defaults, recomputed because they are viewport-relative
  return { l: 12, t: Math.max(8, innerHeight - 273),
           w: Math.min(600, innerWidth * 0.62), h: 261 };
}
function clampComms(g) {
  // Never let the panel be dragged so far that its header — the only way to get
  // it back — is off screen. 90 px of it always stays reachable.
  g.w = clamp(g.w, 300, innerWidth * 0.94);
  g.h = clamp(g.h, 132, innerHeight * 0.78);
  g.l = clamp(g.l, 90 - g.w, innerWidth - 90);
  g.t = clamp(g.t, 0, innerHeight - 28);
  return g;
}
function applyComms(g) {
  commsGeom = clampComms(g);
  commsEl.style.left = commsGeom.l + 'px';
  commsEl.style.top = commsGeom.t + 'px';
  commsEl.style.bottom = 'auto';
  commsEl.style.width = commsGeom.w + 'px';
  commsEl.style.height = commsGeom.h + 'px';
  // the PTT banner is about this radio, so it rides with it instead of staying
  // pinned to a corner the panel has since left
  pttEl.style.left = commsGeom.l + 'px';
  pttEl.style.top = Math.max(0, commsGeom.t - 42) + 'px';
  pttEl.style.bottom = 'auto';
  pttEl.style.width = commsGeom.w + 'px';
}
function saveComms() {
  try { localStorage.setItem(COMMS_KEY, JSON.stringify(commsGeom)); } catch (e) {}
}
function loadComms() {
  let g = null;
  try { g = JSON.parse(localStorage.getItem(COMMS_KEY) || 'null'); } catch (e) {}
  const d = commsDefaults();
  // Validate every field. A stored geometry from a much larger monitor would
  // otherwise put the panel off screen with no way to retrieve it.
  if (!g || ['l', 't', 'w', 'h'].some(k => typeof g[k] !== 'number' || !isFinite(g[k]))) g = d;
  applyComms(g);
}
{
  let drag = null;
  commsHeadEl.addEventListener('pointerdown', e => {
    if (e.button) return;
    drag = { x: e.clientX, y: e.clientY, l: commsGeom.l, t: commsGeom.t };
    commsHeadEl.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  commsHeadEl.addEventListener('pointermove', e => {
    if (!drag) return;
    applyComms({ l: drag.l + (e.clientX - drag.x), t: drag.t + (e.clientY - drag.y),
                 w: commsGeom.w, h: commsGeom.h });
  });
  commsHeadEl.addEventListener('pointerup', e => {
    if (!drag) return;
    drag = null;
    commsHeadEl.releasePointerCapture(e.pointerId);
    saveComms();
  });
  // double-click the header to put it back where it started
  commsHeadEl.addEventListener('dblclick', () => { applyComms(commsDefaults()); saveComms(); });
  // the native resize handle fires no event, so watch the box instead
  if (typeof ResizeObserver === 'function') {
    let t0 = 0;
    new ResizeObserver(() => {
      if (!commsGeom) return;
      const w = commsEl.offsetWidth, h = commsEl.offsetHeight;
      if (Math.abs(w - commsGeom.w) < 1 && Math.abs(h - commsGeom.h) < 1) return;
      commsGeom.w = w; commsGeom.h = h;
      pttEl.style.width = w + 'px';
      clearTimeout(t0);
      t0 = setTimeout(saveComms, 400);      // debounced: a drag-resize fires constantly
    }).observe(commsEl);
  }
}
function log(call, msg, cls) {
  const d = document.createElement('div');
  d.className = cls;
  d.innerHTML = call ? `<span class="call">${call}:</span>` : '';
  d.appendChild(document.createTextNode(msg));
  logEl.appendChild(d);
  logEl.scrollTop = logEl.scrollHeight;
  TLOG.add(cls || 'sys', call, msg);
}

const FDC = {
  lastT: 0,
  lastMsg: '',
  // say(msg, {delay}) queues msg `delay` seconds after the previous FDC line
  // (or now, whichever is later). Returns the absolute sim time it will print.
  say(msg, opts) {
    const delay = (opts && opts.delay !== undefined) ? opts.delay : 0.9;
    const t = Math.max(sim.now, this.lastT) + delay;
    this.lastT = t;
    schedule(t, () => {
      /* G17 — the callsign swap happens HERE, at the single point every FDC
         utterance passes through, rather than in the ~40 strings that mention
         HELLHOUND. Every FDC.say is the fire unit speaking, and on a mortar
         chapter the unit speaking is HACKSAW — including in the quip pools,
         which would otherwise have the mortar section referring to itself by
         the artillery's name. Applied at DELIVERY time, not queue time, so a
         line queued during a chapter transition names whoever actually owns
         the net when it airs. */
      if (activeChapter && activeChapter.asset === 'mortar60')
        msg = msg.replace(/\bHELLHOUND(\s+FIRES)?\b/g,
                          (m0, f) => f ? CONFIG.FDC.fdc60 : fdcShort());
      // 11c — same single-point swap for the satellite; the quip pools speak
      // as SUNLAMP without editing forty strings
      else if (activeChapter && activeChapter.asset === 'sunlamp')
        msg = msg.replace(/\bHELLHOUND(\s+FIRES)?\b/g,
                          (m0, f) => f ? CONFIG.FDC.fdcSun : fdcShort());
      this.lastMsg = msg;
      squelch();
      log(fdcCall(), msg, 'fdc');
      speakFDC(msg);
    });
    return t;
  },
};

