/* tools/lint.js — defect-finding lint over src/.
   Run: node tools/lint.js    (exit 0 clean, 1 on any error-severity finding)

   The src/js/ parts are FRAGMENTS of one module — they share top-level scope,
   so linting them file-by-file would drown in false no-undef/no-unused noise.
   Instead this concatenates them in manifest order (exactly as tools/build.js
   does) and lints the blob, then maps every finding back to the source module
   and line. READ-ONLY, reports only — never rewrites anything (CLAUDE.md bans
   formatters outright).

   The rule set is correctness-only: duplicate keys, redeclares, unreachable
   code, bad typeof, assignment-in-condition, undefined globals. No style rules
   — style churn on a 200 KB sim is review poison for zero functional gain. */
import { Linter } from 'eslint';
import globals from 'globals';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const here = path.dirname(fileURLToPath(import.meta.url));
const srcDir = path.join(here, '..', 'src');
const manifest = JSON.parse(fs.readFileSync(path.join(srcDir, 'manifest.json'), 'utf8'));

// Concatenate in manifest order, recording where each part starts in the blob
// so findings can name the real file:line to edit.
const parts = [];   // { rel, startLine, lineCount }
let blob = '';
let line = 1;
for (const rel of manifest.js) {
  const text = fs.readFileSync(path.join(srcDir, rel), 'utf8');
  const lines = text.split('\n').length;
  parts.push({ rel, startLine: line, lineCount: lines });
  blob += text;
  line += lines - (text.endsWith('\n') ? 1 : 0);
}
function locate(blobLine) {
  for (let i = parts.length - 1; i >= 0; i--)
    if (blobLine >= parts[i].startLine)
      return parts[i].rel + ':' + (blobLine - parts[i].startLine + 1);
  return 'blob:' + blobLine;
}

const linter = new Linter();
const messages = linter.verify(blob, [{
  languageOptions: {
    ecmaVersion: 'latest',
    sourceType: 'module',
    globals: {
      ...globals.browser,
      // Web Speech API — prefixed constructor Chrome actually exposes
      webkitSpeechRecognition: 'readonly',
    },
  },
  rules: {
    'no-undef': 'error',
    'no-redeclare': 'error',
    'no-dupe-keys': 'error',
    'no-dupe-args': 'error',
    'no-dupe-else-if': 'error',
    'no-duplicate-case': 'error',
    'no-unreachable': 'error',
    'no-fallthrough': 'error',
    'use-isnan': 'error',
    'valid-typeof': 'error',
    'no-const-assign': 'error',
    'no-func-assign': 'error',
    'no-class-assign': 'error',
    'no-self-assign': 'error',
    'no-self-compare': 'error',
    'no-cond-assign': 'error',
    'no-constant-condition': ['error', { checkLoops: false }],
    'no-unsafe-negation': 'error',
    'no-unsafe-optional-chaining': 'error',
    'no-sparse-arrays': 'error',
    'no-compare-neg-zero': 'error',
    'no-ex-assign': 'error',
    'no-global-assign': 'error',
    'no-import-assign': 'error',
    'no-obj-calls': 'error',
    'no-setter-return': 'error',
    'no-unexpected-multiline': 'error',
    'no-loss-of-precision': 'error',
    // unused vars: real signal in a concatenated blob (cross-module uses are
    // visible), but warn-only — a stub kept for a coming stage is not a defect.
    'no-unused-vars': ['warn', { args: 'none', caughtErrors: 'none', varsIgnorePattern: '^_' }],
  },
}], 'src-blob.js');

let errors = 0, warnings = 0;
for (const m of messages) {
  const sev = m.severity === 2 ? 'ERROR' : 'warn ';
  if (m.severity === 2) errors++; else warnings++;
  console.log(`${sev} ${locate(m.line)}  ${m.message}  [${m.ruleId || 'parse'}]`);
}
console.log(`lint: ${errors} error(s), ${warnings} warning(s) across ${parts.length} modules`);
process.exit(errors ? 1 : 0);
