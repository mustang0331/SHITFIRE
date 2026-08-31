/* tools/oracle-capture.js — parity-fixture capture rig for the engine port (P0).
   Run: node tools/oracle-capture.js all              (everything, default output)
        node tools/oracle-capture.js level1|level2|level3|manifest
        node tools/oracle-capture.js level3 --missions=01-f1-eyes,04-ch11-five-star
        node tools/oracle-capture.js all --out=path   (redirected output — the
                                                       reproducibility gate runs
                                                       the capture twice into two
                                                       directories and diffs them)

   WHAT THIS IS. PARITY_TESTING.md declares the browser build the golden-master
   oracle for the engine port. This tool freezes that oracle into JSON fixtures
   under ENGINE_PORT/fixtures/: Level 1 = pure-function ground truth (PRNG,
   noise, terrain heights, rays, grids, spotting), Level 2 = scenario-generation
   snapshots per chapter/difficulty and per skirmish type/seed, Level 3 = full
   scripted mission transcripts (TLOG) driven through the real bookshelf menu
   and the real parser, exactly as a player would produce them.

   DETERMINISM (the exit criterion — two runs must be byte-identical):
   - Playwright's fake clock (page.clock) is installed before load. The sim
     clock is requestAnimationFrame timestamps (17-loop-boot.js: sim.now = t),
     and under the fake clock rAF fires at exact 16 ms fake-time steps, so
     every sim event lands on the same 0.1 s TLOG stamp in every run (verified
     by spike, 2026-08-28: two scripted missions came back byte-identical;
     real-time captures cannot pass the gate because rAF jitter moves stamps).
   - Math.random is overridden with a seeded PRNG (mulberry32, 0x5EEDFACE) from
     an init script BEFORE the page loads. The sim only uses Math.random for
     unscored flavor (quip pick, skirmish type draw — PARITY_TESTING.md
     §Where parity is not required), but quip TEXT lands in the TLOG, so the
     capture seeds it to make transcripts reproducible. Harness-side override —
     no sim source is touched.
   - Render cost is cut with the existing QA hooks (perf.freeze + setRatio):
     presentation only; the sim never reads the pixel ratio.

   READ-ONLY against SHITFIRE.html per CLAUDE.md's dev-tooling rules. */
import { chromium } from 'playwright-core';
import { fileURLToPath } from 'url';
import { createHash } from 'crypto';
import { execSync } from 'child_process';
import path from 'path';
import fs from 'fs';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(here, '..');
const simPath = path.join(root, 'SHITFIRE.html');
const missionsDir = path.join(here, 'oracle-missions');

const args = process.argv.slice(2);
const cmd = args.find(a => !a.startsWith('--')) || 'all';
const opt = Object.fromEntries(args.filter(a => a.startsWith('--')).map(a => {
  const [k, v] = a.slice(2).split('=');
  return [k, v === undefined ? true : v];
}));
const outDir = path.resolve(opt.out || path.join(root, 'ENGINE_PORT', 'fixtures'));

/* Capture constants — all recorded into MANIFEST.json. */
const EPOCH = Date.UTC(2026, 0, 1);        // fake wall clock at page load
const PAUSE_AT = EPOCH + 30_000;           // clock frozen here after boot
const MATH_RANDOM_SEED = 0x5EEDFACE;       // harness override of Math.random
const RENDER_RATIO = 0.35;                 // perf.setRatio — presentation only
const VIEWPORT = { width: 640, height: 360 };
const STEP_MS = 512;                       // clock advance per poll (32 frames)
const H_GRID = { from: -4975, step: 50, n: 200 };   // PARITY_TESTING Level 1 grid
const DEM_SPEC = { n: 512, seed: 4242, scale: 260, offset: -20,
  formula: 'value(i,j) = 260 * hash2(i, j, 4242) - 20   // hash2 is the sim\'s own integer hash; IEEE mul+add, bit-recoverable' };
const RAY_SEED = 20260828;                 // in-script generator for raylos.json
const PRNG_SEEDS = [1337, 1, 101, 9021, 5150, 66600];
const PRNG_DRAWS = 10000;
const TERRAIN_SEEDS = [1337, 9021, 5150, 66600];
const GEN_SEEDS = [11, 12, 13];
const SCN_NAME = {   // SCN_META names as rendered on the skirmish buttons
  strongpoint: 'POSITION UNDER ATTACK', troops: 'TROOPS IN THE OPEN',
  bunker: 'DUG-IN BUNKER', convoy: 'CONVOY — MOVING TARGET',
  assault: 'COMBINED ARMS ASSAULT — DANGER CLOSE',
  raid: 'VILLAGE RAID — NO-STRIKE ADJACENT', wreck: 'DERELICT WRECK (TRAINING)',
  battery: 'COUNTERBATTERY — GUNS IN ACTION', defense: 'DEFEND THE OP — DANGER CLOSE',
  callin: 'CALL FOR SUPPORT — ROCK EATERS',
  occupied: 'OCCUPIED SETTLEMENT — NO-STRIKE ADJACENT', qfp: 'QUICK FIRE PLAN — H-HOUR',
  twin: 'TWO TARGETS — TRAFFIC BY NUMBER', chow: 'THE GREAT CHOW RAID — DANGER CLOSE',
  kaiju: 'CLAWS OUT — MOVING HARD TARGET',
};

/* ---------------- deterministic JSON (sorted keys, LF, trailing newline) ---- */
function stableStringify(v, indent) {
  const seen = new Set();
  function enc(x, depth) {
    if (x === null) return 'null';
    const t = typeof x;
    if (t === 'number') return Number.isFinite(x) ? JSON.stringify(x) : 'null';
    if (t === 'string' || t === 'boolean') return JSON.stringify(x);
    if (t !== 'object') return undefined;
    if (seen.has(x)) return '"[cycle]"';
    seen.add(x);
    const pad = indent ? '\n' + ' '.repeat(indent * (depth + 1)) : '';
    const padEnd = indent ? '\n' + ' '.repeat(indent * depth) : '';
    let out;
    if (Array.isArray(x)) {
      const items = x.map(e => { const s = enc(e, depth + 1); return s === undefined ? 'null' : s; });
      out = items.length ? '[' + pad + items.join(',' + pad) + padEnd + ']' : '[]';
    } else {
      const keys = Object.keys(x).sort();
      const items = [];
      for (const k of keys) {
        const s = enc(x[k], depth + 1);
        if (s !== undefined) items.push(JSON.stringify(k) + (indent ? ': ' : ':') + s);
      }
      out = items.length ? '{' + pad + items.join(',' + pad) + padEnd + '}' : '{}';
    }
    seen.delete(x);
    return out;
  }
  return enc(v, 0) + '\n';
}
function writeFixture(rel, obj, indent = 1) {
  const p = path.join(outDir, rel);
  fs.mkdirSync(path.dirname(p), { recursive: true });
  fs.writeFileSync(p, stableStringify(obj, indent));
  console.log('  wrote', rel, `(${fs.statSync(p).size} bytes)`);
}

/* ---------------- page-side helper library ---------------------------------
   Injected into evaluate calls. IMPORTANT: only `function` declarations at the
   top level (they survive a direct eval into the caller's scope; const/let
   would not), and every function reads window.SHITFIRE itself. */
