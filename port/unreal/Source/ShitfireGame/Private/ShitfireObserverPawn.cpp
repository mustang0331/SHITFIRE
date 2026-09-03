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
	// r.DefaultFeature.AutoExposure=False: manual metering with explicit
	// physical camera values - ISO 100, f/8, 1/125 s => EV100 ~ 13, the
	// textbook daylight exposure for the 75k-lux sun the world actor
	// spawns. No auto-exposure surprises, ever.
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 0.0f;
	Camera->PostProcessSettings.bOverride_CameraISO = true;
	Camera->PostProcessSettings.CameraISO = 100.0f;
	Camera->PostProcessSettings.bOverride_CameraShutterSpeed = true;
	Camera->PostProcessSettings.CameraShutterSpeed = 125.0f;
	Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
	Camera->PostProcessSettings.DepthOfFieldFstop = 8.0f;
}
