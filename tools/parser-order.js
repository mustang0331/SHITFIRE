/* tools/parser-order.js — parseMessage branch-precedence guard.
   Run: node tools/parser-order.js        (exit 0 all pass, 1 on any failure)
   Run it whenever a branch is added to parseMessage.

   Parser precedence audit — the residual risk in parseMessage is ORDER, not
   logic: every branch was harness-tested in isolation, but several are only
   correct BECAUSE of where they sit (posrep before grid, safety prowords
   first, at-my-command after the location methods, bare FIRE after everything
   containing the word "fire"). This reads the SOURCE (src/js/11-parser.js —
   the build is a verbatim concatenation, so source order IS shipped order)
   and asserts the order, so a future insertion in the wrong place fails
   loudly.

   BUILD2 (2026-08-27): ported from JScript. The old version read a hardcoded
   OneDrive path that stopped existing when the repo moved, cscript exited 0
   on the resulting runtime error, and the script never set an exit code at
   all — so the pre-commit guard could not fail. A gate that cannot fail is
   worse than no gate, because it is counted as coverage. */
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const here = path.dirname(fileURLToPath(import.meta.url));
const srcPath = path.join(here, '..', 'src', 'js', '11-parser.js');

let fails = 0;
function check(label, cond, detail) {
  console.log((cond ? '  PASS  ' : '  FAIL  ') + label + (detail ? '   [' + detail + ']' : ''));
  if (!cond) fails++;
}

const s = fs.readFileSync(srcPath, 'utf8');
const start = s.indexOf('function parseMessage(');
if (start === -1) { console.log('FAIL: parseMessage not found in ' + srcPath); process.exit(1); }
let end = s.indexOf('\nfunction ', start + 10);
if (end === -1) end = s.length;
const body = s.substring(start, end);
console.log('parseMessage body: ' + body.length + ' chars  (' + path.relative(process.cwd(), srcPath) + ')');

/* markers, in the order they MUST test */
const order = [
  ['say again',              "t.includes('say again')"],
  ['check firing (safety)',  "t.includes('check firing')"],
  ['cease loading (safety)', "t.includes('cease loading')"],
  ['cancel at my command',   "t.includes('cancel at my command')"],
  ['do not load',            "t.includes('do not load')"],
  ['cannot observe',         "t.includes('cannot observe')"],
  ['time on target',         'time on target'],
  ['end of mission',         "t.includes('end of mission')"],
  ['POS REP (before grid!)', 'pos\\s*rep'],
  ['immediate types',        'immediate suppression'],
  ['suppress by number',     'suppresstgt'],
  ['grid extraction',        "toks.indexOf('grid')"],
  ['shift',                  'known point\\s+'],
  ['polar',                  '\\b(?:distance|range)\\s+'],   // NET3 — range is a distance synonym
  ['standalone direction',   "type: 'direction'"],
  ['corrections (incl HOB)', "type: 'adjust'"],
  ['repeat',                 "type: 'repeat'"],
  ['OT factor',              "type: 'otfactor'"],
  ['at my command (late!)',  "t.includes('at my command')"],
  ['bare FIRE (last)',       "type: 'fire'"],
  ['bare warning order',     "type: 'warno'"],
  ['unknown (fallthrough)',  "type: 'unknown'"],
];
console.log('\n=== branch order as shipped ===');
let prev = -1, prevName = '(start)';
for (const [name, marker] of order) {
  const idx = body.indexOf(marker);
  console.log('  ' + (idx >= 0 ? String(idx) : 'MISSING') + '  ' + name);
  check("  '" + name + "' present and after '" + prevName + "'",
        idx > prev, 'at ' + idx + ', prev ' + prev);
  if (idx > prev) { prev = idx; prevName = name; }
}

console.log('\n=== the four load-bearing orderings, stated ===');
check('safety prowords are tested before everything except say-again',
      body.indexOf('check firing') < body.indexOf('end of mission'));
check('POS REP is tested before grid extraction',
      body.indexOf('pos\\s*rep') < body.indexOf("toks.indexOf('grid')"));
check('at-my-command is tested AFTER the location methods (it is a legal T3 element)',
      body.indexOf("t.includes('at my command')") > body.indexOf("type: 'adjust'"));
check("bare FIRE is tested after every branch containing the word 'fire'",
      body.indexOf("type: 'fire'") > body.indexOf("t.includes('at my command')"));

console.log('');
console.log(fails === 0 ? 'ALL CHECKS PASSED' : fails + ' CHECK(S) FAILED');
process.exit(fails === 0 ? 0 : 1);
