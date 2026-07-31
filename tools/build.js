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

/* Vendor inlining: three.js goes INTO the artifact as base64 data: URLs in the
   import map. Chrome refuses ES-module imports over file:// (CORS), so a
   vendored sibling file cannot serve a double-clicked SHITFIRE.html — inlining
   is the only shape that is simultaneously one-file, offline, and
   double-clickable. Deterministic: same vendor bytes, same output. */
function dataUrl(rel) {
  return 'data:text/javascript;base64,' +
    fs.readFileSync(path.join(root, 'vendor', rel)).toString('base64');
}
/* 11d — the postprocessing addons import each other with RELATIVE specifiers
   ('./Pass.js', '../shaders/CopyShader.js'), and a module served from a data:
   URL has no base to resolve those against — the import map is never even
   consulted. So the build rewrites the vendor files' relative specifiers to
   the same BARE 'three/addons/...' names the import map already carries.
   A deterministic string substitution on THIRD-PARTY files only — the sim
   source is still a byte-exact paste, and same vendor bytes always produce
   the same output. */
const PP_REWRITE = [
  ["'./Pass.js'", "'three/addons/postprocessing/Pass.js'"],
  ["'./MaskPass.js'", "'three/addons/postprocessing/MaskPass.js'"],
  ["'./ShaderPass.js'", "'three/addons/postprocessing/ShaderPass.js'"],
  ["'../shaders/CopyShader.js'", "'three/addons/shaders/CopyShader.js'"],
  ["'../shaders/LuminosityHighPassShader.js'", "'three/addons/shaders/LuminosityHighPassShader.js'"],
];
function addonUrl(rel) {
  let src = fs.readFileSync(path.join(root, 'vendor', rel), 'utf8');
  for (const [from, to] of PP_REWRITE) src = src.split(from).join(to);
  return 'data:text/javascript;base64,' + Buffer.from(src).toString('base64');
}
let head = read(manifest.head).toString('latin1');
head = head.replace('__VENDOR_THREE_CORE__', dataUrl('three/three.module.min.js'))
           .replace('__VENDOR_THREE_SKY__', dataUrl('three/addons/objects/Sky.js'))
           .replace('__VENDOR_PP_COMPOSER__', addonUrl('three/addons/postprocessing/EffectComposer.js'))
           .replace('__VENDOR_PP_PASS__', addonUrl('three/addons/postprocessing/Pass.js'))
           .replace('__VENDOR_PP_MASK__', addonUrl('three/addons/postprocessing/MaskPass.js'))
           .replace('__VENDOR_PP_SHADER__', addonUrl('three/addons/postprocessing/ShaderPass.js'))
           .replace('__VENDOR_PP_RENDER__', addonUrl('three/addons/postprocessing/RenderPass.js'))
           .replace('__VENDOR_PP_BLOOM__', addonUrl('three/addons/postprocessing/UnrealBloomPass.js'))
           .replace('__VENDOR_SH_COPY__', addonUrl('three/addons/shaders/CopyShader.js'))
           .replace('__VENDOR_SH_LUM__', addonUrl('three/addons/shaders/LuminosityHighPassShader.js'));

const parts = [];
parts.push(Buffer.from(head, 'latin1'));
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
