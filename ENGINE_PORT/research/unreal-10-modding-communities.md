# Research 10 — Modding-community lessons, tooling, and resources

Agent report, checks performed 2026-08-28 ("active" dates = GitHub API `pushed_at`). Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §11.

## 1. Arma 3 / Arma Reforger community

**(a) Design documentation — readable regardless of code license**
- **ACE3 Artillery Tables** — closest existing doctrinal CFF aid in a game: rangetable with elevation/azimuth, optional wind deflection and air friction, can disable the in-game computer [verified: ace3.acemod.org wiki]. Source: `addons/artillerytables/` in acemod/ACE3 — `RscRangeTable.hpp` + `functions/` hold the firing-table math [verified: contents API]. **License: GPL-2.0+ with .pbo exception; some sounds CC-BY; `addons/apl` under Arma Public License** [verified: license API]. Read for design and math cross-checks only.
- Community FDC/fire-support mods worth reading (design only; licenses not checked): **ITC Land Systems** (player-crewed artillery, "Battery Control System" — closest community FDC console model), **Fire For Effect: The God of War** (AI battery dispatch), **Drongo's AI Fire Support** (simple request UX) [verified: forum/Steam URLs].

**(b) Open tooling reusable outside Arma — mostly no** (toolchain is p3d/pbo-specific): ArmAToolbox (GPL-2.0+ with output exception), Arma3ObjectBuilder (GPL-3.0), **HEMTT** (GPL-2.0) [all verified: license API/README]. HEMTT's *practice* — declarative project file, CI builds, config lints — is worth copying as an idea for SHITFIRE's content pipeline (§6).

**(c) Training-community materials**
- UNITAF SOPs: call-for-fire + **adjust-fire** pages [verified: URLs]. No license statement found — treat as all-rights-reserved: learn the flow, write our own dialogue.
- **Better than any milsim material: USMC The Basic School handout "Call For Indirect Fire" (B2C0299XQ)** on trngcmd.marines.mil [verified: URL]. US Government work = public domain — grading-rubric and dialogue source usable verbatim. **Contents not read this session — the highest-value next fetch.**

**(d) Bohemia EULA — confirmed total bar**
- Licenses FAQ: APL/APL-SA allow use "in any Arma game" — Arma-only, non-commercial [verified: fetched]. Reforger Workshop IP FAQ, operative quote: **"transferring our textures (in original form or edited) to any other games, incl. A3, DayZ, etc. is not permitted"** — porting barred even between Bohemia's own titles [verified: fetched]. BI wiki wording corroborates [snippet only; page 403'd].
- Flat restatement: **no Arma asset, config data, or APL content ships in SHITFIRE under any theory.** ACE's GPL additionally barred by Epic EULA §6(c). Everything in this section is read-only.

## 2. Falcon BMS / DCS communities

**Voice-frag culture (Falcon BMS)**
- Publicly documented structure: a **frag** = one word/phrase recorded by up to 14 voices; an **eval** = a numbered variant set the AI picks from; a **comm** = an ordered frag/eval list forming a transmission [verified: BMS forum topic 21425]. **The three-layer word → variant set → sentence template model is exactly the data model for SHITFIRE's spliced FDC voice — the proven design, written down.** TLKTOOL (archive un/repacker) exists [verified: same topic; license not checked]. lightningstools: no license file [verified] — not needed.

