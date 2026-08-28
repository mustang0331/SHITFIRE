# Research 08 — Tooling that keeps a UE project buildable/testable/verifiable by an AI coding agent

Agent report, checks performed 2026-08-27 (all fetched, not recalled). Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §8.
Problem: the browser project's agent loop (text source, Node build, headless Playwright, replay gates) must survive UE's binary assets, GUI editor, and slow builds.

## 1. MCP servers for Unreal

**The landscape changed June 2026: Epic shipped its own.**

**Epic's official Unreal MCP plugin (the credible choice)**
- Ships in **UE 5.8** ("MCP Server (Experimental)" in the release notes) [verified].
- From Epic's doc [verified: unreal-mcp-in-unreal-editor]: server runs **inside the editor**, HTTP+SSE, default `http://127.0.0.1:8000/mcp`, loopback-only, **no authentication**, Experimental ("many features are incomplete or missing"). Toolsets: spawn/modify actors, lighting, material instances, Slate widget inspection, **running automation tests**. Claude Code first-class: console command `ModelContextProtocol.GenerateClientConfig ClaudeCode` writes `.mcp.json` into the project root. Known limit: **Live Coding doesn't propagate new functions to the MCP server — editor restart required.**
- Companion: **EpicGames/unreal-engine-skills-for-claude-code-plugin** — MIT, 246 stars; installs via `/plugin install unreal-engine-skills-for-claude-code@claude-plugins-official`; `unreal-mcp` skill, SessionStart hook injecting project context, skills for actors/blueprints/assets/materials/Sequencer/Niagara/UMG/C++ testing/Python. **Windows caveat: the SessionStart hook is bash-based — launch Claude Code from Git Bash/WSL, not native PowerShell** [verified: repo].

**Third-party servers (state Aug 2026)**
| Repo | License | UE | Last activity | Notes |
|---|---|---|---|---|
| chongdashu/unreal-mcp | MIT | 5.5+ | **stale ~16 months** (Apr 2025) [verified] | C++ TCP plugin + Python FastMCP; BP node-graph editing; "not for production" |
| kvick-games/UnrealMCP | MIT | 5.5 only | Jun 2025 (last real work Apr 2025) [verified] | "VERY WIP" |
| flopperam/unreal-engine-mcp | MIT (repo) | 5.5–5.7 | active but **acquired by Aura** — best ~50+ tools now hosted/commercial [verified] | broadest surface (BP ~40 node types, Niagara, PCG) |
| runeape-sats/unreal-mcp | MIT | 5.3 | small (19 commits) [verified] | pure Python over Remote Control API |
| prajwalshettydev/UnrealGenAISupport | MIT | 5.4–5.7+ | active [verified] | LLM API bindings + MCP; documents own BP-connection bugs |
| Natfii/UnrealClaude | MIT | **5.7 only** | active, 176 commits [verified] | Claude Code CLI embedded in editor + MCP:3000; BP tools "a few bugs" |

**Verdict:** Epic's 5.8 MCP + Epic's Claude Code plugin. Treat MCP as the *editor-manipulation* channel — hands-and-eyes — never the backbone of the gate loop.

## 2. Headless command-line UE

- Compile: `Engine\Build\BatchFiles\Build.bat <Target> Win64 Development -project=<path>.uproject` [inferred — universal; page not re-verified].
- Package: `RunUAT.bat BuildCookRun -project=X.uproject -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive` [verified: Epic doc, incl. the tip that a Custom Launch Profile prints the exact generated command]. **5.8: Incremental Cooking (Beta)** [verified: release notes].
- Headless tests [verified: Epic 5.8 doc]: `-ExecCmds="Automation RunTest A+B;Quit"` (also `Group:` filters), `-ReportExportPath="<path>"` (JSON + HTML results), `-ResumeRunTest`. Battle-tested full invocation [verified: craftedcart guide; UE5 exe name inferred from the 4→5 rename]:
  `UnrealEditor-Cmd.exe game.uproject "-execcmds=Automation RunTests <Prefix>" -unattended -nopause "-testexit=Automation Test Queue Empty" -log=TestResults.log -game -nullrhi -nosound -nosplash`
