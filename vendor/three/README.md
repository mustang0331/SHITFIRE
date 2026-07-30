# vendor/three

Three.js **r160** (`0.160.0`), vendored so SHITFIRE runs with no network.

| File | Source |
|---|---|
| `three.module.js` | `https://cdn.jsdelivr.net/npm/three@0.160.0/build/three.module.js` |
| `addons/objects/Sky.js` | `https://cdn.jsdelivr.net/npm/three@0.160.0/examples/jsm/objects/Sky.js` |

The import map in `SHITFIRE.html` (authored in `src/shell/head.html`) maps `three`
and `three/addons/` at these local paths. `Sky.js` imports only from the `three`
core, so those two files are the whole dependency — add a file here and map it if
a new addon is ever imported.

**Do not repoint the import map at a CDN** — offline capability is a golden rule
(CLAUDE.md). To upgrade three: replace these files with the new version, keep the
paths, and re-run `tools/shots.js` (it must boot clean) plus a human Chrome pass,
since a three major can move the ACES/Sky/colour-management pipeline this sim
depends on.
