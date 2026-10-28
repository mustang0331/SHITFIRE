// AShitfireUnitsActor - ALL unit presentation for one mission (BUILDER-
// CHARACTERS, P4): troops / friendly squad / civilians as skeletal soldiers,
// convoy + static vehicles, scenario huts / flag / bunker / KP posts, the OP
// watchtower, the friendly battery (Cannon models), muzzle flashes, smoke
// puffs, vehicle burn flames, and dust ribbons behind moving vehicles.
//
// Spawned by AShitfireWorldActor::SpawnPlaceholders (the sanctioned handoff:
// the world actor's unit-placeholder boxes are gone; world features - village
// huts, facilities, rocks, mast - stay with the world actor).
//
// AUTHORITY SPLIT (the whole point):
//   - POSITIONS/STATE come from the core every tick: UnitLayout (placeUnits'
//     ported layout, PRNG stream #7), ScenarioState (convoy veh x/z, S.enemy
//     advance, kaiju wade, suppression window), WorldLayout civs (Lissajous
//     wander run by SimCore::advance), and the six unit-visual presentation
//     cues on UShitfireSimHost::OnPresentationCue. Nothing here re-simulates.
//   - The ONE deterministic presentation draw is the dispersal spread:
//     shitfire::sim::disperseOffsets (stream #11) called per visible figure
//     in mesh order, exactly the browser contract (sim/events.h note).
//   - Everything else animated here is sanctioned unseeded flavor
//     (DETERMINISM_NOTE.md section a): idle-anim phase, dust puff scatter,
//     the disperse scatter LERP (the browser teleports; we slide over ~1.6 s
//     to the same deterministic endpoints). Sim streams are never touched.
//
// Browser visual vocabulary ported 1:1 (05-units-scenario.js, 10-mission.js,
// 03-op-battery.js, 04-world.js - line-cited at each site below):
//   standing  upright, layout yaw            prone  lie-over 1.2 rad + 0.3 m
//   dead      lie-over 1.35 rad (or the Death clip frozen on its last frame)
//   convoy    core veh x/z + heading from pathDir, dead hull = blackened +
//             flicker flame (never tipped);  strongpoint statics tip 0.25
//   bunker    scorch color swap;  kaiju      scorch + settle (0.6 rad)
//   suppression's ONLY visual: the muzzle flashes stop (17-loop-boot.js:156)
//   legibility: figScale floor of 1.8 mils, x2.5 cap, off inside 800 m
//
// Collision is disabled on every component (world rule: the sim ray-march is
// the only geometry authority). All motion runs on engine/sim-clock reads -
// presentation only, never scored.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShitfireVis.h"
#include "bridge/ShitfireSimHost.h"
#include "units/ShitfireUnitAssets.h"
#include "ShitfireUnitsActor.generated.h"

namespace shitfire
{
	class SimCore;
}
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Figure pose vocabulary (browser rotation.z states, 10-mission.js). */
enum class EShitfireFigurePose : uint8
{
	Standing,
	Prone,   // disperse consequence (10:1261, rotation.z = 1.2)
	Dead,    // casualty (10:1372/:1379, rotation.z = 1.35)
};

UCLASS()
class SHITFIREGAME_API AShitfireUnitsActor : public AActor
{
	GENERATED_BODY()

public:
	AShitfireUnitsActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION()
	void HandlePresentationCue(EShitfirePresentation Kind, double SimT,
	                           FVector UEPos, double Num0);

private:
	// One humanoid figure: a skeletal soldier (or a placeholder box when the
	// asset is missing). Feet-origin semantics like the browser (04:62-64):
	// SimX/SimZ is where the boots stand; lift/lie-over applied on top.
	struct FFigure
	{
		USkeletalMeshComponent* Skel = nullptr;
		UStaticMeshComponent* Box = nullptr;      // fallback figure
		UStaticMeshComponent* Head = nullptr;     // rigid head part on the skel
		double SimX = 0, SimZ = 0;
		double YawSimRad = 0;
		EShitfireFigurePose Pose = EShitfireFigurePose::Standing;
		bool bVisible = false;
		double BaseHeightM = FShitfireUnitAssets::TroopHeightM;
		double MeshScale = 1.0;       // asset -> BaseHeightM fit (uniform)
		FVector GullScale = FVector::OneVector;   // chow flock (05:1250)
		// disperse scatter lerp (deterministic endpoints, flavored motion)
		bool bLerping = false;
		double LerpFromX = 0, LerpFromZ = 0, LerpToX = 0, LerpToZ = 0, LerpT = 0;
		// anim bookkeeping (single-node playback; no AnimBP - C++-only rule)
		int32 CurrentClip = -1;       // 0 idle / 1 run / 2 death
		double LastX = 0, LastZ = 0;  // movement estimate for run/idle pick
		double IdleRate = 1.0;        // unseeded flavor
	};

