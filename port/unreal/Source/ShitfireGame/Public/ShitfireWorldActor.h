// AShitfireWorldActor - assembles the whole P3 world procedurally on
// BeginPlay from the sim core: island terrain mesh, road/path ribbons,
// ocean plane, sky/sun/fog, placeholder world features + units, vegetation,
// and the boot golden-ray self-test. The .umap stays near-empty; this actor
// is spawned from C++ by AShitfireGameMode (no level-baked state).
//
// TERRAIN HAS NO COLLISION, ON PURPOSE, FOREVER: the sim core's ray-march
// (shitfire::groundHit / hasLOS) is the only geometry authority - a UE trace
// against cooked terrain collision would be a second, subtly different LOS
// implementation, the classic silent grader-vs-laser mismatch
// (UNREAL_RESEARCH.md 4). Every component spawned here has collision
// disabled; a stray trace returns nothing rather than a plausible wrong hit.
#pragma once

#include "CoreMinimal.h"
#include "RealtimeMeshActor.h"
#include "ShitfireWorldActor.generated.h"

namespace shitfire
{
	class SimCore;
	class HeightField;
}
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class URealtimeMeshProcedural;

UCLASS()
class SHITFIREGAME_API AShitfireWorldActor : public ARealtimeMeshActor
{
	GENERATED_BODY()

public:
	AShitfireWorldActor();

	virtual void BeginPlay() override;

	/** OP tower eye (03-op-battery.js eyeFor) in UE space - pawn placement. */
	FVector ObserverEyeUE() const;
	/** Scenario enemy centroid in UE space (ground level) - bench camera aim. */
	FVector TargetAreaUE() const;

	bool GoldenRaysPassed() const { return bGoldenRaysPassed; }
	shitfire::SimCore* GetSimCore() const { return Sim; }

	/** Chapter to assemble ('1.1' default). Its volume tseed overrides TerrainSeed. */
	UPROPERTY(EditAnywhere, Category = "Shitfire")
	FString ChapterId = TEXT("1.1");

	/** Fallback terrain seed when the chapter has no volume tseed. */
	UPROPERTY(EditAnywhere, Category = "Shitfire")
	double TerrainSeed = 1337.0;

private:
	void InitSimCore();
	void BuildTerrain(URealtimeMeshProcedural* Mesh);
	void BuildRibbonSection(URealtimeMeshProcedural* Mesh, int32 SectionIndex,
	                        const TArray<TArray<FVector2D>>& PolylinesXZ,
	                        double WidthMeters, double LiftMeters);
	void SpawnOcean();
	void SpawnSkyAndLight();
	void SpawnPlaceholders();
	void SpawnVegetation();
	void RunGoldenRaySelfTest();

	UInstancedStaticMeshComponent* MakeInstancedClass(const TCHAR* Name,
	                                                  const TCHAR* MeshPath,
	                                                  const FLinearColor& Color);
	UMaterialInstanceDynamic* MakePlaceholderMID(const FLinearColor& Color);

	/** H(x,z) through the linked core lib (meters, sim frame). */
	double GroundH(double X, double Z) const;

	// NON-OWNING cache of UShitfireSimHost's SimCore (BUILDER-BRIDGE refactor
	// of the 587e630 InitSimCore ownership - the GameInstance subsystem is
	// the one owner now; it outlives this actor, so the cache never dangles).
	shitfire::SimCore* Sim = nullptr;
	bool bGoldenRaysPassed = false;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> OceanPlane;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> InstancedClasses;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlaceholderBaseMat;
};
