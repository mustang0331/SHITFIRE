# Research 09 — Animation: troops, vehicles, buildings, effects

Agent report, checks performed 2026-08-28. Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §10.
Fetch caveats: helpx.adobe.com (Mixamo FAQ) timed out on five attempts; mocap.cs.cmu.edu fails TLS — both quoted via search snippets + corroboration, tagged as such.

**Pixel arithmetic** [inferred, simple trig]: 9° binocular FOV on a 1920px screen ≈ 213 px/degree. A 1.8 m person ≈ **20 px at 1,100 m, 7 px at 3,200 m** through binos; ≈ 2 px / <1 px naked-eye. A 6 m vehicle ≈ 23 px at 3,200 m in binos. A 20 m burst cloud ≈ 76 px. This drives every recommendation.

## 1. Rigged + animated low-poly character sources

**Quaternius** — all CC0, the deepest well:
| Pack | Contents | Formats | Date |
|---|---|---|---|
| Universal Animation Library | 120+ animations: 8-dir locomotion, jog, sprint, **crawling**, swimming, **deaths**; "universal humanoid rig, compatible with Unreal Engine, Godot and Unity, ready for retargeting" | FBX, GLB, Blend | Mar 2025 [verified: pack page] |
| Universal Animation Library 2 | 130+ more: melee/armed combos, parkour | FBX, GLB, Blend | Jan 2026 [verified] |
| Ultimate Animated Character Pack | 50+ animated characters | FBX, OBJ, Blend | Nov 2019 [verified] |
| Animated Men Pack | 4 humans incl. run/die | FBX, OBJ, Blend | Feb 2019 [verified] |
| Ultimate Modular Men Pack | 11 characters, 24 animations, swappable parts | FBX, OBJ, glTF, Blend | Feb 2022 [snippet-verified] |

License model: 60–70% of each pack free (CC0); rest via Patreon "Source" tier [verified]. UAL 1+2 is the find: **CC0 animation library with crawl + death cycles, built for retargeting.**

**Kenney** — Blocky Characters v2.0 (2025 remake): **18 characters, 27 animations, CC0** [verified: announcement + kenney.nl]. v2 file formats not confirmed (JS-rendered page; itch mirror 404) [not checked]. Kenney Character Assets (with Kay Lousberg): 4 characters, 17 animations (death, crouch-walk, shoot), FBX+Blend, CC0 [verified: itch page] — currently "unavailable for purchase."

**KayKit (Kay Lousberg)** — the best library nobody mentions: **KayKit Character Animations — 161 humanoid animations (150+ free tier), CC0**, FBX + GLTF: walking, running, **crawling, sneaking, crouching, deaths, lying down, shooting, aiming, reloading** — nearly the whole SHITFIRE troop vocabulary. Targets KayKit's Rig_Medium/Rig_Large; retargetable in UE. Actively updated [verified: kaylousberg.itch.io]. Also Adventurers (5) and Skeletons (4) rigged+animated CC0 [snippet-verified].

**OpenGameArt** — rigged-only graveyard for this need (CC0 rigged soldiers from 2011/2020, **no animations**) [verified: two listings]. Props, not troops.

**itch.io** — per-page prose licensing; check individually. KayKit/Kenney are the reliable itch citizens.

