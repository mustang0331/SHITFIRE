/* tools/build.js — assemble SHITFIRE.html from src/.
   Run: node tools/build.js        (writes SHITFIRE.html)
        node tools/build.js --check (builds in memory, diffs against the committed
                                     SHITFIRE.html, exits 1 on any byte difference)

   The shipped artifact stays ONE self-contained file that opens in Chrome with
   nothing installed (CLAUDE.md's invariant). This build is dev-only: it
   concatenates, in manifest order, an HTML head, the JS module parts, and an
   HTML tail — a deterministic paste, not a bundler. No minify, no transform, no
   dependency. The output must be byte-for-byte what a human would have typed;
   --check enforces exactly that against git.

   DISCIPLINE: once src/ exists, edit src/ and rebuild. Editing SHITFIRE.html
   directly is lost on the next build. The pre-commit hook enforces this. */
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { execSync } from 'child_process';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(here, '..');
const srcDir = path.join(root, 'src');
const outPath = path.join(root, 'SHITFIRE.html');

// The manifest is the single source of ORDER. JS parts concatenate in exactly
// this sequence — and order is load-bearing in this file (hoisting, top-level
// const/let, the parseMessage branch precedence), so the manifest IS the spec
// for how the module is assembled. Add a new module by naming it here.
const manifest = JSON.parse(fs.readFileSync(path.join(srcDir, 'manifest.json'), 'utf8'));

function read(rel) { return fs.readFileSync(path.join(srcDir, rel)); }

const parts = [];
parts.push(read(manifest.head));
for (const js of manifest.js) parts.push(read(js));
parts.push(read(manifest.tail));
const built = Buffer.concat(parts);

if (process.argv.includes('--check')) {
  // Diff against the committed SHITFIRE.html, not the working copy, so a stray
  // hand-edit to the built file cannot mask a build that no longer reproduces it.
  let committed;
  try {
    committed = execSync('git show HEAD:SHITFIRE.html', { cwd: root, maxBuffer: 1 << 26 });
  } catch (e) {
    console.error('build --check: could not read HEAD:SHITFIRE.html —', e.message);
    process.exit(2);
  }
  if (Buffer.compare(built, committed) === 0) {
    console.log('build --check: OK — src/ reproduces HEAD:SHITFIRE.html byte-for-byte (' +
                built.length + ' bytes)');
    process.exit(0);
  }
  // locate the first differing byte for a useful message
  const n = Math.min(built.length, committed.length);
  let i = 0; while (i < n && built[i] === committed[i]) i++;
  const line = built.slice(0, i).toString('utf8').split('\n').length;
  console.error(`build --check: MISMATCH at byte ${i} (~line ${line}); ` +
                `built ${built.length} vs committed ${committed.length} bytes`);
  process.exit(1);
}

fs.writeFileSync(outPath, built);
console.log('build: wrote SHITFIRE.html (' + built.length + ' bytes, ' +
            manifest.js.length + ' JS module' + (manifest.js.length === 1 ? '' : 's') + ')');