**Radio-comms state machines (DCS) — the big find**
- **SkyEye** (https://github.com/dharmab/skyeye) — self-hostable AI GCI bot: **local Whisper STT → intent parsing → doctrinal brevity responses → TTS over the radio. MIT** [verified: license API]. A complete, permissively-licensed, modern implementation of "player speaks on radio → parse → doctrinal state machine → voice reply" — the same loop as an FDC. Readable AND adoptable (Go — port patterns/logic legally).
- OverlordBot — GPL-3.0 [verified]; superseded by SkyEye as a source. **DCS-SRS** — GPL-3.0 [verified]; read-only for radio modeling ideas (frequency matching, transmission occupancy/blocking).

**Kneeboard/chart culture**
- OpenKneeboard — custom GPLv2-derived license + mandatory rebranding of derivatives [verified: fetched]. Read-only. TacBoard — GPLv3 [unverified]. **DCS Retribution** — campaign generator with kneeboard generation, **LGPL-3.0** [verified] — worth reading for auto-generated per-mission briefing sheets: the model for SHITFIRE auto-generating an observed-fire fan sketch / range card per scenario.
- Ecosystem lesson: the flourishing tools generate *printable/strappable paper* from mission data — a trainer that emits its own range card and target-list sheet per scenario matches proven demand.

**DCS Combined Arms JTAC — design lessons**
- CA's FO experience is thin (camera + laser + map; no doctrinal flow, no grading) — precisely SHITFIRE's gap [verified: Steam discussion].
- **Concrete failure to design around** [verified: Airgoons wiki snippet]: ground-level JTAC lasing fails at grazing angles — the laser bounces off terrain short/long of the vehicle while the player believes they marked it. The canonical **silent failure** of the genre: SHITFIRE should always give the student feedback about where the laser actually hit.

## 3. Squad / Project Reality / Hell Let Loose calculators

| Tool | License | Activity |
|---|---|---|
| **SquadMC** (map-based mortar calc) | **MIT** [verified] | last push Jan 2023 |
| **SquadCalc** | **custom "MIT non-commercial"** — bars commercial use [verified: fetched license] | very active (pushed 2026-08-27) |
| Squad Mortar Helper | CC BY-NC-SA 4.0 [verified] | — |
| **MSMC** (minimalist calc) | **MIT** [verified] | — |
| hll-arty-map-calculator | MIT, **game assets excluded from the grant** [verified: fetched] | — |
| easyarty | **no license file** → all rights reserved [verified: 404] | — |
| hll-mobile-artillery-calculator | MIT [unverified — snippet] | — |

**UX lessons** (SquadMC README [verified]): dual input — map tap/drag markers *or* typed keypad grids, freely mixed (SHITFIRE: map click for instructor, typed 6/8-digit grid for the graded student); heightmap-aware solutions with a **visible, toggleable heightmap** (showing *why* elevation changed the solution is a teaching feature); live min/max range rings (instant infeasibility feedback); mobile-first second-screen use (a companion range-card view is a proven pattern); marker save/switch for multiple targets.
**Adoptable code**: SquadMC + MSMC (MIT) — ballistic solution from grid pairs + heightmap, keypad-grid input logic. **SquadCalc: the best and most active of the family, but non-commercial — read, don't copy.**

## 4. OpenRA / open modding infrastructure

- GPL-3.0 code [verified] — read-only; the *architecture* is documented and freely imitable:
- **Data-driven everything via MiniYAML**: units/weapons/rules as plain text; entities are bags of composable traits [verified: docs.openra.net + book]. Trait docs **auto-generated from code** — the practice that keeps modder docs from rotting.
- **Deterministic lockstep + order-stream replays**: replays store only player orders + RNG seed; sim replays tick-for-tick with periodic state hashes [verified: secondary sources; netcode not read].
- Lessons for SHITFIRE-as-moddable-trainer: (1) shell/gun/weather data as external text from day one — near-zero cost now, enables instructor-authored content later; (2) **order-stream replay of a graded mission (inputs + seed → deterministic re-sim) is the cheapest AAR and a grading audit trail** — only possible because the sim is deterministic (TLOG already is this; keep it sacred); (3) auto-generate the scenario-author reference from the data schema.

## 5. Flight-sim terrain tooling

- **TerraGear** (FlightGear): DEM+vector two-phase tile pipeline [verified: README]. **License unverified** — GitHub mirror has no top-level license file [verified: 404]; community says GPL-2.0 [not checked]. Last push 2022. Read-only; for one 10 km island, GDAL covers everything anyway.
- **X-Plane Scenery Tools (xptools)**: Laminar's `src/` is **MIT/X11** [verified: README quote]; active (pushed 2026-08-14) [verified]. The one *permissive* real-terrain pipeline in the genre: mesh generation from DEM + vector integration (roads/shorelines cut into mesh). Probably overkill here; on file.
- **Ortho4XP**: active but **no license file** [verified: API null] — all-rights-reserved by default despite huge community use. Trap; read-only.
- MSFS community DEM tools exist [URLs verified]; licenses not checked; nothing beyond GDAL.
- **Data-licensing norms**: USGS National Map: "free and in the public domain. There are no restrictions"; requested credit line "Map services and data available from U.S. Geological Survey, National Geospatial Program" [verified: USGS FAQ]. Flight-sim norm: a credits file listing every data source even when public domain — cheap goodwill, do the same. SRTM: public domain to community knowledge [unverified this session].

## 6. Practices worth stealing

- **Scenario file format**: Arma `mission.sqm` = plain-text config, editor-written but hand-editable/diffable [URL verified; page 403'd — partly unverified]. **DCS `.miz` = a zip of plain Lua text files** openable with 7-zip [verified: hoggit wiki snippet]. **The zip-of-text-files design is the one to copy**: human-diffable inside, single-file distributable outside, room for attachments without schema changes. `.sfmission` = zip of JSON/YAML (targets, TRPs, weather, grading thresholds, radio overrides). Key insight: both formats work for non-programmers because **the game's own editor writes them** — hand-editing is polish, not the entry ramp; ship even a minimal in-game scenario editor first.
- **Documentation culture**: BI Community Wiki = reference model; OpenRA's auto-generated docs = the solo-dev version.
- **Distribution/versioning**: HEMTT shows modder expectations (manifest, one-command build, lints); Reforger Workshop adds per-mod license choice + explicit no-monetization [verified: IP FAQ]. Lesson: scenario packs carry (a) version, (b) declared license field, (c) game-version compatibility **from the first release** — retrofitting versioning is the pain every platform went through.
- **Grading/AAR sharing**: milsim units run "schools" against written SOPs — design the mission-grade export (score sheet, call transcript, rounds plot) as a shareable artifact; unit instructors are the multiplier audience.

---

## (a) Top 5 takeaways, ranked

1. **SkyEye (MIT)** — the FDC loop (speech → intent → doctrinal state machine → voice), already architected, legal to learn from and port.
2. **Falcon BMS frag/eval/comm** three-layer voice data model — the proven spec for the spliced FDC voice; adopt the structure, record our own audio.
3. **SquadMC + MSMC (MIT)** — adoptable solution/grid-input code; UX lessons transfer directly. SquadCalc = read-only.
4. **Determinism + order-stream replay (OpenRA), decided now** — replays/AAR/grading audits nearly free; cannot be retrofitted. Pair with external text data files.
5. **USMC B2C0299XQ handout (public domain)** + UNITAF adjust-fire SOP for dialogue/rubric; `.miz`-style zip-of-text for instructor scenarios.

## (b) Read-only shelf (study, never copy)

ACE3 artillerytables (GPL) · ITC Land Systems / Fire For Effect / Drongo's (licenses unchecked) · DCS-SRS (GPL-3.0) · OverlordBot (GPL-3.0) · OpenKneeboard (custom GPL-derived) · SquadCalc (MIT-noncommercial) · Squad Mortar Helper (CC BY-NC-SA) · OpenRA engine code (GPL-3.0; docs freely imitable) · all Bohemia assets/data/tool output, ACRE2/ACE · TerraGear (license unverified, assume GPL)

## (c) License traps

1. **SquadCalc looks MIT but isn't** — custom non-commercial clause.
2. **No-license repos = all rights reserved**: easyarty, Ortho4XP, lightningstools, TerraGear mirror.
3. **MIT calculator repos bundle game-IP images** the code grant doesn't cover (hll-arty explicitly excludes them).
4. **Bohemia's porting bar is total** — no "it's just config data" loophole.
5. **GPL-derived custom licenses** (OpenKneeboard) add obligations beyond GPL — fail both Epic-EULA and standard-OSS assumptions.
6. Licenses that clear Epic's EULA in this survey: **MIT** (SkyEye, SquadMC, MSMC, xptools src/), **LGPL-3.0** (DCS Retribution — static-linking care needed in UE), **public-domain government work**.

Not checked, flagged honestly: TerraGear's exact license; ITC/FFE/Drongo licenses; TacBoard license file; wd-creation HLL calc license; MSFS tool licenses; SRTM current terms; **contents of the USMC B2C0299XQ PDF (fetch next — highest-value single document this pass surfaced).**
