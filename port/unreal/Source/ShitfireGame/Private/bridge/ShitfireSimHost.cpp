#include "bridge/ShitfireSimHost.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireCoords.h"

#include <clocale>
#include <locale>

// Engine-free sim core - all prebuilt /fp:precise code (ShitfireCoreLib).
#include "shitfire/campaign/campaign_data.h"
#include "shitfire/review/tlog.h"
#include "shitfire/scenario/scenario_state.h"
#include "shitfire/scenario/world_layout.h"
#include "shitfire/sim/events.h"
#include "shitfire/sim/mission_clock.h"
#include "shitfire/sim/mission_machine.h"
#include "shitfire/sim_core.h"

namespace
{
	// The setState() vocabulary strings, exactly as sim/events.h documents
	// them per MissionPhase enumerator (and as TLOG 'state' records them).
	const TCHAR* PhaseString(shitfire::sim::MissionPhase P)
	{
		using MP = shitfire::sim::MissionPhase;
		switch (P)
		{
		case MP::Observing:     return TEXT("OBSERVING");
		case MP::MissionSent:   return TEXT("MISSION SENT");
		case MP::AtMyCommand:   return TEXT("AT MY COMMAND");
		case MP::Shot:          return TEXT("SHOT");
		case MP::Adjusting:     return TEXT("ADJUSTING");
		case MP::FireForEffect: return TEXT("FIRE FOR EFFECT");
		case MP::EomQuery:      return TEXT("EOM?");
		case MP::DoNotLoad:     return TEXT("DO NOT LOAD");
		case MP::CheckFiring:   return TEXT("CHECK FIRING");
		case MP::CeaseLoading:  return TEXT("CEASE LOADING");
		case MP::Aar:           return TEXT("AAR");
		}
		return TEXT("OBSERVING");
	}

	EShitfireShell ShellFromTag(const std::string& Tag)
	{
		if (Tag == "smoke") { return EShitfireShell::Smoke; }
		if (Tag == "illum") { return EShitfireShell::Illum; }
		return EShitfireShell::He;   // impact entries omit the tag for HE (tlog.h)
	}

	EShitfirePresentation PresentationFromKind(shitfire::sim::PresentationKind K)
	{
		using PK = shitfire::sim::PresentationKind;
		switch (K)
		{
		case PK::Burst:          return EShitfirePresentation::Burst;
		case PK::SmokeScreen:    return EShitfirePresentation::SmokeScreen;
		case PK::IllumFlare:     return EShitfirePresentation::IllumFlare;
		case PK::SunBeam:        return EShitfirePresentation::SunBeam;
		case PK::ChargeWhine:    return EShitfirePresentation::ChargeWhine;
		case PK::TroopsDown:     return EShitfirePresentation::TroopsDown;
		case PK::TwinTroopsDown: return EShitfirePresentation::TwinTroopsDown;
		case PK::VehiclesTipped: return EShitfirePresentation::VehiclesTipped;
		case PK::BunkerScorched: return EShitfirePresentation::BunkerScorched;
		case PK::KaijuScorched:  return EShitfirePresentation::KaijuScorched;
		case PK::DisperseTroops: return EShitfirePresentation::DisperseTroops;
		}
		return EShitfirePresentation::Burst;
	}
}

// ---- lifecycle ----------------------------------------------------------

void UShitfireSimHost::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeStandalone();
}

void UShitfireSimHost::InitializeStandalone()
{
	PinCoreLocale();
	UE_LOG(LogShitfire, Log, TEXT("SimHost up (locale pinned to classic C)"));
}

void UShitfireSimHost::Deinitialize()
{
	ShutdownMission();
	Super::Deinitialize();
}

void UShitfireSimHost::PinCoreLocale()
{
	// ADVERSARY_SIGNOFF.md F2/F3: jsNum()'s %g fallback is LC_NUMERIC-
	// sensitive, and the parser's function-local static std::regex objects
	// capture the global C++ locale at FIRST construction. Pin both global
	// locales to classic "C" before any core code runs so no engine/plugin
	// setlocale/std::locale::global call can shift core text behavior.
	std::setlocale(LC_ALL, "C");
	std::locale::global(std::locale::classic());
}

// ---- mission control ----------------------------------------------------

bool UShitfireSimHost::LaunchChapter(const FString& ChapterId, double FallbackTerrainSeed)
{
	// Seed resolution exactly as the world-assembly wiring did it
	// (587e630 AShitfireWorldActor::InitSimCore).
	FShitfireMissionParams P;
	P.ChapterId = ChapterId;
	P.Difficulty = TEXT("normal");
	P.StartMs = 0;
	const std::string ChapterUtf8 = TCHAR_TO_UTF8(*ChapterId);
	const shitfire::campaign::ChapterSpec* Ch = shitfire::campaign::findChapter(ChapterUtf8);
	const shitfire::campaign::VolumeSpec* Vol = shitfire::campaign::volumeOf(ChapterUtf8);
	P.MissionSeed = Ch ? Ch->seed : 1.0;
	P.TerrainSeed = (Vol && Vol->tseed.has_value()) ? *Vol->tseed : FallbackTerrainSeed;
	return LaunchMission(P);
}

