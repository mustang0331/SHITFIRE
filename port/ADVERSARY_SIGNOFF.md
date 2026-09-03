# ADVERSARY_SIGNOFF.md — P2 verification, adversarial pass

Role: VERIFIER-ADVERSARY. Mandate: break the P2 claim (Level 1/2/3 green,
33/33 transcript replays exact) or grant sign-off after genuinely trying.

- Worktree: `C:\Users\conno\prj\SHITFIRE-port`, branch `unreal-port`, HEAD `087956b`.
- Dates: 2026-08-27 (attacks 1–2, 6–8 started) / 2026-09-02 (probes, this document).
- All builds and runs are my own, from a clean configure into `port/build-adv`
  (RelWithDebInfo AND Debug). Nothing was taken from the fleet's reported numbers.
- Scope note: `port/unreal/` (P3 scaffold, parallel work) is outside this audit.

## Verdict

**P2 SIGN-OFF GRANTED** — see final line. Six findings below, none blocking;
three deserve a P3 work item.

## 1. Findings table

| # | Severity | Where | Evidence | Verdict |
|---|---|---|---|---|
| F1 | MEDIUM (P3 planning) | `port/core/CMakeLists.txt:20-22` | "Core compiles exceptions-off" (README, header comments) is a **source fact, not a build fact**: zero `throw`/`try`/`catch` under `core/` (grep, all hits are strings/comments), but the build adds no `/EHs-c-` or `_HAS_EXCEPTIONS=0` — CMake's MSVC default is `/EHsc`, so the static lib is built exceptions-ON. `std::regex` ctors (`fdc_say.cpp:20,64`; `parser_message.cpp` throughout) can throw in theory (fixed valid literals — unreachable in practice). When this lib links into exceptions-off UE modules in P3, the EH model of the prebuilt objects must be a deliberate decision, not a default. | Non-blocking for P2 (no parity/determinism effect; suite green in both configs). P3 item: pick the EH model, add a build gate. |
| F2 | LOW (latent) | `fdc_say.cpp:29-38`, `tlog.cpp:23-32`, `aar_analysis.cpp:24-31` | `jsNum()` formats doubles via `snprintf` — integral values take a locale-immune `%lld` path; non-integral fall to `%g`/`%.17g`, which (a) is LC_NUMERIC locale-sensitive (a host `setlocale` would print `0,5`) and (b) is not JS shortest-roundtrip number-to-string. All 33 transcripts pass exact text compare, so no recorded line reaches the divergent path. | Non-blocking. P3 item: pin C locale at bridge init or replace fallback with a JS-exact formatter. |
| F3 | LOW | `parser_message.cpp` (regex-based parser), `fdc_say.cpp:20,64` | `std::regex` captures the global C++ locale at first construction (function-local statics). ASCII-only semantics are claimed in comments and hold for all radio traffic; a UE `std::locale::global` call before first parse could shift `icase`/class behavior for non-ASCII only. | Non-blocking. Same P3 locale-pin item as F2. |
| F4 | INFO (comparator scope) | `port/core-tests/level3/replay_test.cpp` | Comparator diffs parse/impact/fdc/aar only — per spec (DETERMINISM_NOTE.md §a). `spot`/`obs`/`sys`/`caller` kinds (843 entries across the 33 fixtures) are never compared, yet spot-report content is seeded-deterministic (stream #8) and could be compared for free. | Matches the documented scope; free tightening opportunity for P3. |
| F5 | INFO (shared blind spot) | `replay_test.cpp:579-593` | Trailing drain stops 240 s past the last recorded entry; an event the port wrongly scheduled beyond that horizon is invisible. The oracle capture had the same horizon (waited lastT+3.5+2 s), so parity comparison is apples-to-apples; a phantom far-future event cannot be caught by ANY fixture-based comparator. | Accepted limitation, not a weakness of this comparator relative to its oracle. |
| F6 | INFO (cosmetic) | `replay_test.cpp:570,577` (unused `k`), `:583-590` (`parts()` fetched twice per guard iteration) | Dead code in the test harness. | No effect. |

## 2. Grep gates over `port/core/` (attack 1) — ALL CLEAN

Every hit read, not counted. Patterns run against `port/core/` (include + src):

| Gate | Result |
|---|---|
| `float` (incl. literals `\d+[fF]`) | **0 code hits** (word-boundary grep and float-literal grep both empty) |
| `std::round` | 0 code hits (2 comment mentions of the ban itself, e.g. `jsemantics.h:20`) |
| `std::sort` / `qsort` | 0; the only sorts are `std::stable_sort` (`world_features.cpp:567,604`) |
| `unordered_map` / `unordered_set` / `std::map` / `std::set` | **0 anywhere in core** — no unordered iteration can exist |
| `rand()` / `random_device` / `mt19937` | 0; the only PRNG is `Mulberry32` (`math/prng.h`), flavor seam via injected `IFlavorRng` (`fdc_quips.h:47`) |
| wall clock (`system_clock`, `time(`, `GetTickCount`, `performance` counters) | 0; the only clock is `SimClock.now`, written solely by the harness (`mission_clock.h:20`) |
| `throw` / `try` / `catch` | 0 code hits; all 18 grep hits are quip strings or comments (read individually). See F1 for the build-flag caveat |
| file I/O (`fstream`, `fopen`, `FILE*`) | 0; `<cstdio>` appears in 3 files for `snprintf` number formatting only (see F2) |
| engine/platform headers (`windows.h`, UE types) | 0; nlohmann appears nowhere under `core/` (grep) |
| mutable function-local/global `static` state | 0 (only a `static` member *function* declaration, `fdc_rules.h:214`) — corroborated by probe 3a/3b below |

## 3. Inline-header math audit (attack 2) — CLEAN

All 34 headers under `port/core/include/` (plus the one src-side header,
`src/core/scenario_view_adapter.h`) swept with three patterns
(single-line bodies, arithmetic-in-return, arithmetic assignments, transcendental
calls) plus full reads of `prng.h`, `jsemantics.h`, `mission_clock.h`,
`mission_machine.h`, `scenario_state.h`, `world_config.h`, `sim_core.h`,
`fdc_quips.h`. Everything found inline is: trivial getters/setters returning a
member, member-set one-liners, a seed-forwarding constructor
(`mission_machine.h:160`), and integer `constexpr` count constants
(`world_config.h:73-82`). **No floating-point computation exists in any header**,
so UE's `/fp:fast` in P3 has nothing to recompile. The load-bearing flag is
present: `/fp:precise` at `core/CMakeLists.txt:21`.

## 4. Suite re-runs from clean configure (attack 8) — GREEN, BOTH CONFIGS

Own configure: `cmake -S port -B port/build-adv -G "Visual Studio 17 2022" -A x64`
(fixtures via `-DSHITFIRE_FIXTURES`), then:

- **RelWithDebInfo**: 15/15 ctest pass (`-L level1|level2|level3`), total 27.5 s,
  level3.replay 24.2 s.
- **Debug**: 15/15 pass, level3.replay 388.7 s (iterator-debug active — the
  config the README says catches `/fp:` and iterator surprises).
- Direct `[replay]` run for the per-transcript table: **33/33 PASS,
  worst dTgt 0.000 m on every transcript, worst time skew 0.100 s** (= exactly
  one TLOG rounding quantum; tolerance is 0.5 s). Attempts: 28 transcripts pass
  on schedule #1, four (missions 07, 22, 25, 27) on attempt 2,
  `mission_28_sk-defend-the-op` on attempt 132.

## 5. Probes (attacks 3–5 + comparator hazards) — ALL CLEAN

Temporary probe `port/core-tests/level3/zz_adv_probe.cpp` (tag `[advprobe]`,
picked up by the test glob, never registered with ctest; **deleted after this
report** — see git status at the end). All 6 probe cases passed,
119 assertions. RelWithDebInfo, own build.

**3a — time-slicing + repeat determinism.** For mission_13 (convoy),
mission_25 (kaiju moving target), mission_31 (two missions): full replay driven
by (A) `advanceTo` jumps, (B) a second in-process SimCore instance, (C) a
per-frame `advance()` loop. TLOG byte-identical and all PRNG snapshots equal in
all three, for all three transcripts (entries 52/66/83). Note: `advanceTo` is
*by construction* a frame loop (`sim_core.cpp:738-739` — `while (ms < target)
advanceOneFrame()`), so no stepping pattern can skip or split a frame; the probe
demonstrates it and (B) rules out hidden static state.

**3b — interleaved instances.** mission_13 and mission_27 advanced alternately
frame-by-frame in one process: each TLOG equals its solo run byte-for-byte
(9,881 / 11,685 bytes). No cross-instance state.

**5 — PRNG draw-count instrumentation.** Mulberry32 advances its state by the
odd constant 0x6D2B79F5 per draw, so state uniquely encodes draw count;
the probe reads the 4-byte state (memcpy; `static_assert(sizeof==4)`) after
every `submitMessage` plus at end, and converts to exact counts via the modular
inverse. Two full replays each:

| transcript | scenario-stream draws | mission-stream draws | flavor next()/coin() | runs equal |
|---|---|---|---|---|
| mission_13 convoy | 2,656 | 32 | 7 / 3 | yes — every snapshot |
| mission_14 danger-close | 6 | 36 | 10 / 3 | yes — every snapshot |
| mission_27 counterbattery | 85 | 36 | 10 / 3 | yes — every snapshot |

Snapshot-for-snapshot equality at every send point is stronger than a final
count: a transient extra-draw-then-compensate bug would show here.

**4a — off-fixture scenario seeds.** 20 arbitrary (type, seed, terrain) pairs —
all 15 scenario types, seeds 9001..11604 (step 137), terrain 1337 plus 4242 on
every 4th pair — each generated + advanced 120 s, twice: TLOG + final
scenario-rng state identical on all 20 (output sizes 3,169–4,232 bytes).

**4b — off-fixture synthetic mission.** Plain grid CFF (grid computed from the
generated target via `gridOf`) + direction + two corrections + FFE + EOM on
seeds {77001, 4321, 5, 250000, 31337}, twice each: TLOGs identical all 5;
impacts 9/9/5/2/2 (every seed actually fired).

**7x — comparator text-collision hazard.** `fdcSignature` classifies FDC lines
by exact text membership in quip pools; a text collision between a
never-compared (excluded) pool and a compared pool would silently drop a
doctrinal line from BOTH sides. Probed all 8 excluded pools against all 30
compared pools plus the 3 template pools (prefix+suffix match, callsign-
normalized): **0 collisions, 0 cross-pool duplicate lines.**

## 6. Comparator judgment (attack 7) — SOUND

Read in full (`replay_test.cpp`). It asserts exactly what the report claims:
parse 4-tuple (ptype/method/warno/ffe) per message in order with count
equality; impacts in order with **exact** recorded `dTgt`, msg, shell tag,
±0.5 s; FDC doctrinal signature sequence (pool identity for pool lines, exact
normalized text for everything else) with count equality both directions;
AAR payload exact across all 20 listed fields (JS null vs absent handled
symmetrically; both AARs compared for mission_31 — generically, not special-
cased). **No transcript-specific special cases exist** (verified: no filename
conditionals; the single mission_31 mention is a comment).

- **Vacuous-pass check:** counts are compared in every section, so empty-vs-
  nonempty fails. Fixture scan of all 33 transcripts: every one has nonzero
  compared content (mission_01/02 legitimately have 0 impacts/AAR as tutorials
  — and count equality then *forces* the port to produce zero of each; total
  compared entries: 227 parse, 293 impact, 821 fdc, 32 aar). No try/catch
  swallowing; no tolerance wider than the stated ±0.5 s (observed worst 0.1 s).
- **The send-time search cannot pass a wrong sim.** Candidates per send are
  only the 16 ms frame times whose `Math.round(t*10)/10` stamp equals the
  recorded value — schedules the recording itself cannot distinguish; the true
  schedule is by construction in the set. Bound: ≤7 start candidates × 64
  bounded-backtrack attempts. A passing schedule must reproduce every impact
  distance to the metre (chained Box-Muller draws), the full doctrinal
  sequence, and ~20 exact AAR fields — the search recovers frame alignment the
  0.1 s stamps lost, nothing more. Empirically: worst dTgt is 0.000 on all 33
  including mission_28 (the 132-attempt case), and 28/33 pass on the very first
  schedule.
- Scope limits recorded as F4/F5 above.

## 7. License / contamination sweep (attack 6) — CLEAN

- `vendor/nlohmann/json.hpp`: SPDX MIT header present (lines 6-7); version
  macros 3.12.0 match CREDITS.md.
- `vendor/catch2/catch_amalgamated.{hpp,cpp}`: BSL-1.0 headers present in both;
  version macros 3.16.0 match CREDITS.md.
- CREDITS.md table matches exactly what is vendored; both libs are test-only
  (nlohmann appears nowhere under `core/` — grep).
- GPL/LGPL/AGPL text: **zero** hits across `core/`, `core-tests/`, `vendor/`,
  CMake files, README, CREDITS.
- Pasted-code smells (URLs, `Copyright`, `(c) yyyy`, `Licensed under`,
  Stack Overflow refs) outside `vendor/`: **zero** hits.

## 8. Sign-off conditions

- Grep gates: clean (every hit read and justified above). ✔
- Probes 3–5: clean, numbers above. ✔
- Suite green in my own clean-configure runs, RelWithDebInfo and Debug. ✔
- Comparator judged sound (collision probe + search-bound argument + vacuous-
  pass scan). ✔

**P2 SIGN-OFF GRANTED — attacks exhausted without a break.**