**Sketchfab/Fab status**: **CC0/CC-BY-SA/NC/ND models could not migrate to Fab** (those licenses don't exist there); Sketchfab free downloads + API still work mid-2026, CC0 filter available [verified: Sketchfab blog + 2026 guides]. **A declining archive — download what you want now.**

## 2. Mixamo — the license question

Direct FAQ fetch failed ×5; triangulated:
1. FAQ snippets (helpx.adobe.com): royalty-free for "personal, commercial, and non-profit projects" incl. "create video games"; restriction: cannot "create blueprints, templates, or asset packages for video game engines which redistribute character or animation raw files as the product" [verified via snippet; full page not read].
2. Adobe community answer (2022, Adobe-affiliated responder): fine in commercial games "as long as you're not selling the character or animation data exclusively" [verified: thread].
3. LicenseOrg summary agrees [verified; not legal advice].

**Practical reading**: shipping Mixamo animations baked into a distributed game — free now, commercial later — is permitted. **Trap: raw Mixamo FBX committed to a public repo is extractable standalone — gray zone under the "raw files" clause. Keep source FBX out of public repos; cooked .pak is fine.** [inferred]

**Service status 2026**: online, free, needs an Adobe ID; development stopped years ago; June 2025 multi-day outage showed brittleness; no discontinuation announced [verified: 2026 secondary guides]. **Download early.** FBX→UE pain: own bone names, A-pose-ish, in-place only; UE 5.4+ largely tamed retargeting (§4); enziop/mixamo_converter Blender addon (GPL-3.0, renames bones to UE mannequin, batch converts) for root motion [verified: repo] — mostly unneeded here since lerp-driven wandering *wants* in-place clips.

## 3. Open animation libraries

- **Quaternius UAL 1+2** — the winner (§1). CC0, retarget-ready, crawl/death included.
- **KayKit Character Animations** — 161 CC0 clips incl. military verbs (§1).
- **CMU mocap** — free; terms (informal): may be copied/modified/redistributed, may ship in commercial products, may not resell the data directly [verified via snippets; origin page TLS-broken]. Raw research mocap on a bare skeleton, noisy, needs cleanup — **more work than Quaternius/KayKit for 7–20 px troops. Skip.**
- **Bandai Namco Research Motiondataset** — LICENSE file read: **CC BY-NC 4.0** — NonCommercial only; adaptations allowed (contrary to a CG Channel "No Derivatives" claim — resolved by reading the license) [verified: raw LICENSE]. **NC poisons a possibly-later-commercial trainer. Do not use.**

## 4. UE5 retargeting and the cheap routes

- **IK Retargeter (5.8 docs)**: transfers between skeletons with different bone counts/names/orientations; IK Rig editor has **Auto Create Retarget Chains**; since ~5.4 auto-analyzes humanoid skeletons [verified: doc + guides]. Mixamo→UE5 is no longer the 5.0-era ordeal: import, auto-chains both rigs, fix retarget pose alignment, batch-export. Residual fiddliness: pose alignment, unit scale, root motion — an afternoon, not a week.
- **Vertex Animation Textures** (bake per-frame vertex positions into a texture; static mesh "plays" animation in-material, no runtime skeleton): Epic's **AnimToTexture plugin ships in-engine since 5.1** (from City Sample crowds), not enabled by default [verified: tutorial + forums]. **OpenVAT** — Blender extension, GPL-3.0 core/MIT examples, UE5 support but currently prefers forward rendering [verified: repo]. Rexocrates/Vertex_Anim_Toolset — MIT, **archived Nov 2024, README says use Epic's AnimToTexture** [verified]. Per-instance playback: ISM/HISM **PerInstanceCustomData** floats drive per-instance animation index/phase in the vertex shader [verified: 5.8 docs].
- **The honest call**: SHITFIRE has ~30–100 agents, not City Sample's thousands. **A hundred plain SkeletalMeshComponents playing retargeted CC0 clips is not a performance problem** [inferred from VAT's purpose]. At naked-eye range no animation is visible; only through binos at 1,100 m (~20 px) do limbs read. So: **states beat cycles** — standing/prone/dead pose swaps + existing position lerp deliver 90% of the training signal ("troops dispersed and went prone" is a silhouette change, visible at 7 px). Add one retargeted walk/crawl/death set for the binocular case on ordinary skeletal meshes. **Skip VAT** unless the population grows 10× — then AnimToTexture + ISM custom data is the ready-made path.

## 5. Vehicles — convoy without physics

- **Chaos Vehicles confirmed overkill**: Epic's setup doc requires Skeletal Mesh + Physics Asset + AnimBP with Wheel Controller + Vehicle BP + per-wheel BPs + torque curve — nine stages simulating suspension/engine/tires for *driveable* vehicles [verified: doc]. Invisible on a 23 px box; physics injects nondeterminism the authoritative sim doesn't want.
- **The pattern**: one SplineComponent along the road; per vehicle `distance += speed * dt` → `GetTransformAtDistanceAlongSpline(distance - vehicleIndex * spacing)` [verified: API exists]. Pit stop = hold the accumulator at a marked spline distance on a timer; stagger per vehicle. Known wrinkle: sparse spline points cause flutter [verified: forum] — add points/smooth speed. Wheel spin: rotating child components, pitch = speed/radius [inferred, trivial] — but at 23 px wheels don't resolve; **dust puffs behind the convoy (Niagara ribbon) sell motion better.** Quaternius Animated Tanks covers tracked vehicles.

## 6. Building damage states

