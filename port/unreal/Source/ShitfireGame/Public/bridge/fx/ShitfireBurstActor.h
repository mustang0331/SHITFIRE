// AShitfireBurstActor - one HE impact's visual story (BUILDER-BRIDGE, P3/P4):
//   flash    ~0.25 s emissive blob, gone almost immediately;
//   dirt     dark column thrown up over ~1.2 s, faded out by ~7 s;
//   smoke    THE training feature: a persistent column ~60 m tall that lives
//            for minutes and leans/drifts subtly downwind, so the observer
//            can re-find the impact point to adjust (mandate: visible
//            1,500-3,200 m through the 15/8.6/4.3 deg binos FOVs).
//
// WHY NOT NIAGARA (deliberate deviation, verified 2026-09-03 on the installed
// 5.8 engine): stock UE 5.8 exposes NO headless authoring surface for Niagara
// graphs - NiagaraEditor has no Python/Blueprint-callable authoring API (grep:
// only the private Baker library), and UNiagaraSystemFactoryNew's SystemToCopy/
// EmittersToAddToNewSystem knobs are plain C++ members Python cannot set.
// Hand-authoring in the GUI editor is banned by the binary-asset tripwire
// (UNREAL_RESEARCH.md 8, risk 1). So the P3/P4 burst FX are C++-constructed
// from engine primitives + script-generated materials (Tools/gen_fx_assets.py,
// the gen_assets.py regime) - every size, lifetime and color lives in text.
// A Niagara upgrade can come later through an editor-in-the-loop session.
//
// All animation here runs on ENGINE time (presentation only - the sim clock
// never reads engine time; sim_core.h contract). Scale note: at 3,200 m
// through the 4.3 deg FOV a 60 m column subtends ~1.1 deg = a quarter of the
// screen height - sizes below verified by screenshot, not editor eyeball.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShitfireBurstActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class SHITFIREGAME_API AShitfireBurstActor : public AActor
{
	GENERATED_BODY()

public:
	AShitfireBurstActor();

	/** Downwind drift, UE-frame unit direction + m/s (from the sim's
	    WorldLayout::Wind at spawn - the same wind the FDC brief reads).
	    StartAgeS pre-ages the effect (FX probe screenshots: a full-height
	    smoke column without the 25 s real-time grow wait). */
	void InitBurst(const FVector& DownwindUE, double WindSpeedMps,
	               double StartAgeS = 0.0);

	virtual void Tick(float DeltaSeconds) override;

	// ---- tuning (meters / seconds; UE units derived in code) -------------
	static constexpr double FlashLifeS = 0.25;
	static constexpr double FlashRadiusM = 5.0;
	static constexpr double DirtLifeS = 7.0;
	static constexpr double DirtGrowS = 1.2;
	static constexpr double DirtRadiusM = 5.0;
	static constexpr double DirtHeightM = 22.0;
	static constexpr double SmokeLifeS = 300.0;   // "lives for minutes"
	static constexpr double SmokeFadeStartS = 240.0;
	static constexpr double SmokeGrowS = 25.0;    // column reaches full height
	static constexpr double SmokeHeightM = 60.0;  // mandate: ~60 m tall
	static constexpr double SmokeRadiusM = 4.5;   // per segment, widens upward
	static constexpr double SmokeMaxLeanM = 16.0; // downwind cap at the top

protected:
	virtual void BeginPlay() override;

private:
	UStaticMeshComponent* MakePart(const TCHAR* Name, const TCHAR* MeshPath,
	                               const TCHAR* MaterialPath,
	                               const struct FShitfireVis& Vis,
	                               TObjectPtr<UMaterialInstanceDynamic>& OutMid);

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Flash;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Dirt;
	// Three stacked segments approximate a leaning plume: lower anchored,
	// upper segments offset progressively downwind (chimney-smoke read).
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SmokeSegments;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlashMid;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DirtMid;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SmokeMids;

	FVector Downwind = FVector::ZeroVector;   // UE unit dir, horizontal
	double WindSpeed = 0.0;                   // m/s
	double AgeS = 0.0;
};
