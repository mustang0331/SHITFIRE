// SHITFIRE.UI.CommsLogBudget - P4 gate (a): append 2,000 log lines into the
// live comms widget, in a real Slate window, without any single game-thread
// append batch costing more than 2 ms. The panel rides a UListView (entry
// virtualization), so the append path is an item push + refresh request and
// only visible rows own widgets - this test is the proof, not the hope.
//
// Measurement: lines land 50 per engine frame (a full mission books ~100
// TLOG rows TOTAL, so 50/frame is far beyond any live burst). The gate is
// the max per-frame append-batch duration; whole-frame deltas are reported
// as info (they include the whole editor, so they are context, not a gate).
//
// Run:
//   UnrealEditor-Cmd.exe <proj> -ExecCmds="Automation RunTests SHITFIRE.UI.CommsLogBudget"
//       -unattended -testexit="Automation Test Queue Empty"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ShitfireUiTestUtil.h"
#include "ui/CommsLogWidget.h"

#include "Components/ListView.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FBudgetCtx
	{
		FAutomationTestBase* Test = nullptr;
		TStrongObjectPtr<UShitfireCommsWidget> Widget;
		TSharedPtr<SWindow> Window;

		int32 Appended = 0;
		int32 WarmupAppended = 0;
		int32 SettleFramesLeft = 15;

		double MaxBatchMs = 0.0;
		double TotalBatchMs = 0.0;
		int32 Batches = 0;
		TArray<double> FrameMs;
		double LastUpdateTime = 0.0;

		static constexpr int32 WarmupLines = 50;
		static constexpr int32 GateLines = 2000;
		static constexpr int32 PerFrame = 50;
	};

	// Realistic content mix - real net strings of the lengths the FDC speaks
	// (wrapping is the expensive part of a text row; measure it honestly).
	void AppendOne(UShitfireCommsWidget& W, int32 I)
	{
		static const TCHAR* FdcLong =
			TEXT("MESSAGE TO OBSERVER: HELLHOUND, ONE GUN IN ADJUST, BATTERY IN EFFECT, ")
			TEXT("SHELL HE, FUZE QUICK IN EFFECT, FOUR ROUNDS, TARGET AA7001, OVER.");
		static const TCHAR* FdcShort = TEXT("SHOT, OVER.");
		static const TCHAR* ObsLine =
			TEXT("ADJUST FIRE, GRID 245 523, INFANTRY IN THE OPEN, OVER");
		static const TCHAR* SysLine =
			TEXT("The FDC read back your call. Check the readback against what you meant, then wait for the MTO.");
		switch (I % 4)
		{
		case 0: W.AppendLine(EShitfireLogRole::Fdc, I * 1.6, TEXT("HELLHOUND FIRES"), FdcLong); break;
		case 1: W.AppendLine(EShitfireLogRole::Obs, I * 1.6, TEXT("MUSTANG 12"), ObsLine); break;
		case 2: W.AppendLine(EShitfireLogRole::Sys, I * 1.6, FString(), SysLine); break;
		default: W.AppendLine(EShitfireLogRole::Fdc, I * 1.6, TEXT("HELLHOUND FIRES"), FdcShort); break;
		}
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBudgetSetup, TSharedPtr<FBudgetCtx>, Ctx);
bool FBudgetSetup::Update()
{
	UWorld* World = ShitfireUiTest::AnyWorld();
	if (!Ctx->Test->TestNotNull(TEXT("a world exists for widget creation"), World))
	{
		return true;
	}
	UShitfireCommsWidget* W = CreateWidget<UShitfireCommsWidget>(World, UShitfireCommsWidget::StaticClass());
	if (!Ctx->Test->TestNotNull(TEXT("comms widget created"), W))
	{
		return true;
	}
	Ctx->Widget = TStrongObjectPtr<UShitfireCommsWidget>(W);
	// The game mounts it at 600x261 (#comms CSS default) - test at that size.
	const TSharedRef<SWidget> Content = SNew(SBox)
		.WidthOverride(600.f).HeightOverride(261.f)
		[ W->TakeWidget() ];
	Ctx->Window = ShitfireUiTest::OpenWindow(FVector2D(640.f, 300.f), Content,
		TEXT("SHITFIRE comms budget"));
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBudgetPump, TSharedPtr<FBudgetCtx>, Ctx);
bool FBudgetPump::Update()
{
	UShitfireCommsWidget* W = Ctx->Widget.Get();
	if (!W)
	{
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	if (Ctx->LastUpdateTime > 0.0)
	{
		Ctx->FrameMs.Add((Now - Ctx->LastUpdateTime) * 1000.0);
	}
	Ctx->LastUpdateTime = Now;

	// Warmup first (font atlas fill, first widget generation) - ungated.
	if (Ctx->WarmupAppended < FBudgetCtx::WarmupLines)
	{
		for (int32 i = 0; i < 10 && Ctx->WarmupAppended < FBudgetCtx::WarmupLines; i++)
		{
			AppendOne(*W, Ctx->WarmupAppended++);
		}
		Ctx->FrameMs.Reset();
		return false;
	}

	const double T0 = FPlatformTime::Seconds();
	for (int32 i = 0; i < FBudgetCtx::PerFrame && Ctx->Appended < FBudgetCtx::GateLines; i++)
	{
		AppendOne(*W, FBudgetCtx::WarmupLines + Ctx->Appended);
		Ctx->Appended++;
	}
	const double BatchMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	Ctx->MaxBatchMs = FMath::Max(Ctx->MaxBatchMs, BatchMs);
	Ctx->TotalBatchMs += BatchMs;
	Ctx->Batches++;
	return Ctx->Appended >= FBudgetCtx::GateLines;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FBudgetAssert, TSharedPtr<FBudgetCtx>, Ctx);
bool FBudgetAssert::Update()
{
	if (Ctx->SettleFramesLeft-- > 0)
	{
		return false;   // let the list generate/paint the final state
	}
	UShitfireCommsWidget* W = Ctx->Widget.Get();
	FAutomationTestBase* T = Ctx->Test;
	if (W)
	{
		T->TestEqual(TEXT("all lines held (warmup + 2000)"),
			W->NumLines(), FBudgetCtx::WarmupLines + FBudgetCtx::GateLines);
		// SListView only holds a row widget for lines in (or near) view
		int32 LiveRows = 0;
		for (int32 i = 0; i < W->NumLines(); i++)
		{
			if (W->HasRowWidgetFor(i)) { LiveRows++; }
		}
		T->TestTrue(TEXT("list rendered entries"), LiveRows > 0);
		T->TestTrue(FString::Printf(TEXT("virtualized: %d live row widgets for %d lines (< 80)"),
			LiveRows, W->NumLines()), LiveRows < 80);
		T->TestTrue(TEXT("auto-scroll pinned at the bottom"), W->IsPinnedToBottom());
	}
	T->TestTrue(FString::Printf(
		TEXT("budget gate: max 50-line append batch %.3f ms < 2 ms"), Ctx->MaxBatchMs),
		Ctx->MaxBatchMs < 2.0);

	double MedianFrame = 0.0, MaxFrame = 0.0;
	if (Ctx->FrameMs.Num() > 0)
	{
		TArray<double> Sorted = Ctx->FrameMs;
		Sorted.Sort();
		MedianFrame = Sorted[Sorted.Num() / 2];
		MaxFrame = Sorted.Last();
	}
	T->AddInfo(FString::Printf(
		TEXT("append batches: %d x <=50 lines; max %.3f ms, mean %.3f ms; editor frame during appends: median %.1f ms, max %.1f ms (context, whole-editor)"),
		Ctx->Batches, Ctx->MaxBatchMs,
		Ctx->Batches ? Ctx->TotalBatchMs / Ctx->Batches : 0.0, MedianFrame, MaxFrame));

	const FString Shot = W
		? ShitfireUiTest::SaveWidgetScreenshot(Ctx->Window->GetContent(), TEXT("comms_budget_2050_lines"))
		: FString();
	if (!Shot.IsEmpty())
	{
		T->AddInfo(FString::Printf(TEXT("screenshot: %s"), *Shot));
	}

	if (Ctx->Window.IsValid())
	{
		Ctx->Window->RequestDestroyWindow();
		Ctx->Window.Reset();
	}
	Ctx->Widget.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireCommsLogBudgetTest,
	"SHITFIRE.UI.CommsLogBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireCommsLogBudgetTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBudgetCtx> Ctx = MakeShared<FBudgetCtx>();
	Ctx->Test = this;
	ADD_LATENT_AUTOMATION_COMMAND(FBudgetSetup(Ctx));
	ADD_LATENT_AUTOMATION_COMMAND(FBudgetPump(Ctx));
	ADD_LATENT_AUTOMATION_COMMAND(FBudgetAssert(Ctx));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