const PAGE_LIB = `
  function u32Mulberry(seed) {
    var a = seed | 0;
    return function () {
      a = a + 0x6D2B79F5 | 0;
      var t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return (t ^ t >>> 14) >>> 0;
    };
  }
  function serialize(rootV) {
    var seen = new Set();
    function walk(x, depth) {
      if (x === null) return null;
      var t = typeof x;
      if (t === 'number' || t === 'string' || t === 'boolean') return x;
      if (t !== 'object' || depth > 14) return undefined;
      if (seen.has(x)) return undefined;
      if (ArrayBuffer.isView(x)) return Array.from(x);
      seen.add(x);
      var out;
      if (Array.isArray(x)) {
        out = [];
        for (var i = 0; i < x.length; i++) {
          var v = walk(x[i], depth + 1);
          out.push(v === undefined ? null : v);
        }
      } else if (Object.getPrototypeOf(x) === Object.prototype || Object.getPrototypeOf(x) === null) {
        out = {};
        var ks = Object.keys(x);
        for (var j = 0; j < ks.length; j++) {
          var w = walk(x[ks[j]], depth + 1);
          if (w !== undefined) out[ks[j]] = w;
        }
      } else {
        out = undefined;   // THREE objects, DOM nodes, class instances: dropped
      }
      seen.delete(x);
      return out;
    }
    return walk(rootV, 0);
  }
`;

/* Placeholder resolvers for Level 3 playbooks — deterministic functions of the
   (deterministic) live sim state. Function declarations only, same rule. */
