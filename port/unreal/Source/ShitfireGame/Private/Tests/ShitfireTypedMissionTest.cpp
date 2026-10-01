// SHITFIRE.UI.TypedMission - P4 gate (b): one Level-3 oracle transcript
// (mission_05 ch1-2 bunker-bracket, the SimHostReplay transcript) driven
// end-to-end through the TYPED INPUT UI PATH: every player transmission is
// typed character-by-character into the live UEditableTextBox through
// FSlateApplication (the real keyboard route), committed with Enter, and
// reaches the sim only via the widget's submit handler -> SubmitMessage.
// Nothing is injected at the SimHost.
//
// Exactness: the host advances at TimeScale 1000 via manual Tick calls in
// 16 ms sim frames; before each send the clock is PARKED on that send's
// exact frame (the Tier-0 advanceTo(sendMs)+submitMessage semantics that
// QueueMessageAtSimMs reproduces inside StepFrames). The host is standalone
// (InitializeStandalone) so nothing else ticks it - the same arrangement as
// SHITFIRE.Parity.SimHostReplay.
//
// Schedule: first feasible candidate schedule only (no backtracking) - the
// SimHostReplay gate is GREEN on this transcript and its comment records
// that Tier-0 replays it exactly on the first candidate schedule; a failure
// here reports the comparator's story rather than silently retrying.
//
// Asserts:
//   - CompareRun: impacts (count/order/msg/dTgt/shell/skew) + AAR verdict/
//     stars/outcome/rounds/dur match the recording;
//   - OnAarReady carried the recorded verdict + stars;
//   - the comms log rendered EVERY net line: item count and per-line text
//     equal the TLOG's obs/fdc/sys/caller/note rows in order (syserr rows
//     are transcript-only in the browser and stay off-screen here too);
//   - the AAR overlay is up with the recorded verdict + star row.
// Screenshots (gate c): log+strip mid-mission, and the AAR panel.
//
// The transcript/flavor/comparator machinery is duplicated VERBATIM from
// Private/Tests/ShitfireSimHostReplayTest.cpp:55-365 (BUILDER-BRIDGE's
// file, not edited from here) - single-source cleanup is theirs to offer.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "ShitfireFixtures.h"
#include "ShitfireGame.h"
#include "ShitfireSimHostReplayCollector.h"
#include "ShitfireUiTestUtil.h"
#include "bridge/ShitfireSimHost.h"
#include "ui/AarPanelWidget.h"
#include "ui/CommsLogWidget.h"
#include "ui/NetHudWidget.h"

