# Research 05 — 2D UI, vector drawing, cartography, and print/PDF in UE5

Agent report, checks performed 2026-08-27. Synthesis: [../UNREAL_RESEARCH.md](../UNREAL_RESEARCH.md) §5.

## 1. UMG/Slate for dense 2D

- UMG widgets can override `OnPaint` and call `UWidgetBlueprintLibrary::DrawLine/DrawLines/DrawBox/DrawText` — DrawLines takes a point array, tint, AA flag, thickness [verified: Epic API docs]. C++ Slate gets the full `FSlateDrawElement` set: MakeLines, MakeBox, MakeText, splines, custom verts [verified: API docs].
- Viability for a contour map: yes for the mechanism — the entire Unreal Editor (Blueprint/Material graph editors: dense grids, thousands of AA splines, text at many zooms) is Slate [verified: Slate architecture doc]. Line-AA bug history is UE 4.7–4.9-era/mobile [verified: old thread]; no 2025-era desktop complaint found [not checked beyond that]. Real cost is ergonomics: Slate authoring is C++-heavy with thin docs — a 2023 practitioner write-up names "limited documentation for authoring Slate widgets" [verified: dannymcgee.dev].
- Perf with thousands of segments: no published benchmark [unknown]. Engine answer is caching: **Invalidation Box** (skips prepass/tick/paint until invalidated) and **Retainer Box** (flattens children into a render target) [verified: UMG optimization docs]. A static contour map in one pays paint cost only on pan/zoom [inferred].
- Small text: Slate rasterizes via FreeType into atlases; UE 5.5 added optional SDF/MSDF text per font face for scaled text [verified: SDF doc + forum]. Blurriness complaints trace to DPI/widget scaling, not the rasterizer [inferred from recurring threads]. Render text at final pixel size and it's crisp. CommonUI text = same renderer [assumed].

## 2. Canvas render targets — the closest match to the existing code (headline finding)

- `UCanvasRenderTarget2D`: bind `OnCanvasRenderTargetUpdate`, draw, call `UpdateResource()` only when a redraw is wanted — explicitly not per-frame [verified: API]. Canvas API: DrawText (font + per-axis scale), DrawLine (thickness/color), DrawPolygon, DrawBox, DrawTexture, DrawMaterial, DrawTriangles [verified: API]. Blueprint/C++ can also draw onto any target via `BeginDrawCanvasToRenderTarget`/`EndDrawCanvasToRenderTarget` [verified: KismetRenderingLibrary].
- The HTML-canvas map code maps nearly call-for-call: `stroke()` → DrawLine loop / FCanvasLineItem batches; `fillText` → DrawText; `drawImage` → DrawTexture. No path/bezier objects, no dashes, no joins — you emit segments yourself (marching squares already produces segments, so this costs little) [inferred from verified API].
- **Resolution**: 2D render targets supported to 8192×8192 (the old 2048 figure is a 2014 answer; Epic-forum correction: "everything up to 8k" for 2D targets) [verified: thread 652106]. Print needs — A4@300 DPI = 2480×3508, A3@300 = 3508×4961 — fit with room; a 3508×4961 RGBA target ≈ 70 MB VRAM one-off [arithmetic]. 16384 on D3D12: not checked, not needed.
- **Export**: `UKismetRenderingLibrary::ExportRenderTarget` writes straight to PNG for LDR formats [verified: Blueprint API doc]. Map → render target → PNG is a built-in three-node pipeline.
- Caveat: DrawText scale-up of a small-rasterized font will look soft at 300 DPI — set rasterization size large / use a print-sized UFont [assumed — standard atlas behavior; prototype first].

## 3. Vector graphics plugins

