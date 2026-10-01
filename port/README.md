# port/ — SHITFIRE sim core for the Unreal Engine 5.8 port

The engine-free C++ sim core (`shitfire_core`) plus the parity tests that
prove it against the committed browser-oracle fixtures. The sim core lives
OUTSIDE the engine (UNREAL_RESEARCH.md §3a): a plain static library compiled
`/fp:precise`, later linked into UE as a prebuilt third-party lib so UE's
`/fp:fast` build flags can never touch its machine code.

## Version pins

| Thing | Pinned at | Verified |
|---|---|---|
| Fixtures | tag `oracle-v1` (commit 6642f92; capture commit 2b6a41a) | ENGINE_PORT/fixtures/MANIFEST.json |
| CMake | 4.4.3 | `cmake --version` on the build machine |
| MSVC toolset | 14.44.35207 (VS Build Tools 2022 17.14, generator "Visual Studio 17 2022") | vswhere |
| C++ standard | C++20 | core/CMakeLists.txt |
| nlohmann/json (vendored, tests only) | v3.12.0 (MIT) | vendor/nlohmann/json.hpp |
| Catch2 amalgamated (vendored, tests only) | v3.16.0 (BSL-1.0) | vendor/catch2/ |

## Build and run (PowerShell 5.1 — no `&&`)

```powershell
# cmake reachable after refreshing PATH:
$env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')

cmake -S port -B port/build -G "Visual Studio 17 2022" -A x64 -DSHITFIRE_FIXTURES=C:/Users/conno/prj/SHITFIRE-port/ENGINE_PORT/fixtures
cmake --build port/build --config RelWithDebInfo
ctest --test-dir port/build -C RelWithDebInfo -L level1 --output-on-failure
```

Also build and run `-C Debug` once per change set — it catches `/fp:` and
iterator-debug surprises early.

## Layout

- `core/` — the sim core. Double-only, standard library only, no exceptions
  thrown or caught, no file I/O, no engine or platform headers. ALL math
  bodies live in `.cpp` files; NOTHING computational is inline in a header,
  because UE recompiles inline code under `/fp:fast` — this is a hard rule.
- `core-tests/` — Catch2 + ctest parity tests consuming
  `ENGINE_PORT/fixtures` (pass the absolute path via `-DSHITFIRE_FIXTURES`).
  nlohmann/json lives only here, never in core.
- `vendor/` — vendored single-file dependencies (see CREDITS.md).

## Porting rules

| JS | C++ rule |
|---|---|
| number | double, never float in core |
| x\|0, bitwise | ToInt32; 32-bit-clean values: direct u/int32 ops; unknown-range doubles: js::toInt32() helper in jsemantics.h (exact ECMA ToInt32: truncate, mod 2^32, reinterpret signed) — never raw static_cast<int>(double) (UB) |
| >>> / >>> 0 | uint32_t shift / ToUint32 |
| Math.imul(a,b) | low 32 bits of uint32_t product |
| Math.round | js::round(x) = std::floor(x + 0.5) — JS rounds half toward +infinity; NEVER std::round |
| % on doubles | std::fmod |
| transcendentals | std:: equivalents; tolerance-checked, never bit-checked |
| arr.sort | std::stable_sort always |
| object literals | named structs, std::optional for nullables |
| key iteration | insertion order explicit; never iterate std::unordered_map where order feeds logic/PRNG |
| closures with state | one closure = one class |
| PRNG call order | prime directive: port loops, early-exits, retries, candidate scans in source order |

No exceptions thrown/caught anywhere under core/ (UE compiles exceptions-off;
nlohmann lives only in core-tests). No file I/O in core. No engine or
platform headers — standard library only.

**hypot note:** `Math.hypot` is implementation-approximated in JS; the port
uses `std::hypot`. If an H-grid or ray fixture ever fails at the 1e-9 edge,
the documented fallback is `std::sqrt(x*x + z*z)` — whichever matches V8 gets
kept, with the choice recorded here. As of the P1 gate, `std::hypot` passes
every fixture.

## Tier gate

Parity tests are labeled `level1` / `level2` / `level3` (ctest `-L`):

- **Level 1** (this stage, P1): pure-function parity — PRNG streams
  bit-exact (u32 AND double), hash/noise, procedural + DEM terrain grids,
  ray/LOS, grid helpers. **Every Level-1 test must be green, with the PRNG
  test bit-exact, before anything merges. Nothing merges past a failing PRNG
  test.**
- **Level 2** (P2+): scenario-generation parity per chapter seed/difficulty.
- **Level 3** (P2+): full mission replay from recorded transcripts.

Tolerances come from ENGINE_PORT/PARITY_TESTING.md and are never loosened to
make a test pass — a miss outside tolerance gets reported, not absorbed.

### In-engine gates (P3/P4) — Tier 1

The UE automation suite (Development editor, headless):

