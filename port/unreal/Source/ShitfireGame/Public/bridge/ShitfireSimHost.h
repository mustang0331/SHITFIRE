// UShitfireSimHost - THE one owner of shitfire::SimCore inside the engine
// process (BUILDER-BRIDGE, P3). GameInstance subsystem + tickable: pawn, UI,
// world assembly and tests all reach the same sim instance through here.
//
// Ownership history: AShitfireWorldActor created/owned the SimCore in the
// world-assembly commit (587e630, InitSimCore); that wiring moved here so a
// UI widget or automation test never has to find a world actor to reach the
// sim. The world actor now launches through LaunchChapter() and caches a
// non-owning pointer.
//
// CLOCK CONTRACT: the sim clock is integer milliseconds advanced in 16 ms
// frames (sim_core.h header note - the browser's rAF grid). Tick() converts
// real DeltaTime * TimeScale into a millisecond budget and steps whole 16 ms
// frames; the fractional remainder carries in AccumMs. NOTHING in the core
// ever reads engine time - sim clock and frame clock are separate by
// construction, which is what lets tests run at TimeScale up to 1000x.
//
// Delegate surface: shaped from what the core actually emits -
//   - TLOG entries (review/tlog.h): fdc / sys / syserr / impact / aar rows
//     appended as the sim advances -> OnFdcLine / OnSysLine / OnImpact /
//     OnAarReady (+ OnSplashCue matched on the two splash prowords,
//     fdc_sink.cpp:111).
//   - drainPresentation() (sim/events.h PresentationEvent): Burst /
//     SmokeScreen / IllumFlare / SunBeam / ChargeWhine -> the shaped cue
//     delegates below (positions converted via bridge/ShitfireCoords.h);
//     every drained event ALSO goes out raw through OnPresentationCue for
//     consumers not shaped yet (troop/vehicle visuals, disperse).
//   - MissionMachine::state() polled once per tick -> OnStateChange with the
//     setState() vocabulary strings (sim/events.h MissionPhase comments).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "ShitfireSimHost.generated.h"

namespace shitfire
{
	class SimCore;
	namespace doctrine { class IFlavorRng; }
}

/** Shell natures on the net (sim/events.h ShellKind + TLOG 'shell' tags). */
UENUM(BlueprintType)
enum class EShitfireShell : uint8
{
	He,
	Smoke,
	Illum,
};

/** PresentationKind mirror for the raw passthrough (sim/events.h). */
UENUM(BlueprintType)
enum class EShitfirePresentation : uint8
{
	Burst,
	SmokeScreen,
	IllumFlare,
	SunBeam,
	ChargeWhine,
	TroopsDown,
	TwinTroopsDown,
	VehiclesTipped,
	BunkerScorched,
	KaijuScorched,
	DisperseTroops,
};

/** The AAR payload slice the UI presents (TLOG aar entry, 13-aar.js:364-379).
    Stars is -1 when the recording carries none (noDisp runs). */
USTRUCT(BlueprintType)
struct FShitfireAarSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") double SimT = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") FString Verdict;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") int32 Stars = -1;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") FString Outcome;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") FString Intent;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") FString BdaClaim;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") double Rounds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") double DurSeconds = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") double EffPct = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Shitfire") bool bBracketed = false;
};

/** Everything init() needs (sim_core.h MissionInit, UE-typed). */
USTRUCT(BlueprintType)
struct FShitfireMissionParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shitfire") FString ChapterId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shitfire") FString SkirmishType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shitfire") FString Difficulty = TEXT("normal");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shitfire") double MissionSeed = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shitfire") double TerrainSeed = 1337.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shitfire") int64 StartMs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShitfireFdcLineSig,
	double, SimT, const FString&, Call, const FString&, Msg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShitfireSysLineSig,
	double, SimT, const FString&, Msg, bool, bError);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FShitfireImpactSig,
	double, SimT, double, DTgt, EShitfireShell, Shell, const FString&, Msg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShitfireBurstCueSig,
	double, SimT, FVector, UEPos);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShitfireIllumCueSig,
	double, SimT, FVector, UEPos, double, BurnSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShitfireNetCueSig,
	double, SimT);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShitfireChargeWhineSig,
	double, SimT, double, Tof);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShitfireStateChangeSig,
	const FString&, OldState, const FString&, NewState, double, SimT);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShitfireAarReadySig,
	const FShitfireAarSummary&, Aar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FShitfirePresentationSig,
	EShitfirePresentation, Kind, double, SimT, FVector, UEPos, double, Num0);

