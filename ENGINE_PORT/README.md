# ENGINE_PORT — Plan and Strategy for Moving SHITFIRE to a Game Engine

This folder is the complete plan for porting the browser trainer (`../index.html`) to a real game engine. It is planning only — no engine code lives here yet.

## The strategy in six sentences

About **half of SHITFIRE is engine-agnostic domain logic** — ballistics, the observer-target correction math, the call-for-fire parser, the FDC rule engine, seeded scenario generation, star grading — and that half is the hard, valuable half. The other half is browser presentation (Three.js scene, canvas map sheets, Web Speech, Web Audio, DOM UI) that an engine simply does better. So the port is not a rewrite: it is **an extraction of the sim core into a plain, engine-free, unit-tested library, followed by a fresh presentation layer on top of it**.

The move that makes this safe is that the sim is **fully deterministic** — a seed plus a command sequence yields exactly one outcome — so the existing browser build becomes a **golden-master oracle** the port can be tested against automatically, to the metre. Correctness is therefore proven headless (stages P1–P2) *before* anything is rendered, because debugging ballistics through a rendering layer is far harder than debugging it in a test runner.

Recommended engine: **Godot 4 with C#**, because this product's real burdens are dense 2D instrument UI and porting a body of pure logic that must stay testable — precisely where Godot is strongest and Unreal is weakest.

## Read in this order

| Document | What it answers |
|---|---|
| [ENGINE_CHOICE.md](ENGINE_CHOICE.md) | Which engine, on what criteria, and the risks that follow regardless of choice (speech I/O, print/PDF, loss of zero-install distribution) |
| [ARCHITECTURE.md](ARCHITECTURE.md) | The line-by-line inventory of what transfers vs. what gets rebuilt, the target module structure, and the boundary rules that keep the core clean |
| [PARITY_TESTING.md](PARITY_TESTING.md) | How to prove the port is correct rather than hoping it is — three levels of automated parity against the browser oracle |
| [PORT_PLAN.md](PORT_PLAN.md) | Staged migration P0–P8 with objective exit criteria per stage |
| [CONTENT_INVENTORY.md](CONTENT_INVENTORY.md) | Every piece of content the port must carry: campaign chapters and seeds, scenario templates, world features, dialogue pools, tunables, and what must be authored fresh |
| [reference/](reference/) | The engine-agnostic specifications that transfer unchanged — doctrine, campaign narrative, training objectives |

## The three rules that matter most

1. **`SHITFIRE.Core` never references the engine.** If it cannot compile and run in a bare console app, the boundary has been violated and the parity tests stop being possible.
2. **Presentation never decides doctrine.** The UI renders what the core reports; it does not judge whether a call is valid, what a correction means, or how many stars a run earns.
3. **Determinism is a contract.** Same seed, same commands, same impacts — everywhere, forever. It is the property that makes the whole testing strategy work; protect it above any feature.

## Before writing a single line of engine code

Do stage **P0** — record the transcript library and export the fixtures while the browser build is the only implementation and is known-good. Skipping it is the largest avoidable risk in the plan: without a captured oracle, "does the port behave correctly?" reverts to somebody's memory of how it used to feel.

## What is deliberately not in scope

Porting is not the time to add features. Close air support (Volume V), smoke and illumination missions, and the pending Epilogue chapters all wait until parity is reached. The single forward-looking constraint: do not bake mouse-look assumptions into the observer camera, so XR stays possible later.

## Relationship to the browser build

`../index.html` is not retired when the port ships. It stays alive as the parity oracle and as the zero-install distribution channel — one file, any machine with a browser, no installation permission required. For a training tool that property may outrank graphics. Where the two implementations diverge, the documents in [reference/](reference/) are the source of truth, not either implementation.
