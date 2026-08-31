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