bool UShitfireSimHost::LaunchMission(const FShitfireMissionParams& Params)
{
	PinCoreLocale();   // idempotent; guards a host that never saw Initialize
	ShutdownMission();

	Sim = new shitfire::SimCore();
	if (PendingFlavorRng)
	{
		// Replay tests only - before init() per the sim_core.h contract.
		Sim->setFlavorRng(PendingFlavorRng);
	}

	shitfire::MissionInit MI;
	MI.chapterId = TCHAR_TO_UTF8(*Params.ChapterId);
	MI.skirmishType = TCHAR_TO_UTF8(*Params.SkirmishType);
	MI.difficulty = TCHAR_TO_UTF8(*Params.Difficulty);
	MI.missionSeed = Params.MissionSeed;
	MI.terrainSeed = Params.TerrainSeed;
	MI.startMs = Params.StartMs;
	Sim->init(MI);

	AccumMs = 0.0;
	QueuedSends.Reset();
	TlogSeen = 0;
	TlogSizeLastTick = 0;
	LastStateString = PhaseString(Sim->parts().machine->state());

	const shitfire::SimCore::Parts P = Sim->parts();
	UE_LOG(LogShitfire, Log,
		TEXT("SimHost mission up: chapter '%hs' type %hs seed %.0f terrain %.0f startMs %lld, OP (%.0f, %.0f)"),
		MI.chapterId.c_str(), P.scenario->type.c_str(), MI.missionSeed,
		MI.terrainSeed, Params.StartMs, P.op->x, P.op->z);
	return true;
}

void UShitfireSimHost::ShutdownMission()
{
	delete Sim;
	Sim = nullptr;
	QueuedSends.Reset();
	AccumMs = 0.0;
	TlogSeen = 0;
	TlogSizeLastTick = 0;
	LastStateString.Reset();
}

// ---- the net ------------------------------------------------------------

void UShitfireSimHost::SubmitMessage(const FString& Raw)
{
	if (!Sim)
	{
		UE_LOG(LogShitfire, Warning, TEXT("SubmitMessage with no mission live: %s"), *Raw);
		return;
	}
	Sim->submitMessage(TCHAR_TO_UTF8(*Raw));
}

void UShitfireSimHost::QueueMessageAtSimMs(int64 SimMs, const FString& Raw)
{
	FQueuedSend Send;
	Send.SimMs = SimMs;
	Send.Raw = Raw;
	int32 At = 0;
	while (At < QueuedSends.Num() && QueuedSends[At].SimMs <= SimMs) { At++; }
	QueuedSends.Insert(MoveTemp(Send), At);
}

// ---- clock --------------------------------------------------------------

double UShitfireSimHost::GetSimNow() const
{
	return Sim ? Sim->now() : 0.0;
}

int64 UShitfireSimHost::GetSimNowMs() const
{
	return Sim ? Sim->nowMs() : 0;
}

void UShitfireSimHost::SetTimeScale(double Scale)
{
	TimeScale = FMath::Clamp(Scale, 0.0, 1000.0);
}

int32 UShitfireSimHost::GetPendingSimEventCount() const
{
	return Sim ? static_cast<int32>(Sim->parts().queue->size()) : 0;
}

// ---- ticking ------------------------------------------------------------

void UShitfireSimHost::Tick(float DeltaTime)
{
	if (!Sim)
	{
		return;
	}
	AccumMs += static_cast<double>(DeltaTime) * 1000.0 * TimeScale;
	StepFrames();
	FanOutTlog();
	FanOutPresentation();
	PollStateChange();
}

void UShitfireSimHost::StepFrames()
{
	int32 Frames = 0;
	while (AccumMs >= 16.0 && Frames < MaxFramesPerTick)
	{
		// Deliver queued sends with the clock PARKED on their frame - the
		// Tier-0 replay's advanceTo(sendMs) + submitMessage sequence.
		const int64 NowMs = Sim->nowMs();
		while (QueuedSends.Num() > 0 && QueuedSends[0].SimMs <= NowMs)
		{
			Sim->submitMessage(TCHAR_TO_UTF8(*QueuedSends[0].Raw));
			QueuedSends.RemoveAt(0);
		}
		Sim->advance();
		AccumMs -= 16.0;
		Frames++;
	}
	if (Frames >= MaxFramesPerTick)
	{
		UE_LOG(LogShitfire, Warning,
			TEXT("SimHost hit MaxFramesPerTick (%d) - dropping %.0f ms of sim budget"),
			MaxFramesPerTick, AccumMs);
		AccumMs = 0.0;
	}
}

// ---- fan-out ------------------------------------------------------------

