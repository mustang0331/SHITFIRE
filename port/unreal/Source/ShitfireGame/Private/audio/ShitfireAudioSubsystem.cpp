#include "audio/ShitfireAudioSubsystem.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireCoords.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

// Engine-free sim core (battery/ground read - presentation-only).
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim_core.h"
#include "shitfire/terrain/heightfield.h"

bool UShitfireAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UShitfireAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UGameInstance* GI = InWorld.GetGameInstance();
	Host = GI ? GI->GetSubsystem<UShitfireSimHost>() : nullptr;
	if (!Host)
	{
		UE_LOG(LogShitfire, Warning, TEXT("audio: no SimHost - cues unbound"));
		return;
	}

	BoomSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/S_BoomDistant.S_BoomDistant"));
	SquelchSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/S_SquelchClick.S_SquelchClick"));
	HissSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/S_NetHiss.S_NetHiss"));
	if (!BoomSound || !SquelchSound || !HissSound)
	{
		UE_LOG(LogShitfire, Warning,
			TEXT("audio: /Game/Audio assets missing - run Tools/gen_audio_assets.py (boom %d squelch %d hiss %d)"),
			BoomSound != nullptr, SquelchSound != nullptr, HissSound != nullptr);
	}

	// Natural far-field falloff for booms: audible across the whole 10 km
	// map, but clearly range-dependent. Runtime-constructed (no .uasset).
	FarAttenuation = NewObject<USoundAttenuation>(this, TEXT("FarBoomAttenuation"));
	FarAttenuation->Attenuation.bAttenuate = true;
	FarAttenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	FarAttenuation->Attenuation.FalloffDistance = 6000.0f * 100.0f;   // 6 km tail
	FarAttenuation->Attenuation.AttenuationShapeExtents =
		FVector(400.0f * 100.0f, 0, 0);                               // 400 m full-volume core

	Host->OnBurstCue.AddDynamic(this, &UShitfireAudioSubsystem::HandleBurstCue);
	Host->OnShotCue.AddDynamic(this, &UShitfireAudioSubsystem::HandleShotCue);
	Host->OnFdcLine.AddDynamic(this, &UShitfireAudioSubsystem::HandleFdcLine);

	if (HissSound && Host->IsMissionLive())
	{
		HissLoop = UGameplayStatics::SpawnSound2D(&InWorld, HissSound, 0.22f);
	}
	UE_LOG(LogShitfire, Log, TEXT("audio subsystem bound to SimHost cues"));
}

void UShitfireAudioSubsystem::Deinitialize()
{
	if (Host)
	{
		Host->OnBurstCue.RemoveDynamic(this, &UShitfireAudioSubsystem::HandleBurstCue);
		Host->OnShotCue.RemoveDynamic(this, &UShitfireAudioSubsystem::HandleShotCue);
		Host->OnFdcLine.RemoveDynamic(this, &UShitfireAudioSubsystem::HandleFdcLine);
		Host = nullptr;
	}
	if (HissLoop)
	{
		HissLoop->Stop();
		HissLoop = nullptr;
	}
	Super::Deinitialize();
}

FVector UShitfireAudioSubsystem::ListenerUE() const
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FVector Loc;
		FRotator Rot;
		PC->GetPlayerViewPoint(Loc, Rot);
		return Loc;
	}
	return FVector::ZeroVector;
}

bool UShitfireAudioSubsystem::BatteryUE(FVector& Out) const
{
	if (!Host || !Host->GetSimCore())
	{
		return false;
	}
	const shitfire::SimCore::Parts P = Host->GetSimCore()->parts();
	const double X = P.battery->x;
	const double Z = P.battery->z;
	Out = ShitfireCoords::SimToUE(X, P.heightfield->H(X, Z) + 2.0, Z);
	return true;
}

void UShitfireAudioSubsystem::PlayDelayedAt(USoundBase* Sound, const FVector& UEPos,
                                            float Volume, float Pitch)
{
	if (!Sound)
	{
		return;
	}
	const double DistM = FVector::Dist(ListenerUE(), UEPos) / ShitfireCoords::MetersToUE;
	const double DelayS = DistM / SpeedOfSoundMps;
	// Weak-lambda timer: sound arrives after light, delay = distance / 343.
	FTimerHandle Handle;
	TWeakObjectPtr<UShitfireAudioSubsystem> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateLambda([WeakThis, Sound, UEPos, Volume, Pitch]()
		{
			if (WeakThis.IsValid() && WeakThis->GetWorld())
			{
				UGameplayStatics::PlaySoundAtLocation(WeakThis->GetWorld(), Sound,
					UEPos, Volume, Pitch, 0.0f, WeakThis->FarAttenuation);
			}
		}),
		FMath::Max(0.01f, static_cast<float>(DelayS)), false);
}

void UShitfireAudioSubsystem::HandleBurstCue(double SimT, FVector UEPos)
{
	PlayDelayedAt(BoomSound, UEPos, 1.0f, 1.0f);
}

void UShitfireAudioSubsystem::HandleShotCue(double SimT)
{
	// Outgoing guns: muffled, pitched down, from the battery position.
	FVector Battery;
	if (BatteryUE(Battery))
	{
		PlayDelayedAt(BoomSound, Battery, 0.45f, 0.65f);
	}
}

void UShitfireAudioSubsystem::HandleFdcLine(double SimT, const FString& Call, const FString& Msg)
{
	if (SquelchSound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), SquelchSound, 0.5f);
	}
}