```powershell
$env:UE_SDKS_ROOT = 'C:\Users\conno\prj\SHITFIRE-port\port\unreal\AutoSDK-stub'
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  C:\Users\conno\prj\SHITFIRE-port\port\unreal\Shitfire.uproject `
  -ExecCmds="Automation RunTests SHITFIRE" -unattended -nopause -nosplash -nosound `
  -testexit="Automation Test Queue Empty" -ReportExportPath=<dir>
```

(`-nullrhi` is fine for `SHITFIRE.Parity` and `SHITFIRE.Observer`; the
`SHITFIRE.UI` tests render widgets and take screenshots, so run those with
the GPU.) `<dir>/index.json` is the record. Tests, all green 2026-10-01:

| Test | Proves |
|---|---|
| `SHITFIRE.Parity.Smoke` | PRNG u32-exact + one H row through the linked lib inside the engine process |
| `SHITFIRE.Parity.SimHostReplay` | five oracle transcripts (05 bunker-bracket, 13 convoy, 14 danger-close, 20 60mm, 32 fratricide) through `UShitfireSimHost` at 1000x: impacts 0.000 m, AAR exact, delegate surface == TLOG |
| `SHITFIRE.Observer.LaseGate` | all 500 raylos fixture rays through the production lase path |
| `SHITFIRE.UI.TypedMission` | mission_05 typed key-by-key into the live text box: same parity, every TLOG net row rendered, AAR overlay up |
| `SHITFIRE.UI.CommsLogBudget` | 2,050 lines, max 50-line append batch < 2 ms, virtualized |

### The Shipping parity gate — `-ShitfireParityGate`

The automation framework does not exist in Shipping builds, and Shipping is
exactly where the `/fp:fast` risk lives (UNREAL_RESEARCH.md §3a). So the
same replay machinery (`Source/ShitfireGame/Private/parity/`) also runs as a
command-line mode of the game itself, at game-module startup, before any
engine init or content: smoke + the five transcripts, a JSON report, and an
exit code (0 pass · 1 fixtures unreachable · 2 smoke failed · 3 a transcript
failed). It works in every config:

```powershell
# Development, from the editor binary (seconds):
& "...\UnrealEditor-Cmd.exe" <proj>.uproject -game -ShitfireParityGate -unattended -nullrhi -nosound
# report: port/unreal/Saved/ParityGate/parity-gate-Development.{json,txt}
```

A **Shipping** build needs cooked content even to boot (the uncooked project
crashes in package loading during engine pre-init, before the game module
starts — verified 2026-10-01), so the Shipping gate runs from a staged build:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" Shitfire Win64 Shipping -project=<proj>.uproject
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project=<proj>.uproject `
  -platform=Win64 -clientconfig=Shipping -cook -stage -pak -skipbuild -unattended -nop4
& port\unreal\Saved\StagedBuilds\Windows\Shitfire\Binaries\Win64\Shitfire-Win64-Shipping.exe `
  -ShitfireParityGate -ParityFixtures=C:\Users\conno\prj\SHITFIRE-port\ENGINE_PORT\fixtures -unattended -nullrhi -nosound
# exit code is the verdict; report under the staged build's Saved/ParityGate/ (or %LOCALAPPDATA%\Shitfire\Saved\ParityGate\)
```

`-ParityFixtures=<dir>` points a relocated exe at the repo fixtures. The
`*.started` breadcrumb is written before anything else so a boot crash is
distinguishable from a gate failure.

**Result 2026-10-01 (Shipping, staged build, 160 MB exe):** exit 0 — smoke
PRNG 0 mismatches, H row max |err| 7.3e-14 m; transcripts 5/5 exact, worst
|dTgt diff| 0.000 m, worst skew 0.100 s, every one on attempt 1; 0.48 s for
the whole gate. Same numbers as Development and as Tier-0 — the prebuilt
`/fp:precise` lib is byte-for-byte the same math under a Shipping link.

## unreal/ — the UE 5.8 project (P3+)

`port/unreal/Shitfire.uproject`, two modules:

- **ShitfireCoreLib** (`ModuleType.External`) — compiles nothing. Exposes
  `port/core/include` and links the prebuilt `shitfire_core.lib` from
  `port/build/core/RelWithDebInfo/`. **Rebuild the lib via CMake before UBT
  whenever core changes** (`cmake --build port/build --config RelWithDebInfo`);
  UBT does not run CMake and will happily link a stale lib. UE's `/fp:fast`
  flags can never touch the lib's machine code, and core headers carry no
  computational inline bodies — that is the whole `/fp:precise` parity
  guarantee (UNREAL_RESEARCH.md §3a).
- **ShitfireGame** (Runtime) — the shell: C++-only gameplay logic (hard rule:
  no Blueprint logic; the only .uassets are script-generated data assets on
  the allowlist below). World assembly is procedural from C++ on BeginPlay;
  the map asset is near-empty. Layout of the shell as of P4 (2026-10-01):
  `bridge/` (UShitfireSimHost — the one SimCore owner, coords shim, FX
  actors) · `observer/` (pawn, optics, laser) · `ui/` (reticle, mil card,
  observer HUD, net HUD = status strip + comms log/typed input + AAR panel)
  · `units/` (skeletal soldiers, vehicles, watchtower, battery) · `audio/`
  (radio-net audio subsystem) · `parity/` (transcript replay + the Shipping
  gate) · `Tests/` (automation).

Build (PowerShell):

```powershell
$env:UE_SDKS_ROOT = 'C:\Users\conno\prj\SHITFIRE-port\port\unreal\AutoSDK-stub'
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  ShitfireEditor Win64 Development `
  -project="C:\Users\conno\prj\SHITFIRE-port\port\unreal\Shitfire.uproject" -WaitMutex
```