- Epic's SVG Importer is part of Motion Design (5.4+) and converts SVG into 3D geometry for motion graphics — not crisp 2D UI [verified: roadmap + coverage].
- SvgBooga: **no license file, last push 2023-12-31** — dead and legally unusable [verified: API]. Pandoa SVGPlugin: commercial, docs repo active [verified]; price [unknown].
- Skia-in-UE: no maintained public plugin. McGee's 2023 series integrated Skia's CPU raster backend into a Slate widget but published no repo; flags Skia's build system and render-thread sync as the pain [verified: dannymcgee.dev]. 2024–2026 searches for a maintained Skia-UE plugin: empty [verified: searched].
- ThorVG / NanoVG: no UE integration found for either [verified: searched].
- **Bottom line: no maintained open-source vector/SVG runtime renderer exists for UE5.** Vector work goes through Slate elements, Canvas, or your own triangulation.

## 4. ImGui / RmlUi in UE

- ImGui ecosystem catalogued Dec 2025 by Dear ImGui's maintainer (issue #9122 "Converging toward a principal Unreal Engine backend") [verified]. Live: **VesCodes/ImGui** (MIT, pushed 2026-05-22, docking + multi-viewports, flagged most current) [verified]; **IDI-Systems/UnrealImGui** (MIT, pushed 2026-02-10, UE 4.27–5.5, bundles ImGui 1.91.9b + ImPlot) [verified].
- Shipped player-facing UI: ImGui's own README scopes it to tools/debug [verified]. Debug overlays only.
- Epic's alternative: **SlateIM** (UE 5.6, experimental immediate-mode Slate wrapper for debug tools) [verified: plugin index + Unreal Fest talk listing]. Same verdict.
- RmlUi: no free UE integration (thirdparty list has none; the GoaLitiuM port is Flax) [verified]. **VaCuus**: commercial UE 5.6–5.8 plugin vendoring RmlUi 6.x (HTML/CSS-like documents, data binding, live reload); free for prototypes/hobby/academic, $69.99/project commercial, each version → MIT after 4 years, v1.0.1 [verified: vacuus.ufna.dev]. Closest "keep writing HTML/CSS" that isn't CEF — young, single-vendor: candidate, not foundation.

## 5. Web-based UI inside UE (panels only)

