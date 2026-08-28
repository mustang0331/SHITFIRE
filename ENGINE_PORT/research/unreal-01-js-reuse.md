# Research 01 — Reusing the JavaScript sim core inside Unreal Engine

Agent report, checks performed 2026-08-27. Synthesis and decisions: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §3c.
Tags: [verified: URL] = fetched; [inferred]; [not checked] stated plainly.

---

## The determinism finding that shapes every option (read first)

The bit-parity requirement has one landmine that applies to **all** routes, including the browser build already shipping:

- ECMAScript explicitly does **not** pin down the transcendental Math functions. The spec lists `acos, acosh, asin, asinh, atan, atanh, atan2, cbrt, cos, cosh, exp, expm1, hypot, log, log1p, log2, log10, pow, sin, sinh, tan, tanh` as "implementation-approximated" [verified: tc39.es/ecma262 numbers-and-dates section, via search-result quote of the spec text]. MDN states it bluntly: "different browsers can give a different result. Even the same JavaScript engine on a different OS or architecture can give different results!" [verified: developer.mozilla.org/…/Global_Objects/Math].
- `Math.sqrt` was moved out of that list — it is now required to be correctly rounded (spec change approved July 2024) [verified: search results citing the tc39 change]. Basic `+ - * /` and bitwise ops are IEEE-754-exact per the Number type definition [inferred from spec structure].
- Consequence: **mulberry32 is safe everywhere** — bitwise ops, `Math.imul`, exact arithmetic only. But ballistics/azimuth math touching `Math.log/cos/sin/exp/pow` is **not guaranteed** across engines — and not even across V8 versions: current V8 main delegates `sin/cos/log` to LLVM-libc's correctly-rounded implementations, where older V8 used a straight fdlibm port [verified: github.com/v8/v8/blob/main/src/base/ieee754.cc]. When V8 switched is [not checked]; if the switch postdates V8 11.8 (newest puerts ships), puerts-V8 output diverges from current-Chrome output on transcendental calls — silently, in the low bits.
- **Cheap de-risk for every route:** audit the core for transcendental Math calls and vendor pure-JS replacements (bit-identical on every conforming engine), or restructure to avoid them. This also fixes a latent property of the current product — a Chrome update could change the browser build's outputs today.

**Post-report note (local verification, see synthesis §3d):** the audit was run — 137 transcendental calls across the sim path, including Box-Muller at `10-mission.js:29`. But PARITY_TESTING.md requires bit-exactness only for the PRNG; everything else is tolerance-based (1e-9 / 0.01 m), which last-ULP libm differences pass easily. The real residual risk is knife-edge branch flips changing PRNG draw order; Level 2 tests catch those per seed.

---

## Ranked options

### 1. puerts (Tencent) — V8 backend

**URL:** https://github.com/Tencent/puerts

- License: BSD 3-Clause [verified: repo]. Stars 6.2k, 299 open issues [verified].
- Maintenance: latest commits **Aug 25, 2026** [verified: commits/master]; latest Unreal release `Unreal_v1.0.9` published **2025-07-15**, release note "ue 5.6.0 compatible" [verified: GitHub API release object]. `Unreal_v1.0.8` (Mar 26, 2025) covers UE 5.5 [verified]. README claims "unreal engine 4.22 ~ latest" [verified]. UE 5.4–5.6 covered.
- Backends: V8 (default; selectable 8.4.371.19 / 9.4.146.24 / 11.8.172 — 10.6 removed in 1.0.9 for platform instability), QuickJS (strict package-size, no debugging), Node.js (largest) — via `UseV8Version` / `UseQuickJs` / `UseNodeJs` [verified: doc/unreal/en/install.md + release notes].
- Pure-logic JS from C++: yes — `auto JsEnv = MakeShared<puerts::FJsEnv>(); JsEnv->Start("Entry.js");` with C++ objects passed via `TPair<FString, UObject*>`; multiple isolated VMs; no DOM assumed [verified: puerts.github.io getting_started]. C++→JS call mechanism beyond Start/args [not checked in detail]; practical pattern: thin JS entry registering callbacks on a passed-in UObject delegate.
- Packaging: project must convert to a C++ project [verified: install.md]; V8 adds meaningful binary size, QuickJS minimal, Node largest [verified]. Cook/pak handling of shipped `.js`: [not checked].
- Performance: no benchmarks run/found [not checked]. For 1,900 lines at call-for-fire cadence, any backend exceeds requirements by orders of magnitude [inferred]; only interop-crossing frequency matters, and it is low here.
- Determinism: no puerts-specific number-math issues found; 299 open issues not exhaustively searched [explicitly not checked]. The V8-version transcendental risk above is the real one.

**Saves:** the entire sim-core rewrite; the parity harness gets easy — the same JS file runs in Node and UE, diff the outputs.
**Costs:** heavyweight plugin built for full TS gameplay scripting when 5% is needed; Chinese-first docs; C++ project conversion; V8 binary size; engine-upgrade lag (5.6 compat landed ~6 weeks after 5.6).
**Failure mode:** silent low-bit divergence between shipped V8 11.8 and the browser's current engine — mitigated by the Math audit + a vector-diff CI gate. Second: a future UE version breaking puerts before Tencent catches up.
**Verdict:** strongest maintained option; use with the V8 backend and a parity gate, not on faith.

### 2. Embed QuickJS-ng directly in a small UE C++ module

**URL:** https://github.com/quickjs-ng/quickjs