const RESOLVER = PAGE_LIB + `
  function _SH() { return window.SHITFIRE; }
  function gridN(x, z, digits) {
    var en = _SH().worldToEN(x, z);
    var d = digits / 2, div = digits === 6 ? 100 : digits === 8 ? 10 : 1;
    return String(Math.floor(en.e / div)).padStart(d, '0') +
           String(Math.floor(en.n / div)).padStart(d, '0');
  }
  function azR(ax, az, bx, bz) { return Math.atan2(bx - ax, -(bz - az)); }
  function d2R(ax, az, bx, bz) { return Math.hypot(bx - ax, bz - az); }
  function tgtR() { return _SH().Scenario.enemy; }
  function otDirR() {
    var SH = _SH();
    var a = SH.mission ? SH.mission.aim : SH.Scenario.enemy;
    return String(Math.round(SH.radToMils(azR(SH.OP.x, SH.OP.z, a.x, a.z)) / 10) * 10 % 6400);
  }
  function corrPartsR(ffe, cap) {
    var SH = _SH(), m = SH.mission;
    var last = m.rounds[m.rounds.length - 1];
    var sp = SH.spotBurst(last);
    var q = ffe ? 50 : 100;
    var dev = sp.devCorr;
    var rc = -Math.round(sp.overShort / q) * q;
    if (cap) { dev = Math.max(-100, Math.min(100, dev)); rc = Math.max(-100, Math.min(100, rc)); }
    var parts = [];
    if (dev) parts.push((dev > 0 ? 'right ' : 'left ') + Math.abs(dev));
    if (rc) parts.push((rc > 0 ? 'add ' : 'drop ') + Math.abs(rc));
    return parts;
  }
  function friendlyPtsR() {
    // mirror of the sim's friendlyPositions() (05-units-scenario.js:792)
    var SH = _SH(), S = SH.Scenario;
    if (!S) return [];
    if (S.type !== 'assault') return S.friendlies || [];
    var now = performance.now() / 1000;
    var adv = Math.min(S.fSpeed * (now - S.ft0), S.fAdvMax);
    var fx = Math.sin(S.fAdvAz), fz = -Math.cos(S.fAdvAz);
    var rx = Math.cos(S.fAdvAz), rz = Math.sin(S.fAdvAz);
    var out = [];
    for (var i = 0; i < 6; i++) {
      var l = (i - 2.5) * 8;
      out.push({ x: S.fStart.x + fx * adv + rx * l, z: S.fStart.z + fz * adv + rz * l, r: 40 });
    }
    return out;
  }
  function dcWordR(asset) {
    var SH = _SH();
    var r = SH.CONFIG.MISSION.dangerClose[asset === 'mortar60' ? 'mortar60' : 'battery'];
    var t = tgtR(), min = Infinity;
    var pts = friendlyPtsR();
    for (var i = 0; i < pts.length; i++) min = Math.min(min, d2R(t.x, t.z, pts[i].x, pts[i].z));
    return min <= r ? ', danger close' : '';
  }
  function survWordR(asset) {
    var SH = _SH(), S = SH.Scenario, C = SH.CONFIG;
    if (S.type === 'convoy')
      return S.veh.filter(function (v) { return v.dead; }).length >= 3 ? 'destroyed' : 'suppressed';
    var set = C.EFFECTS[asset === 'mortar60' ? 'mortar60' : 'arty'];
    var c = set[S.tgtClass] || set.personnel;
    var m = SH.mission;
    var tw = m && m.tgtRef === 'twin' && S.twinTgt ? S.twinTgt : null;
    var eff = tw ? tw.eff : S.eff;
    return eff >= c.destroyPct ? 'destroyed'
         : eff >= c.neutralizePct ? 'neutralized' : 'suppressed';
  }
  function accomplishedR(asset) {
    // mirror of missionAccomplished() (10-mission.js:1110)
    var SH = _SH(), m = SH.mission, S = SH.Scenario;
    if (m && m.intent === 'illum') return m.rounds.length > 0;
    var o = survWordR(asset);
    var realNone = !(S.type === 'convoy' ? S.veh.some(function (v) { return v.dead; })
                                         : (S.everSuppressed || S.eff > 0));
    if (S.type === 'kaiju') return o === 'destroyed';
    if (o === 'destroyed' || o === 'neutralized') return true;
    return o === 'suppressed' && !realNone && !!m && m.intent === 'suppress';
  }
  function polarCallR() {
    var SH = _SH();
    var g = gridN(SH.OP.x, SH.OP.z, 6);
    var rep = SH.enToWorld(parseInt(g.slice(0, 3)) * 100 + 50, parseInt(g.slice(3)) * 100 + 50);
    var t = tgtR();
    var dir = Math.round(SH.radToMils(azR(rep.x, rep.z, t.x, t.z)) / 10) * 10 % 6400;
    var dist = Math.round(d2R(rep.x, rep.z, t.x, t.z) / 10) * 10;
    var dH = SH.H(t.x, t.z) - SH.OP.h;
    var vert = Math.abs(dH) >= 35
      ? ', ' + (dH > 0 ? 'up ' : 'down ') + (Math.round(Math.abs(dH) / 5) * 5) : '';
    return 'direction ' + dir + ', distance ' + dist + vert;
  }
  function shiftCallR() {
    var SH = _SH(), S = SH.Scenario, t = tgtR();
    var kp = S.kps[0];
    for (var i = 0; i < S.kps.length; i++)
      if (d2R(S.kps[i].x, S.kps[i].z, t.x, t.z) < d2R(kp.x, kp.z, t.x, t.z)) kp = S.kps[i];
    var dir = Math.round(SH.radToMils(azR(SH.OP.x, SH.OP.z, t.x, t.z)) / 10) * 10 % 6400;
    var azr = dir * Math.PI * 2 / 6400;
    var fx = Math.sin(azr), fz = -Math.cos(azr), rx = Math.cos(azr), rz = Math.sin(azr);
    var dx = t.x - kp.x, dz = t.z - kp.z;
    var add = Math.round((dx * fx + dz * fz) / 100) * 100;
    var right = Math.round((dx * rx + dz * rz) / 10) * 10;
    var dH = SH.H(t.x, t.z) - SH.H(kp.x, kp.z);
    var vert = Math.abs(dH) >= 35
      ? ', ' + (dH > 0 ? 'up ' : 'down ') + (Math.round(Math.abs(dH) / 5) * 5) : '';
    return 'shift known point ' + kp.id + ', direction ' + dir +
           (right ? ', ' + (right > 0 ? 'right ' : 'left ') + Math.abs(right) : '') +
           (add ? ', ' + (add > 0 ? 'add ' : 'drop ') + Math.abs(add) : '') + vert;
  }
  function landmarkR() {
    var W = _SH().WORLD;
    return W.facilities.find(function (f) { return f.kind === 'fuel'; }) ||
           W.facilities.find(function (f) { return f.kind === 'mast'; }) ||
           W.facilities[0] ||
           (W.villages[0] ? { x: W.villages[0].x, z: W.villages[0].z } : null);
  }
  function mtoReadbackR() {
    var m = _SH().mission.mto;
    return m.keys.join(' ') + ' target number alpha alpha ' + m.tgt + ', over';
  }
  function corrToPointR(px, pz, fine) {
    var SH = _SH(), a = SH.mission.aim;
    var otAz = azR(SH.OP.x, SH.OP.z, a.x, a.z);
    var dx = px - a.x, dz = pz - a.z;
    var qa = fine ? 10 : 100;
    var add = Math.round((dx * Math.sin(otAz) + dz * -Math.cos(otAz)) / qa) * qa;
    var right = Math.round((dx * Math.cos(otAz) + dz * Math.sin(otAz)) / 10) * 10;
    var parts = [];
    if (right) parts.push((right > 0 ? 'right ' : 'left ') + Math.abs(right));
    if (add) parts.push((add > 0 ? 'add ' : 'drop ') + Math.abs(add));
    return parts.length ? parts.join(', ') : 'right 10';
  }
  function nearestFriendlyR() {
    var SH = _SH(), a = SH.mission ? SH.mission.aim : tgtR();
    var pts = friendlyPtsR();
    var best = pts[0];
    for (var i = 0; i < pts.length; i++)
      if (d2R(pts[i].x, pts[i].z, a.x, a.z) < d2R(best.x, best.z, a.x, a.z)) best = pts[i];
    return best;
  }
  function nearestHutR() {
    var SH = _SH(), a = SH.mission ? SH.mission.aim : tgtR();
    var best = null;
    _SH().WORLD.villages.forEach(function (v) { v.huts.forEach(function (h) {
      if (!best || d2R(h.x, h.z, a.x, a.z) < d2R(best.x, best.z, a.x, a.z)) best = h;
    }); });
    return best;
  }
  function plannedTgtR() {
    var m = null;
    _SH().TLOG.entries.forEach(function (e) {
      var mm = (e.msg || '').match(/TARGET (AB\\d{4}) PLANNED/);
      if (mm) m = mm[1];
    });
    return m;
  }
  function leadGridR(vel) {
    var SH = _SH(), t = tgtR();
    var B = SH.CONFIG.BALLISTICS, F = SH.CONFIG.FDC;
    var range = d2R(SH.BATTERY.x, SH.BATTERY.z, t.x, t.z);
    var tof = Math.min(B.tofMax, Math.max(B.tofMin, range / B.tofDivisor + B.tofBase));
    var lead = F.shotDelay + tof + 4;
    var vx = vel ? vel[0] : 0, vz = vel ? vel[1] : 0;
    return gridN(t.x + vx * lead, t.z + vz * lead, 6);
  }
  function resolveAll(text, ctx) {
    var SH = _SH();
    var subs = {
      TGT_GRID6: function () { var t = tgtR(); return gridN(t.x, t.z, 6); },
      TGT_GRID8: function () { var t = tgtR(); return gridN(t.x, t.z, 8); },
      TGT_GRID10: function () { var t = tgtR(); return gridN(t.x, t.z, 10); },
      TWIN_GRID6: function () { var t = SH.Scenario.twinTgt; return gridN(t.x, t.z, 6); },
      OT_DIR: otDirR,
      OP_GRID6: function () { return gridN(SH.OP.x, SH.OP.z, 6); },
      CORR: function () { var p = corrPartsR(false, false); return p.length ? p.join(', ') : 'right 10'; },
      CORR_FFE: function () { var p = corrPartsR(true, false); p.push('fire for effect'); return p.join(', '); },
      CREEP: function () { var p = corrPartsR(false, true); return p.length ? p.join(', ') : 'drop 100'; },
      CREEP_FFE: function () { var p = corrPartsR(true, true); p.push('fire for effect'); return p.join(', '); },
      DC: function () { return dcWordR(ctx.asset); },
      SURV: function () { return survWordR(ctx.asset); },
      POLAR_CALL: polarCallR,
      SHIFT_CALL: shiftCallR,
      LANDMARK_GRID6: function () { var lm = landmarkR(); return gridN(lm.x, lm.z, 6); },
      VEH_HEAD_GRID6: function () {
        var v = SH.Scenario.veh.find(function (v) { return !v.dead; });
        return gridN(v.x, v.z, 6);
      },
      MTO_READBACK: mtoReadbackR,
      PLANNED_TGT: plannedTgtR,
      CORR_TO_FRIENDLY: function () { var f = nearestFriendlyR(); return corrToPointR(f.x, f.z, true); },
      CORR_TO_HUT: function () { var h = nearestHutR(); return corrToPointR(h.x, h.z, true); },
      CORR_TO_TGT: function () { var t = tgtR(); return corrToPointR(t.x, t.z, false); },
      CORR_TO_TWIN: function () { var t = SH.Scenario.twinTgt; return corrToPointR(t.x, t.z, false); },
      CORR_TO_VEH: function () {
        var v = SH.Scenario.veh.find(function (v) { return !v.dead; });
        return corrToPointR(v.x, v.z, false);
      },
      LEAD_GRID6: function () { return leadGridR(ctx.vel); },
    };
    return text.replace(/\\{([A-Z0-9_]+)\\}/g, function (m0, k) {
      if (!subs[k]) throw new Error('unknown placeholder ' + k);
      return subs[k]();
    });
  }
`;

