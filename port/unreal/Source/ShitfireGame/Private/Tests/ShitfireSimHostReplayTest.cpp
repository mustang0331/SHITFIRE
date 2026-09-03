// SHITFIRE.Parity.SimHostReplay - the P4-gate precursor: ONE Level-3 oracle
// transcript driven end-to-end through UShitfireSimHost (the shipping bridge
// path: Tick -> 16 ms frame budget -> queued sends parked on their exact
// frame -> TLOG/presentation fan-out) at TimeScale 1000, asserting
//   - impacts: count, order, msg, shell tag, |dTgt - recorded| <= 0.01 m,
//     time skew <= 0.5 s;
//   - the AAR: verdict text, stars, outcome, rounds, dur exact;
//   - the delegate surface carried the same story the TLOG recorded.
//
// Transcript: mission_05 ch1-2 bunker-bracket - mid-complexity, successive
// bracketing (adjust) + FFE, 7 sends / 9 impacts / 4-star AAR. Tier-0 replays
// it exactly on the first candidate schedule (ADVERSARY_SIGNOFF.md section 4),
// so the bounded schedule search here is belt-and-braces, not load-bearing.
//
// Timing reconstruction and gated-flavor replay are ports of the Tier-0
// comparator machinery (port/core-tests/level3/replay_test.cpp): recorded
// TLOG stamps are Math.round(t*10)/10 of 16 ms frame times, so each send has
// up to ~6 grid candidates; gated flavor lines (fdc_quips.h FlavorGate) must
// reproduce their recorded PRESENCE because four gates air before say()
// calls whose return time schedules impacts. The full 33-transcript,
// Shipping-config run stays in Tier-0 + the P4 QA gate; this test proves the
// ENGINE wrapper adds zero distortion.
//
// Run headless:
//   UnrealEditor-Cmd.exe <proj> -ExecCmds="Automation RunTests SHITFIRE.Parity"
//       -unattended -nullrhi -nosound -testexit="Automation Test Queue Empty"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "ShitfireFixtures.h"
#include "ShitfireGame.h"
#include "ShitfireSimHostReplayCollector.h"
#include "bridge/ShitfireSimHost.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/math/prng.h"
#include "shitfire/review/tlog.h"
#include "shitfire/sim_core.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
	namespace sf = shitfire;

	constexpr const TCHAR* kTranscript = TEXT("level3/mission_05_ch1-2-bunker-bracket.json");

	/* ---- recorded transcript (UE-Json flavor of the Tier-0 loader) ------ */

	struct FRecEntry
	{
		double T = 0;
		FString Kind;
		FString Msg;
		double DTgt = 0;            // impact
		FString Shell;              // impact ('he' when absent, tlog.h)
		int32 Stars = -1;           // aar
		FString Outcome;            // aar
		double Rounds = 0;          // aar
		double Dur = 0;             // aar
	};

	struct FTranscript
	{
		FString Chapter;            // "" = skirmish
		FString ScenarioType;
		FString Difficulty;
		double SeedMission = 1;
		double SeedTerrain = 1337;
		TArray<FRecEntry> Entries;
	};

	bool LoadTranscript(FTranscript& Out)
	{
		const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(kTranscript);
		if (!J.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Meta = J->GetObjectField(TEXT("meta"));
		Meta->TryGetStringField(TEXT("chapter"), Out.Chapter);   // null -> stays ""
		Out.ScenarioType = Meta->GetStringField(TEXT("scenarioType"));
		Out.Difficulty = Meta->GetStringField(TEXT("difficulty"));
		Out.SeedMission = Meta->GetNumberField(TEXT("seedMission"));
		Out.SeedTerrain = Meta->GetNumberField(TEXT("seedTerrain"));
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("tlog")))
		{
			const TSharedPtr<FJsonObject> E = V->AsObject();
			FRecEntry R;
			R.T = E->GetNumberField(TEXT("t"));
			R.Kind = E->GetStringField(TEXT("kind"));
			E->TryGetStringField(TEXT("msg"), R.Msg);
			if (R.Kind == TEXT("impact"))
			{
				R.DTgt = E->GetNumberField(TEXT("dTgt"));
				if (!E->TryGetStringField(TEXT("shell"), R.Shell))
				{
					R.Shell = TEXT("he");
				}
			}
			else if (R.Kind == TEXT("aar"))
			{
				double S = -1;
				if (E->TryGetNumberField(TEXT("stars"), S)) { R.Stars = static_cast<int32>(S); }
				E->TryGetStringField(TEXT("outcome"), R.Outcome);
				E->TryGetNumberField(TEXT("rounds"), R.Rounds);
				E->TryGetNumberField(TEXT("dur"), R.Dur);
			}
			Out.Entries.Add(MoveTemp(R));
		}
		return Out.Entries.Num() > 0;
	}

	/* ---- clock helpers (replay_test.cpp:104-124, verbatim ports) -------- */

	double JsRound1(double X) { return std::floor(X * 10 + 0.5) / 10; }

	TArray<int64> GridCandidates(double TRec)
	{
		TArray<int64> Out;
		const int64 Center = static_cast<int64>(std::floor(TRec * 1000 + 0.5));
		for (int64 M = ((Center - 64) / 16) * 16; M <= Center + 64; M += 16)
		{
			if (M < 0) { continue; }
			if (JsRound1(static_cast<double>(M) / 1000.0) == JsRound1(TRec))
			{
				Out.Add(M);
			}
		}
		return Out;
	}

	int64 DeliveryFrameMs(double T)
	{
		const double Raw = T * 1000.0;
		int64 M = static_cast<int64>(std::ceil(Raw / 16.0 - 1e-9)) * 16;
		if (static_cast<double>(M) < Raw - 1e-9) { M += 16; }
		return M;
	}

	/* ---- gated-flavor replay (replay_test.cpp:126-311, trimmed) ---------
	   Presence of the five gated flavor lines is answered from the recording
	   (fdc_quips.h FlavorGate note); pool selection stays free flavor. */

	void ReplaceAll(std::string& S, const std::string& From, const std::string& To)
	{
		std::size_t Pos = 0;
		while ((Pos = S.find(From, Pos)) != std::string::npos)
		{
			S.replace(Pos, From.size(), To);
			Pos += To.size();
		}
	}

	std::string NormalizeCallsigns(std::string S)
	{
		ReplaceAll(S, "HACKSAW FIRES", "HELLHOUND FIRES");
		ReplaceAll(S, "SUNLAMP ACTUAL", "HELLHOUND FIRES");
		ReplaceAll(S, "HACKSAW", "HELLHOUND");
		ReplaceAll(S, "SUNLAMP", "HELLHOUND");
		return S;
	}

	class FReplayFlavor final : public sf::doctrine::IFlavorRng
	{
	public:
		explicit FReplayFlavor(const FTranscript& Tr) : Rng(0x5F3759DFu)
		{
			const sf::doctrine::QuipSet Quips = sf::doctrine::makeQuips();
			const std::map<sf::doctrine::FlavorGate, const std::vector<std::string>*> Gates = {
				{sf::doctrine::FlavorGate::FfeAck, &Quips.ffeAck.lines},
				{sf::doctrine::FlavorGate::CompleteTail, &Quips.completeTail.lines},
				{sf::doctrine::FlavorGate::ReadbackTail, &Quips.readbackTail.lines},
				{sf::doctrine::FlavorGate::SnideRound, &Quips.snideRound.lines},
				{sf::doctrine::FlavorGate::CorrSnark, &Quips.corrSnark.lines},
			};
			for (const FRecEntry& E : Tr.Entries)
			{
				if (E.Kind != TEXT("fdc")) { continue; }
				const std::string Msg = NormalizeCallsigns(TCHAR_TO_UTF8(*E.Msg));
				for (const auto& G : Gates)
				{
					std::vector<std::string> Normalized;
					for (const std::string& L : *G.second)
					{
						Normalized.push_back(NormalizeCallsigns(L));
					}
					if (std::find(Normalized.begin(), Normalized.end(), Msg) != Normalized.end())
					{
						Times[G.first].push_back(E.T);
						break;
					}
				}
			}
		}

		double next() override { return Rng.next(); }

		bool coin(double /*P*/, sf::doctrine::FlavorGate Gate, double PredictedAirTime) override
		{
			if (Gate == sf::doctrine::FlavorGate::CareerCallback) { return false; }
			const auto It = Times.find(Gate);
			if (It == Times.end()) { return false; }
			auto& Used = Consumed[Gate];
			const double TPred = JsRound1(
				static_cast<double>(DeliveryFrameMs(PredictedAirTime)) / 1000.0);
			for (std::size_t i = 0; i < It->second.size(); i++)
			{
				if (Used.count(i)) { continue; }
				if (std::fabs(It->second[i] - TPred) <= 0.75)
				{
					Used.insert(i);
					return true;
				}
				if (It->second[i] > TPred + 0.75) { break; }
			}
			return false;
		}

		void ResetRun()
		{
			Consumed.clear();
			Rng = sf::Mulberry32(0x5F3759DFu);
		}

	private:
		sf::Mulberry32 Rng;
		std::map<sf::doctrine::FlavorGate, std::vector<double>> Times;
		std::map<sf::doctrine::FlavorGate, std::set<std::size_t>> Consumed;
	};

	/* ---- one attempt's verdict ------------------------------------------ */

	struct FAttemptResult
	{
		bool bPass = true;
		double FirstFailT = 1e300;
		double WorstDTgtDiff = 0;
		double WorstSkew = 0;
		TArray<FString> Errors;

		void Fail(double T, const FString& What)
		{
			bPass = false;
			if (T < FirstFailT) { FirstFailT = T; }
			if (Errors.Num() < 10) { Errors.Add(What); }
		}
	};

	FAttemptResult CompareRun(const FTranscript& Tr,
	                          const std::vector<sf::review::TlogEntry>& Got)
	{
		FAttemptResult R;

		// impacts: count, order, msg, dTgt within 0.01 m, shell, t +/- 0.5 s
		TArray<const FRecEntry*> RecImpacts;
		for (const FRecEntry& E : Tr.Entries)
		{
			if (E.Kind == TEXT("impact")) { RecImpacts.Add(&E); }
		}
		std::vector<const sf::review::TlogEntry*> GotImpacts;
		for (const auto& E : Got)
		{
			if (E.kind == "impact") { GotImpacts.push_back(&E); }
		}
		if (RecImpacts.Num() != static_cast<int32>(GotImpacts.size()))
		{
			R.Fail(RecImpacts.Num() > 0 ? RecImpacts[0]->T : 0,
				FString::Printf(TEXT("impact count: recorded %d, got %d"),
					RecImpacts.Num(), static_cast<int32>(GotImpacts.size())));
		}
		const int32 NImp = FMath::Min(RecImpacts.Num(), static_cast<int32>(GotImpacts.size()));
		for (int32 i = 0; i < NImp; i++)
		{
			const FRecEntry& Rec = *RecImpacts[i];
			const sf::review::TlogEntry& Mine = *GotImpacts[static_cast<std::size_t>(i)];
			const FString MineMsg = UTF8_TO_TCHAR(Mine.msg.c_str());
			if (!Rec.Msg.Equals(MineMsg))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d msg: recorded '%s', got '%s'"),
					i, *Rec.Msg, *MineMsg));
			}
			const double Diff = std::fabs(Rec.DTgt - Mine.dTgt.value_or(-1.0));
			R.WorstDTgtDiff = FMath::Max(R.WorstDTgtDiff, Diff);
			if (Diff > 0.01)   // the gate tolerance; Tier-0 holds this at 0.000
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d dTgt: recorded %.3f, got %.3f"),
					i, Rec.DTgt, Mine.dTgt.value_or(-1.0)));
			}
			const FString MineShell = UTF8_TO_TCHAR(Mine.shell.value_or("he").c_str());
			if (!Rec.Shell.Equals(MineShell))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d shell: recorded %s, got %s"),
					i, *Rec.Shell, *MineShell));
			}
			const double Skew = std::fabs(Rec.T - Mine.t);
			R.WorstSkew = FMath::Max(R.WorstSkew, Skew);
			if (Skew > 0.5)
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d time skew %.3f s"), i, Skew));
			}
		}

		// the AAR: verdict, stars, outcome, rounds, dur
		TArray<const FRecEntry*> RecAars;
		for (const FRecEntry& E : Tr.Entries)
		{
			if (E.Kind == TEXT("aar")) { RecAars.Add(&E); }
		}
		std::vector<const sf::review::TlogEntry*> GotAars;
		for (const auto& E : Got)
		{
			if (E.kind == "aar") { GotAars.push_back(&E); }
		}
		if (RecAars.Num() != static_cast<int32>(GotAars.size()))
		{
			R.Fail(RecAars.Num() > 0 ? RecAars[0]->T : 1e299,
				FString::Printf(TEXT("aar count: recorded %d, got %d"),
					RecAars.Num(), static_cast<int32>(GotAars.size())));
		}
		const int32 NAar = FMath::Min(RecAars.Num(), static_cast<int32>(GotAars.size()));
		for (int32 i = 0; i < NAar; i++)
		{
			const FRecEntry& Rec = *RecAars[i];
			const sf::review::TlogEntry& Mine = *GotAars[static_cast<std::size_t>(i)];
			const FString MineMsg = UTF8_TO_TCHAR(Mine.msg.c_str());
			if (!Rec.Msg.Equals(MineMsg))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar verdict: recorded '%s', got '%s'"),
					*Rec.Msg, *MineMsg));
			}
			const int32 MineStars = Mine.stars.has_value() ? static_cast<int32>(*Mine.stars) : -1;
			if (Rec.Stars != MineStars)
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar stars: recorded %d, got %d"),
					Rec.Stars, MineStars));
			}
			const FString MineOutcome = UTF8_TO_TCHAR(Mine.outcome.value_or("").c_str());
			if (!Rec.Outcome.Equals(MineOutcome))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar outcome: recorded '%s', got '%s'"),
					*Rec.Outcome, *MineOutcome));
			}
			if (Rec.Rounds != Mine.rounds.value_or(-1.0))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar rounds: recorded %.0f, got %.0f"),
					Rec.Rounds, Mine.rounds.value_or(-1.0)));
			}
			if (Rec.Dur != Mine.dur.value_or(-1.0))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar dur: recorded %.0f, got %.0f"),
					Rec.Dur, Mine.dur.value_or(-1.0)));
			}
		}
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireSimHostReplayTest,
	"SHITFIRE.Parity.SimHostReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireSimHostReplayTest::RunTest(const FString& Parameters)
{
	FTranscript Tr;
	if (!TestTrue(TEXT("transcript fixture loads"), LoadTranscript(Tr)))
	{
		return false;
	}
	const double LastRecT = Tr.Entries.Last().T;

	// send list = parse entries (raw player transmissions)
	TArray<const FRecEntry*> Sends;
	for (const FRecEntry& E : Tr.Entries)
	{
		if (E.Kind == TEXT("parse")) { Sends.Add(&E); }
	}

	FReplayFlavor Flavor(Tr);

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

	// candidate schedule search (replay_test.cpp:604-662, bounded backtrack)
	const TArray<int64> StartCands = GridCandidates(Tr.Entries[0].T);
	TArray<TArray<int64>> SendCands;
	for (const FRecEntry* S : Sends) { SendCands.Add(GridCandidates(S->T)); }

	FAttemptResult Best;
	Best.bPass = false;
	Best.Errors.Add(TEXT("no attempt ran"));
	int32 Attempts = 0;
	bool bPassed = false;

	for (int32 Si = 0; Si < StartCands.Num() && !bPassed; Si++)
	{
		TArray<int32> Pick;
		Pick.Init(0, SendCands.Num());
		for (int32 Attempt = 0; Attempt < 64; Attempt++)
		{
			// materialize strictly-increasing send times after the start frame
			TArray<int64> SendMs;
			SendMs.Init(0, SendCands.Num());
			int64 Prev = StartCands[Si];
			bool bFeasible = true;
			for (int32 i = 0; i < SendCands.Num(); i++)
			{
				int64 Chosen = -1;
				for (int32 C = Pick[i]; C < SendCands[i].Num(); C++)
				{
					if (SendCands[i][C] > Prev)
					{
						Chosen = SendCands[i][C];
						Pick[i] = C;
						break;
					}
				}
				if (Chosen < 0) { bFeasible = false; break; }
				SendMs[i] = Chosen;
				Prev = Chosen;
			}
			if (!bFeasible) { break; }

			// ---- one attempt through the HOST ---------------------------
			Flavor.ResetRun();
			Collector->ResetAll();
			Host->SetFlavorRngForNextLaunch(&Flavor);
			FShitfireMissionParams P;
			P.ChapterId = Tr.Chapter;
			P.SkirmishType = Tr.ScenarioType;
			P.Difficulty = Tr.Difficulty;
			P.MissionSeed = Tr.SeedMission;
			P.TerrainSeed = Tr.SeedTerrain;
			P.StartMs = StartCands[Si];
			Host->LaunchMission(P);
			Host->SetTimeScale(1000.0);
			for (int32 i = 0; i < Sends.Num(); i++)
			{
				Host->QueueMessageAtSimMs(SendMs[i], Sends[i]->Msg);
			}

			// drive: 16 ms real ticks = 16 s sim each at 1000x
			int32 Guard = 0;
			while (Host->GetSimNow() < LastRecT + 12.0 && Guard++ < 5000)
			{
				Host->Tick(0.016f);
			}
			// trailing drain (replay_test.cpp:579-593 horizon)
			while (Host->GetPendingSimEventCount() > 0 &&
			       Host->GetSimNow() <= LastRecT + 240.0 && Guard++ < 20000)
			{
				Host->Tick(0.016f);
			}

			const FAttemptResult R = CompareRun(Tr, Host->GetSimCore()->tlog().entries());
			Attempts++;
			if (R.bPass)
			{
				Best = R;
				bPassed = true;
				break;
			}
			if (!Best.bPass &&
			    (Best.Errors.Num() == 1 || R.Errors.Num() < Best.Errors.Num() ||
			     R.FirstFailT > Best.FirstFailT))
			{
				Best = R;
			}
			// bump the latest send at or before the first failure
			const int64 FailMs = DeliveryFrameMs(Best.bPass ? 0 : R.FirstFailT);
			int32 Bump = -1;
			for (int32 i = SendMs.Num() - 1; i >= 0; i--)
			{
				if (SendMs[i] <= FailMs && Pick[i] + 1 < SendCands[i].Num())
				{
					Bump = i;
					break;
				}
			}
			if (Bump < 0) { break; }
			Pick[Bump]++;
			for (int32 i = Bump + 1; i < Pick.Num(); i++) { Pick[i] = 0; }
		}
	}

	TestTrue(TEXT("SimHost replay reproduces the recording"), bPassed);
	for (const FString& E : Best.Errors)
	{
		if (!bPassed) { AddError(E); }
	}
	AddInfo(FString::Printf(
		TEXT("mission_05 through SimHost: %s after %d attempt(s); worst |dTgt diff| %.3f m (tol 0.01), worst time skew %.3f s (tol 0.5)"),
		bPassed ? TEXT("PASS") : TEXT("FAIL"), Attempts, Best.WorstDTgtDiff, Best.WorstSkew));

	// ---- delegate surface carried the same story ------------------------
	if (bPassed)
	{
		const std::vector<sf::review::TlogEntry>& Entries = Host->GetSimCore()->tlog().entries();
		int32 TlogImpacts = 0, TlogFdc = 0, HeImpacts = 0;
		for (const auto& E : Entries)
		{
			if (E.kind == "impact")
			{
				TlogImpacts++;
				if (E.shell.value_or("he") == "he") { HeImpacts++; }
			}
			if (E.kind == "fdc") { TlogFdc++; }
		}
		TestEqual(TEXT("OnImpact broadcasts == TLOG impact rows"),
			Collector->Impacts.Num(), TlogImpacts);
		TestEqual(TEXT("OnFdcLine broadcasts == TLOG fdc rows"),
			Collector->FdcLines, TlogFdc);
		TestEqual(TEXT("OnBurstCue broadcasts == HE impacts"),
			Collector->BurstCues, HeImpacts);
		TestEqual(TEXT("OnAarReady fired once"), Collector->Aars.Num(), 1);
		if (Collector->Aars.Num() == 1)
		{
			for (const FRecEntry& E : Tr.Entries)
			{
				if (E.Kind == TEXT("aar"))
				{
					TestEqual(TEXT("OnAarReady verdict"), Collector->Aars[0].Verdict, E.Msg);
					TestEqual(TEXT("OnAarReady stars"), Collector->Aars[0].Stars, E.Stars);
				}
			}
		}
		TestTrue(TEXT("OnShotCue fired"), Collector->ShotCues > 0);
		TestEqual(TEXT("shot cues == splash cues (proword pairs)"),
			Collector->ShotCues, Collector->SplashCues);
		TestTrue(TEXT("OnStateChange fired"), Collector->StateChanges.Num() > 0);
		AddInfo(FString::Printf(
			TEXT("delegates: %d impacts, %d fdc, %d sys, %d bursts, %d shots, %d splashes, %d state changes, %d aar"),
			Collector->Impacts.Num(), Collector->FdcLines, Collector->SysLines,
			Collector->BurstCues, Collector->ShotCues, Collector->SplashCues,
			Collector->StateChanges.Num(), Collector->Aars.Num()));
	}

	Host->SetFlavorRngForNextLaunch(nullptr);
	Host->ShutdownMission();
	Collector->RemoveFromRoot();
	Host->RemoveFromRoot();
	HostOuter->RemoveFromRoot();
	return bPassed;
}

#endif // WITH_DEV_AUTOMATION_TESTS
