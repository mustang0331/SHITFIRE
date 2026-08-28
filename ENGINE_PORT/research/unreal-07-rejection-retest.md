# Research 07 — Adversarial re-test of ENGINE_CHOICE.md's Unreal rejection (2026 evidence)

Agent report, checks performed 2026-08-27. Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §1–§2.
Method: Epic's EULA fetched live 2026-08-27 from unrealengine.com/en-US/eula/unreal (via curl; Cloudflare blocks normal fetch); EULA quotes verbatim. Engine state: UE 5.7 released Nov 12, 2025; UE 5.8 released June 2026 [verified: news pages].

## Claim 1 — "UMG is heavy and fights precise 2D vector work" / "painful"

**VERDICT: PARTLY CONFIRMED.** No runtime vector-graphics API; `USlateVectorArtData` converts static-mesh data as a workaround [verified: API doc]. Strongest evidence: a UI engineer integrated Skia into UE to escape the built-in path (2023 series) [verified: dannymcgee.dev] — people don't bolt on a second renderer when the built-in one is adequate. Nothing in 5.5–5.8 changed it [verified: release coverage]. "Heavy" (perf) not measured here — research 05 owns that half (and finds the immediate-mode Canvas path fits SHITFIRE's map code well; see its verdict).

## Claim 2 — "Headless/unit testing of the sim core: Poor"

**VERDICT: REFUTED as of 2026**, two independent grounds:
- **Low-Level Tests**: current, documented for 5.8, Catch2-based, build to standalone executables in `Binaries/<PLATFORM>` run directly or via BuildGraph — no editor [verified: two Epic docs]. (Introduction version not re-verified.)
- Automation Framework runs headless in CI: `UnrealEditor-Cmd … -ExecCmds="Automation RunTests …" -Unattended -NullRHI -NoSplash` is the standard GPU-less container pattern [verified: unrealcontainers.com].
- **The pattern that voids the objection**: sim core as a plain C++ static lib outside UE, linked via `ModuleType.External` — first-class, documented [verified: Epic third-party-libraries doc]. The objection only ever applied to code living inside the engine. Caveat: testing UE-side glue still means the heavier machinery; that isn't the sim core.

**Correct this row in ENGINE_CHOICE.md.**

## Claim 3 — Determinism: "Strong but the engine is opinionated"

**VERDICT: PARTLY CONFIRMED** — opinionation real, every pitfall has an off-switch:
- **UE defaults to `/fp:fast` on MSVC** — engine `VCToolChain.cs` adds it [verified: quoted in forum thread 409626; quote is 2017-era — current VCToolChain.cs is access-gated, but non-precise default corroborated]. `/fp:fast` permits reassociation/FMA — breaks bit-parity with strict-IEEE JS [inferred from MSVC semantics].
- Per-module override exists: `ModuleRules.FPSemantics` (5.8 docs), plus experimental `DeterministicWarningLevel`; `#pragma float_control` is the version-proof alternative [verified].
- **Strongest guarantee**: external static lib with own `/fp:precise` flags — UE's build can't alter prebuilt machine code [inferred from linking].
- Ticking: variable by default; `bUseFixedFrameRate` exists; irrelevant if the sim advances on injected deltas [verified/inferred]. Chaos physics: moot, none used.
- Residual named risk: FTZ/DAZ CPU flags — no evidence UE sets them on the game thread [unknown, not fully checked]; practically irrelevant (sim values never approach subnormals) [inferred]; golden-value CI catches the class.

## Claim 4 — "C++/Blueprint — highest friction" for a solo dev

**VERDICT: CONFIRMED**, with 2026 softeners. No production C# (UnrealSharp FAQ: "not production ready," source-build, C++-familiar engineer recommended) [verified]. Softeners: Rider free for non-commercial since Oct 2024 [verified: JetBrains blog]; Live Coding exists (2026 reliability not deep-checked); LLMs know UE C++; headless agent tooling exists. Dev environment ~150 GB [verified: filfreire]. For 1,900 lines of logic, highest-friction option on the table.

## Claim 5 — Licensing: "Royalty model above threshold"

**VERDICT: REFUTED as a blocker — on Epic's own current EULA text.** The stated reason (royalty) was always irrelevant for a free product; the real 2026 question (per-seat for non-game use) also resolves to $0 for this developer. Verbatim quotes:

- Preamble blesses the use case: Unreal can be used for *"simulation for training purposes."*
- §3: royalties for Royalty Products; *"(b) For any other use … you will need to purchase seat subscriptions … unless you qualify for an exception."*
- §4(a)(ii): *"You will not be obligated to pay us any royalty payments for Distributing Products to any person or entity if the Products do not directly generate revenue."* (Threshold otherwise $1M gross, 5%; 3.5% via Launch Everywhere addendum.)
- §4(a): royalty-free products *"may still be subject to seat subscription requirements"* — **the clause that could have bitten** — but §6(b)(i) personal/indie exception: *"During any period that you, together with any entities in your corporate group, have generated less than $1,000,000 USD in gross revenue over the last 12 months, the Seat subscription requirement will not apply to your Users."* Solo dev, no revenue: **zero fees, hard stop.** (§6(b)(iii) additionally exempts purely non-commercial use.) The $1,850/seat/yr subscription [verified: Engadget] applies only above the line.
- **Military/government distribution expressly contemplated**: §18(b): licensed to all end users *"including any U.S. Government end users"* as a Commercial Item. "Military" appears **zero times** in the EULA [verified: grep of fetched text]. End users need no Epic license (§4(c)).
- Real obligations: product ships under an EULA disclaiming Epic warranties (§4(c), trivial); **no GPL/CC-BY-SA code combined in** (§6(c)); §6(b)(i) counts *"any advances received or other funds raised"* — **a funded government contract counts toward the $1M test**; if the military wanted to modify/rebuild the trainer itself, that's a Epic custom-license conversation [unverified, irrelevant to distributing a binary].

**Correct this row in ENGINE_CHOICE.md.**

## Claim 6 — "Very large" distribution size

**VERDICT: PARTLY CONFIRMED.** Default empty UE5 Shipping ≈ 300 MB; plugin-stripping saved only ~5% [verified: filfreire]; truly minimal 5.7 builds reported under 80 MB [snippet-verified, direct fetch 403]; Nano minimal template 113–127 MB [verified]. Counter-fact the doc missed: **Godot 4 + C# is ~175 MB unzipped** (bundled .NET) [verified: Godot forum/issue] — real gap 2–3×, not 6–10×. Immaterial for USB/share distribution; the actual locked-down-laptop question is runtime prerequisites (MSVC redist vs .NET runtime) on no-admin machines — same problem class both engines [inferred].

## Claim 7 — "Team ramp for a solo dev: High"

**VERDICT: CONFIRMED**, softeners noted (Rider free, huge community, UMG/Slate Compendium, LLM familiarity). Realistic ramp: months to comfortable vs days-to-weeks for Godot C# [assumed calibration; wrong only by weeks either way].

## Claim 8 — What changed in 5.5–5.8 that helps THIS product

**VERDICT: Nothing material.** Headline work (Nanite Foliage, PCG, Mesh Terrain, MetaHuman Crowds, MegaLights) targets large-3D production [verified: release notes/coverage]. The two quiet relevant improvements: mature Low-Level Tests (claim 2) and FPSemantics (claim 3).

---

## Bottom line

**No — the 2026 evidence does not overturn Godot 4 + C# for this product. But two of the six documented reasons are wrong and should be corrected in ENGINE_CHOICE.md so the record doesn't rot.** Deciding facts: (1) port cost still favors Godot decisively — no production C# in UE; 1,900 lines transliterate into Godot C# vs a UE C++ rewrite plus months of ramp, for a product using none of what UE invests in; (2) dense precise 2D remains Slate's weak spot for retained-mode vector work (with research 05's canvas-path mitigation noted); (3) licensing was never a valid reason — free product owes nothing, seats waived under $1M/12mo (watch the "funds raised" clause if government funding appears), US Government end users expressly licensed; and "headless testing: Poor" is stale — Catch2 standalone tests are first-class and the outside-the-engine core pattern voids it under either engine.

Sources: UE EULA (live fetch) · Engadget per-seat pricing · Low-Level Tests docs ×2 · third-party libraries doc · module-properties (FPSemantics) · fp-model forum 409626 · unrealcontainers CI · dannymcgee Skia series · filfreire UE file size · Nano template · Godot C# export-size forum/issue · UnrealSharp FAQ · JetBrains Rider announcement · UE 5.7/5.8 release pages · CG Channel 5.8 · display-rate doc · mcp-unreal · UMG-Slate Compendium