/* ---------------- browser session ------------------------------------------ */
async function openSession() {
  const browser = await chromium.launch({ channel: 'chrome', headless: !opt.headed });
  const ctx = await browser.newContext({ viewport: VIEWPORT });
  await ctx.addInitScript(`(() => {
    let a = ${MATH_RANDOM_SEED} | 0;
    Math.random = function () {
      a = a + 0x6D2B79F5 | 0;
      let t = Math.imul(a ^ a >>> 15, 1 | a);
      t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  })()`);
  const page = await ctx.newPage();
  await page.clock.install({ time: EPOCH });
  const errors = [];
  await page.route(/^https?:\/\//, r => r.abort());
  page.on('pageerror', e => errors.push('pageerror: ' + e.message));
  page.on('console', m => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  await page.goto('file:///' + simPath.replace(/\\/g, '/'));
  await page.waitForFunction(() => window.SHITFIRE && window.SHITFIRE.Scenario, null, { timeout: 60000 });
  await page.clock.pauseAt(PAUSE_AT);
  await page.evaluate(r => { window.SHITFIRE.perf.freeze(true); window.SHITFIRE.perf.setRatio(r); }, RENDER_RATIO);

  const S = {
    browser, page, errors,
    async close() {
      await browser.close();
      if (errors.length) throw new Error('page errors during capture:\n  ' + errors.join('\n  '));
    },
    frames: ms => page.clock.runFor(ms),
    evaluate: (fn, arg) => page.evaluate(fn, arg),
    lib: expr => page.evaluate(`(() => { ${PAGE_LIB}\n return ${expr}; })()`),
    resolve: (text, ctx) => page.evaluate(
      ({ lib, text, ctx }) => { eval(lib); return resolveAll(text, ctx); },   // eslint-disable-line no-eval
      { lib: RESOLVER, text, ctx }),
    state: () => page.evaluate(() =>
      (document.getElementById('status').textContent.match(/STATE (\S+)/) || [])[1] || ''),
    send: raw => page.evaluate(s => window.SHITFIRE.onPlayerMessage(s), raw),
    press: k => page.keyboard.press(k),
    /* advance the fake clock until pred() holds; the poll sequence is the same
       every run, so the polling itself cannot introduce nondeterminism */
    async until(pred, capSimMs, what) {
      let spent = 0;
      while (spent <= capSimMs) {
        if (await pred()) return;
        await page.clock.runFor(STEP_MS);
        spent += STEP_MS;
      }
      throw new Error(`timed out after ${capSimMs} sim-ms waiting for ${what}`);
    },
    async untilState(target, capSimMs = 600_000) {
      const want = Array.isArray(target) ? target : [target];
      await S.until(async () => want.includes(await S.state()), capSimMs, 'state ' + want.join('|'));
    },
    roundsCount: () => page.evaluate(() => {
      const m = window.SHITFIRE.mission;
      return m ? m.rounds.length : 0;
    }),
  };
  return S;
}

/* ---------------- menu driving --------------------------------------------- */
async function toMenuFromTitle(S) {
  await S.page.click('#tsMenu');                     // dismiss title → mission menu
  await S.frames(64);
}
async function ensureMenu(S) {
  const open = await S.evaluate(() => document.getElementById('menu').classList.contains('on'));
  if (!open) {
    await S.press('Escape');                         // close briefing/AAR/map if any
    await S.frames(32);
    await S.press('k');
    await S.frames(32);
  }
}
async function devUnlock(S) {
  const on = await S.evaluate(() =>
    document.getElementById('mdevunlock').textContent.includes('ON'));
  if (!on) { await S.page.click('#mdevunlock'); await S.frames(32); }
}
async function setDifficulty(S, diff) {
  await S.page.click(`#mdiffs .mbtn:has-text("${diff.toUpperCase()}")`);
  await S.frames(32);
}
async function launchChapter(S, chId) {
  const volTab = await S.evaluate(id => {
    const v = window.SHITFIRE.CAMPAIGN.find(v => v.chapters.some(c => c.id === id));
    return v ? v.tab : null;
  }, chId);
  if (!volTab) throw new Error('no such chapter ' + chId);
  await S.page.click(`#mvols .mbtn:text-is("${volTab}")`);
  await S.frames(16);
  const rows = await S.page.$$('#mchapters .chrow');
  for (const r of rows) {
    const id = await r.$eval('.chid', el => el.textContent).catch(() => null);
    if (id === chId) { await r.click(); await S.frames(16); return; }
  }
  throw new Error('chapter row not clickable: ' + chId);
}
async function launchSkirmish(S, type) {
  const name = SCN_NAME[type];
  if (!name) throw new Error('unknown skirmish type ' + type);
  await S.page.click(`#mtypes .mbtn:text-is("${name}")`);
  await S.frames(16);
}
async function closeBriefing(S) {
  const on = await S.evaluate(() => document.getElementById('brief').classList.contains('on'));
  if (on) { await S.press('Escape'); await S.frames(32); }
}

/* ============================================================ LEVEL 1 ====== */
async function captureLevel1() {
  console.log('LEVEL 1 — pure-function fixtures');
  const S = await openSession();
  try {
    // --- prng_streams.json --------------------------------------------------
    const prng = await S.lib(`(() => {
      const SH = window.SHITFIRE;
      const seeds = ${JSON.stringify(PRNG_SEEDS)};
      const out = {};
      for (const seed of seeds) {
        const mine = u32Mulberry(seed);
        const theirs = SH.mulberry32(seed);
        const u32 = new Array(${PRNG_DRAWS});
        const dbl = new Array(${PRNG_DRAWS});
        for (let i = 0; i < ${PRNG_DRAWS}; i++) {
          const u = mine();
          const d = u / 4294967296;
          const ref = theirs();
          if (d !== ref)
            throw new Error('u32-instrumented mulberry32 diverged from the sim at seed ' +
                            seed + ' draw ' + i + ': ' + d + ' vs ' + ref);
          u32[i] = u; dbl[i] = d;
        }
        out[seed] = { u32, double: dbl };
      }
      return out;
    })()`);
    writeFixture('level1/prng_streams.json', {
      what: 'mulberry32 streams — the one thing that must be bit-exact (PARITY_TESTING.md)',
      algorithm: 'a|=0; a=a+0x6D2B79F5|0; t=imul(a^a>>>15,1|a); t=t+imul(t^t>>>7,61|t)^t; u32=(t^t>>>14)>>>0; double=u32/4294967296',
      verification: 'a u32-instrumented copy ran in-page; its doubles matched window.SHITFIRE.mulberry32 for every draw of every seed (mismatch aborts the capture)',
      draws: PRNG_DRAWS, seeds: PRNG_SEEDS, streams: prng,
    }, 0);

    // --- hash_noise.json ----------------------------------------------------
    const hn = await S.lib(`(() => {
      const SH = window.SHITFIRE;
      const out = { hash2: [], vnoise: [], fbm: [] };
      const seeds = [0, 7, 1337, 1344, 9028, 5157, 66607, 131];
      for (const s of seeds)
        for (let ix = -6; ix <= 6; ix += 3)
          for (let iz = -6; iz <= 6; iz += 3)
            out.hash2.push({ ix, iz, seed: s, v: SH.hash2(ix, iz, s) });
      const vseeds = [7, 1344, 9028, 66607];
      for (const s of vseeds)
        for (let i = 0; i < 60; i++) {
          const x = -41.7 + i * 1.437, z = 52.9 - i * 1.911;
          out.vnoise.push({ x, z, seed: s, v: SH.vnoise(x, z, s) });
        }
      for (const oct of [4, 5])
        for (const s of vseeds)
          for (let i = 0; i < 40; i++) {
            const x = -6.31 + i * 0.317, z = 5.87 - i * 0.293;
            out.fbm.push({ x, z, seed: s, oct, v: SH.fbm(x, z, s, oct) });
          }
      return out;
    })()`);
    writeFixture('level1/hash_noise.json', {
      what: 'hash2 / vnoise / fbm sample sets incl. negative and fractional inputs; fbm at oct 4 and 5',
      counts: { hash2: hn.hash2.length, vnoise: hn.vnoise.length, fbm: hn.fbm.length },
      samples: hn,
    }, 0);

    // --- grid_roundtrip.json ------------------------------------------------
    const grid = await S.lib(`(() => {
      const SH = window.SHITFIRE;
      const xs = [-5000, -4999.999999999, -4975, -2500.5, -1000.0000001, -123.456,
                  -0.5, 0, 0.5, 49.999999999, 50, 50.000000001, 123.456, 999.995,
                  2499.5, 4949.999999, 4975, 4999.999999999];
      const worldToEN = [], gridOf = [], enToWorld = [], radToMils = [];
      for (const x of xs) for (const z of [x, -x, x / 3]) {
        const en = SH.worldToEN(x, z);
        worldToEN.push({ x, z, e: en.e, n: en.n });
        gridOf.push({ x, z, grid: SH.gridOf(x, z) });
      }
      const C = SH.CONFIG.MAP;
      for (const de of [0, 0.4999999, 5, 50.5, 3333.33, 9999.999]) {
        const e = C.originE + de, n = C.originN + 10000 - de;
        const w = SH.enToWorld(e, n);
        enToWorld.push({ e, n, x: w.x, z: w.z });
      }
      const angs = [-13.7, -6.4, -Math.PI, -1e-12, 0, 1e-12, 0.1, 1, Math.PI,
                    Math.PI * 2, 6.9, 13.1, 100.001];
      for (const a of angs) radToMils.push({ rad: a, mils: SH.radToMils(a) });
      return { worldToEN, gridOf, enToWorld, radToMils,
               map: { originE: C.originE, originN: C.originN } };
    })()`);
    writeFixture('level1/grid_roundtrip.json', {
      what: 'worldToEN / enToWorld / gridOf / radToMils incl. negatives, edges, 8-digit-relevant precision points',
      cases: grid,
    });

    // --- hxz_proc_<seed>.json ----------------------------------------------
    for (const seed of TERRAIN_SEEDS) {
      const res = await S.evaluate(({ seed, g }) => {
        const SH = window.SHITFIRE;
        const saved = SH.CONFIG.SEED.terrain;
        SH.qaSetDem(0, null);                 // procedural path, guaranteed
        SH.CONFIG.SEED.terrain = seed;        // H_proc reads it live
        const grid = new Array(g.n * g.n);
        try {
          for (let j = 0; j < g.n; j++)
            for (let i = 0; i < g.n; i++)
              grid[j * g.n + i] = SH.H(g.from + i * g.step, g.from + j * g.step);
        } finally {
          SH.CONFIG.SEED.terrain = saved;     // restored inside the same evaluate —
        }                                     // no frame can interleave
        const T = {};
        for (const k of Object.keys(SH.CONFIG.TERRAIN))
          if (typeof SH.CONFIG.TERRAIN[k] === 'number') T[k] = SH.CONFIG.TERRAIN[k];
        return { grid, terrainConfig: T };
      }, { seed, g: H_GRID });
      writeFixture(`level1/hxz_proc_${seed}.json`, {
        what: `H(x,z) procedural island, terrain seed ${seed}`,
        grid: { xFrom: H_GRID.from, zFrom: H_GRID.from, step: H_GRID.step, n: H_GRID.n,
                order: 'value[j*n+i] = H(xFrom+i*step, zFrom+j*step)' },
        terrainSeed: seed, terrainConfig: res.terrainConfig, values: res.grid,
      }, 0);
    }

    // --- hxz_dem.json -------------------------------------------------------
    const dem = await S.lib(`(() => {
      const SH = window.SHITFIRE;
      const D = ${JSON.stringify(DEM_SPEC)};
      const g = ${JSON.stringify(H_GRID)};
      const data = new Float64Array(D.n * D.n);
      for (let j = 0; j < D.n; j++)
        for (let i = 0; i < D.n; i++)
          data[j * D.n + i] = D.scale * SH.hash2(i, j, D.seed) + D.offset;
      const edgeXs = [-5000, -4999.9999, -4990.123, -0.001, 0, 9.7656, 4999.999999995,
                      4999.9999, 5000, 5001, -5001, 2500.0000001];
      SH.qaSetDem(D.n, data);
      let grid;
      const edges = [];
      try {
        grid = new Array(g.n * g.n);
        for (let j = 0; j < g.n; j++)
          for (let i = 0; i < g.n; i++)
            grid[j * g.n + i] = SH.H(g.from + i * g.step, g.from + j * g.step);
        for (const x of edgeXs) for (const z of edgeXs.slice(0, 9))
          edges.push({ x, z, h: SH.H(x, z) });
      } finally {
        SH.qaSetDem(0, null);                 // procedural terrain restored —
      }                                       // set+sample+restore in ONE evaluate
      return { grid, edges, restoredH00: SH.H(0, 0) };
    })()`);
    writeFixture('level1/hxz_dem.json', {
      what: 'H(x,z) with a synthetic deterministic DEM injected via qaSetDem; exercises the bilinear demSample path',
      demSpec: DEM_SPEC,
      grid: { xFrom: H_GRID.from, zFrom: H_GRID.from, step: H_GRID.step, n: H_GRID.n,
              order: 'value[j*n+i] = H(xFrom+i*step, zFrom+j*step)' },
      note: 'regenerate the DEM in the port test from demSpec.formula; out-of-map samples return CONFIG.TERRAIN.oceanFloor',
      edgeCases: dem.edges, values: dem.grid,
    }, 0);

    // --- raylos.json --------------------------------------------------------
    const raylos = await S.lib(`(() => {
      const SH = window.SHITFIRE;
      if (SH.CONFIG.SEED.terrain !== 1337) throw new Error('terrain seed drifted');
      SH.qaSetDem(0, null);
      const f = u32Mulberry(${RAY_SEED});
      const rng = () => f() / 4294967296;
      const rays = [];
      for (let i = 0; i < 500; i++) {
        const ox = -4000 + rng() * 8000, oz = -4000 + rng() * 8000;
        const oy = Math.max(SH.H(ox, oz), 0) + 5 + rng() * 115;
        const az = rng() * Math.PI * 2, pitch = -0.5 + rng() * 0.55;
        const dx = Math.sin(az) * Math.cos(pitch), dy = Math.sin(pitch),
              dz = -Math.cos(az) * Math.cos(pitch);
        const hit = SH.groundHit(ox, oy, oz, dx, dy, dz, 6000);
        rays.push({ o: [ox, oy, oz], d: [dx, dy, dz],
                    hit: hit ? { x: hit.x, y: hit.y, z: hit.z, dist: hit.dist } : null });
      }
      const los = [];
      for (let i = 0; i < 500; i++) {
        const ax = -4000 + rng() * 8000, az2 = -4000 + rng() * 8000;
        const bx = -4000 + rng() * 8000, bz = -4000 + rng() * 8000;
        const ay = Math.max(SH.H(ax, az2), 0) + 2 + rng() * 38;
        const by = Math.max(SH.H(bx, bz), 0) + 1 + rng() * 29;
        los.push({ a: [ax, ay, az2], b: [bx, by, bz], vis: SH.hasLOS(ax, ay, az2, bx, by, bz) });
      }
      return { rays, los };
    })()`);
    writeFixture('level1/raylos.json', {
      what: '500 groundHit rays + 500 hasLOS pairs on procedural terrain seed 1337',
      generator: `in-script mulberry32(${RAY_SEED}) drives all origins/directions; the literal values are stored in rays[].o/.d and los[].a/.b`,
      terrainSeed: 1337, maxDist: 6000,
      rays: raylos.rays, los: raylos.los,
    }, 0);
  } finally { await S.close(); }

  // --- spotburst.json (needs a LIVE Scenario/OP — drive chapter 1.1) --------
  const S2 = await openSession();
  try {
    await toMenuFromTitle(S2);
    await devUnlock(S2);
    await setDifficulty(S2, 'normal');
    await launchChapter(S2, '1.1');
    await closeBriefing(S2);
    const sb = await S2.evaluate(() => {
      const SH = window.SHITFIRE;
      const S = SH.Scenario;
      const cases = [];
      const radii = [30, 60, 100, 150, 220, 300, 400, 600, 800, 1200];
      for (const r of radii)
        for (let k = 0; k < 20; k++) {
          const th = k * Math.PI * 2 / 20;
          const impact = { x: S.enemy.x + r * Math.sin(th), z: S.enemy.z + r * Math.cos(th) };
          const sp = SH.spotBurst(impact);
          cases.push({ impact: [impact.x, impact.z],
                       mils: sp.mils, otf: sp.otf, otRange: sp.otRange,
                       overShort: sp.overShort, devCorr: sp.devCorr });
        }
      const otf = [];
      for (let range = 0; range <= 12000; range += 250)
        otf.push({ range, otf: SH.otFactor(range) });
      return { cases, otf,
        identity: { chapter: '1.1', seedMission: SH.CONFIG.SEED.mission,
                    seedTerrain: SH.CONFIG.SEED.terrain, difficulty: S.difficulty,
                    OP: { x: SH.OP.x, z: SH.OP.z, h: SH.OP.h },
                    enemy: { x: S.enemy.x, z: S.enemy.z } } };
    });
    writeFixture('level1/spotburst.json', {
      what: 'spotBurst for 200 synthetic impacts around the live target + otFactor range sweep',
      offsets: 'rings r in [30,60,100,150,220,300,400,600,800,1200] x 20 angles (k*2*pi/20), impact = enemy + (r*sin(th), r*cos(th))',
      capturedUnder: sb.identity, otFactorSweep: sb.otf, cases: sb.cases,
    }, 0);
  } finally { await S2.close(); }
  console.log('LEVEL 1 complete.');
}

/* ============================================================ LEVEL 2 ====== */
async function captureLevel2() {
  console.log('LEVEL 2 — scenario generation fixtures');

  // --- gen_<type>_<seed>.json on default terrain (no chapter driven) --------
  const S = await openSession();
  try {
    await toMenuFromTitle(S);            // title dismissed; NO chapter launched
    const menuTypes = await S.evaluate(() =>
      [...document.querySelectorAll('#mtypes .mbtn')].map(b => b.textContent));
    const typeKeys = Object.keys(SCN_NAME).filter(k => menuTypes.includes(SCN_NAME[k]));
    if (typeKeys.length !== 15)
      throw new Error('expected 15 skirmish types on the menu, matched ' + typeKeys.length +
                      ' — menu shows: ' + menuTypes.join(' | '));
    for (const type of typeKeys) {
      for (const seed of GEN_SEEDS) {
        const snap = await S.lib(`(() => {
          const SH = window.SHITFIRE;
          const s = SH.genScenario(${JSON.stringify(type)}, ${seed});
          return { scenario: serialize(s),
                   meta: { type: ${JSON.stringify(type)}, seed: ${seed},
                           terrainSeed: SH.CONFIG.SEED.terrain,
                           difficulty: s.difficulty } };
        })()`);
        writeFixture(`level2/gen_${type}_${seed}.json`, {
          what: `genScenario('${type}', ${seed}) on default terrain — plain-data snapshot (functions/THREE dropped)`,
          meta: snap.meta, scenario: snap.scenario,
        });
      }
      console.log('  genScenario', type, 'seeds', GEN_SEEDS.join('/'));
    }
  } finally { await S.close(); }

  // --- ch_<id>_<diff>.json — drive the real bookshelf menu ------------------
  const S2 = await openSession();
  try {
    await toMenuFromTitle(S2);
    await devUnlock(S2);
    const chapters = await S2.evaluate(() =>
      window.SHITFIRE.CAMPAIGN.flatMap(v => v.chapters
        .filter(c => c.impl)
        .map(c => ({ id: c.id, diffs: c.diffs || ['easy', 'normal', 'hard'] }))));
    for (const ch of chapters) {
      for (const diff of ch.diffs) {
        await ensureMenu(S2);
        await setDifficulty(S2, diff);
        await launchChapter(S2, ch.id);
        // snapshot at the same fake-clock instant as generation — zero frames later
        const snap = await S2.lib(`(() => {
          const SH = window.SHITFIRE;
          const W = SH.WORLD;
          return {
            meta: { chapter: ${JSON.stringify(ch.id)}, difficulty: SH.Scenario.difficulty,
                    seedMission: SH.CONFIG.SEED.mission, seedTerrain: SH.CONFIG.SEED.terrain },
            scenario: serialize(SH.Scenario),
            world: serialize({ roads: W.roads, paths: W.paths, facilities: W.facilities,
                               villages: W.villages, civs: W.civs, rocks: W.rocks }),
            op: serialize(SH.OP), battery: serialize(SH.BATTERY),
          };
        })()`);
        writeFixture(`level2/ch_${ch.id.replace(/\./g, '_')}_${snap.meta.difficulty}.json`, {
          what: `chapter ${ch.id} launched from the mission menu at ${diff} — Scenario/WORLD/OP/BATTERY snapshot at generation time`,
          meta: snap.meta, scenario: snap.scenario, world: snap.world,
          op: snap.op, battery: snap.battery,
        });
        await closeBriefing(S2);
      }
      console.log('  chapter', ch.id, '(' + ch.diffs.join('/') + ')');
    }
  } finally { await S2.close(); }
  console.log('LEVEL 2 complete.');
}

/* ============================================================ LEVEL 3 ====== */
async function runStep(S, step, ctx) {
  if (step.wait) {
    const w = step.wait;
    const cap = w.capMs || 600_000;
    if (w.simMs !== undefined) await S.frames(w.simMs);
    else if (w.state) await S.untilState(w.state, cap);
    else if (w.tlogMsg) {
      await S.until(() => S.evaluate(src => {
        const re = new RegExp(src, 'i');
        return window.SHITFIRE.TLOG.entries.some(e => re.test(e.msg || ''));
      }, w.tlogMsg), cap, 'tlog /' + w.tlogMsg + '/');
    }
    else if (w.netQuiet) {
      await S.until(() => S.evaluate(() =>
        window.SHITFIRE.FDC.lastT <= performance.now() / 1000 + 0.05), cap, 'net quiet');
    }
    else if (w.roundLanded !== undefined) {
      await S.until(() => S.evaluate(n => {
        const m = window.SHITFIRE.mission;
        return !!m && m.rounds.length >= n;
      }, w.roundLanded), cap, 'round ' + w.roundLanded);
    }
    else if (w.stopped) {
      let prev = null;
      await S.until(async () => {
        const cur = await S.evaluate(() => {
          const v = window.SHITFIRE.Scenario.veh.find(v => !v.dead);
          return v ? [v.x, v.z] : null;
        });
        const still = prev && cur &&
          Math.hypot(cur[0] - prev[0], cur[1] - prev[1]) < 0.05;
        prev = cur;
        return !!still;
      }, cap, 'convoy stopped');
    }
  }
  if (step.measureVel) {
    const p0 = await S.evaluate(() => [window.SHITFIRE.Scenario.enemy.x, window.SHITFIRE.Scenario.enemy.z]);
    await S.frames(1024);
    const p1 = await S.evaluate(() => [window.SHITFIRE.Scenario.enemy.x, window.SHITFIRE.Scenario.enemy.z]);
    ctx.vel = [(p1[0] - p0[0]) / 1.024, (p1[1] - p0[1]) / 1.024];
  }
  if (step.send) {
    const text = await S.resolve(step.send, ctx);
    await S.send(text);
    await S.frames(64);
    if (step.resendOnRefusal) {
      /* the dangerous-deviation and creeping-fire gates refuse once and accept
         the identical resend; the danger-close gate accepts no override, so a
         refused call is resent WITH the proword. A fixed number of attempts —
         deterministic either way. */
      let attempt = text;
      for (let att = 0; att < 4; att++) {
        await S.frames(4096);
        const fired = await S.roundsCount() >= step.resendOnRefusal;
        const st = await S.state();
        if (fired || st === 'SHOT' || st === 'FIRE FOR EFFECT' || st === 'MISSION SENT') break;
        if (att >= 1 && !/danger close/.test(attempt))
          attempt = attempt.replace(/, (over|out)\s*$/, ', danger close, $1');
        await S.send(attempt);
        await S.frames(64);
      }
    }
  }
  if (step.press) { await S.press(step.press); await S.frames(32); }
  if (step.lookat) {
    if (step.lookat === 'enemy')
      await S.evaluate(() => { const SH = window.SHITFIRE; SH.qaLookAt(SH.Scenario.enemy.x, SH.Scenario.enemy.z); });
    else if (step.lookat === 'landmark')
      await S.page.evaluate(({ lib }) => { eval(lib);   // eslint-disable-line no-eval
        const lm = landmarkR();
        window.SHITFIRE.qaLookAt(lm.x, lm.z); }, { lib: RESOLVER });
    else await S.evaluate(p => window.SHITFIRE.qaLookAt(p[0], p[1]), step.lookat);
    await S.frames(48);
  }
  if (step.sweep) {
    for (let i = 0; i < step.sweep; i++) {
      await S.evaluate(k => {
        const SH = window.SHITFIRE;
        const a = (k % 2 ? 1 : -1) * (1.2 + 0.3 * k);
        SH.qaLookAt(SH.OP.x + Math.sin(a) * 1000, SH.OP.z - Math.cos(a) * 1000);
      }, i);
      await S.frames(64);
    }
  }
  if (step.ensureFail) {
    /* deliberately keep firing at the current (bad) aimpoint until the sim
       records the failure — fratricide or collateral — then stop */
    const want = step.ensureFail.reason;
    for (let i = 0; i < (step.ensureFail.maxExtra || 4); i++) {
      const failed = await S.evaluate(() => window.SHITFIRE.mission.failReason);
      if (failed) break;
      const n = await S.roundsCount();
      await S.send('repeat, over');
      await S.frames(64);
      await S.until(async () => (await S.roundsCount()) > n ||
        !!(await S.evaluate(() => window.SHITFIRE.mission.failReason)), 200_000, 'fail round');
    }
    const got = await S.evaluate(() => window.SHITFIRE.mission.failReason);
    if (got !== want) throw new Error(`ensureFail wanted ${want}, mission.failReason is ${got}`);
  }
  if (step.adjust) {
    /* auto-adjust primitive: correct off the sim's own spotBurst arithmetic
       until the miss closes, enter fire for effect, REPEAT volleys until the
       mission accomplishes or the round budget runs out. Deterministic. */
    const A = step.adjust;
    const maxRounds = A.maxRounds || 12;
    const ffeAt = A.ffeWhenM || 60;
    // a declared danger-close mission is corrected by creeping fire (<=100 m)
    const creep = !!A.creep || await S.evaluate(() =>
      !!(window.SHITFIRE.mission && window.SHITFIRE.mission.gridStr.includes('DANGER CLOSE')));
    /* wait for the sent correction to take effect. The dangerous-deviation,
       creeping-fire and near-village gates each refuse once and accept the
       IDENTICAL resend; a correction can trip several in sequence (converging
       on a target next to friendlies does), so resend on a fixed cadence while
       the refusal state persists. Polls ride the same 512 ms grid as S.until,
       so an ACCEPTED correction keeps timing identical to a plain wait. */
    const overrideWait = async (text, prevRounds, refuseState, doneStates, capMs, what, sends0) => {
      let spent = 0, sinceSend = 0, sends = sends0;
      while (spent <= capMs) {
        const n = await S.roundsCount();
        const st = await S.state();
        if (n > prevRounds || doneStates.includes(st)) return;
        if (st === refuseState && sinceSend >= 4096 && sends < 5) {
          await S.send(text);
          await S.frames(64);
          sends++; sinceSend = 0; spent += 64;
          continue;
        }
        await S.frames(STEP_MS);
        spent += STEP_MS; sinceSend += STEP_MS;
      }
      throw new Error(`timed out after ${capMs} sim-ms waiting for ${what}`);
    };
    const sendAndOverride = async (text, prevRounds, refuseState, doneStates, capMs, what) => {
      await S.send(text);
      await S.frames(64);
      await overrideWait(text, prevRounds, refuseState, doneStates, capMs, what, 1);
    };
    for (let guard = 0; guard < maxRounds + 8; guard++) {
      await S.until(async () => ['ADJUSTING', 'EOM?', 'AAR'].includes(await S.state()),
        400_000, 'adjust settle');
      const st = await S.state();
      if (st === 'AAR') return;
      const info = await S.evaluate(() => {
        const m = window.SHITFIRE.mission;
        return { rounds: m.rounds.length, miss: m.lastAdjDist, done: m.done };
      });
      if (st === 'EOM?') {
        const acc = await S.page.evaluate(({ lib, asset }) => { eval(lib);   // eslint-disable-line no-eval
          return accomplishedR(asset); }, { lib: RESOLVER, asset: ctx.asset || null });
        if (acc || !A.repeat || info.rounds >= maxRounds) return;
        const rep = A.plainRepeat ? 'repeat, over'
                                  : await S.resolve('{CORR}, repeat, over', ctx);
        await sendAndOverride(rep, info.rounds, 'EOM?',
          ['FIRE FOR EFFECT', 'SHOT', 'ADJUSTING', 'AAR'], 300_000, 'repeat volley');
        continue;
      }
      if (info.rounds >= maxRounds) return;
      if (info.miss !== null && info.miss <= ffeAt) {
        const ffeText = await S.resolve(creep ? '{CREEP_FFE}, over' : '{CORR_FFE}, over', ctx);
        await sendAndOverride(ffeText, info.rounds, 'ADJUSTING',
          ['FIRE FOR EFFECT', 'SHOT', 'EOM?', 'AAR'], 300_000, 'volley start');
        await S.until(async () => ['EOM?', 'AAR'].includes(await S.state()), 400_000, 'volley');
        continue;
      }
      const text = await S.resolve(creep ? '{CREEP}, over' : '{CORR}, over', ctx);
      if (creep) {
        /* preserved shape from the first capture batch: an unconditional beat
           and one resend before the grid wait (creep missions were captured
           with this cadence; changing it would move their TLOG stamps) */
        await S.send(text);
        await S.frames(64);
        await S.frames(4096);
        const grew = await S.roundsCount() > info.rounds;
        const st2 = await S.state();
        if (st2 === 'ADJUSTING' && !grew) { await S.send(text); await S.frames(64); }
        await overrideWait(text, info.rounds, 'ADJUSTING',
          ['EOM?', 'AAR'], 300_000, 'next round', 2);
      } else {
        await sendAndOverride(text, info.rounds, 'ADJUSTING',
          ['EOM?', 'AAR'], 300_000, 'next round');
      }
    }
  }
}

async function runPlaybook(pb, index) {
  console.log(`  [${pb.id}] ${pb.chapter ? 'chapter ' + pb.chapter : 'skirmish ' + pb.skirmish} (${pb.difficulty})`);
  const S = await openSession();
  try {
    await toMenuFromTitle(S);
    await devUnlock(S);
    await setDifficulty(S, pb.difficulty);
    await S.evaluate(() => window.SHITFIRE.TLOG.clear());
    const ctx = { asset: pb.asset || null };
    if (pb.chapter) await launchChapter(S, pb.chapter);
    else await launchSkirmish(S, pb.skirmish);
    await closeBriefing(S);
    for (const step of pb.steps) await runStep(S, step, ctx);
    /* let ALL trailing traffic land — the AAR entry is scheduled at
       FDC.lastT + 1.6, which can sit many seconds behind a deep say-queue */
    for (let i = 0; i < 12; i++) {
      const gap = await S.evaluate(() =>
        window.SHITFIRE.FDC.lastT + 3.5 - performance.now() / 1000);
      if (gap <= 0) break;
      await S.frames(Math.ceil(gap * 1000 / 16) * 16);
    }
    await S.frames(2048);
    const out = await S.evaluate(() => {
      const SH = window.SHITFIRE;
      const entries = SH.TLOG.entries.map(e => { const { wall, ...rest } = e; return rest; });
      const s = SH.Scenario;
      return { entries,
        meta: { seedMission: SH.CONFIG.SEED.mission, seedTerrain: SH.CONFIG.SEED.terrain,
                scenarioType: s.type, scenarioSeed: s.seed, difficulty: s.difficulty } };
    });
    const aar = out.entries.filter(e => e.kind === 'aar').pop() || null;
    const nn = String(index + 1).padStart(2, '0');
    writeFixture(`level3/mission_${nn}_${pb.slug}.json`, {
      what: `scripted mission transcript — playbook ${pb.id}`,
      playbook: pb.id,
      meta: { ...out.meta, chapter: pb.chapter || null,
              skirmish: pb.skirmish || null, notes: pb.notes || '' },
      result: aar ? { verdict: aar.msg, stars: aar.stars !== undefined ? aar.stars : null,
                      outcome: aar.outcome, rounds: aar.rounds, dur: aar.dur } : null,
      tlog: out.entries,
    });
    return { id: pb.id, ok: true,
             verdict: aar ? aar.msg : '(no AAR entry — tutorial or open mission)',
             stars: aar && aar.stars !== undefined ? aar.stars : null };
  } finally { await S.close(); }
}

async function captureLevel3() {
  console.log('LEVEL 3 — scripted mission transcripts');
  const files = fs.readdirSync(missionsDir).filter(f => f.endsWith('.json')).sort();
  const filter = opt.missions ? String(opt.missions).split(',') : null;
  const results = [];
  for (let i = 0; i < files.length; i++) {
    const pb = JSON.parse(fs.readFileSync(path.join(missionsDir, files[i]), 'utf8'));
    if (filter && !filter.includes(pb.id)) continue;
    try {
      results.push(await runPlaybook(pb, i));
    } catch (err) {
      console.error(`  [${pb.id}] FAILED: ${err.message}`);
      results.push({ id: pb.id, ok: false, error: err.message });
      if (opt.bail) throw err;
    }
  }
  console.log('\nLEVEL 3 results:');
  for (const r of results)
    console.log(`  ${r.ok ? 'ok  ' : 'FAIL'} ${r.id}  ${r.ok
      ? r.verdict + (r.stars !== null && r.stars !== undefined ? ` (${r.stars} stars)` : '')
      : r.error}`);
  const failed = results.filter(r => !r.ok);
  if (failed.length) throw new Error(failed.length + ' playbook(s) failed');
}

/* ============================================================ MANIFEST ===== */
function sha256(p) { return createHash('sha256').update(fs.readFileSync(p)).digest('hex'); }
function buildManifest() {
  console.log('MANIFEST');
  let commit = 'unknown';
  try { commit = execSync('git rev-parse HEAD', { cwd: root }).toString().trim(); } catch (e) { void e; }
  const files = [];
  for (const lvl of ['level1', 'level2', 'level3']) {
    const dir = path.join(outDir, lvl);
    if (!fs.existsSync(dir)) continue;
    for (const f of fs.readdirSync(dir).sort()) {
      if (!f.endsWith('.json')) continue;
      const p = path.join(dir, f);
      files.push({ path: lvl + '/' + f, bytes: fs.statSync(p).size, sha256: sha256(p) });
    }
  }
  writeFixture('MANIFEST.json', {
    what: 'machine-readable inventory of the P0 parity fixtures (the golden-master oracle record)',
    captureCommit: commit,
    captureParameters: {
      clock: 'Playwright fake clock; epoch ' + EPOCH + ' ms UTC, paused at +30000 ms, advanced in ' +
             STEP_MS + ' ms steps (rAF fires every 16 ms of fake time; sim.now is the rAF timestamp)',
      mathRandomSeed: MATH_RANDOM_SEED,
      mathRandomNote: 'Math.random overridden with seeded mulberry32 from the harness so unscored flavor ' +
        '(quip pick, skirmish type draw) reproduces; PARITY_TESTING.md excludes quip selection from parity',
      renderRatio: RENDER_RATIO, viewport: VIEWPORT,
      hGrid: H_GRID, demSpec: DEM_SPEC, raySeed: RAY_SEED,
      prngSeeds: PRNG_SEEDS, prngDraws: PRNG_DRAWS,
      terrainSeeds: TERRAIN_SEEDS, genSeeds: GEN_SEEDS,
      tlogNote: 'wall (Date.now) stripped from every TLOG entry — capture-time noise, not sim state',
      comparatorNote: 'the PRESENCE of probability-gated FDC tail/snark lines in these transcripts ' +
        'follows the harness\'s seeded Math.random, not the sim seed (DETERMINISM_NOTE.md §a). A port ' +
        'comparator must diff parse classifications, impacts, FDC decision points and the AAR — ' +
        'never raw FDC-line lists.',
    },
    counts: {
      level1: files.filter(f => f.path.startsWith('level1/')).length,
      level2: files.filter(f => f.path.startsWith('level2/')).length,
      level3: files.filter(f => f.path.startsWith('level3/')).length,
    },
    files,
  });
}

/* ============================================================ MAIN ========= */
const t0 = Date.now();
try {
  if (!['level1', 'level2', 'level3', 'manifest', 'all'].includes(cmd))
    throw new Error('unknown command ' + cmd);
  if (cmd === 'level1' || cmd === 'all') await captureLevel1();
  if (cmd === 'level2' || cmd === 'all') await captureLevel2();
  if (cmd === 'level3' || cmd === 'all') await captureLevel3();
  if (cmd === 'manifest' || cmd === 'all') buildManifest();
} catch (err) {
  console.error('\noracle-capture FAILED:', err.stack || err.message);
  process.exit(1);
}
console.log(`\noracle-capture ${cmd} done in ${((Date.now() - t0) / 60000).toFixed(1)} min → ${outDir}`);