- Built-in Web Browser widget: **Experimental for a decade**; Chromium 90 through 5.5/5.6; CEF 128 binaries in 5.6 source but disabled on Windows (crash if forced); proper CEF 128 in the **5.7 preview** [verified: forum thread 2617726 with Epic engineer]. Third-party: BLUI (MIT, CEF 128.0.6613.138, pushed 2025-12-05) [verified].
- Assessment: works, but ships a ~100+ MB browser runtime to draw a comms log, takes on CEF churn Epic itself struggles with, and bridges every game event across a JS boundary. The maintenance trap the decision doc feared — justified only if keeping *most* of the existing HTML UI (research 01's brief).

## 6. Print/PDF export

- **(a) Render target → PNG → PDF writers**: **libharu** — zlib/libpng license, alive: v2.4.5 (2025-03-11), v2.4.6 (2026-03-26), commits into Nov 2025 [verified]. C API, no UE dependency; "one A4 page containing one image" is a few dozen lines. PNG embedding needs libpng build [assumed from documented dependency]. **PDFium** — BSD-3/Apache-2.0 [verified: LICENSE], weekly prebuilt binaries (bblanchon) [verified]; has a writing API. **FF_PDFium** — UE5 plugin wrapping PDFium, pushed 2026-07-03, but **Business Source License 1.1** (free non-commercial, restricted commercial) [verified: LICENSE]. Fab plugins found are PDF viewers, not writers [verified]. Practical path: link libharu or PDFium yourself. (JPEG embeds nearly byte-for-byte in PDF via DCTDecode if a zero-dependency writer is ever wanted [inferred from spec, not prototyped].)
- **(b) OS print dialog from UE on Windows**: feasible — `ShellExecuteEx` with the `print` verb; `PrintDlg` for a real dialog [verified: MS docs]; platform-guarded Win32 in UE is standard practice [inferred; no shipping example checked].
- **(c) Export PNG/PDF and tell the user to print it: acceptable — and the correct v1.** Users printing a 1:50,000 sheet care about scale accuracy, which the OS print pipeline mangles unless printed at 100% with known DPI. A PDF sized exactly A4/A3 at 300 DPI (libharu sets page size in points) protects scale better than any in-app dialog — every PDF viewer has a competent print flow. PNG-only is one notch worse but fine for a training aid. **Recommendation: PNG first, libharu PDF second, OS print dialog never.**

## 7. High-res capture

- `HighResShot` renders the 3D scene up to 16× or explicit dimensions but **does not capture Slate/UMG at all** — long-documented [verified: forum + doc]. `FScreenshotRequest` with UI flags captures at screen resolution only. High multipliers break screen-space effects [verified: guide]. **Irrelevant to the map sheet — the render-target pipeline is strictly better** (exact DPI, no viewport). Keep for AAR beauty shots.

---

## Recommended stack

- **(a) HUD/mil reticle**: UMG; static reticle as texture or material (mil ticks via material math); dynamic elements via OnPaint DrawLines in a C++ SLeafWidget if Blueprint paint gets slow; static parts in an Invalidation Box.
- **(b) Menus/comms log/AAR**: plain UMG (ListView for the log, rich text). SDF fonts only where text scales. No CEF. If genuinely mourning CSS: evaluate VaCuus before CEF.
- **(c) Screen map**: UCanvasRenderTarget2D at ~display resolution, Canvas calls ported nearly line-for-line; redraw on pan/zoom/change; display via UMG Image brush; interactive markers as UMG widgets on top (hit-testing stays in UMG).
- **(d) Printable sheet + export**: same map-draw code at 2480×3508 / 3508×4961 — one code path, two scales — then ExportRenderTarget → PNG. v1: "print at 100%." v1.1: libharu PDF. Skip OS print dialog.
- Debug only: VesCodes/ImGui or SlateIM.

## Top risks

1. Canvas text fidelity at 300 DPI [assumed — the only unproven link; prototype first].
2. FF_PDFium's BSL 1.1 if commercializing later — link PDFium/libharu directly instead.
3. Slate paint iteration speed — C++ recompiles vs browser refresh; the cost is dev time, not capability.
4. CEF anywhere = version churn — avoid for panels.
5. No SVG runtime — any vector assets become textures/draw code.

## Verdict on "Unreal is weak/painful for precise 2D"

**Mostly refuted for this workload, one conceded point.** The claim holds for retained-mode vector documents (no runtime SVG, no CSS, no maintained vector plugin, thin Slate docs — verified pain). But SHITFIRE's heavy piece is immediate-mode canvas drawing, and UE's Canvas render target API is a near line-for-line port target with an 8K ceiling covering A3@300 DPI and built-in PNG export. Precision isn't the problem (the whole editor is Slate); ergonomics and iteration speed are the residual tax. "Painful" downgrades to "more C++, fewer conveniences, every required capability exists and is maintained by Epic itself."

Sources: UWidgetBlueprintLibrary::DrawLines · FSlateDrawElement::MakeLines · Slate architecture doc · UMG optimization/Invalidation docs · SDF text doc + forum 2268003 · blurry-text thread 430831 · CanvasRenderTarget2D API · unreal.Canvas API · KismetRenderingLibrary · render-target max-res thread 652106 · ExportRenderTarget · Motion Design SVG importer · SvgBooga · Pandoa SVGPlugin · dannymcgee.dev Skia series · thorvg · imgui issue #9122 · VesCodes/ImGui · IDI-Systems/UnrealImGui · SlateIM · GoaLitiuM/RmlUi · VaCuus · CEF thread 2617726 · BLUI · libharu · PDFium LICENSE · pdfium-binaries · FF_PDFium · Fab PDF-viewer listing · Win32 print verb · screenshots doc + thread 347903 · irendering.net guide
