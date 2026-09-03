#include "ShitfireGameMode.h"
#include "ShitfireGame.h"
#include "ShitfireObserverPawn.h"
#include "ShitfireWorldActor.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/math/prng.h"

AShitfireGameMode::AShitfireGameMode()
{
	DefaultPawnClass = AShitfireObserverPawn::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AShitfireGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Linkage proof: one draw through the prebuilt core lib. Seed 1337's
	// first double is 0.1844118325971067 in the oracle fixture; the real
	// gates are SHITFIRE.Parity.Smoke and the boot golden-ray self-test.
	shitfire::Mulberry32 Rng(1337u);
	UE_LOG(LogShitfire, Log, TEXT("shitfire_core linked: mulberry32(1337) first draw = %.17g"),
		Rng.next());

	// The whole world comes from C++ - the .umap holds nothing.
	WorldActor = GetWorld()->SpawnActor<AShitfireWorldActor>();
	if (!WorldActor)
	{
		UE_LOG(LogShitfire, Error, TEXT("failed to spawn AShitfireWorldActor"));
		return;
	}

	// Park the observer on the OP tower, facing the target area.
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && PC->GetPawn())
	{
		const FVector Eye = WorldActor->ObserverEyeUE();
		const FVector Target = WorldActor->TargetAreaUE();
		PC->GetPawn()->SetActorLocation(Eye, false, nullptr, ETeleportType::TeleportPhysics);
		const FRotator LookAt = (Target - Eye).Rotation();
		PC->SetControlRotation(LookAt);
		BaseYaw = LookAt.Yaw;
		BasePitch = LookAt.Pitch;
		UE_LOG(LogShitfire, Log, TEXT("observer at eye (%s) looking at (%s)"),
			*Eye.ToCompactString(), *Target.ToCompactString());
	}

	bBench = FParse::Param(FCommandLine::Get(), TEXT("ShitfireBench"));
	if (bBench)
	{
		UE_LOG(LogShitfire, Display, TEXT("bench mode: 5 s warmup, 30 s CSV sweep"));
	}
}

void AShitfireGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBench)
	{
		return;
	}
	BenchT += DeltaSeconds;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	if (BenchT >= 5.0 && !bCsvStarted)
	{
		bCsvStarted = true;
		PC->ConsoleCommand(TEXT("CsvProfile Start"));
	}
	if (bCsvStarted && !bCsvStopped)
	{
		// Deterministic sweep: +/-15 deg yaw around the target bearing over 30 s.
		const double T = FMath::Clamp((BenchT - 5.0) / 30.0, 0.0, 1.0);
		const float Yaw = BaseYaw + 15.f * FMath::Sin(T * 2.0 * PI);
		PC->SetControlRotation(FRotator(BasePitch, Yaw, 0.f));
		if (BenchT >= 35.0)
		{
			bCsvStopped = true;
			PC->ConsoleCommand(TEXT("CsvProfile Stop"));
		}
	}
	if (bCsvStopped && !bShotTaken && BenchT >= 37.0)
	{
		bShotTaken = true;
		PC->SetControlRotation(FRotator(BasePitch, BaseYaw, 0.f));
		PC->ConsoleCommand(TEXT("HighResShot 1920x1080 filename=ShitfireBench"));
	}
	if (bShotTaken && BenchT >= 39.0)
	{
		bBench = false;   // fire once
		PC->ConsoleCommand(TEXT("quit"));
	}
}
