// SHITFIRE.Parity.SimHostReplay - the in-engine P4 gate (dev/editor half):
// Level-3 oracle transcripts driven end-to-end through UShitfireSimHost (the
// shipping bridge path: Tick -> 16 ms frame budget -> queued sends parked on
// their exact frame -> TLOG/presentation fan-out) at TimeScale 1000,
// asserting per transcript
//   - impacts: count, order, msg, shell tag, |dTgt - recorded| <= 0.01 m,
//     time skew <= 0.5 s;
//   - the AAR: verdict text, stars, outcome, rounds, dur exact;
//   - the delegate surface carried the same story the TLOG recorded.
//
// Transcripts: ShitfireParityGate::DefaultTranscripts() - five missions
// spanning adjust+FFE bracketing, a convoy pit stop, danger-close creeping
// fire, 60mm precision grids and the fratricide fail path. Tier-0 replays
// each on the first candidate schedule (ADVERSARY_SIGNOFF.md section 4), so
// the bounded schedule search is belt-and-braces, not load-bearing.
//
// The replay machinery (timing reconstruction, gated-flavor replay, the
// comparator, the driver) lives in parity/ShitfireTranscriptReplay.h - ONE
// copy shared with the -ShitfireParityGate runner, which is the SHIPPING
// half of this gate (the automation framework does not exist there).
//
// Run headless:
//   UnrealEditor-Cmd.exe <proj> -ExecCmds="Automation RunTests SHITFIRE.Parity"
//       -unattended -nullrhi -nosound -testexit="Automation Test Queue Empty"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ShitfireGame.h"
#include "ShitfireSimHostReplayCollector.h"
#include "bridge/ShitfireSimHost.h"
#include "parity/ShitfireParityGate.h"
#include "parity/ShitfireTranscriptReplay.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/review/tlog.h"
#include "shitfire/sim_core.h"

#include <vector>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireSimHostReplayTest,
	"SHITFIRE.Parity.SimHostReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireSimHostReplayTest::RunTest(const FString& Parameters)
{
	namespace sf = shitfire;

	// UGameInstanceSubsystem is UCLASS(Within = GameInstance) - the host must
	// live inside a GameInstance outer or NewObject trips the ClassWithin
	// ensure (UObjectGlobals.cpp StaticAllocateObject validation).
	UGameInstance* HostOuter = NewObject<UGameInstance>(GEngine);
	HostOuter->AddToRoot();
	UShitfireSimHost* Host = NewObject<UShitfireSimHost>(HostOuter);
	Host->AddToRoot();
	Host->InitializeStandalone();
	UShitfireReplayCollector* Collector = NewObject<UShitfireReplayCollector>(GetTransientPackage());
	Collector->AddToRoot();
	Collector->BindAll(Host);

	bool bAllPassed = true;
	int32 Passed = 0;
	const TArray<FString> Transcripts = ShitfireParityGate::DefaultTranscripts();
	for (const FString& Rel : Transcripts)
	{
		ShitfireParity::FTranscript Tr;
		if (!TestTrue(FString::Printf(TEXT("transcript fixture loads: %s"), *Rel),
		              ShitfireParity::LoadTranscript(Rel, Tr)))
		{
			bAllPassed = false;
			continue;
		}
		ShitfireParity::FReplayFlavor Flavor(Tr);
		const ShitfireParity::FReplayOutcome R = ShitfireParity::ReplayThroughHost(
			*Host, Tr, Flavor, [Collector]() { Collector->ResetAll(); });

		TestTrue(FString::Printf(TEXT("SimHost replay reproduces the recording: %s"), *Rel), R.bPassed);
		if (!R.bPassed)
		{
			for (const FString& E : R.Best.Errors) { AddError(Rel + TEXT(": ") + E); }
		}
		AddInfo(FString::Printf(
			TEXT("%s through SimHost: %s after %d attempt(s); worst |dTgt diff| %.3f m (tol 0.01), worst time skew %.3f s (tol 0.5)"),
			*Rel, R.bPassed ? TEXT("PASS") : TEXT("FAIL"), R.Attempts,
			R.Best.WorstDTgtDiff, R.Best.WorstSkew));

		// ---- delegate surface carried the same story (last attempt live) ---
		if (R.bPassed && Host->IsMissionLive())
		{
			const std::vector<sf::review::TlogEntry>& Entries = Host->GetSimCore()->tlog().entries();
			int32 TlogImpacts = 0, TlogFdc = 0, HeImpacts = 0, TlogAars = 0;
			const sf::review::TlogEntry* LastAar = nullptr;
			for (const auto& E : Entries)
			{
				if (E.kind == "impact")
				{
					TlogImpacts++;
					if (E.shell.value_or("he") == "he") { HeImpacts++; }
				}
				if (E.kind == "fdc") { TlogFdc++; }
				if (E.kind == "aar") { TlogAars++; LastAar = &E; }
			}
			TestEqual(TEXT("OnImpact broadcasts == TLOG impact rows"),
				Collector->Impacts.Num(), TlogImpacts);
			TestEqual(TEXT("OnFdcLine broadcasts == TLOG fdc rows"),
				Collector->FdcLines, TlogFdc);
			TestEqual(TEXT("OnBurstCue broadcasts == HE impacts"),
				Collector->BurstCues, HeImpacts);
			TestEqual(TEXT("OnAarReady broadcasts == TLOG aar rows"),
				Collector->Aars.Num(), TlogAars);
			if (LastAar && Collector->Aars.Num() == TlogAars && TlogAars > 0)
			{
				TestEqual(TEXT("OnAarReady verdict"),
					Collector->Aars.Last().Verdict, FString(UTF8_TO_TCHAR(LastAar->msg.c_str())));
				const int32 RecStars = LastAar->stars.has_value() ? static_cast<int32>(*LastAar->stars) : -1;
				TestEqual(TEXT("OnAarReady stars"), Collector->Aars.Last().Stars, RecStars);
			}
			if (TlogImpacts > 0)
			{
				TestTrue(TEXT("OnShotCue fired"), Collector->ShotCues > 0);
				TestEqual(TEXT("shot cues == splash cues (proword pairs)"),
					Collector->ShotCues, Collector->SplashCues);
			}
			TestTrue(TEXT("OnStateChange fired"), Collector->StateChanges.Num() > 0);
			AddInfo(FString::Printf(
				TEXT("  delegates: %d impacts, %d fdc, %d sys, %d bursts, %d shots, %d splashes, %d state changes, %d aar"),
				Collector->Impacts.Num(), Collector->FdcLines, Collector->SysLines,
				Collector->BurstCues, Collector->ShotCues, Collector->SplashCues,
				Collector->StateChanges.Num(), Collector->Aars.Num()));
		}

		Host->SetFlavorRngForNextLaunch(nullptr);
		Host->ShutdownMission();
		if (R.bPassed) { Passed++; } else { bAllPassed = false; }
	}
	AddInfo(FString::Printf(TEXT("SimHost replay: %d/%d transcripts exact"), Passed, Transcripts.Num()));

	Collector->RemoveFromRoot();
	Host->RemoveFromRoot();
	HostOuter->RemoveFromRoot();
	return bAllPassed;
}

#endif // WITH_DEV_AUTOMATION_TESTS
