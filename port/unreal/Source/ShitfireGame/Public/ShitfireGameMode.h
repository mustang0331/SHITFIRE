// P3 game mode - spawns AShitfireWorldActor from C++ (the map is
// near-empty by rule), parks the observer pawn on the OP tower looking at
// the target area, and drives the -ShitfireBench fixed camera sweep for the
// performance snapshot (CSV profiler + screenshot + quit).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShitfireGameMode.generated.h"

class AShitfireWorldActor;

UCLASS()
class SHITFIREGAME_API AShitfireGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShitfireGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<AShitfireWorldActor> WorldActor;

	// -ShitfireBench state: warmup 5 s -> CsvProfile 30 s sweep -> shot -> quit.
	bool bBench = false;
	double BenchT = 0.0;
	bool bCsvStarted = false;
	bool bCsvStopped = false;
	bool bShotTaken = false;
	float BaseYaw = 0.f;
	float BasePitch = 0.f;
};
