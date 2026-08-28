# Research 03 — Open-source / free-adoption survey (projects, prior art, assets)

Agent report, checks performed 2026-08-27. Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §7.

License scope note: "Fab Standard License" = commercial use allowed, any engine, cannot redistribute the raw asset [verified: fab.com/eula via Epic forum summary]. Epic's own samples carry a "UE-Only Content" restriction — usable for a UE port, can never flow to the browser build or any non-UE build.

## 1. Open-source UE5 games/projects worth stripping for parts

- **Lyra Starter Game (Epic)** — UE-only license [verified: Fab listing + UE EULA]. Multiplayer arena shooter, heavy abstraction; for a single-player observation game the built-in **First Person template** is likely the better skeleton, Lyra a parts bin (ADS/zoom, HUD patterns). Workload saved: low-to-moderate; learning tax real.
- **OnAllFronts / ProjectM (HaywireInteractive)** — UE5 milsim FPS/RTS on Mass Entity. **MIT** [verified: LICENSE fetched]. UE 5.0.3; development moved closed-source, repo frozen as a learning resource [verified]. **Adoptable**: reference C++ for soldier AI, projectiles, milsim structure. Old engine version — days of reference value, not a base.
- **OpenTournament** — license **NOASSERTION**; README: "do not share or redistribute any content without prior approval" [verified: repo + API, pushed 2026-07-14]. **Do-not-use.**
- **ArtilleryEco (OversizedSunCoreDev)** — despite the name, a determinism/abilities/input framework, not artillery. MIT, active (pushed 2026-07-31) [verified]. Off-target; interesting only for deterministic replays later.
- **Cesium for Unreal** — Apache-2.0, very active [verified]. Overkill for a fictional 10 km low-poly island (see research 04); keep on file for real-world training areas.
- **Voxel Plugin free legacy** — **no license file** (API: null) despite public repo [verified]. **Avoid.**
- Radio/comms: **no open-source UE radio-net project found.** Nearest real implementation: ACRE2 (Arma), **GPL-3.0**, active [verified]. **Viral — design-reading only.** TFAR license: not checked.

## 2. Prior art: call-for-fire / forward-observer trainers

