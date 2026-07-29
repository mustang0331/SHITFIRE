# ENGINE_CHOICE.md — Which Engine, and Why

**Recommendation: Godot 4.x with C#.** Unity is the defensible alternative if institutional distribution or XR breadth becomes a hard requirement. Unreal is the wrong tool for this specific product.

This document exists so the decision is made once, on stated criteria, and not re-litigated halfway through the port.

## What this product actually demands

SHITFIRE is not a shooter that happens to have menus. Ranked by how much engine support they need:

1. **Dense, precise 2D UI over a 3D world** — mil reticle, comms log, mission menu, AAR panels, and two full map renderers (screen map + printable 1:50,000 topographic sheet with contours, marginalia, and a legend). This is the single heaviest UI burden in the project and the most common reason a port stalls.
2. **Deterministic simulation** — seeded PRNG driving terrain, scenarios, and dispersion. Determinism is what makes the campaign reproducible and the parity testing in [PARITY_TESTING.md](PARITY_TESTING.md) possible. The engine must not force nondeterminism into the sim path.
3. **Heightfield terrain with cheap ray queries** — `H(x,z)` plus ray-march for impacts, laser ranging, and line-of-sight. No physics engine is used or wanted.
4. **Speech in and out** — push-to-talk recognition and a synthesized FDC voice. **No engine provides this natively.** It is a platform problem in every candidate (see Risks).
5. **Document output** — printable map sheets are a real training feature, not decoration. Browsers give this away free; engines do not.
6. **Deliberately low-poly, flat-shaded art** — the visual bar is *low*, and that inverts the usual engine calculus. Rendering horsepower is nearly irrelevant here.

Note what is absent: no networking, no character animation, no physics simulation, no photorealism, no large streaming world. The features that justify a heavyweight engine are the features this product does not use.

## Evaluation

| Criterion | Godot 4 | Unity 6 | Unreal 5 |
|---|---|---|---|
| 2D/UI system for dense instrument panels | **Strong** — Control nodes are first-class, theming is straightforward | Good — UI Toolkit is capable but a second system to learn | Weak for this — UMG is heavy and fights precise 2D vector work |
| Custom 2D vector drawing (contours, reticle, map sheet) | **Strong** — `_draw()` / `CanvasItem` maps almost line-for-line onto the existing canvas code | Workable — needs `Mesh`/`VectorGraphics` or a texture-blit approach | Painful |
| Deterministic sim control | **Strong** — easy to keep sim off the frame clock | Strong | Strong but the engine is opinionated |
| Heightfield + custom ray queries | Fine — plain code, no physics needed | Fine | Fine |
| Language fit for porting ~1,900 lines of JS logic | **C# is the closest sane target**; GDScript also viable | C# — identical benefit | C++/Blueprint — highest friction |
| Headless/unit testing of the sim core | **Strong** — a plain C# class library tests with zero engine | Good (needs care to keep logic out of `MonoBehaviour`) | Poor |
| Print/PDF export | Weak natively — needs a library or an external step | Weak — same | Weak — same |
| Speech I/O | None native — platform/plugin work | None native — more prebuilt options exist | None native |
| Licensing / cost | **MIT, free, no revenue terms, no seat cost** | Free tier with terms; policy history is a governance risk | Royalty model above threshold |
| Distribution to a barracks laptop | Small binary, trivial | Larger, fine | Very large |
| Team ramp for a solo/small dev | **Low** | Moderate | High |

## The decision

**Godot 4 + C#** wins because the two things that dominate this port — *precise custom 2D UI and drawing*, and *porting a body of pure logic that must stay testable and deterministic* — are exactly where Godot is strongest and where Unreal is weakest. C# is chosen over GDScript because the existing logic is JavaScript: static typing catches porting errors that would otherwise surface as subtly wrong ballistics, and a plain C# class library can be unit-tested with no engine in the loop, which the parity strategy depends on.

Godot's real weaknesses — a smaller asset store and thinner XR tooling — do not bite a product with no purchased assets and no current XR requirement.

**Choose Unity instead if** any of these become firm: the trainer must ship to Quest or another XR headset as a primary target; an institution mandates a supported commercial engine; or you need to hire from a large pool of contract developers on short notice. The port plan and architecture in this folder are written to survive that switch — the sim core is engine-independent by design, so changing engines costs the presentation layer only.

**Do not choose Unreal** unless the product's identity changes from "training tool" to "high-fidelity visual simulation." Then the calculus flips entirely and much of this folder should be rewritten.

## Risks that are independent of engine choice

These follow the product regardless of which engine is picked, and each needs an owner before the port starts:

- **Speech recognition.** The browser's Web Speech API is free, keyless, and gone the moment you leave the browser. Options: bundle an offline recognizer (Vosk is small and license-friendly; Whisper is better but heavier), call a cloud STT service (adds keys, cost, and a network dependency the current design deliberately avoids), or use platform APIs (Windows `System.Speech`, which is Windows-only). **Recommendation: Vosk offline with a small command grammar**, because the vocabulary is tiny and doctrinal — the parser only needs numbers, directions, and about forty prowords. A constrained grammar will beat general-purpose dictation on exactly this input.
- **Speech synthesis.** `SpeechSynthesis` disappears too. Options: platform TTS (Windows SAPI via `System.Speech`), a bundled engine (Piper is small and good), or — the option worth serious consideration — **pre-recorded voice lines**. The FDC's dialogue is a finite, authored set of pools; recording them yields a dramatically better HELLHOUND than any TTS, and the quip pools are already structured as discrete lines. The cost is losing dynamic readback of arbitrary grids, which can be solved by splicing recorded digits.
- **Printable map sheets.** Losing the browser print dialog is a genuine regression. Plan: render the sheet to a high-resolution image in-engine and either write a PDF with a small library (e.g. `PdfSharpCore`) or hand off to the OS print dialog. **This must be scheduled, not assumed** — it is a graded training feature per [reference/TLO.md](reference/TLO.md) ELO F.2.
- **The single-file, zero-install property dies.** Today the trainer is one HTML file that runs on any machine with a browser and no permission from anyone. A game engine build is an executable that some environments will not permit. If "runs on a locked-down barracks laptop" is a real requirement, keep the browser version alive as the low-friction distribution channel (see [PORT_PLAN.md](PORT_PLAN.md), "Keeping the browser build alive").

## What is explicitly out of scope for the port

Porting is not the moment to add features. Close air support (Volume V), smoke and illumination missions, and multiplayer stay out until the port reaches parity. The one exception worth naming: the port should not *preclude* XR, so avoid baking mouse-look assumptions into the observer camera.
