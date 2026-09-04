// UShitfireAudioSubsystem - the P4 audio director (BUILDER-BRIDGE). World
// subsystem, event-driven (no tick): binds the SimHost delegates and turns
// them into the net's soundscape:
//
//   OnBurstCue  -> impact boom DELAYED by listener-distance / 343 m/s (sound
//                  arrives after the flash, like the real thing - the
//                  training cue for estimating range by crack-boom);
//   OnShotCue   -> muffled outgoing thump from the battery position, same
//                  speed-of-sound delay from the guns;
//   OnFdcLine   -> radio squelch click framing each FDC transmission (the
//                  voice itself is P7; the click carries the radio character
//                  - 300-3000 Hz bandpass + light distortion baked into the
//                  WAV by Tools/gen_audio_src.py);
//   mission up  -> low looping net-hiss bed.
//
// Assets under /Game/Audio are script-imported build products
// (Tools/gen_audio_assets.py); the boom is a CC0 recording (port/CREDITS.md).
// All timing here is ENGINE time (presentation); the sim never waits on it.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "bridge/ShitfireSimHost.h"
#include "ShitfireAudioSubsystem.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;

UCLASS()
class SHITFIREGAME_API UShitfireAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION()
	void HandleBurstCue(double SimT, FVector UEPos);

	UFUNCTION()
	void HandleShotCue(double SimT);

	UFUNCTION()
	void HandleFdcLine(double SimT, const FString& Call, const FString& Msg);

	static constexpr double SpeedOfSoundMps = 343.0;

private:
	void PlayDelayedAt(USoundBase* Sound, const FVector& UEPos,
	                   float Volume, float Pitch);
	FVector ListenerUE() const;
	/** Battery muzzle point (sim parts, ground + a little) in UE space. */
	bool BatteryUE(FVector& Out) const;

	UPROPERTY() TObjectPtr<UShitfireSimHost> Host;
	UPROPERTY() TObjectPtr<USoundBase> BoomSound;
	UPROPERTY() TObjectPtr<USoundBase> SquelchSound;
	UPROPERTY() TObjectPtr<USoundBase> HissSound;
	UPROPERTY() TObjectPtr<USoundAttenuation> FarAttenuation;
	UPROPERTY() TObjectPtr<UAudioComponent> HissLoop;
};
