# vendor/three

Three.js **r160** (`0.160.0`), vendored so SHITFIRE runs with no network.

| File | Source |
|---|---|
| `three.module.min.js` | `https://cdn.jsdelivr.net/npm/three@0.160.0/build/three.module.min.js` (BUILD1, 2026-07-31 — the minified core replaced the readable `three.module.js`; same revision, ~660 KB vs 1.24 MB, so the shipped artifact drops ~36%) |
| `addons/objects/Sky.js` | `https://cdn.jsdelivr.net/npm/three@0.160.0/examples/jsm/objects/Sky.js` |
| `addons/postprocessing/*.js` (6 files, 11d) | `https://cdn.jsdelivr.net/npm/three@0.160.0/examples/jsm/postprocessing/…` |
| `addons/shaders/CopyShader.js`, `LuminosityHighPassShader.js` | `…/examples/jsm/shaders/…` |

The import map in `SHITFIRE.html` (authored in `src/shell/head.html`) maps `three`
and each `three/addons/…` module to an inlined `data:` URL. `Sky.js` imports only
from the `three` core; the postprocessing addons (11d, SUNLAMP bloom) import each
other with RELATIVE specifiers, which cannot resolve from a `data:` URL — so
`tools/build.js` rewrites those relative specifiers to the same bare
`three/addons/…` names the import map carries (a deterministic substitution on
these third-party files only; see `PP_REWRITE` in the build). Add a file here,
map it in `head.html`, and extend `PP_REWRITE` if it has relative imports.

**Do not repoint the import map at a CDN** — offline capability is a golden rule
(CLAUDE.md). To upgrade three: replace these files with the new version, keep the
paths, and re-run `tools/shots.js` (it must boot clean) plus a human Chrome pass,
since a three major can move the ACES/Sky/colour-management pipeline this sim
depends on.
