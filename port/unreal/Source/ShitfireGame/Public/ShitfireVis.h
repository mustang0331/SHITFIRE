// ShitfireVis - the optic-signature convention (row ART-a, PORT3).
//
// The browser never had an NVG or thermal shader. src/js/08-burst-optics.js
// swaps every material's colour for a per-mode variant, and the variant is
// read from a tag the material carries: `vis = {nvg, th}` (04-world.js:93-
// 159). nvg is the tube luminance 0..1, th the [r,g,b] thermal signature.
// Anything untagged falls back to its daytime luminance - the "silent
// luminance fallback" that the ART-a gate forbids in the engine, because a
// pack material without a tag would make figures un-countable at night and
// nothing would fail.
//
// The engine convention is therefore a PARAMETER contract, checked by test:
//   * every UMaterial under /Game declares scalar `VisNvg` + vector
//     `VisThermal`, wired into its colour output through the preview wrap
//     (Tools/vis_material.py); imported pack materials are instances of
//     /Game/Materials/M_ShitfireMaster (Tools/import_assets.py re-parents
//     them, so no Interchange material survives import);
//   * every primitive component a Shitfire actor spawns carries a tagged
//     material on every slot - AuditWorld() walks them and logs an Error per
//     offender (also `shitfire.VisAudit` on the console);
//   * SHITFIRE.Art.MaterialVisTag sweeps the asset registry
//     (Tests/ShitfireMaterialVisTagTest.cpp).
// The numbers below are the browser's, verbatim (file:line beside each), and
// Derived() reproduces the browser's fallback for the few colours it left
// untagged, so the port's own placeholder colours get a signature instead of
// the engine default material. The real NVG/thermal rig is P4.5f; until then
// `shitfire.VisMode 1|2` renders the flat signature (/Game/Materials/
// MPC_Vision `VisMode`) so a new pack's tagging can be checked by eye.
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UWorld;

struct FShitfireVis
{
	float Nvg = 0.5f;                                      // 0..1 tube brightness
	FLinearColor Thermal = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);   // rgb 0..1

	constexpr FShitfireVis() = default;
	constexpr FShitfireVis(float InNvg, float R, float G, float B)
		: Nvg(InNvg), Thermal(R, G, B, 1.0f) {}
	constexpr FShitfireVis(float InNvg, float Grey)
		: Nvg(InNvg), Thermal(Grey, Grey, Grey, 1.0f) {}
};

namespace ShitfireVis
{
	// ---- the contract (names shared with Tools/vis_material.py) -----------
	inline const TCHAR* ParamNvg = TEXT("VisNvg");
	inline const TCHAR* ParamThermal = TEXT("VisThermal");
	inline const TCHAR* CollectionPath = TEXT("/Game/Materials/MPC_Vision.MPC_Vision");
	inline const TCHAR* ModeParam = TEXT("VisMode");
	inline const TCHAR* MasterPath = TEXT("/Game/Materials/M_ShitfireMaster.M_ShitfireMaster");

	// ---- figures (04-world.js:121-127) --------------------------------------
	constexpr FShitfireVis EnemyBody  (0.16f, 1.00f);               // NIR-dark cloth
	constexpr FShitfireVis EnemyCap   (0.32f, 0.86f);
	constexpr FShitfireVis FriendBody (0.20f, 1.00f);
	constexpr FShitfireVis FriendCap  (1.00f, 0.28f, 1.00f, 1.00f); // IR beacon / CID panel
	constexpr FShitfireVis CivBody    (0.90f, 0.70f);               // undyed cotton
	constexpr FShitfireVis CivHat     (0.20f, 0.14f);               // straw insulates
	constexpr FShitfireVis Disc       (0.02f, 0.03f);