#include "Components/EditableTextBox.h"
#include "UObject/StrongObjectPtr.h"

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

	/* ---- recorded transcript (ShitfireSimHostReplayTest.cpp:59-121) ----- */

	struct FRecEntry
	{
		double T = 0;
		FString Kind;
		FString Msg;
		double DTgt = 0;
		FString Shell;
		int32 Stars = -1;
		FString Outcome;
		double Rounds = 0;
		double Dur = 0;
	};

	struct FTranscript
	{
		FString Chapter;
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
		Meta->TryGetStringField(TEXT("chapter"), Out.Chapter);
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

	/* ---- clock helpers (ShitfireSimHostReplayTest.cpp:125-148) ----------- */

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

	/* ---- gated-flavor replay (ShitfireSimHostReplayTest.cpp:154-239) ----- */

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

	private:
		sf::Mulberry32 Rng;
		std::map<sf::doctrine::FlavorGate, std::vector<double>> Times;
		std::map<sf::doctrine::FlavorGate, std::set<std::size_t>> Consumed;
	};

	/* ---- comparator (ShitfireSimHostReplayTest.cpp:244-365) -------------- */

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
			if (Diff > 0.01)
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

	FString StarStr(int32 N)
	{
		FString Out;
		for (int32 i = 0; i < 5; i++)
		{
			Out += (i < N) ? TEXT("★") : TEXT("☆");
		}
		return Out;
	}

	/* ---- typed-drive context --------------------------------------------- */

	struct FTypedCtx
	{
		FAutomationTestBase* Test = nullptr;
		FTranscript Tr;
		double LastRecT = 0;
		TUniquePtr<FReplayFlavor> Flavor;
		TStrongObjectPtr<UGameInstance> HostOuter;
		TStrongObjectPtr<UShitfireSimHost> Host;
		TStrongObjectPtr<UShitfireReplayCollector> Collector;
		TStrongObjectPtr<UShitfireNetHudWidget> NetHud;
		TSharedPtr<SWindow> Window;

		TArray<FString> SendMsgs;
		TArray<int64> SendMs;
		int32 NextSend = 0;
		int32 Phase = 0;          // 0 sends, 1 run-out, 2 drain, 3 assert
		int32 GuardFrames = 0;
		bool bDone = false;
		FString MidShotPath, AarShotPath;

		static constexpr int32 FramesPerUpdate = 4000;   // 64 s sim per editor frame
		static constexpr int32 GuardMax = 400000;

		void Fail(const FString& Why)
		{
			Test->AddError(Why);
			bDone = true;
		}

		/** One-frame ticks toward TargetMs; true when parked exactly on it. */
		bool AdvanceToward(int64 TargetMs)
		{
			UShitfireSimHost* H = Host.Get();
			int32 N = 0;
			while (H->GetSimNowMs() < TargetMs && N++ < FramesPerUpdate && GuardFrames++ < GuardMax)
			{
				H->Tick(1.6e-5f);   // 16 ms sim at TimeScale 1000, one frame per call
			}
			return H->GetSimNowMs() >= TargetMs;
		}
	};
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTypedSetup, TSharedPtr<FTypedCtx>, Ctx);
bool FTypedSetup::Update()
{
	FAutomationTestBase* T = Ctx->Test;
	if (!T->TestTrue(TEXT("transcript fixture loads"), LoadTranscript(Ctx->Tr)))
	{
		Ctx->bDone = true;
		return true;
	}
	Ctx->LastRecT = Ctx->Tr.Entries.Last().T;

	// send list + first feasible candidate schedule (strictly increasing)
	const TArray<int64> StartCands = GridCandidates(Ctx->Tr.Entries[0].T);
	if (!T->TestTrue(TEXT("start frame candidates exist"), StartCands.Num() > 0))
	{
		Ctx->bDone = true;
		return true;
	}
	const int64 StartMs = StartCands[0];
	int64 Prev = StartMs;
	for (const FRecEntry& E : Ctx->Tr.Entries)
	{
		if (E.Kind != TEXT("parse")) { continue; }
		int64 Chosen = -1;
		for (int64 C : GridCandidates(E.T))
		{
			if (C > Prev) { Chosen = C; break; }
		}
		if (Chosen < 0)
		{
			Ctx->Fail(FString::Printf(TEXT("no feasible send frame for t=%.1f"), E.T));
			return true;
		}
		Ctx->SendMsgs.Add(E.Msg);
		Ctx->SendMs.Add(Chosen);
		Prev = Chosen;
	}
	T->AddInfo(FString::Printf(TEXT("%d typed sends, start frame %lld ms, TimeScale 1000"),
		Ctx->SendMsgs.Num(), StartMs));

	// standalone host (ShitfireSimHostReplayTest.cpp:391-401 arrangement)
	Ctx->Flavor = MakeUnique<FReplayFlavor>(Ctx->Tr);
	UGameInstance* HostOuter = NewObject<UGameInstance>(GEngine);
	Ctx->HostOuter = TStrongObjectPtr<UGameInstance>(HostOuter);
	UShitfireSimHost* Host = NewObject<UShitfireSimHost>(HostOuter);
	Ctx->Host = TStrongObjectPtr<UShitfireSimHost>(Host);
	Host->InitializeStandalone();
	UShitfireReplayCollector* Collector = NewObject<UShitfireReplayCollector>(GetTransientPackage());
	Ctx->Collector = TStrongObjectPtr<UShitfireReplayCollector>(Collector);
	Collector->BindAll(Host);

	// the net HUD under test, in a real window at 1080p-ish proportions
	UWorld* World = ShitfireUiTest::AnyWorld();
	if (!T->TestNotNull(TEXT("a world exists for widget creation"), World))
	{
		Ctx->bDone = true;
		return true;
	}
	UShitfireNetHudWidget* NetHud = CreateWidget<UShitfireNetHudWidget>(World, UShitfireNetHudWidget::StaticClass());
	Ctx->NetHud = TStrongObjectPtr<UShitfireNetHudWidget>(NetHud);
	NetHud->BindHost(Host);
	NetHud->SetMissionContext(Ctx->Tr.Chapter);
	const TSharedRef<SWidget> Content = SNew(SBox)
		.WidthOverride(1280.f).HeightOverride(800.f)
		[ NetHud->TakeWidget() ];
	Ctx->Window = ShitfireUiTest::OpenWindow(FVector2D(1300.f, 830.f), Content,
		TEXT("SHITFIRE typed mission"));

	// launch exactly like the replay gate
	Host->SetFlavorRngForNextLaunch(Ctx->Flavor.Get());
	FShitfireMissionParams P;
	P.ChapterId = Ctx->Tr.Chapter;
	P.SkirmishType = Ctx->Tr.ScenarioType;
	P.Difficulty = Ctx->Tr.Difficulty;
	P.MissionSeed = Ctx->Tr.SeedMission;
	P.TerrainSeed = Ctx->Tr.SeedTerrain;
	P.StartMs = StartMs;
	Host->LaunchMission(P);
	Host->SetTimeScale(1000.0);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTypedDrive, TSharedPtr<FTypedCtx>, Ctx);
