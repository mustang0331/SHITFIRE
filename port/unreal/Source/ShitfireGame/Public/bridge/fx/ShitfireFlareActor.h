// AShitfireFlareActor - illumination round (BUILDER-BRIDGE, P3/P4): a
// descending point of light + emissive glow blob, burning for the sim-given
// duration (IllumFlare presentation event num0, 08-burst-optics.js:514-526;
// spawn height y = max(H,0)+330 comes IN the event position). Same
// C++-constructed regime as AShitfireBurstActor (see its header for the
// verified why-not-Niagara note).
//
// The point light is the functional part: illum exists to LIGHT THE GROUND.
// Intensity is physical-ish (inverse-square candela) so it reads under the
// pawn camera's fixed manual exposure (EV100 ~13, no auto-exposure).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShitfireFlareActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class SHITFIREGAME_API AShitfireFlareActor : public AActor
{
	GENERATED_BODY()

public:
	AShitfireFlareActor();

	/** BurnSeconds = presentation num0 (sim-authored burn time). */
	void InitFlare(double BurnSeconds);

	virtual void Tick(float DeltaSeconds) override;

	// ---- tuning ----------------------------------------------------------
	static constexpr double GlowRadiusM = 3.0;      // emissive blob
	static constexpr double DescentMps = 4.5;       // parachute rate
	static constexpr double LightIntensityCd = 6.0e5;   // ~real illum order
	static constexpr double LightRangeM = 1600.0;
	static constexpr double SwayAmpM = 6.0;         // slow pendulum drift
	static constexpr double SwayPeriodS = 9.0;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Glow;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMid;

	double BurnS = 30.0;
	double AgeS = 0.0;
	FVector SpawnUE = FVector::ZeroVector;
};