**AutoSDK-stub**: this machine has no .NET Framework SDK, which makes UBT's
rules pass throw while instantiating the editor's SwarmInterface (Lightmass)
module — a module this fully-dynamic project never uses and which is
precompiled in the Launcher engine anyway. `port/unreal/AutoSDK-stub` carries
a stub `mscoree.h` at the path UBT's NetFxSDK AutoSDK probe checks
(`HostWin64/Win64/Windows Kits/NETFXSDK/4.6.2/Include/um/mscoree.h`);
pointing `UE_SDKS_ROOT` at the stub satisfies the probe without installing
anything. AutoSDK roots are additive — the real Windows SDK/MSVC discovery
still runs from the registry. If the .NET Framework SDK is ever installed
properly, delete the stub and drop the env var.

### Binary-asset tripwire (pre-commit discipline — every P3/P4 agent)

```powershell
powershell -ExecutionPolicy Bypass -File port\check-uassets.ps1
```

Fails listing any `*.uasset`/`*.umap` under `port/unreal` not matching
`port/unreal/uasset-allowlist.txt`. Run it before EVERY commit that touches
`port/unreal`. A GUI-made asset that lands unlisted is permanently invisible
to the agents — that is the silent failure this gate exists for. Allowed
assets are script-regenerated (`port/unreal/Tools/*.py` via
`UnrealEditor-Cmd -run=pythonscript`); the scripts are the source of truth.

Git LFS (git-lfs 3.5.1 on the build machine) tracks `*.uasset`/`*.umap` via
`port/unreal/.gitattributes`.

### P3 performance snapshot (how to rerun)

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  C:\Users\conno\prj\SHITFIRE-port\port\unreal\Shitfire.uproject `
  -game -windowed -ResX=1920 -ResY=1080 -ForceRes -novsync -ShitfireBench -log
```

Self-driving: 5 s warmup, 30 s CSV-profiled deterministic camera sweep from
the OP tower across the island toward the target area, screenshot, quit.
CSV lands in `port/unreal/Saved/Profiling/CSV/`, screenshot in
`port/unreal/Saved/Screenshots/WindowsEditor/ShitfireBench.png`.
Add `-ExecCmds="r.DynamicGlobalIlluminationMethod 0,r.ReflectionMethod 0"`
for the Lumen-off comparison run.

The 2026-09-02 P3 numbers were taken ON BATTERY (Windows caps the frame
rate at ~30 and downclocks the GPU) - rerun PLUGGED IN before making the
Lumen keep/drop call. Battery-throttled GPU cost at native 1080p:
26.4 ms with Lumen GI, 20.4 ms without (delta ~6 ms); GameThread ~5 ms.

### Art asset pipeline (BUILDER-ASSETS, P3)

Every downloaded art asset is pinned in `port/unreal/Tools/asset_manifest.json`
(source URL, license, author, fetch date - provenance duplicated per asset in
`port/CREDITS.md`). Regeneration is two commands:

```powershell
python port\unreal\Tools\download_assets.py   # URLs -> %LOCALAPPDATA%\shitfire-asset-cache
$env:UE_SDKS_ROOT = 'C:\Users\conno\prj\SHITFIRE-port\port\unreal\AutoSDK-stub'
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  C:\Users\conno\prj\SHITFIRE-port\port\unreal\Shitfire.uproject `
  -run=pythonscript -script="C:\Users\conno\prj\SHITFIRE-port\port\unreal\Tools\import_assets.py" `
  -unattended -nopause -nosplash
```

Raw archives live only in the cache, never the repo; the imported .uassets are
the committed build products (LFS). `import_assets.py` also regenerates
`M_Terrain`/`M_Road` as texture-sampling graphs that keep `gen_assets.py`'s
parameter names (run order: gen_assets.py first if you run both; the textured
versions are the shipped ones). Paid packs slot in the same way: add a manifest
entry (or drop files in the cache), extend import_assets.py's manifest lists,
re-run, allowlist, credit.
