/* tools/shots.js — the screenshot QA rig.
   Run: node tools/shots.js   (from the repo root or tools/; paths self-resolve)

   Loads SHITFIRE.html in headed-less real Chrome (playwright-core, channel
   'chrome' — no bundled browser download), drives it through the states on the
   GRAPHICS.md visual-QA backlog, and writes PNGs to tools/test-results/. The
   agent (or a human) then LOOKS at them — this is the tool that closes the
   "verified by harness, never seen running" gap.

   READ-ONLY against SHITFIRE.html per CLAUDE.md's dev-tooling rules. It also
   doubles as a smoke test: any pageerror/console.error fails the run — the
   syntax gate proves the file parses; this proves it BOOTS. */
import { chromium } from 'playwright-core';
import { fileURLToPath } from 'url';
import path from 'path';
import fs from 'fs';

const here = path.dirname(fileURLToPath(import.meta.url));
const simPath = path.join(here, '..', 'SHITFIRE.html');
const outDir = path.join(here, 'test-results');
fs.mkdirSync(outDir, { recursive: true });

const errors = [];
const browser = await chromium.launch({ channel: 'chrome', headless: true });
const page = await browser.newPage({ viewport: { width: 1600, height: 900 } });
// OFFLINE PROOF: abort every network request. The trainer must run with no
// connectivity (CLAUDE.md golden rule), so any surviving http(s) dependency —
// a CDN three.js, a web font, an analytics ping — makes the run FAIL here
// rather than passing silently on a connected machine. file:// is not routed
// through this, so local vendored modules load normally.
const blocked = [];
await page.route(/^https?:\/\//, r => { blocked.push(r.request().url()); r.abort(); });
page.on('pageerror', e => errors.push('pageerror: ' + e.message));
page.on('console', m => { if (m.type() === 'error') errors.push('console: ' + m.text()); });

await page.goto('file:///' + simPath.replace(/\\/g, '/'));
// boot = the debug surface exists and the first frame has rendered
await page.waitForFunction(() => window.SHITFIRE && window.SHITFIRE.Scenario, null, { timeout: 30000 });
await page.waitForTimeout(2500);   // let the FDC's opening traffic land and the scene settle

async function shot(name, note) {
  await page.waitForTimeout(400);
  await page.screenshot({ path: path.join(outDir, name + '.png') });
  console.log('  shot', name + '.png', note ? '— ' + note : '');
}
const key = async k => { await page.keyboard.press(k); await page.waitForTimeout(350); };
const sf = expr => page.evaluate(expr);

console.log('SHITFIRE booted; capturing.');

// --- daylight, naked eye (13c sky/horizon, 13d relief, E4/E6/E7 world detail)
await shot('01-day-op', 'default view: horizon seam, hillshade relief, world detail');

// --- binoculars at the three powers (G4: reticle true and unclipped)
await key('b');
await shot('02-binos-7x', 'default power, mil reticle');
await key('z');
await shot('03-binos-14x', 'reticle must not clip (13d fixed extent bug)');
await key('z');
await shot('04-binos-4x', 'wide scan field');
await key('r');
await shot('05-milcard', 'G5: card must be clear of the comms panel');
await key('r'); await key('b');

// --- overlays
await key('m');
await shot('06-map', 'E5/E6/E7: roads, town, rock symbols + legend');
await key('Escape');
await key('h');
await shot('07-cheatsheet', 'F3 legibility');
await key('Escape');

// --- time of day + optics (13c/13d/E3) via the QA hooks
await sf("window.SHITFIRE.applyTOD('dawn')");
await shot('08-dawn', '13d: low sun must rake the relief');
await sf("window.SHITFIRE.applyTOD('night')");
await shot('09-night-eye', 'E3: near-dark');
await sf("window.SHITFIRE.setVision('nvg')");
await shot('10-night-nvg', 'E3: usable, vegetation NIR-bright, asphalt dark');
await sf("window.SHITFIRE.setVision('thermal')");
await shot('11-night-thermal', 'E3/E4: asphalt warm, rock cold, figures hot');
await sf("window.SHITFIRE.setVision('day'); window.SHITFIRE.applyTOD('day')");

// --- printed sheet (13b: no tint; G2: declination diagram; E5/E7 symbols)
await key('p');
await shot('12-library-sheet', 'printed sheet: monochrome line art only');
await key('Escape');

if (blocked.length)
  console.log('OFFLINE FAIL: ' + blocked.length + ' network request(s) attempted — the ' +
              'trainer is not self-contained:\n  ' + [...new Set(blocked)].join('\n  '));
else
  console.log('offline OK — zero network requests; runs fully local.');
console.log(errors.length
  ? 'BOOT/RUNTIME ERRORS:\n  ' + errors.join('\n  ')
  : 'no page errors — boots clean.');
await browser.close();
process.exit(errors.length || blocked.length ? 1 : 0);
