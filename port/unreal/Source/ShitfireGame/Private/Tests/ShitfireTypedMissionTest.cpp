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
// The transcript/flavor/comparator machinery is ONE copy in
// parity/ShitfireTranscriptReplay.{h,cpp} (shared with SimHostReplay and the
// -ShitfireParityGate runner); this file owns only the typed-drive context.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "ShitfireFixtures.h"
#include "ShitfireGame.h"
#include "ShitfireSimHostReplayCollector.h"
#include "ShitfireUiTestUtil.h"
#include "bridge/ShitfireSimHost.h"
#include "parity/ShitfireTranscriptReplay.h"
#include "ui/AarPanelWidget.h"
#include "ui/CommsLogWidget.h"
#include "ui/NetHudWidget.h"

#include "Components/EditableTextBox.h"
#include "UObject/StrongObjectPtr.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/review/tlog.h"
#include "shitfire/sim_core.h"

#include <vector>

namespace
{
	namespace sf = shitfire;

	constexpr const TCHAR* kTranscript = TEXT("level3/mission_05_ch1-2-bunker-bracket.json");

	/* ---- transcript / flavor / comparator: parity/ShitfireTranscriptReplay.h
	   LoadTranscript, GridCandidates, DeliveryFrameMs, FReplayFlavor and
	   CompareRun are the shared copy (same checks and tolerances the
	   SimHostReplay gate asserts); nothing is re-implemented here. */
	using ShitfireParity::FRecEntry;
	using ShitfireParity::FTranscript;
	using ShitfireParity::FReplayFlavor;
	using ShitfireParity::FAttemptResult;
	using ShitfireParity::LoadTranscript;
	using ShitfireParity::GridCandidates;
	using ShitfireParity::DeliveryFrameMs;
	using ShitfireParity::CompareRun;

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
	if (!T->TestTrue(TEXT("transcript fixture loads"), LoadTranscript(kTranscript, Ctx->Tr)))
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

	// standalone host (ShitfireSimHostReplayTest.cpp:52-62 arrangement)
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
	case 2:   // trailing drain (ShitfireTranscriptReplay.cpp:367-372)
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
