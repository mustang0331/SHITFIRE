// P3 game mode - owns nothing but defaults; the world is assembled by
// AShitfireWorldActor spawned from C++ (no level-baked logic, no Blueprint).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShitfireGameMode.generated.h"

UCLASS()
class SHITFIREGAME_API AShitfireGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShitfireGameMode();

	virtual void BeginPlay() override;
};
