/* tools/perflegibility.js — PERF1's non-negotiable gate.
   Run: node tools/perflegibility.js

   PERF1 lowers the pixel ratio the observer glasses at (tier 0 pin -> tier 2
   floor). GRAPHICS.md defect #2 and row 13a both say the ONE thing a frame-rate
   fix may never buy its speed with is target resolution. So this renders the
   SAME view at the old pinned ratio and the new floor ratio and writes both,
   plus a magnified centre crop, to be compared by eye.

   It also prints the arithmetic, because the eye check and the numbers should
   agree: a figure is held at CONFIG.GFX.legFloorMil mils by E1's angular floor,
   and screen height = mils * innerHeight / (fovDeg * 17.7778).

   READ-ONLY against SHITFIRE.html per CLAUDE.md's dev-tooling rules. */
import { chromium } from 'playwright-core';
import { fileURLToPath } from 'url';
import path from 'path';
import fs from 'fs';

const here = path.dirname(fileURLToPath(import.meta.url));
const simPath = path.join(here, '..', 'SHITFIRE.html');
const outDir = path.join(here, 'test-results');
fs.mkdirSync(outDir, { recursive: true });

const DSF = 2, VW = 1920, VH = 1080;
const browser = await chromium.launch({ channel: 'chrome', headless: true });
const page = await browser.newPage({
  viewport: { width: VW, height: VH }, deviceScaleFactor: DSF });
const errors = [];
page.on('pageerror', e => errors.push('pageerror: ' + e.message));

await page.goto('file:///' + simPath.replace(/\\/g, '/'));
await page.waitForFunction(() => window.SHITFIRE && window.SHITFIRE.Scenario, null, { timeout: 30000 });
await page.waitForTimeout(3000);

const G = await page.evaluate(() => ({
  floorMil: window.SHITFIRE.CONFIG.GFX.legFloorMil,
  zooms: window.SHITFIRE.CONFIG.CAMERA.binoZooms,
  fov: window.SHITFIRE.CONFIG.CAMERA.fov,
  h: innerHeight,
}));

console.log(`E1 angular floor: ${G.floorMil} mils, held at ANY range.`);
console.log('figure height on screen, by optical power and pixel ratio:\n');
console.log('  power   fov     CSS px   @1.75 (old pin)   @1.1 (new floor)');
for (const z of G.zooms) {
  const fov = G.fov / z;
  const css = G.floorMil * G.h / (fov * 17.7778);
  console.log(`  ${String(z + 'X').padStart(4)}  ${fov.toFixed(2).padStart(6)}  ` +
              `${css.toFixed(1).padStart(8)}  ${(css * 1.75).toFixed(1).padStart(15)} px  ` +
              `${(css * 1.1).toFixed(1).padStart(15)} px`);
}
console.log('\n  (13a pinned tier 0 because an UNSCALED 2 m figure was ~4 px at full res\n' +
            '   and ~2 px at tier 3. E1 changed the input: the figure is now scaled to\n' +
            '   hold the angular floor, so none of these depend on range at all.)\n');

await page.keyboard.press('b');            // binos up
await page.waitForTimeout(600);

async function capture(tag, ratio) {
  await page.evaluate(r => window.SHITFIRE.perf.setRatio(r), ratio);
  await page.waitForTimeout(900);
  await page.evaluate(r => window.SHITFIRE.perf.setRatio(r), ratio);
  await page.waitForTimeout(500);
  const info = await page.evaluate(() => {
    const c = document.getElementById('gl');
    return { ...window.SHITFIRE.perf.info(), buf: [c.width, c.height] };
  });
  const full = path.join(outDir, `leg-${tag}-full.png`);
  await page.screenshot({ path: full });
  // centre crop, where the reticle and anything being measured actually sit
  await page.screenshot({ path: path.join(outDir, `leg-${tag}-crop.png`),
    clip: { x: VW / 2 - 220, y: VH / 2 - 140, width: 440, height: 280 } });
  console.log(`  ${tag}: ratio ${info.ratio} · buffer ${info.buf[0]}x${info.buf[1]} · ` +
              `${info.calls} calls · ${info.tris} tris`);
}

console.log('captures (7X):');
await capture('7x-pin-1.75', 1.75);
await capture('7x-floor-1.1', 1.1);
await page.keyboard.press('z');            // -> 14X
await page.waitForTimeout(600);
console.log('captures (14X):');
await capture('14x-pin-1.75', 1.75);
await capture('14x-floor-1.1', 1.1);

console.log(errors.length ? '\nPAGE ERRORS:\n  ' + errors.join('\n  ') : '\nno page errors.');
console.log('wrote leg-*.png to tools/test-results/ — compare pin vs floor by eye.');
await browser.close();
