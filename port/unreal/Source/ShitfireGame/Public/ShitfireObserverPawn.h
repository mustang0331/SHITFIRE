// Placeholder observer pawn - a camera on a stick. The real observer (tower
// position, binos, lasing UI) is the P4 observer/bridge wave's job; this pawn
// exists so the world is inspectable and the perf sweep has a camera.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ShitfireObserverPawn.generated.h"

class UCameraComponent;

UCLASS()
class SHITFIREGAME_API AShitfireObserverPawn : public APawn
{
	GENERATED_BODY()

public:
	AShitfireObserverPawn();

private:
	UPROPERTY(VisibleAnywhere, Category = "Shitfire")
	TObjectPtr<UCameraComponent> Camera;
};
