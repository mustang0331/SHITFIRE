#include "bridge/fx/ShitfireFxSubsystem.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireCoords.h"
#include "bridge/fx/ShitfireBurstActor.h"
#include "bridge/fx/ShitfireFlareActor.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "ShitfireWorldActor.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// Engine-free sim core (wind read - presentation-only consumption).
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim_core.h"

bool UShitfireFxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UShitfireFxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UGameInstance* GI = InWorld.GetGameInstance();
	Host = GI ? GI->GetSubsystem<UShitfireSimHost>() : nullptr;
	if (!Host)
	{
		UE_LOG(LogShitfire, Warning, TEXT("FX subsystem: no SimHost - cues unbound"));
		return;
	}
	Host->OnBurstCue.AddDynamic(this, &UShitfireFxSubsystem::HandleBurstCue);
	Host->OnSmokeScreenCue.AddDynamic(this, &UShitfireFxSubsystem::HandleSmokeScreenCue);
	Host->OnIllumFlareCue.AddDynamic(this, &UShitfireFxSubsystem::HandleIllumFlareCue);
	UE_LOG(LogShitfire, Log, TEXT("FX subsystem bound to SimHost cues"));

	if (FParse::Param(FCommandLine::Get(), TEXT("ShitfireFxProbe")))
	{
		StartFxProbe();
	}
}

void UShitfireFxSubsystem::StartFxProbe()
{
	// Vertical binos FOVs 15 / 8.6 / 4.3 deg -> horizontal at 16:9 for
	// APlayerCameraManager::SetFOV: h = 2*atan(tan(v/2) * 16/9).
	static const double VertFovs[3] = {15.0, 8.6, 4.3};
	ProbeStep = 0;
	UE_LOG(LogShitfire, Display, TEXT("FX probe: burst+flare at target area, 3 FOV screenshots"));

	GetWorld()->GetTimerManager().SetTimer(ProbeTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (!PC)
			{
				return;
			}
			if (ProbeStep == 0)
			{
				// Spawn at the scenario target area, pre-aged to full column.
				for (TActorIterator<AShitfireWorldActor> It(GetWorld()); It; ++It)
				{
					ProbeTargetUE = It->TargetAreaUE();
					break;
				}
				ReadWind();
				if (AShitfireBurstActor* B = GetWorld()->SpawnActor<AShitfireBurstActor>(
						AShitfireBurstActor::StaticClass(), ProbeTargetUE, FRotator::ZeroRotator))
				{
					B->InitBurst(DownwindUE, WindSpeedMps, /*StartAgeS=*/45.0);
				}
				// +80 m keeps the flare INSIDE the 4.3 deg vertical frame at
				// 3.2 km (half-FOV 2.15 deg = ~120 m of height there); the
				// gameplay spawn height stays the sim's own +330 m.
				if (AShitfireFlareActor* F = GetWorld()->SpawnActor<AShitfireFlareActor>(
						AShitfireFlareActor::StaticClass(),
						ProbeTargetUE + FVector(0, 0, 80.0 * ShitfireCoords::MetersToUE),
						FRotator::ZeroRotator))
				{
					F->InitFlare(60.0);
				}
				const double DistM = FVector::Dist(
					PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector,
					ProbeTargetUE) / ShitfireCoords::MetersToUE;
				UE_LOG(LogShitfire, Display,
					TEXT("FX probe: spawned at target area, observer range %.0f m"), DistM);
			}
			else if (ProbeStep >= 1 && ProbeStep <= 3)
			{
				const double V = VertFovs[ProbeStep - 1];
				const double H = 2.0 * FMath::RadiansToDegrees(FMath::Atan(
					FMath::Tan(FMath::DegreesToRadians(V) * 0.5) * 16.0 / 9.0));
				PC->PlayerCameraManager->SetFOV(static_cast<float>(H));
				PC->ConsoleCommand(FString::Printf(
					TEXT("HighResShot 1920x1080 filename=FxProbe_v%.1fdeg"), V));
			}
			else
			{
				PC->ConsoleCommand(TEXT("quit"));
			}
			ProbeStep++;
		}),
		3.0f, /*bLoop=*/true, /*FirstDelay=*/5.0f);
}

void UShitfireFxSubsystem::Deinitialize()
{
	if (Host)
	{
		Host->OnBurstCue.RemoveDynamic(this, &UShitfireFxSubsystem::HandleBurstCue);
		Host->OnSmokeScreenCue.RemoveDynamic(this, &UShitfireFxSubsystem::HandleSmokeScreenCue);
		Host->OnIllumFlareCue.RemoveDynamic(this, &UShitfireFxSubsystem::HandleIllumFlareCue);
		Host = nullptr;
	}
	Super::Deinitialize();
}

void UShitfireFxSubsystem::ReadWind()
{
	DownwindUE = FVector::ZeroVector;
	WindSpeedMps = 0.0;
	if (Host && Host->GetSimCore())
	{
		const shitfire::scenario::Wind& W = Host->GetSimCore()->parts().world->wind;
		// Wind dx/dz is the sim-frame DRIFT VECTOR in m/s - "the direction
		// smoke actually travels (downwind)" (04-world.js:925-933); speed is
		// carried separately, so normalize the direction and keep W.speed.
		// Direction convert only, no scale (ShitfireCoords contract).
		DownwindUE = ShitfireCoords::SimDirToUE(W.dx, 0.0, W.dz).GetSafeNormal2D();
		WindSpeedMps = W.speed;
	}
}

void UShitfireFxSubsystem::HandleBurstCue(double SimT, FVector UEPos)
{
	ReadWind();
	AShitfireBurstActor* Burst = GetWorld()->SpawnActor<AShitfireBurstActor>(
		AShitfireBurstActor::StaticClass(), UEPos, FRotator::ZeroRotator);
	if (Burst)
	{
		Burst->InitBurst(DownwindUE, WindSpeedMps);
	}
}

void UShitfireFxSubsystem::HandleSmokeScreenCue(double SimT, FVector UEPos)
{
	// Placeholder read: the persistent column again (denser pass is a later
	// smoke-mission item); the SCORED smoke effect was booked by the core
	// before this cue ever fired ("model first, show second").
	HandleBurstCue(SimT, UEPos);
}

void UShitfireFxSubsystem::HandleIllumFlareCue(double SimT, FVector UEPos, double BurnSeconds)
{
	AShitfireFlareActor* Flare = GetWorld()->SpawnActor<AShitfireFlareActor>(
		AShitfireFlareActor::StaticClass(), UEPos, FRotator::ZeroRotator);
	if (Flare)
	{
		Flare->InitFlare(BurnSeconds);
	}
}
