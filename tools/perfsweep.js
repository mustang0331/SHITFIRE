/* tools/perfsweep.js — pixel-ratio sensitivity sweep (row PERF1).
   Run: node tools/perfsweep.js [deviceScaleFactor] [w] [h]

   perfprobe.js established WHERE the frame goes (nowhere: ~90% idle, so the
   main thread is parked waiting on the GPU). This answers the follow-on
   question that decides the fix: is the cost proportional to PIXELS?

   If frame time is linear in the drawing-buffer pixel count, the renderer is
   fill-rate bound and the only levers that matter are resolution and per-pixel
   shader cost — geometry, draw calls and JS are irrelevant. If it is flat, the
   cost is elsewhere and lowering resolution would be pure quality loss for
   nothing.

   Sweeps with BINOS UP, because that is the state the user reports as laggy and
   the state applyQuality() pins to tier 0.

   READ-ONLY against SHITFIRE.html per CLAUDE.md's dev-tooling rules. */
import { chromium } from 'playwright-core';
import { fileURLToPath } from 'url';
import path from 'path';

const here = path.dirname(fileURLToPath(import.meta.url));
const simPath = path.join(here, '..', 'SHITFIRE.html');

const DSF = Number(process.argv[2] || 2);
const VW = Number(process.argv[3] || 1920), VH = Number(process.argv[4] || 1080);
const SAMPLE_MS = 3000;
const TIERS = [1.75, 1.4, 1.1, 0.85];   // must match QUALITY.tiers

const errors = [];
const browser = await chromium.launch({ channel: 'chrome', headless: true });
const page = await browser.newPage({
  viewport: { width: VW, height: VH }, deviceScaleFactor: DSF });
page.on('pageerror', e => errors.push('pageerror: ' + e.message));
page.on('console', m => { if (m.type() === 'error') errors.push('console: ' + m.text()); });

await page.goto('file:///' + simPath.replace(/\\/g, '/'));
await page.waitForFunction(() => window.SHITFIRE && window.SHITFIRE.Scenario, null, { timeout: 30000 });
await page.waitForTimeout(3000);

const hasPerf = await page.evaluate(() => !!(window.SHITFIRE && window.SHITFIRE.perf));
if (!hasPerf) {
  console.error('window.SHITFIRE.perf missing — rebuild SHITFIRE.html from src/ first ' +
                '(node tools/build.js).');
  await browser.close(); process.exit(2);
}

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
    t.shift();
    const s = t.slice().sort((a, b) => a - b);
    const sum = t.reduce((a, b) => a + b, 0);
    return { fps: +(1000 / (sum / t.length)).toFixed(1),
             mean: +(sum / t.length).toFixed(2),
             p95: +s[Math.floor(s.length * 0.95)].toFixed(2) };
  }, ms);
}

// binos up — the reported-laggy state, and the one that is pinned to tier 0
await page.keyboard.press('b');
await page.waitForTimeout(800);
/* Freeze the adaptive controller BEFORE sweeping. Since PERF1 it runs while
   glassing, so without this it re-tiers the renderer mid-measurement and every
   row comes back attached to a resolution it was not measured at. */
await page.evaluate(() => window.SHITFIRE.perf.freeze(true));

console.log(`sweep: dSF ${DSF} · CSS ${VW}x${VH} · BINOS UP · ${SAMPLE_MS} ms per point\n`);
console.log('  ratio   MPix    fps    mean     p95   calls    tris');

const rows = [];
for (const r of TIERS) {
  await page.evaluate(x => window.SHITFIRE.perf.setRatio(x), r);
  await page.waitForTimeout(700);
  const st = await frameStats(SAMPLE_MS);
  const info = await page.evaluate(() => {
    const c = document.getElementById('gl');
    return { ...window.SHITFIRE.perf.info(), mpix: +(c.width * c.height / 1e6).toFixed(2) };
  });
  /* Trust nothing: assert the buffer actually matches the ratio we asked for.
     A silent mismatch here is what produced the impossible first sweep. */
  const want = +(VW * r * VH * r / 1e6).toFixed(2);
  if (Math.abs(info.mpix - want) > 0.02)
    console.log(`    !! ratio ${r}: buffer is ${info.mpix} MPix, expected ${want} — ` +
                `sample discarded, the controller moved under us`);
  rows.push({ r, ...st, ...info });
  console.log(`  ${String(r).padStart(5)}  ${String(info.mpix).padStart(5)}  ` +
              `${String(st.fps).padStart(5)}  ${String(st.mean).padStart(6)}  ` +
              `${String(st.p95).padStart(6)}  ${String(info.calls).padStart(5)}  ` +
              `${String(info.tris).padStart(6)}`);
}

/* Linearity test. If fill-rate bound, ms/MPix is roughly CONSTANT across the
   sweep and the intercept (fixed per-frame cost) is small. A big intercept and
   a small slope would mean resolution is NOT the lever. */
console.log('\n  ms per MPix (flat => fill-rate bound):');
for (const x of rows)
  console.log(`    ratio ${String(x.r).padStart(5)}  ${(x.mean / x.mpix).toFixed(2)} ms/MPix`);

// least squares of mean-ms against MPix
const n = rows.length;
const sx = rows.reduce((a, b) => a + b.mpix, 0), sy = rows.reduce((a, b) => a + b.mean, 0);
const sxx = rows.reduce((a, b) => a + b.mpix * b.mpix, 0);
const sxy = rows.reduce((a, b) => a + b.mpix * b.mean, 0);
const slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
const inter = (sy - slope * sx) / n;
console.log(`\n  fit: frame_ms = ${slope.toFixed(2)} * MPix + ${inter.toFixed(2)}`);
console.log(`  => ${(100 * slope * rows[0].mpix / rows[0].mean).toFixed(0)}% of the tier-0 ` +
            `frame is resolution-dependent; ${inter.toFixed(1)} ms is fixed cost.`);
console.log(`  60 fps (16.7 ms) needs <= ${((16.7 - inter) / slope).toFixed(2)} MPix ` +
            `=> pixel ratio <= ${Math.sqrt((16.7 - inter) / slope / (VW * VH / 1e6)).toFixed(2)}`);

console.log(errors.length ? '\nPAGE ERRORS:\n  ' + errors.join('\n  ') : '\nno page errors.');
await browser.close();
