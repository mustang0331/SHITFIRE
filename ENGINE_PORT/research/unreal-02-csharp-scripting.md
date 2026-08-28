# Research 02 — C# and alternative scripting in UE5 for SHITFIRE.Core

Agent report, checks performed 2026-08-27. Synthesis and decisions: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §3a/§3b/§3d.
Question: can the "plain C# class library, engine-free, bit-parity-tested" plan survive a switch to Unreal, and what does each language route cost a solo dev.

---

## 1. UnrealSharp — largest raw workload reduction, with one existential risk

**Facts** [verified: https://github.com/UnrealSharp/UnrealSharp + GitHub API, 2026-08-27]:
- MIT. 1,912 stars, 207 forks, 48 contributors, 1,957 commits. Created Nov 2023. Last push **2026-08-22**.
- README: C# on top of **.NET 10**. Supports **UE 5.6–5.8**, requires .NET 10.0.5+ SDK (FAQ still says .NET 9 — docs lag).
- Binding: C# API auto-generated from all reflected C++ — C# sees what Blueprint sees; unreflected things need C++ exposure [verified: unrealsharp.com/faq.md].
- Hot reload: yes [verified: README]. Packaging: ships in packaged builds, "Currently only Windows/Mac" [verified: packaging.md] — fine for SHITFIRE.
- **No binary releases** — source-only; compile the plugin inside a C++-enabled UE project [verified: releases page + FAQ].
- Maturity, their words: **"The plugin is not production ready yet, but the plan is yes"** [verified: FAQ].
- Bus factor: top contributor has ~71% of commits [verified: contributors API]. Effectively one lead maintainer.
- Known issues: Newtonsoft.Json can break AssemblyLoadContext hot reload; glue generation **silently skips on engine upgrades** until `Intermediate/`/`Saved/` are deleted [verified: known-issues.md]. Note the silent-failure class.
- External class libraries: site advertises full .NET ecosystem/NuGet; setup generates a normal C# solution. An explicit "reference an external engine-free .csproj" doc page was not found — that SHITFIRE.Core sits as a plain ProjectReference and runs under `dotnet test` outside the editor is **[inferred]** from standard .NET tooling; verify in a 1-hour spike.

**Saves:** the existing port plan survives almost verbatim — Core stays a plain .NET library, `dotnet test` parity suite unchanged, UE shell in C# with hot reload.
**Costs:** building the plugin from source; tracking a fast-moving pre-1.0 project across UE upgrades; C++ still needed at edges.
**Failure mode:** the maintainer stops → dead binding layer; the shell (not the core) is hostage, and UE integration would need a C++ rewrite. Secondary: the silent skipped-glue-generation bug.
**Determinism:** good — runs on real CoreCLR, `double` is IEEE-754 binary64 [verified: MS docs]; UnrealSharp hosts .NET, it does not transpile.

**Verdict:** the only route where the current plan ports unchanged; credible and alive in 2026, but self-described not-production-ready with a ~1-person bus factor. Acceptable for the shell; a bad foundation for the core.

## 2. Plain C++ core + UE shell — moderate cost, zero dependency risk, best float control

- **The pattern is real and documented.** Epic officially documents linking externally built static libraries via a ThirdParty module in `.Build.cs` [verified: dev.epicgames.com integrating-third-party-libraries]. The "engine-agnostic core + GoogleTest executable + UE shell" pattern is written up end-to-end by Eric Lemes [verified: ericlemes.com 2018 series — UE4-era, version-agnostic]. Pure C++ sim core, CMake + Catch2/GoogleTest, headless, linked into UE: established practice.
- **Cost for ~1,900 lines:** JS→C++ only marginally harder than JS→C# for pure logic. Real overhead vs C# is toolchain ceremony (CMake, two build systems) and slower shell iteration — but the shell must be C++ regardless. The parity harness and the transcendental-math work dominate cost on **every** route; the language delta is days, not weeks.
- **The float-model trap (the silent killer):** UE compiles Development and Shipping with **/fp:fast**; only DebugGame uses /fp:precise [verified: forums.unrealengine.com thread 409626, corroborated by arxiv.org/pdf/2104.06262]. /fp:fast permits contraction and reassociation — sim code as a normal UE module passes parity in DebugGame and **silently diverges in Shipping**. Two verified escapes:
  1. Build the core outside UE (CMake, `/fp:precise`) and link the static lib — UE's flags never touch your translation units. Keep math out of inline headers.
  2. Or keep it a UE module and set the `FPSemantics` module property (`FPSemanticsMode.Precise`) — UE 5.8 docs, requires a private PCH [verified: module-properties doc].
  Since VS2022, `/fp:precise` generates **no FMA contractions by default** [verified: MSVC blog] — a `/fp:precise` static lib gives per-op IEEE-correct doubles matching JS.

**Verdict:** costs a real but bounded port; buys total float-model control and zero third-party survival risk. The plan's "engine-free core + engine shell" transfers to UE intact — in C++.

## 3. UnrealEngine-Angelscript (Hazelight) — great language, wrong infrastructure

- Engine modifications + plugin; **not** installable into stock UE — requires Epic source access and building the editor yourself [verified: angelscript.hazelight.se installation]. Repo 404s without an Epic-linked account [verified: API].
- License: plugin MIT, Angelscript zlib, engine changes under Epic's EULA [verified]. Pedigree: It Takes Two, Split Fiction (1.7M lines, 16k script files); but "Hazelight does not guarantee any maintenance or support" [verified: development-status].
- Which UE version current branch tracks: **[unknown — access-gated]**. Community vanilla ports (UNREANGEL: UE 5.4, 6 stars, early) not load-bearing [verified].
- Script `float` is 64-bit double [verified: cpp-differences via search excerpt].

**Failure modes:** (a) the headless-parity plan dies — no `dotnet test` equivalent; scripts execute in the engine's VM, so parity testing means editor/commandlets. (b) Silent: script math executes inside a VM compiled into a /fp:fast engine, math functions bind to engine/libm — determinism unverifiable without reverse-engineering [not checked: AS VM FP behavior under /fp:fast].
**Verdict:** proven at AAA scale, wrong shape for a solo dev whose core requirement is a headless bit-parity harness. The custom-engine-build tax (multi-hour compiles, manual merges every UE update) is the largest recurring cost on the list.

## 4. UnrealCLR — dead; UnrealSharp is the de facto successor

- Not formally archived but last push **2023-06-07**; README says .NET 6 / UE 4.25+; no activity since [verified: API + commits]. 3.3k stars are historical. No successor named in-repo [verified: checked]; in practice UnrealSharp is it.
**Verdict:** do not build on it.

## 5. Verse — not available; do not wait

- Verse remains exclusive to UEFN/Fortnite; no experimental path for standard UE 5.x [verified: ludusengine.com overview; 5.6/5.7 release materials list no Verse path — verified by absence, not exhaustively read].
- Epic's plan (State of Unreal, June 17 2026): Verse becomes the foundation of Scene Graph in **UE6**, early access targeted **end of 2027** [verified: same source + Wikipedia].
**Verdict:** irrelevant to a 2026 project. Revisit in 2028.

## 6. Bit-determinism across the viable routes

Three independent problems (applies to the Godot plan too):

**(a) Basic arithmetic (+, −, ×, ÷, sqrt) on doubles.** ECMAScript specifies these exactly per IEEE-754. Match bit-for-bit in:
- **C#/.NET 10:** `double` is IEEE binary64 [verified]. The spec's "higher precision evaluation" clause is an x87-era artifact; on x64/ARM64 RyuJIT emits scalar SSE/NEON at declared precision [inferred from x64 ABI; not checked in JIT source]. Whether RyuJIT auto-contracts to FMA: could **not** conclusively verify. Mitigation: the parity suite is itself the detector — run on the target runtime/hardware.
- **C++ under /fp:precise:** no contractions since VS2022 [verified]. Under UE's default /fp:fast the guarantee is gone — see §2 fixes.

**(b) Integer ops for mulberry32.** All 32-bit integer arithmetic plus one exact division by 4294967296. C#: `uint` wraps unchecked; `Math.imul` ≡ `unchecked((int)((uint)a * (uint)b))`. C++: `uint32_t` wraps by definition. Bit-identical streams achievable in both — the easy part.

**(c) Transcendentals (sin, cos, atan2, pow, exp, log).** ECMAScript Math.* is implementation-approximated; V8 implements them in its own fdlibm-derived code [verified: v8 ieee754.cc]. .NET's Math and MSVC's libm are different implementations; real cross-library divergence is documented [verified: zenn.dev libm-precision survey]. If bit-parity on transcendental paths were required, port one implementation (practically V8's, zlib-style-licensed) to both sides. `Math.sqrt` is safe. Identical work item for C#, C++, Angelscript; latent in the Godot plan too. **Audit the JS for `Math.` calls before choosing anything.** (Done post-report — see synthesis §3d: tolerances make this mostly moot; knife-edge branches are the residual risk.)

---

## Bottom line

**Best bet: plain C++ core, built and parity-tested outside the engine (CMake + Catch2/GoogleTest, `/fp:precise`), linked into UE as a ThirdParty static library — with UnrealSharp as the challenger worth a one-day spike.** For 1,900 well-specified lines the language delta is small; what differs is risk. The C++ route has no dependency that can die under you and is the only route where the float model is fully, verifiably yours (UE's /fp:fast default is the one silent parity-breaker on the board; the external-lib pattern sidesteps it completely). Angelscript's engine-fork tax and untestable-outside-engine scripts disqualify it; UnrealCLR is dead; Verse doesn't exist outside UEFN until ~2028.

Sources: UnrealSharp repo + unrealsharp.com (faq/packaging/known-issues) · UnrealCLR · angelscript.hazelight.se (installation/license/development-status) · UNREANGEL · Ludus Verse overview · Verse Wikipedia · UE fp-model forum 409626 · arxiv 2104.06262 · UE module-properties (FPSemantics) · MSVC /fp:contract blog · MS C# floating-point docs · v8 ieee754.cc · zenn.dev libm survey · Epic third-party-libraries doc · ericlemes.com unit-tests-in-unreal pt-3
