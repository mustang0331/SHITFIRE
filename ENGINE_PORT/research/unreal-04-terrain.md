# Research 04 — Terrain & geospatial pipeline in UE5

Agent report, checks performed 2026-08-27. Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §4.

## 1. GeoTIFF → UE Landscape (the standard path)

**Import formats & limits** [verified: Epic Landscape Technical Guide]
- Landscape imports 16-bit grayscale PNG or 16-bit r16 raw (raw now wants a JSON sidecar: width/height/bit depth).
- Recommended component-aligned resolutions: 127², 253², 505², 1009², 2017², 4033², 8129²; practical ceiling 1024 components.
- **Vertical scale math**: heights are 16-bit mapped to −256…+255.992 units at Z scale 100 (Z scale 100 = 512 m total range). `Z scale = height_range_in_cm × 0.001953125`. Mid-gray (32768) = 0 m. You need the raster's true min/max elevation (Epic's Mauna Kea example confirms: 4207 m → Z scale 821.68).
- For 10 km: 2017² ≈ 4.96 m/quad, 4033² ≈ 2.48 m, 1009² ≈ 9.92 m [inferred: arithmetic].

**GDAL conversion** [verified: gdal_translate docs]
- One command: `gdal_translate -ot UInt16 -of PNG -outsize 2017 2017 -scale <src_min> <src_max> 0 65535 in.tif out.png`. Feed `-scale` the actual elevation min/max (`gdalinfo -stats`) and **record them** — they define the Z scale. [assumed: `kofa_king_valley_usgs_3dep_raw.tif` is projected-CRS with square-meter pixels (USGS 3DEP 1 m is UTM); if it's the 1/3-arc-second geographic product, `gdalwarp` first — not checked which product the repo file is.]
- QGIS Heightmap Export plugin: last updated **March 2019** [verified] — use plain gdal_translate instead.

**Automation plugins**
- **UnrealGDAL (TensorWorks)** — MIT, "UE 4.25.0 or newer", no UE5 statement, ~3 commits, **dormant** [verified]. **No getnamo GDAL fork exists** [verified: searched]. Live-ish: ssav7912/GDALforUE5 (MIT, UE 5.3, 11 commits) [verified]; iwer/GeotiffLandscape depends on the old chain [existence verified, not deep-checked].
- **TerraForm PRO/LITE** — commercial Fab GIS importer; LITE free [existence verified; site 403'd — versions/pricing not checked].

**Bottom line §1**: the no-plugin path (gdal_translate + built-in import) is complete and unmaintained-dependency-free. Every in-editor GDAL plugin is dormant or a thin fork.

## 2. Cesium for Unreal

- Apache-2.0, very active (~6,950 commits, monthly releases) [verified].
- Current: v2.29.0 (Aug 3, 2026), requires UE 5.6+; v2.28.0 (Jul 1, 2026) last with 5.5, added 5.8; policy "three most recent engine versions" [verified: CHANGES.md — an earlier fetch misread the releases page as 2024; CHANGES.md resolves it].
- **Offline**: local tilesets from disk work with no ion key, but "Cesium World Terrain and other Cesium ion tilesets are not currently available offline" [verified: FAQ]. You'd tile the Kofa GeoTIFF into 3D Tiles yourself [tiler tooling not checked]; community thread shows real friction with local tilesets [verified: community.cesium.com 21254].
- **Determinism**: tile/LOD selection is camera- and load-timing-dependent; ECEF georeferenced frame. Wrong shape for a fixed seed-deterministic 10 km island [inferred from architecture].
- **Verdict: overkill and a determinism liability.** Solves problems SHITFIRE doesn't have; creates ones it can't afford.

## 3. Custom heightfield instead of UE Landscape

- **RealtimeMeshComponent (TriAxis)** — free Core edition **MIT**, README **UE 5.5–5.8**, commits **Aug 21–23, 2026** (GitHub tagged releases stale at 2024; distribution is Fab + master) [verified: repo/README/releases/commits]. Core: LODs, sections, materials, optional collision. Pro (paid): runtime Nanite, streaming.
- ProceduralMeshComponent (built-in) is adequate for a one-shot static build; RMC is the better citizen and costs nothing. Either works — mesh generated once per seed, not per-frame [inferred from verified feature lists].
- **Skipping Landscape sane at 10 km / 60 fps? Yes.** 10 km at 25 m posts = 401² ≈ 161k verts; even 10 m posts = 1001² ≈ 1M verts / 2M tris is comfortable as static chunked geometry. Chunk (e.g. 16×16) for frustum culling; 2–3 LOD index buffers per chunk if needed (RMC Core supports LODs). Flat shading: compute face normal with ddx/ddy of world position in the material — no vertex duplication [inferred: arithmetic + verified RMC LODs; ddx/ddy is standard practice, not doc-checked].
- **Nanite: skip.** Fixed 9–20% GPU overhead; hurts low-poly content; payoff is multi-million-poly assets [verified: forum thread + article]. Nanite-for-Landscape (5.3+) irrelevant [verified: doc].
- **The parity win is decisive**: port H(x,z) as plain C++ with no UE types; three consumers read the same array — RMC mesh builder, ray-march/LOS/ballistics, marching-squares sheet renderer. UE Landscape can't offer that: stored heights are 16-bit quantized, so Landscape-H ≠ sim-H (~1.5 cm class error — visually nothing, numerically not identical) [verified quantization: Epic guide; argument inferred].
- Generate with **no collision at all** — skip cooking a 2M-triangle trimesh; the sim ray-march is the authority.

## 4. Line traces without physics

- If UE collision were used: Landscape collides via `LandscapeHeightfieldCollisionComponent`; Collision Mip Level 0–5 trades memory for accuracy; Epic recommends mip 0 for gameplay [verified: Landscape Collision Guide]. Per-query cost is fine; the cost is memory + startup cooking.
- **Keep the sim ray-march authoritative — the argument is agreement, not cost.** A UE trace samples collision geometry: mip-reduced, 16-bit quantized, fixed-diagonal triangulated — three ways to disagree with sim-H interpolation. Two LOS implementations = the classic silent failure: laser range off UE collision vs ballistic impact off sim H; a grazing ridge shot scores differently than the laser said, no error anywhere. Route lasing/LOS/impact through one sim-core function. If UE traces exist at all (UI actors), scope them to actors, never terrain.

## 5. Water / ocean

- Water plugin **still Experimental**; crash reports in 5.4+, forward-rendering/VR regressions [verified: two forum threads; 5.6+ plugin-browser flag not directly checked]. Its shoreline features work by carving a Landscape with Edit Layers [verified: docs + threads] — on the custom-mesh path there is no Landscape, so you'd pay its costs for none of its features.
- **Recommendation: flat translucent plane at sea level, like the browser build.** One quad (or small grid), panning normal map if wanted. **Its Z must derive from the sim's sea-level constant in code** — sea level participates in LOS/impact logic.

## 6. World size / precision — the math

- UE units are cm; 10 km = 1,000,000 cm. float32 spacing at [2^19, 2^20) cm is 0.0625 cm ≈ **0.6 mm** worst-case at the far corner [arithmetic].
- 1 NATO mil ≈ 0.9817 mrad ≈ 9.8 m at 10 km. A 0.6 mm position error ≈ 6×10⁻⁵ mil — five orders below a 0.1-mil tick [arithmetic].
- **LWC not needed for correctness — but free.** UE5 uses double-precision world coordinates by default (doc labels LWC "Beta") [verified: LWC doc]. Center the map on origin, do nothing else. Rendering is camera-relative under LWC — no vertex jitter. Separately: **keep the sim core in double** — JS numbers are float64; float32 in the core would be the actual parity breaker.

---

## Recommended pipeline

Port the sim core (noise island, DEM sampler, ray-march, ballistics) as a plain double-precision C++ module with zero UE dependencies; at load, generate visual terrain from that module's H(x,z) into chunked **RealtimeMeshComponent** meshes with no collision cooked, flat shading via ddx/ddy, centered on origin with LWC defaults; ocean = flat translucent plane at the sim's sea-level constant; feed the same H array to the marching-squares sheet renderer; load the Kofa DEM by resampling the GeoTIFF once with gdal_translate into your own image-load path (same grayscale route the browser uses — not UE Landscape import), recording min/max as vertical scale. Skip UE Landscape, Cesium, Nanite, the Water plugin, and all physics. Cesium stays a documented non-choice; Landscape import stays a fallback only if editor sculpting is ever wanted.

## Top silent-failure risks

1. **Two LOS/range implementations.** Mitigation: terrain has no collision (a stray trace returns nothing, not a plausible wrong answer) — make "no terrain collision" a deliberate invariant, plus a startup self-test ray-marching N known rays against golden browser values.
2. **Float vs double drift in the ported noise.** Compile the core in double, no fast-math; golden-test H at sampled (x,z) against browser output.
3. **DEM scale silently wrong.** Wrong `-scale` min/max = normal-looking terrain, every ballistic solution subtly off. Pin min/max as repo constants next to the PNG.
4. **Sea level mismatch.** Derive the water plane's Z from the sim constant in code, never by eye.

Sources: Epic Landscape Technical Guide · gdal_translate docs · QGIS HeightmapExport · TensorWorks/UnrealGDAL · ssav7912/GDALforUE5 · iwer/GeotiffLandscape · TerraForm on Fab · CesiumGS/cesium-unreal + CHANGES.md + FAQ + community thread 21254 · TriAxis RealtimeMeshComponent · Nanite low-poly forum thread · Nanite for Landscapes doc · Landscape Collision Guide · Water plugin threads ×2 · Water Body Actors doc · Large World Coordinates doc
