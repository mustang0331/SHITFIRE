/* tools/perfprobe.js — the frame-budget probe (row PERF1).
   Run: node tools/perfprobe.js

   Boots SHITFIRE.html in real Chrome and answers ONE question with numbers
   instead of suspicion: where does the frame go, and does glassing change it?

   Method: measure sustained frame time from inside the page with rAF, then run
   Chrome's SAMPLING PROFILER over the same window via CDP and aggregate SELF
   time by function. Self time is the honest metric here — a wrapper that calls
   an expensive child should not be blamed for the child.

   Three states, because the row's prime suspect is the bino quality pin:
     eye   — naked eye, 60 deg fov, whatever tier the adaptive system settled on
     7X    — binos up: fov 8.57, and pixel ratio PINNED to tier 0 (1.75)
     14X   — binos up: fov 4.29, same pin

   READ-ONLY against SHITFIRE.html per CLAUDE.md's dev-tooling rules: this file
   loads the artifact and observes it. It never writes it. */
import { chromium } from 'playwright-core';
import { fileURLToPath } from 'url';
import path from 'path';

const here = path.dirname(fileURLToPath(import.meta.url));
const simPath = path.join(here, '..', 'SHITFIRE.html');

const SAMPLE_MS = 4000;          // per state; long enough to ride out GC and jitter
const errors = [];

/* deviceScaleFactor is the whole ballgame and the reason a naive headless run
   says "no problem here." Headless Chrome reports devicePixelRatio = 1, so
   applyQuality's `Math.min(devicePixelRatio, 1.75)` collapses to 1 and the
   pixel-ratio pin the row suspects NEVER ENGAGES. On a real high-DPI laptop it
   resolves to 1.75 and the drawing buffer is (1.75/1)^2 = 3.06x the pixels.
   Pass a scale factor to reproduce that:  node tools/perfprobe.js 2          */
const DSF = Number(process.argv[2] || 1);
const VW = Number(process.argv[3] || 1600), VH = Number(process.argv[4] || 900);

const browser = await chromium.launch({ channel: 'chrome', headless: true });
const page = await browser.newPage({
  viewport: { width: VW, height: VH }, deviceScaleFactor: DSF });
page.on('pageerror', e => errors.push('pageerror: ' + e.message));
page.on('console', m => { if (m.type() === 'error') errors.push('console: ' + m.text()); });

await page.goto('file:///' + simPath.replace(/\\/g, '/'));
await page.waitForFunction(() => window.SHITFIRE && window.SHITFIRE.Scenario, null, { timeout: 30000 });
await page.waitForTimeout(3000);   // let boot traffic land and the adaptive tier settle

const env = await page.evaluate(() => {
  const c = document.getElementById('gl');
  return { dpr: devicePixelRatio, css: [innerWidth, innerHeight],
           buf: [c.width, c.height], mpix: +(c.width * c.height / 1e6).toFixed(2) };
});
console.log(`env: devicePixelRatio ${env.dpr} · CSS ${env.css[0]}x${env.css[1]} · ` +
            `drawing buffer ${env.buf[0]}x${env.buf[1]} = ${env.mpix} MPix`);

const cdp = await page.context().newCDPSession(page);
await cdp.send('Profiler.enable');
await cdp.send('Profiler.setSamplingInterval', { interval: 100 });   // microseconds

/* Frame timing, measured in-page. Returns the distribution, not just a mean:
   a mean of 16 ms with a p95 of 40 ms is a stuttery view, and "laggy" is a
   complaint about the tail, not the average. */
async function frameStats(ms) {
  return page.evaluate(async (dur) => {
    const t = [];
    let last = performance.now();
    await new Promise(res => {
      const stop = last + dur;
      (function tick(now) {
        t.push(now - last); last = now;
        if (now < stop) requestAnimationFrame(tick); else res();
      })(last);
    });
    t.shift();                       // first delta straddles the setup
    const s = t.slice().sort((a, b) => a - b);
    const sum = t.reduce((a, b) => a + b, 0);
    return {
      frames: t.length,
      fps: +(1000 / (sum / t.length)).toFixed(1),
      mean: +(sum / t.length).toFixed(2),
      p50: +s[Math.floor(s.length * 0.5)].toFixed(2),
      p95: +s[Math.floor(s.length * 0.95)].toFixed(2),
      max: +s[s.length - 1].toFixed(2),
      over16: +(100 * t.filter(x => x > 16.7).length / t.length).toFixed(1),
    };
  }, ms);
}

/* Aggregate the sampling profile by SELF time. Chrome reports hitCount per
   node; self time = hits * interval. Anonymous/native frames are kept and
   labelled, because "(program)" and "(garbage collector)" being at the top is
   itself a finding. */
function topSelf(profile, n = 14) {
  const total = profile.endTime - profile.startTime;
  const byName = new Map();
  let hits = 0;
  for (const node of profile.nodes) {
    if (!node.hitCount) continue;
    hits += node.hitCount;
    const f = node.callFrame;
    const name = (f.functionName || '(anonymous)') +
      (f.url && f.url.startsWith('data:') ? ' [three]' : '');
    byName.set(name, (byName.get(name) || 0) + node.hitCount);
  }
  return { total, rows: [...byName.entries()]
    .sort((a, b) => b[1] - a[1]).slice(0, n)
    .map(([name, h]) => ({ name, pct: +(100 * h / hits).toFixed(1) })) };
}

async function state(label, setup) {
  if (setup) { await setup(); await page.waitForTimeout(1200); }
  await cdp.send('Profiler.start');
  const stats = await frameStats(SAMPLE_MS);
  const { profile } = await cdp.send('Profiler.stop');
  const prof = topSelf(profile);
  const buf = await page.evaluate(() => {
    const c = document.getElementById('gl');
    return +(c.width * c.height / 1e6).toFixed(2);
  });
  console.log(`\n=== ${label} ===`);
  console.log(`  buffer ${buf} MPix · fps ${stats.fps}  mean ${stats.mean}ms  ` +
              `p50 ${stats.p50}  p95 ${stats.p95}  max ${stats.max}  ` +
              `frames>16.7ms: ${stats.over16}%`);
  console.log('  top self-time:');
  for (const r of prof.rows) console.log(`    ${String(r.pct).padStart(5)}%  ${r.name}`);
  return { label, stats, prof, buf };
}

const key = async k => { await page.keyboard.press(k); await page.waitForTimeout(500); };

console.log('SHITFIRE booted. Sampling ' + SAMPLE_MS + ' ms per state.');

const out = [];
out.push(await state('NAKED EYE (fov 60, adaptive tier)'));
out.push(await state('BINOS 7X (fov 8.57, pixel ratio PINNED 1.75)', () => key('b')));
out.push(await state('BINOS 14X (fov 4.29, pixel ratio PINNED 1.75)', () => key('z')));

// Back to the naked eye, then re-measure: proves the first reading was not a
// warm-up artefact and that exiting binos actually restores the cheaper state.
out.push(await state('NAKED EYE again (control)', async () => { await key('z'); await key('b'); }));

console.log('\n=== SUMMARY ===');
for (const o of out)
  console.log(`  ${o.label.padEnd(46)} ${String(o.buf).padStart(5)} MPix  ` +
              `${String(o.stats.fps).padStart(6)} fps   ` +
              `p95 ${String(o.stats.p95).padStart(6)} ms`);

console.log(errors.length ? '\nPAGE ERRORS:\n  ' + errors.join('\n  ') : '\nno page errors.');
await browser.close();
