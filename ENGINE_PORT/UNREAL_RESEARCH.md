# UNREAL_RESEARCH.md — Browser → Unreal Engine Port Research

Compiled 2026-08-28 from eight parallel research agents plus local source verification.
Every third-party claim was fetched, not recalled: each named project carries license,
last-activity date, and UE-version support, verified against its repo/docs on 2026-08-27.
Tags: **[verified]** = fetched/read, **[inferred]** = follows from verified facts,
**[not checked]** = stated plainly. Where two agents disagreed, the conflict was resolved
by evidence, not averaged — resolutions are footnoted in place.

**Standing context:** `ENGINE_CHOICE.md` (2026-07-29) chose Godot 4 + C# and rejected
Unreal. This document does not overturn that decision by itself — see the verdict — but
it corrects two of its stated reasons and maps the concrete Unreal path if the author
overrides it.

**Full agent reports** (per-subsystem deep dives with complete source lists) live in
[research/](research/): 01 JS-reuse · 02 C#/scripting · 03 open-source adoptions ·
04 terrain · 05 UI/cartography/print · 06 speech · 07 rejection re-test (EULA quotes) ·
08 agent tooling · 09 animation · 10 modding communities. **A builder agent should read
this file in full before writing any engine code, then consult the matching research/
file when starting each subsystem.**

---

## 1. Verdict on the Godot decision **[YOUR CALL]**

An adversarial re-test of every documented rejection reason against 2026 evidence
(live Epic EULA fetch, current docs, real repos):

| # | ENGINE_CHOICE.md claim | 2026 verdict |
|---|---|---|
| 1 | UMG fights precise 2D vector work / "painful" | **PARTLY** — true for retained-mode vector documents (no SVG/CSS/vector renderer exists; practitioners bolt on Skia to escape it). **But SHITFIRE's map code is immediate-mode canvas drawing**, and `UCanvasRenderTarget2D` ports it nearly call-for-call (§5). The residual tax is ergonomics and C++ iteration speed, not capability.¹ |
| 2 | Headless/unit testing: Poor | **REFUTED** — Low-Level Tests (Catch2, standalone exes, no editor) are first-class in 5.8, and the sim-core-outside-the-engine pattern (officially supported external static lib) voids the objection entirely. **Correct this row in ENGINE_CHOICE.md.** |
| 3 | Deterministic sim: "engine is opinionated" | **PARTLY** — UE compiles `/fp:fast` by default (a genuine silent parity-breaker), but per-module `FPSemantics` override exists in 5.8, and an externally-built `/fp:precise` static lib sidesteps UE's flags completely. Fully controllable. |
| 4 | C++/Blueprint: highest friction for solo dev | **CONFIRMED** — no production C# path exists (UnrealSharp's own FAQ: "not production ready"). Softeners: Rider free for non-commercial, strong LLM familiarity. Still months of ramp vs days-to-weeks for Godot C#. |
| 5 | Royalty model (licensing) | **REFUTED as a blocker.** See §2 — a free trainer owes Epic nothing. **Correct this row in ENGINE_CHOICE.md.** |
| 6 | Very large distribution | **PARTLY** — ~300 MB default Shipping build, 80–130 MB with aggressive stripping; but Godot 4 **with C#** is ~175 MB unzipped (bundled .NET runtime), so the real gap is 2–3×, not 6–10×. Immaterial for USB/share distribution. |
| 7 | Solo-dev ramp: High | **CONFIRMED** — ~150 GB dev environment, C++/UBT idioms, no escape hatch. |
| 8 | (new) Anything in 5.5–5.8 that helps this product? | **Nothing material.** Release investment (Nanite Foliage, MegaLights, MetaHuman crowds) targets big-3D teams. The two quiet relevant improvements: mature Low-Level Tests, `FPSemantics`. |