UCLASS()
class SHITFIREGAME_API UShitfireSimHost : public UGameInstanceSubsystem,
                                          public FTickableGameObject
{
	GENERATED_BODY()

public:
	// ---- subsystem lifecycle --------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Everything Initialize() does minus the collection - lets automation
	    tests drive a NewObject'd host outside a live GameInstance. */
	void InitializeStandalone();

	// ---- mission control ------------------------------------------------
	/** newMission for a campaign chapter: seed + tseed resolved through the
	    ported campaign tables exactly as the world-assembly commit did
	    (587e630 InitSimCore - campaign::findChapter / volumeOf). Always a
	    FRESH SimCore: relaunch = restart, matching the old BeginPlay flow. */
	UFUNCTION(BlueprintCallable, Category = "Shitfire")
	bool LaunchChapter(const FString& ChapterId, double FallbackTerrainSeed = 1337.0);

	/** Full-control launch (replay tests, skirmish menu later). */
	bool LaunchMission(const FShitfireMissionParams& Params);

	UFUNCTION(BlueprintCallable, Category = "Shitfire")
	void ShutdownMission();

	UFUNCTION(BlueprintPure, Category = "Shitfire")
	bool IsMissionLive() const { return Sim != nullptr; }

	/** The one sim instance (C++ consumers: world actor, pawn, tests).
	    Null while no mission is live. */
	shitfire::SimCore* GetSimCore() const { return Sim; }

	/** Replay tests only: installed on the NEXT launch, before init()
	    (sim_core.h setFlavorRng contract). Must outlive the mission. The
	    live game leaves this null -> SimCore's built-in default RNG. */
	void SetFlavorRngForNextLaunch(shitfire::doctrine::IFlavorRng* Rng)
	{
		PendingFlavorRng = Rng;
	}

	// ---- the net --------------------------------------------------------
	/** onPlayerMessage passthrough at the current sim time. */
	UFUNCTION(BlueprintCallable, Category = "Shitfire")
	void SubmitMessage(const FString& Raw);

	/** Deliver a message exactly when the sim clock reaches SimMs (16 ms
	    multiple). The step loop submits it with the clock parked ON that
	    frame, the same advanceTo(sendMs)+submitMessage sequence the Tier-0
	    replay uses - THE mechanism that makes in-engine replay exact. */
	void QueueMessageAtSimMs(int64 SimMs, const FString& Raw);

	// ---- clock ----------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Shitfire")
	double GetSimNow() const;

	int64 GetSimNowMs() const;

	UFUNCTION(BlueprintPure, Category = "Shitfire")
	FString GetMissionStateString() const { return LastStateString; }

	/** Sim seconds per real second. Clamped [0, 1000] (test ceiling). */
	UFUNCTION(BlueprintCallable, Category = "Shitfire")
	void SetTimeScale(double Scale);

	UFUNCTION(BlueprintPure, Category = "Shitfire")
	double GetTimeScale() const { return TimeScale; }

	/** Pending core event-queue size (trailing-drain checks in tests). */
	int32 GetPendingSimEventCount() const;

	// ---- FTickableGameObject --------------------------------------------
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override
	{
		return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
	}
	virtual bool IsTickable() const override { return Sim != nullptr; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UShitfireSimHost, STATGROUP_Tickables);
	}

	// ---- delegate surface -----------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireFdcLineSig OnFdcLine;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireSysLineSig OnSysLine;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireImpactSig OnImpact;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireBurstCueSig OnBurstCue;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireBurstCueSig OnSmokeScreenCue;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireIllumCueSig OnIllumFlareCue;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireBurstCueSig OnSunBeamCue;
	// Shot/splash cues ride the two proword pairs the FDC actually speaks
	// (fdc_sink.cpp:110-111 ShotOver/SplashOver) - for standard tubes AND the
	// sunlamp swaps. ChargeWhine is the sunlamp-only charge-up presentation
	// event (mission_machine.cpp:474/:517, 10-mission.js:806/:829).
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireNetCueSig OnShotCue;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireNetCueSig OnSplashCue;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireChargeWhineSig OnChargeWhineCue;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireStateChangeSig OnStateChange;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfireAarReadySig OnAarReady;
	UPROPERTY(BlueprintAssignable, Category = "Shitfire") FShitfirePresentationSig OnPresentationCue;

private:
	/** Step whole 16 ms frames out of AccumMs, delivering queued sends with
	    the clock parked on their frame. Then fan out TLOG + presentation. */
	void StepFrames();
	void FanOutTlog();
	void FanOutPresentation();
	void PollStateChange();

	/** F2/F3 (ADVERSARY_SIGNOFF.md): pin C locale + global C++ locale to
	    classic "C" before any core code runs in this process. */
	static void PinCoreLocale();

	// Raw owned pointer, not TUniquePtr: UHT's generated vtable-helper ctor
	// instantiates member dtors with SimCore still incomplete (same reasoning
	// as the world actor pre-refactor, 587e630). Deleted in ShutdownMission.
	shitfire::SimCore* Sim = nullptr;
	shitfire::doctrine::IFlavorRng* PendingFlavorRng = nullptr;

	double TimeScale = 1.0;
	double AccumMs = 0.0;
	/** Runaway guard for one tick, not a pacing knob: at 1000x and 60 fps a
	    tick is ~1000 frames; only a pathological dt ever hits this. */
	int32 MaxFramesPerTick = 250000;

	struct FQueuedSend
	{
		int64 SimMs = 0;
		FString Raw;
	};
	TArray<FQueuedSend> QueuedSends;   // kept sorted by SimMs

	/** TLOG fan-out watermark. TranscriptLog trims its front at maxEntries
	    (2000, tlog.h) - far beyond any mission here; if it ever trims, the
	    fan-out logs an error rather than silently re-broadcasting. */
	int32 TlogSeen = 0;
	int32 TlogSizeLastTick = 0;

	FString LastStateString;
};
