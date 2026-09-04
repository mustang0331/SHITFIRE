// UShitfireFxSubsystem - binds the SimHost's presentation cues to spawned
// FX actors (BUILDER-BRIDGE, P3/P4). World subsystem: exists per game world,
// finds the GameInstance's UShitfireSimHost on world begin play, subscribes,
// spawns AShitfireBurstActor / AShitfireFlareActor at the cue positions
// (already UE-converted by the host via bridge/ShitfireCoords.h).
//
// Smoke-screen cues reuse the burst actor's persistent column (wider read
// comes with the real smoke mission pass); SunBeam stays raw-cue-only until
// the sunlamp chapter gets its own treatment (noted in the P3/P4 report).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "bridge/ShitfireSimHost.h"
#include "ShitfireFxSubsystem.generated.h"

UCLASS()
class SHITFIREGAME_API UShitfireFxSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION()
	void HandleBurstCue(double SimT, FVector UEPos);

	UFUNCTION()
	void HandleSmokeScreenCue(double SimT, FVector UEPos);

	UFUNCTION()
	void HandleIllumFlareCue(double SimT, FVector UEPos, double BurnSeconds);

private:
	/** Downwind (UE frame) + speed from the sim's WorldLayout wind. */
	void ReadWind();

	/** -ShitfireFxProbe: scripted screenshot pass - spawn a pre-aged burst +
	    flare at the scenario target area, then HighResShot through the three
	    binos FOVs (15 / 8.6 / 4.3 deg VERTICAL, converted to the horizontal
	    fov the camera manager takes), then quit. The mandate's "verify sizes
	    by screenshot, not editor eyeball" hook. */
	void StartFxProbe();

	UPROPERTY() TObjectPtr<UShitfireSimHost> Host;

	FVector DownwindUE = FVector::ZeroVector;
	double WindSpeedMps = 0.0;
	FTimerHandle ProbeTimer;
	int32 ProbeStep = 0;
	FVector ProbeTargetUE = FVector::ZeroVector;
};