- Flag semantics [verified: 5.8 command-line reference]: `-nullrhi` = headless null RHI; `-unattended` = no dialogs; `-RenderOffscreen` documented.
- **Commandlets**: `-run=<Name>` runs a UCommandlet with no UI — the classic CI hook; maintained example plugin exists [verified]. Write one per gate (validate data tables, rebuild derived assets, dump state as JSON).

## 3. Editor scripting

- **Python Editor Scripting** [verified: 5.8 doc]: embedded Python 3.11.8; automates asset management, imports, procedural level layout, property setting. CLI entry points: `-ExecutePythonScript=` (full editor) and **`-run=pythonscript` — fast, headless**; `init_unreal.py` auto-runs. **Hard limit: editor-only** — never in PIE/standalone/cooked builds.
- **The Blueprint gap**: Python's `BlueprintEditorLibrary` exposes **no methods to create or connect graph nodes** [verified: API docs sweep + sackbird article]. Node-graph authoring exists only via custom C++ plugins (what MCP servers ship).
- **Remote Control API** [verified: 5.8 doc, Beta]: HTTP call any BP-callable function, get/set properties (undoable transactions), asset-registry search, batching. No auth documented; editor-focused.
- Scripted-only workflows: assets/data tables/imports/level population — **yes**, established practice. Blueprint logic — **no**. Editor Utility Widgets are themselves .uassets [inferred: agent-hostile]; Python + commandlets covers the ground textually.

## 4. The binary-asset problem

- **No credible Blueprint↔text round-trip. Confirmed.** T3D import removed in UE 4.13; experimental TextAssetFormatSupport (~4.21) status unconfirmed; BP logic compiles through a 9-stage pipeline to bytecode [verified: sackbird article, Mar 2025 + forums]. Clipboard copy-paste-as-text pastes back **only into the GUI editor**; no headless paste API [verified clipboard behavior; API absence inferred from the Python gap].
- "UnrealBlueprintify": **no such tool** [searched, not found]. One-way BP→text exists: milestones14/BP-To-Text (clipboard→pseudocode), protospatial/NodeToCode (BP→C++) [verified] — for *reading* BPs, useless for writing. Programmatic BP authoring via MCP C++ plugins is the flakiest part of every server [verified: their own issue notes]. Last resort.
- **UDataTable from CSV/JSON: fully automatable** — `unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string` / `FillDataTableFromCSVFile`; setting `import_row_struct` forces dialog-free automated import [verified: 5.5 Python API]. Campaign/config lives as CSV/JSON in git; checked-in Python regenerates tables. Alternative skipping .uassets entirely: runtime JSON via UE's Json module [verified: module]; community SGDynamicTextAssets does exactly this [existence verified].
- Procedural level assembly from C++ (near-empty .umap, spawn in BeginPlay): standard technique [verified: tutorials]; no famous "whole game this way for agent-friendliness" writeup found [not found].
- Git: `*.uasset` / `*.umap` → LFS via .gitattributes; LFS-track raw media; enable **One File Per Actor** so levels split into many small files [verified: ue5-gitignore + anchorpoint guide].
- **Text-based assets from Epic: no announcement.** The forum request thread remains open; 5.8's new UE Diff Tool is a **GUI-only visual BP diff** [verified]. Epic's bet is MCP access to binary assets, not text formats.

## 5. Visual verification

- **Screenshot Comparison Tool** alive in 5.8: ground-truth vs incoming, capture via Functional Screenshot Test actor, failures in the Screenshot Browser [verified: doc]. Agent can *detect* regressions headlessly (test failure + ReportExportPath with images); **a human blesses new baselines** (Session Frontend GUI).
- HighResShot: console command, `filename=`, resolution/multiplier; triggerable via `-ExecCmds`; saves under Saved/Screenshots [verified]. Cheap frame-grab primitive.
- **Movie Render Queue CLI**: `-LevelSequence= -MoviePipelineConfig= -Unattended`; custom Python executors; works in runtime builds [verified: 5.8 doc]. The deterministic-frames option (fixed timestep).
- **Input injection**: Enhanced Input `Inject Input for Action` / `Inject Input Vector for Action` [verified: snippet + guide]; **Automation Driver** simulates input against Slate/UMG [verified: 5.7 doc]. Commercial Playwright-alike: GameDriver [exists; not evaluated].
- **Gauntlet**: documented through 5.7, usable outside Epic, thin docs; community UE5 demo repo exists [verified]. Best 2025 survey: Andrew Fray's "Topography of Unreal Test Automation in 2025" — recommends functional/Spec-style tests; flags **CQTest** (Rare's framework, now in-engine) as best ergonomics [verified].
- **Critical interaction: `-nullrhi` kills rendering — screenshot tests can't run under it.** Visual gates need a GPU pass (`-RenderOffscreen` or windowed), separate from the fast logic gate [inferred from flag definition; two-tier split standard].