	// ---- world materials (04-world.js:128-159, 1064-1068; 02-scene.js) ---
	constexpr FShitfireVis HutCiv     (0.40f, 0.38f);   // village hut, cooking fire
	constexpr FShitfireVis HutCivB    (0.38f, 0.36f);
	constexpr FShitfireVis TownMasonry(0.78f, 0.46f);   // civM lime render
	constexpr FShitfireVis TownRoof   (0.30f, 0.34f);   // civR tile
	constexpr FShitfireVis Asphalt    (0.10f, 0.62f);   // sealed: NIR-black, holds heat
	constexpr FShitfireVis AsphaltEdge(0.34f, 0.30f);
	constexpr FShitfireVis Path       (0.52f, 0.24f);   // bare soil
	constexpr FShitfireVis Rock       (0.14f, 0.14f);
	constexpr FShitfireVis Canopy     (0.85f, 0.24f);   // NIR-bright foliage
	constexpr FShitfireVis Scatter    (0.50f, 0.27f);
	constexpr FShitfireVis Ocean      (0.06f, 0.22f);   // 02-scene.js:251
	constexpr FShitfireVis Shallows   (0.09f, 0.25f);   // 02-scene.js:263
	constexpr FShitfireVis Terrain    (0.55f, 0.14f);   // 02-scene.js:447

	// ---- units (05-units-scenario.js:70-76) --------------------------------
	constexpr FShitfireVis Vehicle    (0.14f, 0.90f);   // painted metal, hot engine
	constexpr FShitfireVis VehicleDead(1.00f, 1.00f);   // burning wreck
	constexpr FShitfireVis UnitHut    (0.40f, 0.34f);
	constexpr FShitfireVis Flash      (1.00f, 1.00f);   // muzzle flash
	constexpr FShitfireVis Flame      (1.00f, 1.00f);
	constexpr FShitfireVis Bunker     (0.30f, 0.30f);   // scorch/kaiju recolour keeps it
	constexpr FShitfireVis Post       (0.85f, 0.16f);   // painted KP marker

	// ---- burst FX (08-burst-optics.js:36-42, 400-557) ----------------------
	constexpr FShitfireVis BurstFlash (1.00f, 1.00f);
	constexpr FShitfireVis BurstRing  (0.95f, 0.95f);
	constexpr FShitfireVis BurstDust  (0.85f, 0.62f);   // hot for a few seconds
	constexpr FShitfireVis BurstPuff  (0.60f, 0.44f);
	constexpr FShitfireVis Debris     (0.20f, 0.40f);
	constexpr FShitfireVis Crater     (0.22f, 0.60f);
	constexpr FShitfireVis Wisp       (0.40f, 0.34f);
	constexpr FShitfireVis Screen     (0.60f, 0.30f);
	constexpr FShitfireVis Flare      (1.00f, 1.00f);   // illum
	constexpr FShitfireVis Beam       (1.00f, 1.00f);   // laser

	/** The browser's fallback for an UNTAGGED material, reproduced so the
	    port's own colours (tower, tent, flag, mast, dust) get a deliberate
	    signature: nvg = sRGB luminance (08-burst-optics.js:96, visLum over
	    the colour read in sRGB), thermal = the wash-0 cold grey
	    0.05 + 0.13 * lum (08:123). Pass the LINEAR colour the material gets. */
	SHITFIREGAME_API FShitfireVis Derived(const FLinearColor& LinearColor);

	/** Set both parameters on a dynamic instance. */
	SHITFIREGAME_API void Apply(UMaterialInstanceDynamic* Mid, const FShitfireVis& Vis);

	/** Create a dynamic instance of Base tagged with Vis (nullptr if !Base). */
	SHITFIREGAME_API UMaterialInstanceDynamic* MakeTagged(UMaterialInterface* Base,
		UObject* Outer, const FShitfireVis& Vis);

	/** True when the material (or its parent chain) declares both parameters.
	    A null material is untagged (that is the engine default material). */
	SHITFIREGAME_API bool IsTagged(const UMaterialInterface* Mat, FString* OutWhy = nullptr);

	/** Walk every primitive component in World; every material slot must be
	    tagged. Logs one Error per offender, one Log summary. Returns the
	    offender count; OutOffenders gets "Actor/Component[slot]: why". */
	SHITFIREGAME_API int32 AuditWorld(UWorld* World, TArray<FString>* OutOffenders = nullptr);

	/** Push `shitfire.VisMode` (0 day / 1 NVG preview / 2 thermal preview)
	    into MPC_Vision for World. Called by the console variable's change
	    callback for every game world; call it after spawning a world so a
	    mode set on the command line (-ExecCmds="shitfire.VisMode 1") lands. */
	SHITFIREGAME_API void ApplyPreviewMode(UWorld* World);
}