bool FTypedDrive::Update()
{
	if (Ctx->bDone)
	{
		return true;
	}
	FAutomationTestBase* T = Ctx->Test;
	UShitfireSimHost* H = Ctx->Host.Get();
	UShitfireNetHudWidget* NetHud = Ctx->NetHud.Get();
	if (!H || !NetHud || !H->IsMissionLive())
	{
		Ctx->Fail(TEXT("host/widget vanished mid-drive"));
		return true;
	}
	UShitfireCommsWidget* Comms = NetHud->GetComms();

	if (Ctx->GuardFrames >= FTypedCtx::GuardMax)
	{
		Ctx->Fail(TEXT("frame guard tripped - sim never reached the mission end"));
		return true;
	}

	switch (Ctx->Phase)
	{
	case 0:   // park on each send frame, then type it through the widget
	{
		if (Ctx->NextSend >= Ctx->SendMs.Num())
		{
			Ctx->Phase = 1;
			return false;
		}
		const int64 Target = Ctx->SendMs[Ctx->NextSend];
		if (!Ctx->AdvanceToward(Target))
		{
			return false;   // keep advancing next editor frame
		}
		if (H->GetSimNowMs() != Target)
		{
			Ctx->Fail(FString::Printf(TEXT("overshot send frame: parked %lld, wanted %lld"),
				H->GetSimNowMs(), Target));
			return true;
		}

		// ---- the typed path: [ENTER]-focus seam, then real key events ----
		NetHud->FocusInput();
		if (!Comms || !Comms->IsInputFocused())
		{
			Ctx->Fail(FString::Printf(TEXT("send #%d: input box did not take focus"), Ctx->NextSend));
			return true;
		}
		const int32 LinesBefore = Comms->NumLines();
		ShitfireUiTest::TypeLineAndEnter(Ctx->SendMsgs[Ctx->NextSend]);
		// The obs echo lands synchronously at submit (HandleTx pumps the
		// transcript right after SubmitMessage); the core may book a sys
		// notice in the same submit, so count obs rows rather than lines.
		// FDC replies follow on later sim frames.
		int32 NewObs = 0;
		for (int32 i = LinesBefore; i < Comms->NumLines(); i++)
		{
			if (Comms->LineRoleAt(i) == EShitfireLogRole::Obs) { NewObs++; }
		}
		if (NewObs != 1)
		{
			Ctx->Fail(FString::Printf(TEXT("send #%d: expected one obs echo after Enter, got %d (lines %d -> %d)"),
				Ctx->NextSend, NewObs, LinesBefore, Comms->NumLines()));
			return true;
		}
		if (!Comms->GetInputBox()->GetText().IsEmpty())
		{
			Ctx->Fail(FString::Printf(TEXT("send #%d: box not cleared after Enter"), Ctx->NextSend));
			return true;
		}
		Ctx->NextSend++;

		if (Ctx->NextSend == 3 && Ctx->Window.IsValid())
		{
			// mid-mission: log + strip with live traffic on screen (gate c)
			Ctx->MidShotPath = ShitfireUiTest::SaveWidgetScreenshot(
				Ctx->Window->GetContent(), TEXT("typed_mission_log_strip"));
		}
		return false;
	}
	case 1:   // run the recording out (replay_test horizon: last T + 12 s)
	{
		if (!Ctx->AdvanceToward(DeliveryFrameMs(Ctx->LastRecT + 12.0)))
		{
			return false;
		}
		Ctx->Phase = 2;
		return false;
	}
	case 2:   // trailing drain (ShitfireSimHostReplayTest.cpp:468-472)
	{
		if (H->GetPendingSimEventCount() > 0 &&
			H->GetSimNow() <= Ctx->LastRecT + 240.0)
		{
			if (!Ctx->AdvanceToward(H->GetSimNowMs() + 16 * FTypedCtx::FramesPerUpdate))
			{
				return false;
			}
			return false;
		}
		Ctx->Phase = 3;
		return false;
	}
	default:
		return true;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTypedAssert, TSharedPtr<FTypedCtx>, Ctx);
bool FTypedAssert::Update()
{
	FAutomationTestBase* T = Ctx->Test;
	UShitfireSimHost* H = Ctx->Host.Get();
	UShitfireNetHudWidget* NetHud = Ctx->NetHud.Get();
	UShitfireReplayCollector* Collector = Ctx->Collector.Get();

	if (!Ctx->bDone && H && H->IsMissionLive() && NetHud && Collector)
	{
		const std::vector<sf::review::TlogEntry>& Entries = H->GetSimCore()->tlog().entries();

		// ---- parity: the typed path reproduced the recording -------------
		const FAttemptResult R = CompareRun(Ctx->Tr, Entries);
		T->TestTrue(TEXT("typed mission reproduces the recording (first candidate schedule)"), R.bPass);
		for (const FString& E : R.Errors)
		{
			T->AddError(E);
		}
		T->AddInfo(FString::Printf(
			TEXT("typed mission_05: worst |dTgt diff| %.3f m (tol 0.01), worst time skew %.3f s (tol 0.5)"),
			R.WorstDTgtDiff, R.WorstSkew));

		// ---- the recorded AAR reached the delegate surface ---------------
		T->TestEqual(TEXT("OnAarReady fired once"), Collector->Aars.Num(), 1);
		const FRecEntry* RecAar = nullptr;
		for (const FRecEntry& E : Ctx->Tr.Entries)
		{
			if (E.Kind == TEXT("aar")) { RecAar = &E; }
		}
		if (RecAar && Collector->Aars.Num() == 1)
		{
			T->TestEqual(TEXT("OnAarReady verdict"), Collector->Aars[0].Verdict, RecAar->Msg);
			T->TestEqual(TEXT("OnAarReady stars"), Collector->Aars[0].Stars, RecAar->Stars);
		}

		// ---- the log rendered every net line, in order -------------------
		TArray<FString> Expected;
		int32 ObsRows = 0;
		for (const sf::review::TlogEntry& E : Entries)
		{
			if (E.kind == "obs" || E.kind == "fdc" || E.kind == "sys" ||
				E.kind == "caller" || E.kind == "note")
			{
				Expected.Add(UTF8_TO_TCHAR(E.msg.c_str()));
				if (E.kind == "obs") { ObsRows++; }
			}
		}
		T->TestEqual(TEXT("core echoed one obs row per typed send"), ObsRows, Ctx->SendMsgs.Num());
		UShitfireCommsWidget* Comms = NetHud->GetComms();
		if (T->TestNotNull(TEXT("comms widget"), Comms))
		{
			T->TestEqual(TEXT("log line count == TLOG net rows"), Comms->NumLines(), Expected.Num());
			const int32 N = FMath::Min(Comms->NumLines(), Expected.Num());
			int32 Mismatches = 0;
			for (int32 i = 0; i < N; i++)
			{
				const FString Got = Comms->LineMsgAt(i);
				if (Got != Expected[i])
				{
					if (Mismatches++ < 5)
					{
						T->AddError(FString::Printf(TEXT("log line %d: expected '%s', got '%s'"),
							i, *Expected[i], *Got));
					}
				}
			}
			T->TestEqual(TEXT("log line texts match the TLOG verbatim"), Mismatches, 0);
			int32 LiveRows = 0;
			for (int32 i = 0; i < Comms->NumLines(); i++)
			{
				if (Comms->HasRowWidgetFor(i)) { LiveRows++; }
			}
			T->TestTrue(TEXT("list rendered entry widgets"), LiveRows > 0);
		}

		// ---- the AAR overlay is up with the recorded verdict/stars -------
		UShitfireAarWidget* Aar = NetHud->GetAar();
		if (T->TestNotNull(TEXT("aar widget"), Aar) && RecAar)
		{
			T->TestTrue(TEXT("AAR overlay shown on OnAarReady"), Aar->IsShowing());
			T->TestEqual(TEXT("AAR panel verdict text"), Aar->VerdictLineText(), RecAar->Msg);
			if (RecAar->Stars >= 0)
			{
				T->TestEqual(TEXT("AAR panel star row"), Aar->StarsLineText(), StarStr(RecAar->Stars));
			}
			if (Ctx->Window.IsValid())
			{
				Ctx->AarShotPath = ShitfireUiTest::SaveWidgetScreenshot(
					Ctx->Window->GetContent(), TEXT("typed_mission_aar"));
			}
		}

		if (!Ctx->MidShotPath.IsEmpty())
		{
			T->AddInfo(FString::Printf(TEXT("screenshot (log+strip mid-mission): %s"), *Ctx->MidShotPath));
		}
		if (!Ctx->AarShotPath.IsEmpty())
		{
			T->AddInfo(FString::Printf(TEXT("screenshot (AAR panel): %s"), *Ctx->AarShotPath));
		}
	}

	// ---- teardown --------------------------------------------------------
	if (H)
	{
		H->SetFlavorRngForNextLaunch(nullptr);
		H->ShutdownMission();
	}
	if (Ctx->Window.IsValid())
	{
		Ctx->Window->RequestDestroyWindow();
		Ctx->Window.Reset();
	}
	Ctx->NetHud.Reset();
	Ctx->Collector.Reset();
	Ctx->Host.Reset();
	Ctx->HostOuter.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireTypedMissionTest,
	"SHITFIRE.UI.TypedMission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireTypedMissionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FTypedCtx> Ctx = MakeShared<FTypedCtx>();
	Ctx->Test = this;
	ADD_LATENT_AUTOMATION_COMMAND(FTypedSetup(Ctx));
	ADD_LATENT_AUTOMATION_COMMAND(FTypedDrive(Ctx));
	ADD_LATENT_AUTOMATION_COMMAND(FTypedAssert(Ctx));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