## 6. Build speed for an agent loop

- Live Coding: official docs; community reports 30–60 s even for minor tweaks in larger projects; improved by small modules [verified]. Bigger agent problem: it patches a *running editor* the agent can't fully see, and MCP doesn't get new functions without restart [verified, §1]. Realistic loop: precompiled Launcher engine + small game module + `Build.bat` incremental (~15–60 s) + fresh `UnrealEditor-Cmd` run (tens of seconds boot) [inferred; unbenchmarked — sub-minute achievable for the logic gate only].
- **The lightweight alternative is the real answer**: sim core as an engine-agnostic C++ library (Epic-supported external-lib linking, `RuntimeDependencies`/`GetDllHandle` for DLLs [verified]) — compiles in seconds with MSVC/CMake, unit/replay/parity tests **without UE in the loop at all**. The single best transplant of the browser project's loop.
- 5.8 Incremental Cooking (Beta) helps the packaging gate [verified].

## 7. AI-assist infrastructure specific to UE

- Epic's own, all real in 2026: web **Epic Developer Assistant** (5.6-era) [verified: forum + portal]; **in-editor AI Assistant (5.7, Experimental)** [verified: coverage; Epic page 403'd]; the 5.8 MCP plugin; the official Claude Code plugin (§1).
- Community: UnrealClaude, UnrealGenAISupport (§1). **No established "CLAUDE.md for Unreal" convention found** [searched] — nearest is Epic's SessionStart hook injecting project context. Commercial: Aura (absorbed flopperam's MCP).

---

## Recommended agent workflow (three tiers, fastest first)

**Tier 0 — sim core outside UE**: engine-agnostic C++ static lib (ballistics, missions, scoring, replay), own CMake + CLI test/replay harness; replay-parity gates live here; compiles in seconds; verified exactly like the Node project. UE links it via a thin module.
**Tier 1 — UE logic gate**: C++-only gameplay code (hard rule: no Blueprint logic, no hand-edited assets), config/campaign as CSV/JSON → UDataTables via checked-in Python run through `UnrealEditor-Cmd -run=pythonscript`, levels assembled procedurally so .umap stays near-empty (OFPA + LFS). Gate: `Build.bat` incremental → the `-execcmds="Automation RunTests SHITFIRE" … -nullrhi …` invocation with a wrapper parsing the JSON report — the Playwright-equivalent CI command.
**Tier 2 — visual gate (GPU, less often)**: functional tests + Screenshot Comparison actors + Enhanced Input injection; ground truths in LFS; run without `-nullrhi`; MRQ CLI for deterministic frame dumps.
MCP (Epic 5.8 + Claude Code plugin, launched from Git Bash) = interactive hands-and-eyes only, never a gate.

**Top risks where the loop breaks down**: (1) **silent drift into binary content** — one hand-made BP/GUI-tweaked asset becomes permanently agent-invisible; CI tripwire on new .uassets outside an allowlist. (2) **editor-in-the-loop fragility** — Experimental MCP, no auth, needs a live GUI editor, restart-after-Live-Coding; keep MCP out of gates, `-unattended` everywhere. (3) **the visual gate is weaker than Playwright** — GPU needed, GUI baseline approval, TAA/Lumen flake; tune tolerances, keep Tier 2 small. (4) even the "fast" UE gate pays 30–90 s editor boot — which is why pushing maximum logic into Tier 0 is the load-bearing decision.