- **FOPCSim / FOPCSim 2 (Naval Postgraduate School, 2003/2005)** — the closest open ancestor: free laptop CFF trainer with DTED terrain and real-time grading vs USMC T&R standards. Thesis documents are **US Government works — public domain** [verified: archive.org metadata, both theses]. Software binaries/source: no live download found; engine not checked (PDF wouldn't render). **Adopt the design, not the code** — the cognitive task analysis and grading rubric are directly reusable, free doctrinal validation.
- Government/commercial trainers (all closed): Army call-for-fire simulators [verified: dvidshub], Quantum3D AFOS, Applied Virtual Simulation A-JFOT. Value: feature checklists only.
- **Arma 3 ecosystem** — Eden modules + community FO systems (MCC Forward Observer Artillery, [EKS] AI Artillery) actively used for CFF training nights [verified: Steam/wiki links]. Mod licenses share-alike/non-portable; ACE/ACRE GPL — **assets/code do-not-use**. Genuinely stealable: community SOP documentation, esp. UnitedTaskForce's written CFF/adjust-fire procedures [verified: unitedtaskforce.net] — excellent free script for radio dialogue flow.
- **GitHub "call for fire" landscape** [verified: search API]: essentially empty apart from mortar calculators (ArmaReforgerMortarCalculator — license "other"; FoxholeFDC — C#; ArmaBallisticComputer — GPL-3.0). Nothing adoptable; **SHITFIRE has no open competitor — the search's top hit for the exact phrase was mustang0331/SHITFIRE itself.**

## 3. Ballistics / artillery plugins for UE

(SHITFIRE's dispersion model ports bit-identically; these are for VFX/feel only.)
- **ECSProjectiles (EmpiresCommunity)** — MIT, UE4-era, experimental; Niagara-rendered mass projectiles (claims 40k) [verified: README]. Adoptable pattern for shells/tracers; expect UE5 porting work. Small saving.
- **UE5-EnhancedProjectile (HanYe0817)** — MIT, UE5, StateTree-driven with pooling + editor simulator, pushed 2026-06-01, 2 stars [verified]. Young/unproven but current-engine; best UE5 starting point for the visual shell-flight layer. A few days saved.
- **ballistics-engine (ajokela)** — MIT, Rust, engine-agnostic [verified]. Not needed; noted for completeness.
- Fab "Advanced Ballistics" etc. are paid — not checked.

## 4. Free/CC0 asset avenues

**Models** (the thin spot — no dedicated modern-military low-poly kit at Kenney or Quaternius):
- **Kenney** — packs checked are CC0 [verified: blocky-characters, tower-defense-kit, particle-pack pages]. Best fits: **Blocky Characters** (rigged low-poly people — re-tint to OD green = troops), Car Kit, Nature Kits. No soldiers/trucks/bunkers pack as such [verified: 3D category listing].
- **Quaternius** — CC0, author-stated incl. commercial + modification [verified: X post]. **Animated Tanks Pack** (tagged War) directly relevant; big nature packs for the island. Per-pack formats not checked (community says FBX/GLTF — unverified).
- **Poly Pizza** — search engine over ~10k+ CC0 and CC-BY low-poly models (FBX/GLTF; aggregates Quaternius/Kenney/ex-Google-Poly) [verified via page snippets; site blocks direct fetch]. Search "soldier", "bunker", "military truck" here first. **Check the per-model license badge; CC-BY needs a credits line.**
- **itch.io** — free/CC0 low-poly military vehicle packs exist (e.g. RGS_Dev Free Low Poly Vehicles, CC0 [verified: snippet]); licenses vary per pack, check each.
- **Fab Limited-Time Free** — new third-party packs go free every two weeks; claimed = kept permanently under Fab Standard [verified: fab.com/limited-time-free + forum]. Zero-cost habit: claim everything military/VFX each cycle.
- **Quixel Megascans 2026**: free-for-UE era over (ended end-2024); Bridge sunset; new Megascans Fab-only, priced; only select assets free [verified: quixel.com news + forums]. No loss for flat-shaded low-poly.

**Terrain textures**: ambientCG (CC0) and Poly Haven (CC0) [verified] — for flat shading, likely just HDRI skies and ground tints.

**SFX**: **Freesound** (filter CC0 — artillery, explosions, radio squelch usable without attribution) [verified: license forums + example]; **Sonniss #GameAudioGDC bundles** — tens of GB professional SFX, royalty-free commercial, no attribution, can't resell standalone, no AI-training use [verified: sonniss.com/gdc-bundle-license]. Between the two, artillery/radio SFX are a curation afternoon.

**Voice**:
- Public-domain recordings: US federal works carry no US copyright; Vietnam-era fire-support radio traffic exists (AFVN on Internet Archive, Texas Tech Vietnam Archive) [verified: both URLs]. Per-item provenance varies — verify each clip is a US gov work. Ambience/texture, too noisy for FDC readbacks.
- Local TTS for the FDC voice: Piper (MIT, 11k stars, pushed 2025-08-26 [verified via API]) + radio filter chain. **Post-report correction (research 06): rhasspy/piper was archived Oct 2025; successor piper1-gpl is GPL-3. Use Kokoro-82M (Apache-2.0) as the generator instead.** Per-voice model licenses always apply.

## 5. Niagara VFX packs

- **Niagara Examples Pack (Epic)** — 50+ ready systems: explosions, impacts, smoke, fire, sparks, trails + gallery level. Free, ~Jan 2026, UE 5.7 [verified: 80.lv + unrealengine.com news]. Epic content — assume UE-only (fine here); exact Fab listing terms 403'd. **The single biggest VFX shortcut**: retint/simplify a proven explosion into a flat-shaded airburst. Days-to-a-week saved.
- **FX Variety Pack (Kakky)** — permanently free (Epic Permanent Collection, 2019) [verified: snippet]. Cascade-era; UE5 compat not checked.
- "FREE! Realistic Niagara Explosions Pack" (15 systems) — titled free [verified: snippet]; license tier unconfirmed (403) — check on claim.
- Stylized-explosion Niagara packs on Fab fit the low-poly look better — browse against the limited-time-free rotation.
- **Kenney Particle Pack** — 80 CC0 sprites [verified] to feed homegrown Niagara emitters, fully restriction-free.

## Bottom line — highest-value adoptions

1. **Epic Niagara Examples Pack** (free, UE-only) — solves burst/smoke VFX, the least-built asset class. Largest single cut.
2. **CC0 model trio: Poly Pizza + Quaternius + Kenney** — troops/vehicles/vegetation, license-clean in any engine, matching the low-poly style. Bunker/watchtower/battery stay DIY boxes.
3. **Sonniss GDC + Freesound CC0** — artillery and radio SFX become curation, not synthesis.
4. **Kokoro TTS + radio filter chain** (corrected from Piper) — full doctrinal FDC voice, locally generated, no VO cost.
5. **FOPCSim theses (public domain)** — doctrinally-vetted blueprint for grading and task flow from the actual USMC trainer lineage; an evening's read before finalizing the grading port.

Explicit warnings: OpenTournament (restricted) and anything GPL (ACRE2, ArmaBallisticComputer) are do-not-copy; Arma mod content non-portable; Epic sample content (Lyra, Niagara pack) locks to UE builds forever — keep out of any shared/browser codepath.

Sources: OnAllFronts · OpenTournament · ArtilleryEco · cesium-unreal · ACRE2 · FOPCSim theses (archive.org ×2) · DVIDS · Quantum3D AFOS · A-JFOT · UTF CFF SOP · MCC FOA wiki · EKS AI Artillery · ECSProjectiles · UE5-EnhancedProjectile · ballistics-engine · Kenney (blocky-characters / tower-defense-kit / particle-pack) · Quaternius CC0 statement · Poly Pizza · itch.io military tag · Fab Limited-Time Free · Fab EULA · Quixel-on-Fab news · Poly Haven license · ambientCG · Sonniss GDC license · Freesound example · AFVN recordings · Vietnam Archive audio · rhasspy/piper · Epic Niagara pack news + 80.lv · FX Variety Pack · Lyra on Fab · UE EULA
