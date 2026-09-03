// Test-support collector for SHITFIRE.Parity.SimHostReplay: a UObject with
// UFUNCTION handlers so the host's DYNAMIC multicast delegates can be bound
// from an automation test (dynamic delegates only bind UFUNCTIONs). Records
// everything broadcast so the test can assert the delegate surface carried
// exactly what the TLOG recorded. Private/Tests only - never game code.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "bridge/ShitfireSimHost.h"
#include "ShitfireSimHostReplayCollector.generated.h"

UCLASS()
class UShitfireReplayCollector final : public UObject
{
	GENERATED_BODY()

public:
	struct FImpactRec
	{
		double SimT = 0;
		double DTgt = 0;
		EShitfireShell Shell = EShitfireShell::He;
		FString Msg;
	};

	TArray<FImpactRec> Impacts;
	TArray<FShitfireAarSummary> Aars;
	TArray<FString> StateChanges;
	int32 FdcLines = 0;
	int32 SysLines = 0;
	int32 BurstCues = 0;
	int32 ShotCues = 0;
	int32 SplashCues = 0;
	FVector LastBurstUEPos = FVector::ZeroVector;

	void BindAll(UShitfireSimHost* Host)
	{
		Host->OnImpact.AddDynamic(this, &UShitfireReplayCollector::HandleImpact);
		Host->OnAarReady.AddDynamic(this, &UShitfireReplayCollector::HandleAar);
		Host->OnFdcLine.AddDynamic(this, &UShitfireReplayCollector::HandleFdc);
		Host->OnSysLine.AddDynamic(this, &UShitfireReplayCollector::HandleSys);
		Host->OnBurstCue.AddDynamic(this, &UShitfireReplayCollector::HandleBurst);
		Host->OnShotCue.AddDynamic(this, &UShitfireReplayCollector::HandleShot);
		Host->OnSplashCue.AddDynamic(this, &UShitfireReplayCollector::HandleSplash);
		Host->OnStateChange.AddDynamic(this, &UShitfireReplayCollector::HandleState);
	}

	void ResetAll()
	{
		Impacts.Reset();
		Aars.Reset();
		StateChanges.Reset();
		FdcLines = SysLines = BurstCues = ShotCues = SplashCues = 0;
		LastBurstUEPos = FVector::ZeroVector;
	}

	UFUNCTION()
	void HandleImpact(double SimT, double DTgt, EShitfireShell Shell, const FString& Msg)
	{
		Impacts.Add({SimT, DTgt, Shell, Msg});
	}

	UFUNCTION()
	void HandleAar(const FShitfireAarSummary& Aar) { Aars.Add(Aar); }

	UFUNCTION()
	void HandleFdc(double SimT, const FString& Call, const FString& Msg) { FdcLines++; }

	UFUNCTION()
	void HandleSys(double SimT, const FString& Msg, bool bError) { SysLines++; }

	UFUNCTION()
	void HandleBurst(double SimT, FVector UEPos)
	{
		BurstCues++;
		LastBurstUEPos = UEPos;
	}

	UFUNCTION()
	void HandleShot(double SimT) { ShotCues++; }

	UFUNCTION()
	void HandleSplash(double SimT) { SplashCues++; }

	UFUNCTION()
	void HandleState(const FString& OldState, const FString& NewState, double SimT)
	{
		StateChanges.Add(NewState);
	}
};