	// One scenario vehicle slot (units.vehicles[i], 05:17).
	struct FVehicle
	{
		UStaticMeshComponent* Mesh = nullptr;
		UStaticMeshComponent* Flame = nullptr;    // dead-hull flicker (05:853)
		bool bVisible = false;
		bool bDeadShown = false;                  // blackened material applied
		bool bTipped = false;                     // strongpoint DESTROYED (10:1383)
		double SimX = 0, SimZ = 0, YawSimRad = 0;
		double YawOffsetDeg = 0;                  // model long-axis -> +X
		double FitScale = 1.0;                    // long axis -> 5.0 m
		double ZLiftUE = 0;                       // bounds base -> ground
		FVector LayoutScale = FVector::OneVector; // wreck (2.2, 1.3, 1.6)
		double RotZSimRad = 0;                    // wreck list / tip
		double DustCooldown = 0;
	};

	// One dust puff behind a moving vehicle (research 09: "dust sells motion
	// better than wheels at 23 px"). Pooled; M_FxDirt MID, sand-tinted.
	struct FDustPuff
	{
		UStaticMeshComponent* Mesh = nullptr;
		UMaterialInstanceDynamic* Mid = nullptr;
		double Age = -1;                          // <0 = free
		FVector BaseUE = FVector::ZeroVector;
		FVector DriftUE = FVector::ZeroVector;
	};

	// ---- build ----------------------------------------------------------
	void BuildFigures();
	void BuildVehicles();
	void BuildStructures();
	void BuildTowerAndBattery();
	void BuildEffects();
	void ApplyInitialLayout();

	// Vis = the optic signature every slot must carry (ShitfireVis.h, ART-a).
	// With a null BaseMat the mesh's own (re-parented, tagged) materials stay.
	UStaticMeshComponent* MakeStatic(const TCHAR* Name, UStaticMesh* Mesh,
	                                 UMaterialInterface* BaseMat,
	                                 const FLinearColor& Tint,
	                                 const FShitfireVis& Vis,
	                                 UMaterialInstanceDynamic** OutMid = nullptr);
	// BodyVis/CapVis: the family's body and cap/hat signatures (04-world.js:
	// 121-126) - the pack's imported tag is the enemy's; squad and civs
	// override per slot so the three families stay distinguishable at night.
	void MakeFigure(FFigure& F, const TCHAR* Name, const FLinearColor& Tint,
	                bool bTintBody, double BaseHeightM,
	                const FShitfireVis& BodyVis, const FShitfireVis& CapVis);
	void FitVehicleMesh(FVehicle& V, UStaticMesh* Mesh, double TargetLengthM);

	// ---- per-frame ------------------------------------------------------
	void UpdateTroops(double SimNow, double Dt);
	void UpdateSquad(double SimNow, double Dt);
	void UpdateCivs(double Dt);
	void UpdateVehicles(double SimNow, double Dt);
	void UpdateBunker(double SimNow);
	void UpdateFlashes(double SimNow);
	void UpdatePuffs(double SimNow);
	void UpdateDust(double Dt);
	void LegibilityPass();

	void ApplyFigure(FFigure& F, double Dt);
	void SetFigureClip(FFigure& F, int32 Clip);
	void SpawnDustPuff(const FVehicle& V);

	// ---- state drives ---------------------------------------------------
	void OnDisperseTroops();
	void OnTroopsDown(bool bTwinElement);
	void OnVehiclesTipped();

	double GroundH(double X, double Z) const;
	FVector EyeUE() const;

	// ---- probe ----------------------------------------------------------
	// -ShitfireUnitsProbe: scripted screenshot pass through the binos FOVs
	// (8.6 / 4.3 deg vertical = 7x / 14x) at the real mission range: standing,
	// dispersing, prone, dead, a forced convoy run with dust on the real road
	// polyline, then the battery. Presentation forcing only - the sim is
	// never written. Same regime as the FX probe (ShitfireFxSubsystem).
	void StartUnitsProbe();
	void ProbeStep();
	void ForceConvoyDemo(double T);

	UPROPERTY() TObjectPtr<UShitfireSimHost> Host;
	shitfire::SimCore* Sim = nullptr;   // non-owning (subsystem outlives us)

	FShitfireUnitAssets Assets;

	TArray<FFigure> Troops;    // 8 (units.troops)
	TArray<FFigure> Squad;     // 6 (units.fSquad)
	TArray<FFigure> Civs;      // WORLD.civs
	TArray<FVehicle> Vehicles; // 4 (units.vehicles)

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Huts;       // 3
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FlagPole;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FlagBanner;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Bunker;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BunkerMid;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> KpPosts;    // 2
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TowerParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BatteryParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Flashes;    // 6
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Puffs;      // 4
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PuffMids;
	// GC keep-alive for everything referenced from the plain structs above.
	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;

	TArray<FDustPuff> DustPool;

	// puff anchor (strongpoint compound / chow bbq, 05:1118/1263)
	bool bPuffsOn = false;
	double PuffBaseX = 0, PuffBaseY = 0, PuffBaseZ = 0;

	// unit-cue latches (browser globals mirrored)
	bool bEnemyAliveVisual = true;    // false after TroopsDown/KaijuScorched
	bool bTwinDown = false;
	bool bDispersed = false;
	bool bBunkerScorched = false;
	bool bKaijuDead = false;

	// probe state
	bool bProbe = false;
	bool bDiagLogged = false;   // one-shot figure-calibration log
	int32 ProbeStage = 0;
	FTimerHandle ProbeTimer;
	double ProbeConvoyT = -1;         // >=0: convoy demo running
	FVector ProbeAimUE = FVector::ZeroVector;
};
