#include "ShitfireObserverPawn.h"
#include "Camera/CameraComponent.h"

AShitfireObserverPawn::AShitfireObserverPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = true;

	// Belt-and-suspenders exposure clamp on top of the project-wide
	// r.DefaultFeature.AutoExposure=False: manual metering, fixed bias.
	// No auto-exposure surprises, ever. Bias value is a stylized-scale
	// starting point pending an eyeball pass.
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 1.0f;
}
