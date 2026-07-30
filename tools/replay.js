/* tools/replay.js — TLOG replay: parser regression against real recorded traffic.
   Run: node tools/replay.js "Dialogue History/whatever.json" [more.json ...]
        node tools/replay.js            (no args: every .json under Dialogue History/)

   Every exported TLOG transcript records, for each observer transmission, how
   the parser classified it at the time (kind:"parse" → ptype/method/warno/ffe).
   This tool boots SHITFIRE.html headless, feeds each recorded utterance back
   through parseMessage (exposed as a QA hook, side-effect-free), and diffs the
   classification now against the classification then.

   A diff is not automatically a bug — improving the parser CHANGES old
   classifications (that is the point of improving it). The tool's job is to
   make every such change VISIBLE so it is a decision, not an accident. Exit 0
   when identical, 1 when anything moved; a human reads the diff either way.

   Caveat: parseMessage consults live scenario state only for known-point NAMES
   (shift-by-name), so a transcript from a different island can show kp-name
   diffs that are scenery, not parser drift. Everything else replays true.
   READ-ONLY against SHITFIRE.html per CLAUDE.md's dev-tooling rules. */
import { chromium } from 'playwright-core';
import { fileURLToPath } from 'url';
import path from 'path';
import fs from 'fs';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(here, '..');
const simPath = path.join(root, 'SHITFIRE.html');

let files = process.argv.slice(2);
if (!files.length) {
  const dir = path.join(root, 'Dialogue History');
  files = fs.existsSync(dir)
    ? fs.readdirSync(dir).filter(f => f.endsWith('.json')).map(f => path.join(dir, f))
    : [];
}
if (!files.length) { console.error('replay: no transcript .json given or found'); process.exit(2); }

const browser = await chromium.launch({ channel: 'chrome', headless: true });
const page = await browser.newPage();
await page.route(/^https?:\/\//, r => r.abort());   // same offline stance as shots.js
await page.goto('file:///' + simPath.replace(/\\/g, '/'));
await page.waitForFunction(() => window.SHITFIRE && window.SHITFIRE.parseMessage, null, { timeout: 30000 });

let total = 0, moved = 0;
for (const file of files) {
  const entries = JSON.parse(fs.readFileSync(file, 'utf8')).filter(e => e.kind === 'parse');
  console.log(`\n${path.basename(file)} — ${entries.length} recorded transmissions`);
  for (const e of entries) {
    total++;
    const now = await page.evaluate(raw => {
      const p = window.SHITFIRE.parseMessage(raw);
      return { ptype: p.type, method: p.method || null, warno: p.warno || null, ffe: !!p.ffe };
    }, e.msg);
    const then = { ptype: e.ptype, method: e.method ?? null, warno: e.warno ?? null, ffe: !!e.ffe };
    const fmt = c => c.ptype + (c.method ? '/' + c.method : '') + (c.warno ? '/' + c.warno : '') + (c.ffe ? '/FFE' : '');
    if (fmt(now) !== fmt(then)) {
      moved++;
      console.log(`  MOVED  "${e.msg}"`);
      console.log(`         then ${fmt(then)}  →  now ${fmt(now)}`);
    }
  }
}
console.log(`\nreplay: ${total} transmissions, ${moved} classification(s) moved` +
            (moved ? ' — review each: improvement or regression?' : ' — parser output unchanged'));
await browser.close();
process.exit(moved ? 1 : 0);