- **Authored swap meshes win, not close**: intact/damaged/rubble statics swapped on sim-ruled hits — deterministic, sim-authoritative by construction, zero runtime cost, save = one enum per building. For flat-shaded box huts, each damaged variant is ~an hour of Blender (caved roof, tilted walls, rubble skirt). Mask the swap with the impact's own smoke/flash.
- **Chaos Destruction**: fracture pattern is authored, but **runtime debris simulation diverges run-to-run** — community guidance for consistent destruction is "replicate the damage event, not the result" [verified: guide + forums]. Could only ever be cosmetic garnish here; a one-shot Niagara debris burst over a mesh swap gives the same picture for 5% of the effort. Chaos Cache (record once, replay deterministically) exists [not checked beyond a mention].
- CC0 rubble assets: OGA Destroyed City Assets (CC0, 2017, high-poly) [verified]; skyscraper ruins on itch [unverified; wrong scale]. **Nothing matches flat-shaded boxes — author your own; found rubble will look like a different game.**

## 7. Effect animation beyond the Niagara pack

- **Flipbook sources** (flipbook = sprite-sheet animation on a camera-facing quad): Kenney Particle Pack (80 sprites, CC0) and **Kenney Smoke Particles** (77 sprites in 5 styles: black smoke, explosion, flash, white puff — CC0) [verified: listings]; OGA explosion sprite atlas (3×3 512px, CC0) [verified]; many more CC0 sheets on OGA [search-level].
- **Blender→flipbook bakers (open source)**: **Spritify** (FreezingMoon) assembles rendered frames into sheets [verified: repo]; theloneplant/blender-spritesheets, chrishayesmu/Blender-Spritesheet-Renderer [exist; licenses/activity not checked]. Pipeline: Mantaflow smoke → frames → Spritify → Niagara SubUV. (EmberGen = commercial gold standard, noted for contrast.)
- **Burst at 3 km**: 20 m HE cloud ≈ 76 px in binos — a world-scaled billboard flipbook reads perfectly; volumetrics are wasted pixels. Sequence: 1-frame white flash → dirt-brown bloom ~2 s → **persistent smoke column** (slow ribbon or tall scrolling-panner plane; 60 m column ≈ 230 px), alive for minutes so the observer can re-find the impact for adjustment. **Persistence is a training feature, not decoration.**
- **Night fire**: Niagara **Light Renderer** attaches real lights to particles; emissive >1 blooms [verified: Epic docs]. 100–200 ms point light painting the terrain + emissive flash sprite = the whole night-fire effect; ongoing fires = flicker sprite + noise-driven light.

---

## Recommended plan (ranked by workload, respecting the pixels)

1. **Effects first** — the observer stares at these; they get the polish budget. Niagara pack + Kenney CC0 SubUV flipbooks; billboard burst → persistent column; Light Renderer at night. (~days)
2. **Buildings** — authored swap variants + enum-driven swap + one-shot debris. No Chaos. (~1 hr/building + a day of plumbing)
3. **Vehicles** — spline + GetTransformAtDistanceAlongSpline + timed pit-stop hold + dust ribbon. No Chaos Vehicles. (~1–2 days)
4. **Troops** — (a) states first: standing/prone/dead pose swaps on existing lerp; disperse = radial velocity + prone. Naked-eye complete alone. (b) binocular tier: Blocky Characters v2 / Quaternius modular men as ordinary skeletal meshes + UAL/KayKit clips via auto-chain IK Retargeter (walk, crawl, death). No Mixamo needed unless a clip is missing; no VAT at this head count. (~2–4 days incl. retarget learning curve)
5. **Civilians** — troop pipeline, different skins. (~half a day)

**Explicitly not recommended**: VAT/AnimToTexture (crowd tech; revisit at 10× population), CMU mocap, Chaos Destruction, Chaos Vehicles.

## License traps

1. **Bandai Namco datasets: CC BY-NC** — forecloses ever charging. Excluded. [verified]
2. **Mixamo**: in-game shipping fine; raw FBX in a public repo is the gray zone; service unmaintained — download early; eyeball the FAQ in a browser before shipping Mixamo content commercially.
3. **Sketchfab in managed decline; CC-licensed models can't migrate to Fab** — grab now. [verified]
4. **CC-BY needs attribution — start a CREDITS file now**, before provenance is forgotten.
5. GPL tools (OpenVAT, mixamo_converter): GPL covers the tool; baked assets are yours [inferred — standard FSF position; READMEs not checked for contrary claims]. Don't paste their code/shaders in without honoring GPL.
6. Epic Niagara/AnimToTexture/City Sample content: **UE-projects-only** — never follows the project to another engine.
7. **Zero-risk backbone: Kenney + Quaternius + KayKit — all CC0, all active 2025–2026.** Build the character pipeline entirely on these; Mixamo becomes optional, not a dependency.