void UShitfireSimHost::FanOutTlog()
{
	const std::vector<shitfire::review::TlogEntry>& Entries = Sim->tlog().entries();
	const int32 Size = static_cast<int32>(Entries.size());
	if (Size < TlogSizeLastTick)
	{
		// The TLOG trimmed its front (maxEntries, tlog.h). Missions here run
		// ~100 entries; if this fires something is very wrong - do not
		// rebroadcast, just resync and say so.
		UE_LOG(LogShitfire, Error,
			TEXT("TLOG trimmed under the fan-out watermark (%d -> %d) - entries lost"),
			TlogSizeLastTick, Size);
		TlogSeen = Size;
	}
	TlogSizeLastTick = Size;

	for (int32 i = TlogSeen; i < Size; i++)
	{
		const shitfire::review::TlogEntry& E = Entries[static_cast<std::size_t>(i)];
		const FString Msg = UTF8_TO_TCHAR(E.msg.c_str());
		if (E.kind == "fdc")
		{
			OnFdcLine.Broadcast(E.t, UTF8_TO_TCHAR(E.call.c_str()), Msg);
			// The shot/splash prowords, both callsign flavors
			// (fdc_sink.cpp:110-111 ShotOver/SplashOver).
			if (E.msg == "SHOT, OVER." || E.msg == "DISCHARGE, OVER.")
			{
				OnShotCue.Broadcast(E.t);
			}
			else if (E.msg == "SPLASH, OVER." || E.msg == "SOLAR EVENT, OVER.")
			{
				OnSplashCue.Broadcast(E.t);
			}
		}
		else if (E.kind == "sys" || E.kind == "syserr")
		{
			OnSysLine.Broadcast(E.t, Msg, E.kind == "syserr");
		}
		else if (E.kind == "impact")
		{
			OnImpact.Broadcast(E.t, E.dTgt.value_or(0.0),
				ShellFromTag(E.shell.value_or("he")), Msg);
		}
		else if (E.kind == "aar")
		{
			FShitfireAarSummary Aar;
			Aar.SimT = E.t;
			Aar.Verdict = Msg;
			Aar.Stars = E.stars.has_value() ? static_cast<int32>(*E.stars) : -1;
			Aar.Outcome = UTF8_TO_TCHAR(E.outcome.value_or("").c_str());
			Aar.Intent = UTF8_TO_TCHAR(E.intent.value_or("").c_str());
			Aar.BdaClaim = UTF8_TO_TCHAR(E.bdaClaim.value_or("").c_str());
			Aar.Rounds = E.rounds.value_or(0.0);
			Aar.DurSeconds = E.dur.value_or(0.0);
			Aar.EffPct = E.effPct.value_or(0.0);
			Aar.bBracketed = E.bracketed.value_or(false);
			OnAarReady.Broadcast(Aar);
		}
		// 'spot' / 'obs' / 'caller' / 'note' rows stay UI-transcript-only for
		// now; the observer UI reads them straight off tlog() when it needs.
	}
	TlogSeen = Size;
}

void UShitfireSimHost::FanOutPresentation()
{
	// PresentationEvent carries no timestamp (sim/events.h) - cues go out at
	// drain granularity, stamped with the current sim time. At TimeScale 1
	// that is at most one tick after booking; presentation only, never scored.
	const double SimT = Sim->now();
	const std::vector<shitfire::sim::PresentationEvent> Events = Sim->drainPresentation();
	for (const shitfire::sim::PresentationEvent& Ev : Events)
	{
		const FVector UEPos = ShitfireCoords::SimToUE(Ev.x, Ev.y, Ev.z);
		const EShitfirePresentation Kind = PresentationFromKind(Ev.kind);
		OnPresentationCue.Broadcast(Kind, SimT, UEPos, Ev.num0);
		switch (Ev.kind)
		{
		case shitfire::sim::PresentationKind::Burst:
			OnBurstCue.Broadcast(SimT, UEPos);
			break;
		case shitfire::sim::PresentationKind::SmokeScreen:
			OnSmokeScreenCue.Broadcast(SimT, UEPos);
			break;
		case shitfire::sim::PresentationKind::IllumFlare:
			OnIllumFlareCue.Broadcast(SimT, UEPos, Ev.num0);
			break;
		case shitfire::sim::PresentationKind::SunBeam:
			OnSunBeamCue.Broadcast(SimT, UEPos);
			break;
		case shitfire::sim::PresentationKind::ChargeWhine:
			OnChargeWhineCue.Broadcast(SimT, Ev.num0);
			break;
		default:
			break;   // unit visuals ride OnPresentationCue only for now
		}
	}
}

void UShitfireSimHost::PollStateChange()
{
	const FString Now = PhaseString(Sim->parts().machine->state());
	if (!Now.Equals(LastStateString))
	{
		const FString Old = LastStateString;
		LastStateString = Now;
		OnStateChange.Broadcast(Old, Now, Sim->now());
	}
}
