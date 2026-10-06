// FShitfireUnitAssets - THE one place unit art is named (BUILDER-CHARACTERS,
// P4). Every mesh/anim/material path the unit presentation consumes lives in
// this header; swapping a better soldier pack in later (the mandate's paid-
// pack seam) means editing the paths + heights below and NOTHING else - the
// logic in ShitfireUnitsActor only ever touches the loaded bundle.
//
// Current tier ($0, port/unreal/Tools/asset_manifest.json + port/CREDITS.md):
//   troops/squad/civs  Quaternius "Character Soldier" (CC0) - the skeletal
//                      body plus its own imported animation clips (the GLB
//                      shipped rigged AND animated, so the clips are already
//                      on the in-game skeleton - no IK retargeting needed;
//                      a paid pack with its own skeleton swaps in the same
//                      way: mesh + clips on ITS skeleton, paths edited here).
//   convoy             Humvee (madtrollstudio, CC-BY) + M939 (J-Toastie,
//                      CC-BY); strongpoint statics Quaternius Tank (CC0);
//                      battery emplacement Quaternius Cannon (CC0).
// Anything that fails to load degrades to the P3 placeholder boxes - the
// mission never breaks over a missing asset, it just gets uglier.
#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UStaticMesh;
class UAnimSequence;
class UMaterialInterface;

struct FShitfireUnitAssets
{
	// ---- humanoid (one body serves troops / squad / civs; tint differs) --
	USkeletalMesh* SoldierMesh = nullptr;
	UStaticMesh* SoldierHead = nullptr;      // rigid part; attached to the head bone
	UAnimSequence* AnimIdle = nullptr;
	UAnimSequence* AnimRun = nullptr;
	UAnimSequence* AnimDeath = nullptr;

	// ---- vehicles / emplacements -----------------------------------------
	UStaticMesh* HumveeMesh = nullptr;
	UStaticMesh* TruckMesh = nullptr;
	UStaticMesh* TankMesh = nullptr;
	UStaticMesh* TankTurret = nullptr;       // separate GLB part, same-origin attach
	UStaticMesh* CannonMesh = nullptr;
	UStaticMesh* CannonWheels = nullptr;     // separate GLB part, same-origin attach

	// ---- materials --------------------------------------------------------
	UMaterialInterface* UnitBodyMat = nullptr;   // /Game/Units tintable (gen_unit_assets.py)
	UMaterialInterface* PlaceholderMat = nullptr; // /Game/Materials/M_Placeholder
	UMaterialInterface* FxFlashMat = nullptr;    // /Game/FX (BUILDER-BRIDGE, reused read-only)
	UMaterialInterface* FxDirtMat = nullptr;
	UMaterialInterface* FxSmokeMat = nullptr;

	// ---- engine primitives ------------------------------------------------
	UStaticMesh* Cube = nullptr;
	UStaticMesh* Sphere = nullptr;

	bool bHumanoidReal = false;   // SoldierMesh loaded - else box figures
	bool bAnimsReal = false;      // all three clips loaded - else pose-only tier

	/** LoadObject everything above; logs one line per missing asset. A
	    humanoid that fails HumanoidImportSane() is REJECTED (Error log) and
	    the bundle degrades to the box tier - never a 100x figure. */
	static FShitfireUnitAssets Load();

	/** Import-sanity gate for the humanoid body (2026-10-06 bench frame: the
	    Interchange glTF import of the FBX2glTF soldier left the vertex buffer
	    at 272 x 222 x 69 METRES lying along -Y, and a ref-pose Root bone scale
	    of 170.6 that every SnapToTargetIncludingScale attachment inherits -
	    the "wreck pile" on the beach and the helmet in the sky). True only
	    when the vertex bounds fit a SoldierNativeHeightCm rig and the root
	    bone carries no scale. OutWhy names the failed check. */
	static bool HumanoidImportSane(const USkeletalMesh* Mesh, FString& OutWhy);

	/** The body asset path Load() uses (tests call the gate on it directly). */
	static const TCHAR* SoldierBodyPath();

	// Browser figure heights the silhouettes are scaled to (04-world.js:66 -
	// FIG.h: troop 2.40 m incl. cap, civ 2.08 m). The mil card deliberately
	// lists no man (figScale breaks man-ranging; 05:1284-1290), so scaling the
	// real model to these heights is safe and keeps browser-parity silhouettes.
	static constexpr double TroopHeightM = 2.40;
	static constexpr double CivHeightM = 2.08;

	// Soldier body native height, measured from the RENDERED rig (UNITCAL
	// probe log 2026-09-04: Head_end rel-z 16.7 cm at component scale 0.0835
	// = 200 cm native; Root/feet at origin, rig upright). The ASSET-claimed
	// bounds are unusable (272 x 222 x 69 m prop/bone grid in the GLB) - do
	// not fit scale from GetBounds(). A swapped-in pack re-measures this via
	// the -ShitfireUnitsProbe UNITCAL log.
	//
	// 2026-10-06: those "junk" bounds ARE the rendered geometry (UE Python
	// probe: Body bounds extent 13626 x 11119 x 3458 cm; SWAT alternate 8377
	// x 2750 x 1301 cm, same defect). The bones sit at 2 m, the vertices at
	// ~100x, so the asset cannot be fit by any scale - it must be re-imported
	// (Tools/import_assets.py, default AssetImportTask options). Until then
	// HumanoidImportSane() rejects it and the box tier draws the figures.
	static constexpr double SoldierNativeHeightCm = 200.0;
	// Gate limits: a 200 cm rig in T-pose spans ~270 cm; anything past 3x the
	// rig height is an import defect, not a tall soldier. Root-bone scale is
	// 1.0 on a clean import; 1.5 leaves room for unit-conversion noise only.
	static constexpr double HumanoidBoundsSlack = 3.0;
	static constexpr double HumanoidMaxRootScale = 1.5;

	// Frozen mil-relation dimensions - the mil card teaches truck 5 m and the
	// range formula divides by them (05:6-13). Vehicle meshes are uniformly
	// scaled so the long axis matches; DO NOT restyle these numbers.
	static constexpr double VehicleLengthM = 5.0;   // browser Box(5.0, 2.1, 2.3)
	static constexpr double BatteryGunLengthM = 5.8; // browser gun Box(1.6,1.4,5.8)
};