**Bottom line from the re-test agent:** the 2026 evidence does **not** overturn Godot for
this product. The deciding facts are port cost (no production C#; full rewrite into UE C++
plus engine ramp vs near-transliteration into Godot C#) and the 2D ergonomics tax. But the
two loudest-sounding rejection reasons — licensing and headless testing — were wrong and
should be struck from ENGINE_CHOICE.md so the record doesn't rot. If the author overrides
and picks Unreal anyway (institutional credibility, XR trajectory, personal preference are
legitimate reasons this research can't weigh), §3–§8 are the map. Roughly half the
findings below are engine-independent and improve the Godot plan too (§9).

¹ Resolution note: the UI agent called claim 1 "mostly refuted for this workload," the
re-test agent "partly confirmed." Both agree on the facts: no vector/SVG runtime exists
(confirmed weakness), and Canvas render targets fit SHITFIRE's immediate-mode map code
(confirmed capability). The difference was emphasis; the table row states both halves.

## 2. Licensing — the facts (live EULA, fetched 2026-08-27)

Quotes verbatim from https://www.unrealengine.com/en-US/eula/unreal:

- The preamble expressly blesses the use case: Unreal may be used for *"simulation for
  training purposes."*
- **Free product ⇒ no royalty.** §4(a)(ii): *"You will not be obligated to pay us any
  royalty payments for Distributing Products to any person or entity if the Products do
  not directly generate revenue."* (Threshold otherwise: 5% above $1M gross, unchanged.)
- **Seat subscriptions don't apply either.** §6(b)(i) personal/indie exception: seat fees
  waived while you (plus corporate group) are under **$1M gross revenue in the trailing
  12 months**. The $1,850/seat/yr non-game subscription only bites above that line.
- **Government/military end users are expressly licensed.** §18(b): licensed to all end
  users *"including any U.S. Government end users"* as a Commercial Item. The word
  "military" appears zero times in the EULA. End users running the packaged trainer need
  nothing from Epic (§4(c)).
- Real obligations: ship the product under an EULA disclaiming Epic warranties (§4(c),
  trivial); **no GPL or CC-BY-SA code combined into the product** (§6(c) — this makes the
  GPL flags in §7/§8 hard requirements, not preferences).
- **The one clause to watch [YOUR CALL trigger]:** the §6(b)(i) revenue test counts
  *"any advances received or other funds raised."* If SHITFIRE ever becomes a funded
  government contract, that funding counts toward the $1M test; crossing it makes seats
  ($1,850/yr/dev) due. For a solo dev: an annoyance, not a blocker.

## 3. The sim core — where the ~1,900 parity-critical lines live

Every track converged on the same load-bearing decision: **the sim core lives outside the
engine.** Three routes, ranked:

### 3a. Recommended: plain C++ static library, built outside UE
- CMake + Catch2/GoogleTest, compiled **`/fp:precise`**, linked into UE as a
  `ModuleType.External` third-party lib [verified: Epic docs; the pattern is documented
  end-to-end by Eric Lemes]. UE's build flags cannot touch prebuilt machine code.
- Why `/fp:precise` matters: UE compiles Development and Shipping with **`/fp:fast`**
  (reassociation + FMA contraction) — sim code inside a normal UE module would pass
  parity in DebugGame and **silently diverge in Shipping**. This is the single worst
  silent failure on the board. Since VS2022, `/fp:precise` emits no FMA contractions —
  per-op IEEE-correct doubles matching JS. Keep all math out of inline headers or the
  game's flags recompile it. Alternative if the core must be a UE module: `FPSemantics =
  FPSemanticsMode.Precise` (5.8 module property).
- Compiles in seconds, tests headlessly with zero UE — this is also what keeps the AI
  agent's build loop fast (§8).

### 3b. Challenger: UnrealSharp (C# — preserves the existing plan verbatim)
- MIT, active (pushed 2026-08-22), UE 5.6–5.8, .NET 10, hot reload, ships in packaged
  Windows builds [verified: repo + docs]. SHITFIRE.Core would stay a plain .NET class
  library under `dotnet test`, exactly as PORT_PLAN.md wrote it.
- Against it: self-described **"not production ready"**; source-only (no binary
  releases); ~71% of commits from one maintainer; documented silent failure where glue
  generation skips after engine upgrades until `Intermediate/` is deleted.
- Sane split if used at all: UnrealSharp for the *shell*, never for the core. Worth a
  one-day spike before deciding. (UnrealCLR is dead — last push June 2023. Verse doesn't
  exist outside UEFN until UE6, ~2028. Hazelight AngelScript = maintaining a custom
  engine fork; disqualified for a solo dev needing a headless parity harness.)

### 3c. Radical option: don't rewrite — run the existing JS via puerts
- **puerts** (Tencent): BSD-3, commits Aug 2026, releases covering UE 5.5/5.6, V8 or
  QuickJS backend, two-line C++ embed (`FJsEnv::Start`) for DOM-free logic [verified].
  The identical JS file then runs in Node, browser, and UE — parity becomes a diff.
- Costs: heavyweight plugin for 5% of its features, C++ project conversion, engine-
  upgrade lag (~6 weeks behind 5.6), V8 binary size.
- Fallbacks: getnamo/UnrealJs fork (V8 14.6, UE 5.7, bus factor 1), or embedding
  quickjs-ng (MIT, active) yourself — days of glue, full control.
- Not credible: Javy, Porffor (experimental compiler in a doctrinal parser = silent
  miscompile risk), full-game-in-CEF (viable but it isn't a port — UE contributes a
  window and 150 MB you don't use; Tauri/Electron do that job better).

### 3d. Float determinism — verified against this repo's actual source
- **The parity standard is looser than "bit-exact"** [verified: PARITY_TESTING.md]: only
  the mulberry32 PRNG stream must be bit-identical — and it is pure integer ops
  (`Math.imul`, xor, shifts), exactly portable to C++ `uint32_t` / C# `uint unchecked`.
  Everything else is tolerance-based: H(x,z) within 1e-9, positions/impacts within
  0.01 m.
- **The sim path is saturated with transcendentals** [verified: grep of `src/js`]:
  137 calls across 12 modules — sin/cos on every azimuth, `atan2`/`hypot` throughout,
  `pow`/`exp` in the terrain generator, and a Box-Muller Gaussian (`Math.log` +
  `Math.cos`, `10-mission.js:29`) inside the follow-up dispersion model itself.
  ECMAScript leaves these implementation-approximated — they differ across engines *and
  across Chrome versions* (V8 recently swapped fdlibm for LLVM-libc implementations).
  This is a latent property of the shipping browser build today, not just a port issue.
- **Why it mostly doesn't bite:** libm differences are last-ULP (~10⁻¹² m at map scales)
  — far inside every documented tolerance. The real risk is a **knife-edge branch flip**:
  a 1-ULP height difference flips a borderline `hasLOS`/placement check, changes how many
  PRNG draws are consumed, and the whole scenario diverges (the PRNG-order drift
  PARITY_TESTING.md warns about). Level 2 per-chapter generation tests catch it
  deterministically per seed — a debuggable failure, not an invisible one.
- Belt-and-suspenders (optional): vendor one transcendental implementation (V8's
  fdlibm-derived `ieee754.cc`, zlib-style licensed) on both sides. Do this only if a
  seed actually proves knife-edge.

## 4. Terrain & geospatial

**Recommendation: skip UE Landscape entirely.** Port H(x,z) (procedural island + DEM
sampler) into the §3 core; at load, build the visual mesh from it.

- **RealtimeMeshComponent** (TriAxis) — Core edition MIT, commits Aug 2026, UE 5.5–5.8
  [verified]. Chunked mesh, LODs, **no collision cooked at all** — the sim ray-march
  stays the only LOS/range/impact authority. Budget: 10 km at 10 m posts ≈ 1M verts of
  static geometry — trivial at 60 fps. Flat shading via ddx/ddy in the material (no
  vertex duplication).
- **One H, three consumers**: 3D mesh, ballistics/LOS ray-march, marching-squares map
  sheet — all read the same array. UE Landscape can't offer this (16-bit quantized
  heights ⇒ Landscape-H ≠ sim-H).
- **DEM path**: one command — `gdal_translate -outsize 2017 2017 -ot UInt16 -scale <min>
  <max> 0 65535` — into your own image-load path (same grayscale route the browser build
  uses). **Pin the min/max elevations as constants in the repo** — wrong scale produces
  normal-looking terrain with every ballistic solution subtly off (silent failure).
- **Rejected with reasons**: Cesium for Unreal (Apache-2.0, very active, but streaming
  LOD is camera/timing-dependent — a determinism liability — and offline local tiles are
  real friction); Nanite (fixed 9–20% GPU overhead, *hurts* low-poly content); Water
  plugin (still Experimental, crash reports, and its shoreline features need a Landscape
  to carve — use a flat translucent plane whose Z **derives from the sim's sea-level
  constant in code**); every in-editor GDAL plugin (dormant or thin forks).
- LWC/precision: unneeded but free — worst-case float granularity at the 10 km corner is
  ~0.6 mm ≈ 6×10⁻⁵ mil, five orders below a 0.1-mil tick. Center map on origin, done.
- Invariant worth a startup self-test: **terrain has no collision** — ray-march N known
  rays against golden browser values at boot. Two LOS implementations that can disagree
  is the classic silent grader-vs-laser mismatch.

## 5. UI, cartography, print/PDF (the port plan's "Large/High-risk" P5)

**The headline: the existing canvas map code ports nearly line-for-line.**

- **Screen map + printable sheet**: `UCanvasRenderTarget2D` — `stroke()`→`DrawLine`,
  `fillText()`→`DrawText`, `drawImage`→`DrawTexture`; redraw only on pan/zoom (it is
  explicitly not per-frame). Render targets go to 8192² — A3 @ 300 DPI (3508×4961) fits
  with room. **One map-draw code path, two resolutions** (screen + print).
- **Export**: built-in `ExportRenderTarget` → PNG. **v1 = "here's your PNG, print at
  100%."** v1.1 = wrap in a correctly-dimensioned PDF via **libharu** (zlib license,
  releases into 2026) — a PDF actually protects 1:50,000 scale better than any print
  dialog. Skip the OS print dialog. Avoid FF_PDFium (BSL 1.1 — restricted commercial).
  P5's "print/PDF has no free answer" is now answered.
- **HUD/reticle**: UMG; mil ticks via material math; dynamic overlays via `OnPaint`
  DrawLines in a C++ SLeafWidget if needed; static parts in an Invalidation Box.
- **Menus/comms log/AAR**: bread-and-butter UMG (ListView + rich text). 5.5 SDF fonts
  only where text scales. **No CEF for panels** — Chromium 90 until 5.7, version churn
  Epic itself struggles with.
- No maintained SVG/vector runtime exists for UE (SvgBooga dead/unlicensed; Skia
  integrations are blog-ware; ThorVG/NanoVG: nothing) — any vector assets become
  textures or draw code. ImGui (VesCodes fork, MIT, active) is debug-only by design.
- Known unknown to prototype first: Canvas text fidelity at 300 DPI (rasterize fonts at
  native large sizes for the print path).
- `HighResShot` cannot capture UMG at all — irrelevant to the map sheet; keep it for AAR
  beauty shots only.

## 6. Speech I/O (P7)

**STT: wrap vosk-api directly** in a small ThirdParty module.
- The killer feature is verified at the C API level: `vosk_recognizer_new_grm` — a
  **hard grammar constraint** fitting the ~60-word doctrinal vocabulary, with `[unk]`
  catching out-of-grammar speech. Apache-2.0, prebuilt win64 DLL, 40 MB English model,
  streaming-native (partials during PTT hold, final on release). ~15-function API; no UE
  plugin exists but the wrap is days, not weeks.
- **Safety property, not just accuracy**: grammar-constrained recognizers force-match
  garbage to legal phrases — always keep `[unk]` + a confidence gate so a bad read
  becomes "SAY AGAIN," never a wrong fire mission.
- Fallback: whisper.cpp (MIT, active; grammar fields exist in `whisper_full_params` and
  a guided-command example runs tiny.en on a Raspberry Pi). Utterance-final only, known
  drift on radio phraseology (Whisper-ATC measured it), plugins are archived-MIT or paid.

**FDC voice: recorded lines + digit splicing — generate the bank with Kokoro-82M.**
- **Kokoro-82M is the license-clean generator**: Apache-2.0 code *and* weights, no
  restriction on generated audio. Batch-render the ~120 quips + digit/proword bank once,
  ship WAVs.
- **Piper situation (agents disagreed; resolved):** rhasspy/piper (MIT) was archived
  Oct 6 2025; the successor OHF-Voice/piper1-gpl is **GPL-3** (embeds espeak-ng) — and
  §6(c) of Epic's EULA bars GPL in the product anyway. Piper-generated WAVs would be
  fine (GPL covers the tool, not its output; per-voice dataset licenses still apply),
  but Kokoro is cleaner on every axis. **XTTS: never** — non-commercial license covering
  the *output*, and Coqui is dead so no commercial license can ever be bought.
- Runtime: one MetaSound Wave Player queue (On Finished → next WaveAsset) for arbitrary
  grid readbacks — the FlightGear/Falcon-BMS spliced-digits pattern, proven for exactly
  this radio use. Radio DSP (bandpass 300–3000 Hz, bitcrush, noise bed, squelch clips)
  confirmed native via the Synthesis plugin + submix effects — and it hides splice seams.
- Splice cadence quality is a taste judgment **[YOUR CALL]** — budget an hour of
  listening.

## 7. Open-source adoptions (the named priority)

**Landscape fact: no open-source call-for-fire trainer exists to adopt** — on GitHub the
top hit for the phrase is SHITFIRE itself. Adopt parts, not bases:

| Adoption | License | What it saves |
|---|---|---|
| **Epic Niagara Examples Pack** (50+ systems, ~Jan 2026, 5.7) | Epic/UE-only (fine for a UE build) | The entire learn-Niagara-from-zero hump — retint a proven explosion into a flat-shaded airburst. Biggest single VFX cut. |
| **Poly Pizza search + Quaternius + Kenney** (Blocky Characters, tanks, nature) | CC0 (check per-model badge on Poly Pizza; CC-BY needs a credit line) | Troops/vehicles/vegetation in the deliberate low-poly style, license-clean in any engine. Bunker/watchtower/battery stay DIY boxes. |
| **Sonniss #GameAudioGDC bundles + Freesound (CC0 filter)** | Royalty-free / CC0 | Artillery cracks, distant thumps, radio squelch become a curation afternoon, not a synthesis project. |
| **Kokoro-82M** (§6) | Apache-2.0, outputs unrestricted | The whole FDC voice bank. |
| **FOPCSim / FOPCSim 2 theses** (Naval Postgraduate School, 2003/05) | US Gov work — public domain | Doctrinally-vetted grading rubric and task analysis from the actual USMC trainer lineage — design validation, free. Worth an evening before finalizing the grading port. |
| **OnAllFronts/ProjectM** | MIT (frozen at UE 5.0.3) | Reference C++ for milsim structure in UE — days of reference value, not a base. |
| **Fab Limited-Time Free** (biweekly rotation) | Fab Standard | Zero-cost habit: claim military/VFX packs each cycle; claimed = kept. |

**Do-not-use list**: OpenTournament (custom restricted license), ACRE2/ArmaBallistic-
Computer and all GPL code (viral + barred by Epic §6(c) — read for design only), Arma
mod content (non-portable licenses), Voxel Plugin free legacy (no license file), Quixel
free-era (over since end-2024). Epic sample content (Lyra, Niagara pack) is UE-only
forever — keep it out of any shared/browser codepath.

## 8. Tooling for the AI agent building the port

**The landscape changed June 2026: Epic shipped first-party agent tooling.**
- **UE 5.8 MCP plugin** (Experimental, in-editor HTTP/SSE server, loopback, no auth) +
  **Epic's official Claude Code plugin** (`unreal-engine-skills-for-claude-code`, MIT —
  installs via the official marketplace; `ModelContextProtocol.GenerateClientConfig
  ClaudeCode` writes `.mcp.json`). This pair outlives the hobby MCP servers (chongdashu,
  kvick stale ~16 months; flopperam commercialized). **Windows caveat: the plugin's
  SessionStart hook is bash-based — launch Claude Code from Git Bash.** Known limit:
  Live-Coded functions don't reach the MCP server without an editor restart. Treat MCP
  as hands-and-eyes, never as a gate.
- **The three-tier gate loop** (recreating this repo's build/lint/replay/shots stack):
  - **Tier 0 — sim core outside UE** (§3a): CMake build, CLI test/replay harness,
    parity gates. Compiles in seconds. This is where the browser project's loop
    survives intact — the load-bearing decision.
  - **Tier 1 — UE logic gate**: `Build.bat` incremental compile → `UnrealEditor-Cmd
    <proj>.uproject "-execcmds=Automation RunTests SHITFIRE" -unattended -nopause
    -testexit="Automation Test Queue Empty" -game -nullrhi -nosound -nosplash
    -ReportExportPath=…` (JSON+HTML report a wrapper script parses). Editor boot adds
    30–90 s — sub-minute iteration only exists in Tier 0.
  - **Tier 2 — visual gate** (GPU, less often): functional tests + Screenshot
    Comparison actors, Enhanced Input `InjectInputForAction` to drive the player,
    ground truths in LFS. `-nullrhi` kills rendering, so this tier is separate by
    construction. Baseline approval is GUI-side (human blesses new ground truths).
- **The binary-asset problem — hard rules**: no credible Blueprint↔text round-trip
  exists (confirmed; T3D import removed in 4.13, 5.8's new Diff Tool is GUI-only). So:
  **C++-only gameplay logic, no Blueprint logic**; campaign/config as CSV/JSON in git,
  imported to UDataTables by a checked-in Python script run via `UnrealEditor-Cmd
  -run=pythonscript` (headless, verified automatable); levels assembled procedurally
  from C++ so `.umap` stays near-empty; One File Per Actor + LFS `.gitattributes` for
  the binaries that remain; **a CI tripwire that fails when new .uassets appear outside
  an allowlist** — one GUI-tweaked asset becoming invisible to the agent is a silent,
  permanent hole.
- Python Editor Scripting (3.11 embedded) automates assets/imports/level population but
  **cannot author Blueprint node graphs** — the strongest argument for the C++-only rule.

## 9. What transfers to Godot regardless

If the Godot decision stands, these findings carry over directly: the sim-core-outside-
the-engine pattern with `/fp:precise` (§3a/§3d — Godot C# has the same transcendental
situation); the parity analysis (only the PRNG needs bit-exactness; knife-edge branches
are the real risk; Level 2 catches them); the entire speech stack (Vosk grammar API,
Kokoro voice bank, spliced digits — engine-agnostic C libraries and WAVs); the DEM
pipeline (gdal_translate one-liner + pinned min/max); every asset adoption in §7 except
the Epic-licensed Niagara pack; and the FOPCSim grading rubric.

## 10. Animation & unit presentation (added 2026-08-28)

**The pixel math that governs everything** [inferred, simple trig]: through the 9° binos
on a 1920px screen, a 1.8 m troop is ~20 px at 1,100 m and ~7 px at 3,200 m; naked-eye,
1–2 px. A 6 m vehicle ≈ 23 px at max range; a 20 m burst cloud ≈ 76 px. Animation
investment should follow the pixels — and the thing the observer actually stares at is
the burst, not the troops.

Plan, ranked by payoff per hour:

1. **Effects first.** Niagara Examples pack + Kenney CC0 sprite sheets (Particle Pack 80
   sprites, Smoke Particles 77 — both CC0 [verified]) as Niagara SubUV flipbooks.
   World-scaled billboard burst: flash frame → ~2 s dirt bloom → **persistent smoke
   column** (ribbon/panner plane, ~60 m ≈ 230 px in binos, alive for minutes — a
   training feature: the observer re-finds the impact to adjust). Night fire: Niagara
   Light Renderer (particle-attached lights, verified 5.8) + emissive flash.
2. **Buildings: authored swap meshes, no Chaos.** Intact/damaged/rubble variants of the
   existing boxes (~1 hr each in Blender), enum-driven swap keyed to sim events, one-shot
   Niagara debris to mask the pop. Chaos Destruction's runtime debris is nondeterministic
   (community guidance: "replicate the damage event, not the result") — cosmetic garnish
   at 20× the effort. No CC0 rubble pack matches flat-shaded boxes; author them.
3. **Vehicles: spline-follow, no physics.** One SplineComponent along the coast road;
   per vehicle `distance += speed·dt` → `GetTransformAtDistanceAlongSpline(d − i·spacing)`
   [verified API]; pit stop = hold the accumulator at a marked distance. Dust ribbon
   sells motion better than wheels (which don't resolve at 23 px). **Chaos Vehicles
   confirmed overkill** — nine setup stages simulating suspension/tires for a box.
4. **Troops: states before cycles.** Tier (a): standing/prone/dead pose swaps on the
   existing position-lerp — disperse = radial velocity + prone. Naked-eye complete on
   its own. Tier (b), binocular polish **[YOUR CALL whether it's worth it]**: ordinary
   skeletal meshes with retargeted CC0 clips — UE 5.4+ IK Retargeter auto-chains make
   this an afternoon, not the 5.0-era ordeal. At 30–100 agents, plain SkeletalMesh
   components are fine — **skip VAT/AnimToTexture** (crowd tech for thousands).
5. **Civilians**: troop pipeline, different skins, nearly free.

**The CC0 animation backbone (no Mixamo dependency needed):**
- **Quaternius Universal Animation Library 1+2** — 250+ clips incl. crawl and deaths,
  CC0, "ready for retargeting," FBX/GLB/Blend [verified, Mar 2025/Jan 2026].
- **KayKit Character Animations** — 161 humanoid clips (150+ free tier), CC0, incl.
  crawl/sneak/crouch/death/aim/shoot/reload — nearly the whole troop vocabulary
  [verified, active].
- **Kenney Blocky Characters v2** — 18 characters, 27 animations, CC0 [verified].
- **Mixamo** is optional: in-game use (free or commercial) is permitted; the trap is raw
  FBX in a public repo (the "redistribute raw files" clause) — keep source assets
  private, and note the service is unmaintained (download early). FAQ page resisted
  direct fetch — eyeball it once in a browser before shipping Mixamo content.
- **Excluded**: Bandai Namco motion datasets (CC BY-NC — forecloses ever charging), CMU
  mocap (cleanup cost ≫ value at 7 px), Sketchfab as a future source (in managed
  decline; CC-licensed models can't migrate to Fab — grab anything wanted now).

## 11. Modding-community lessons, tooling, resources (added 2026-08-28)

**The single best repo found: SkyEye** — https://github.com/dharmab/skyeye, **MIT**
[verified]. A self-hostable DCS AI radar-controller bot: local Whisper STT → intent
parse → doctrinal brevity state machine → TTS reply over the radio. It is SHITFIRE's
FDC loop, already architected, legal to study *and* port logic from (Go, so patterns
not drop-in code).

- **Voice data model**: Falcon BMS's frag/eval/comm three-layer structure (word →
  multi-voice variant set → sentence template) is the proven spec for the spliced FDC
  voice [verified: BMS forum]. Adopt the structure; record our own audio (Kokoro, §6).
- **Adoptable calculator code/UX**: SquadMC and MSMC (both MIT [verified]) — heightmap-
  aware solutions from grid pairs, keypad-grid input. UX lessons: dual map-click/typed
  grid input, visible toggleable heightmap (teaching feature), live min/max range rings,
  second-screen companion use. **Trap: SquadCalc looks MIT but carries a custom
  non-commercial clause** [verified: fetched license].
- **Doctrine sources**: USMC TBS handout *"Call For Indirect Fire" (B2C0299XQ)* on
  trngcmd.marines.mil — US Gov work, public domain, verbatim-usable for dialogue and
  grading rubric [verified URL; **contents not yet read — highest-value next fetch**].
  UNITAF's call-for-fire + adjust-fire SOPs: no license stated — learn the flow, write
  our own words.
- **Design lesson from DCS Combined Arms**: its JTAC lasing fails silently at grazing
  angles (laser marks short/long of the vehicle with no feedback) — the canonical
  silent failure of the genre. SHITFIRE should always give the student feedback about
  where the laser actually hit.
- **Architecture lessons from OpenRA** (GPL code, freely-imitable documented design):
  data-driven unit/weapon definitions in external text; order-stream replays (inputs +
  seed → deterministic re-sim) as the cheapest AAR and grading audit trail — SHITFIRE's
  TLOG replay already is this; keep it sacred. Auto-generate scenario-schema docs from
  code so they don't rot.
- **Scenario format for instructor authoring** (future): DCS's `.miz` pattern — **a zip
  of plain-text files** (human-diffable inside, single-file distributable outside).
  A `.sfmission` = zip of JSON (targets, TRPs, weather, grading thresholds) is the
  shape; and ship a minimal in-game editor first — hand-editing is polish, not the
  entry ramp. Version + license + compatibility fields on scenario packs from day one.
- **Read-only shelf** (study, never copy): ACE3 artillerytables (GPL — real firing-table
  math and range-table UI), DCS-SRS (GPL — radio occupancy/blocking model), ITC Land
  Systems / Fire For Effect / Drongo's (FDC workflow design, licenses unchecked),
  OpenKneeboard (custom GPL-derived), Squad Mortar Helper (CC BY-NC-SA). **Bohemia's
  porting bar is total** [verified quotes]: Arma assets/data can't move even between
  Bohemia's own titles — no "it's just config data" loophole.
- **License hygiene findings**: several popular repos have *no license file at all*
  (easyarty, Ortho4XP, lightningstools — all-rights-reserved by default); MIT
  calculator repos routinely bundle game-IP images the code grant doesn't cover; start
  a CREDITS file now (CC-BY attributions, USGS courtesy line) before provenance is
  forgotten.

## 12. Assumptions

- The Kofa GeoTIFF is a projected-CRS product with square-meter pixels; if it's the
  1/3-arc-second geographic product, a `gdalwarp` reprojection precedes the
  `gdal_translate` step. Breaks if wrong: distances on the DEM map are subtly non-metric.
- UnrealSharp external-.csproj referencing works as standard .NET tooling implies
  [inferred, not spiked]. Breaks: the C# plan needs the core inside their solution shape.
- MetaSound Wave-Player queue pattern assembled from verified pins but not built.
  Breaks: fall back to SoundCue Concatenator (verified, less dynamic).
- Sub-minute Tier 1 loop assumes precompiled Launcher engine + small game module
  [inferred, unbenchmarked].
- Agent-verified repo states are as of 2026-08-27; Fab listings that 403'd (prices,
  some licenses) are marked not-checked in the underlying reports.