- MIT, active fork of Bellard's QuickJS, maintainers include bnoordhuis and saghul, 3.7k stars, ~1,200 commits [verified]. Latest release date: [not checked].
- No dedicated maintained QuickJS-UE plugin exists — searched; nothing beyond generic embedding gists [verified: search returned none]. puerts's QuickJS backend is the closest thing.
- Effort: QuickJS is deliberately embeddable ("a few C files and no external dependency"). A UE module exposing eval/call/JSON-out is days for someone comfortable in C++, not weeks [inferred — estimate].

**Saves:** the rewrite, with total control, no third-party plugin to track across UE upgrades — the sim core needs no UE bindings, just strings/JSON in and out.
**Costs:** you write and own the glue (module build rules, GC lifetime, error surfacing); no debugger story like V8's.
**Failure mode:** QuickJS's transcendental Math goes through the platform's C libm (Bellard's new mquickjs ships its own `libm.c` precisely because of this [verified: github.com/bellard/mquickjs README via search]; original QuickJS's use of system libm [inferred — source not read]). Without the Math audit, transcendental bit-parity essentially guaranteed to fail silently. With the tolerance-based standard (see post-report note), acceptable.
**Verdict:** best effort-to-control ratio if the core passes/is made to pass the exact-ops audit; otherwise ranked below puerts-V8.

### 3. Unreal.js — dead upstream, one live community fork

**Upstream:** https://github.com/ncsoft/Unreal.js — last push **July 20, 2023**; last meaningful commit Mar 28, 2023 ("Support UE 5.1"); README supports 4.11–5.1; 159 open issues [verified]. **Effectively dead for UE 5.4+.**
**Fork:** https://github.com/getnamo/UnrealJs — v2.0.0 "targets UE 5.7 and embeds V8 14.6.202", last push June 22, 2026, 74 stars, Windows-validated, README warns "Not all upstream functionality is guaranteed" [verified].

**Saves:** same as puerts in principle; V8 14.6 is much closer to current Chrome, which helps transcendental parity [inferred].
**Costs:** bus factor of one (getnamo), 74 stars, Windows-only validation, UE 5.7 target.
**Verdict:** viable fallback, not the primary bet.

### 4. The embedded-browser (CEF) route

- UE's built-in Web Browser Widget shipped Chromium/CEF **90** for years; **UE 5.6 introduced CEF 128** fully enabled only on Linux, Windows experimental/buggy/partially reverted; Epic engineer confirmed active modernization; a user reports **UE 5.7 preview ships CEF 128 properly** [verified: forums.unrealengine.com thread 2617726]. Not deprecated, slowly maintained.
- WebGL/Three.js inside it: community KB documents Three.js scenes rendering in the CEF widget, WebGL2 working [verified as a claim on jdot274.github.io Prism-KB — secondary source, untested]. CEF 128 is 2024-era Chromium [inferred].
- **BLUI** (getnamo): https://github.com/getnamo/BLUI-Unreal — MIT, v5.0.0 Dec 5 2025 "for UE 5.7", CEF 128.4.12, 424 stars, local files via `local://` [verified].
- **WebUI** — by **Tracer Interactive** (not getnamo). Fab listing 403'd; supported UE 5.x versions [unverified].
- **UCefView** (https://cefview.github.io/UCefView/): claims UE 5.0–5.8, v1.0.27, JS bridge; CEF version/license/price not stated [verified page; not checked further].

**Honest assessment:** running the whole HTML game inside a UE shell is technically viable (BLUI/CEF 128) but is **not a port** — UE contributes a window, an input path, and ~150 MB of unused engine. Versus Tauri (Apache-2.0, pushed Aug 27 2026, 110.6k stars [verified]) or Electron (MIT, pushed Aug 27 2026, 122.8k stars [verified]), the UE shell buys exactly one thing: a strangler-fig migration path where UE gradually takes over rendering while the sim stays in Chromium JS (the best transcendental-parity story of any option). Strategic failure mode: the "temporary" CEF layer never gets removed.
**Verdict:** wrong tool for the port; defensible only as (a) HTML FDC panels inside the real UE game, or (b) a deliberate strangler-fig migration you commit to finishing.

### 5. Transpile/compile routes — not credible

- **Javy** (bytecodealliance): Apache-2.0, active, 2.7k stars; compiles JS to Wasm by bundling a JS runtime (QuickJS [assumed]) [verified: repo]. Requires a Wasm runtime in UE — two layers of embedding, same libm question, zero benefit over embedding QuickJS directly. **Not credible here.**
- **Porffor** (CanadaHonk): MIT, very active, AOT-compiles JS to C/Wasm [verified]. Pre-1.0 research-grade; a doctrinal parser through an experimental compiler is the definition of silent-miscompile risk. **Not yet.**
- JS→C++/C# transpilers: nothing maintained surfaced [not exhaustively searched].

---

## Bottom line

**Best single bet in this avenue: puerts with the V8 backend** — BSD-3, commits Aug 2026, releases covering UE 5.5/5.6, documented two-line C++ path to run DOM-free logic. Eliminates the 1,900-line rewrite; the identical JS runs in Node, browser, and UE. The decision that actually protects bit-parity is engine-independent: audit the sim for implementation-approximated Math, vendor exact-ops replacements as needed, and put a seeded-scenario output-diff gate in CI before any UE code. QuickJS-direct is the fully-owned fallback.

Sources: Tencent/puerts + releases + install.md + puerts.github.io · ncsoft/Unreal.js · getnamo/UnrealJs · getnamo/NodeJs-Unreal · quickjs-ng · bellard/mquickjs · UE forum CEF thread 2617726 · getnamo/BLUI-Unreal · UCefView · Prism-KB three-js-in-ue · MDN Math · v8/src/base/ieee754.cc · ECMA-262 numbers-and-dates · bytecodealliance/javy · CanadaHonk/porffor · tauri-apps/tauri · electron/electron
