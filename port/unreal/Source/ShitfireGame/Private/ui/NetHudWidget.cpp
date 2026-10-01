#include "ui/NetHudWidget.h"
#include "ShitfireGame.h"
#include "ShitfireWorldActor.h"
#include "observer/ObserverPawn.h"
#include "ui/AarPanelWidget.h"
#include "ui/CommsLogWidget.h"
#include "ui/NetHudStyle.h"
#include "ui/StatusStripWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/review/tlog.h"
#include "shitfire/sim_core.h"

void UShitfireNetHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildTree();
}

TSharedRef<SWidget> UShitfireNetHudWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UShitfireNetHudWidget::BuildTree()
{
	if (bTreeBuilt)
	{
		return;
	}
	bTreeBuilt = true;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("NetCanvas"));
	WidgetTree->RootWidget = Canvas;

	// ---- status strip: full-width top band (#topbar, head.html:21-26) ------
	Strip = CreateWidget<UShitfireStatusStripWidget>(this, UShitfireStatusStripWidget::StaticClass());
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Strip))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
		S->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		S->SetAutoSize(true);
	}

	// ---- comms panel: left 12 / bottom 12, 600x261 (#comms CSS default,
	// head.html:46-51; drag/resize/persistence is browser chrome, not ported)
	Comms = CreateWidget<UShitfireCommsWidget>(this, UShitfireCommsWidget::StaticClass());
	Comms->OnTx.BindUObject(this, &UShitfireNetHudWidget::HandleTx);
	Comms->OnFocusChanged.BindUObject(this, &UShitfireNetHudWidget::HandleInputFocusChanged);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Comms))
	{
		S->SetAnchors(FAnchors(0.f, 1.f));
		S->SetAlignment(FVector2D(0.0, 1.0));
		S->SetPosition(FVector2D(12.0, -12.0));
		S->SetSize(FVector2D(600.0, 261.0));
	}

	// ---- AAR overlay: full screen, z-top (#aar z 10, head.html:184) --------
	Aar = CreateWidget<UShitfireAarWidget>(this, UShitfireAarWidget::StaticClass());
	Aar->OnDismissed.BindUObject(this, &UShitfireNetHudWidget::HandleAarDismissed);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Aar))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
		S->SetZOrder(10);
	}
}

void UShitfireNetHudWidget::BindHost(UShitfireSimHost* InHost)
{
	BuildTree();
	if (!InHost || Host.Get() == InHost)
	{
		return;
	}
	Host = InHost;
	InHost->OnFdcLine.AddDynamic(this, &UShitfireNetHudWidget::HandleFdcLine);
	InHost->OnSysLine.AddDynamic(this, &UShitfireNetHudWidget::HandleSysLine);
	InHost->OnShotCue.AddDynamic(this, &UShitfireNetHudWidget::HandleShotCue);
	InHost->OnSplashCue.AddDynamic(this, &UShitfireNetHudWidget::HandleSplashCue);
	InHost->OnStateChange.AddDynamic(this, &UShitfireNetHudWidget::HandleStateChange);
	InHost->OnAarReady.AddDynamic(this, &UShitfireNetHudWidget::HandleAarReady);
	if (Strip)
	{
		Strip->SetHost(InHost);
		Strip->SetStateText(InHost->GetMissionStateString().IsEmpty()
			? TEXT("OBSERVING") : InHost->GetMissionStateString());
	}
	TlogUiSeen = 0;
}

void UShitfireNetHudWidget::BindObserver(AObserverPawn* Pawn)
{
	if (Pawn)
	{
		// The documented [ENTER] seam (ObserverPawn.h:79-82).
		Pawn->OnFocusTextRequested.AddUObject(this, &UShitfireNetHudWidget::FocusInput);
	}
}

void UShitfireNetHudWidget::SetMissionContext(const FString& ChapterId)
{
	BuildTree();
	MissionChapterId = ChapterId;
	bMissionContextSet = true;
	if (Strip)
	{
		Strip->SetChapterContext(ChapterId);
	}
}

void UShitfireNetHudWidget::FocusInput()
{
	BuildTree();
	if (Aar && Aar->IsShowing())
	{
		return;   // UX9: an open surface owns the keyboard
	}
	if (Comms)
	{
		Comms->FocusInput();
	}
}

void UShitfireNetHudWidget::AppendLocalSys(const FString& Msg)
{
	BuildTree();
	if (Comms)
	{
		UShitfireSimHost* H = Host.Get();
		Comms->AppendLine(EShitfireLogRole::Sys,
			(H && H->IsMissionLive()) ? H->GetSimNow() : 0.0, FString(), Msg);
	}
}

// ---------------------------------------------------------------- handlers ----

void UShitfireNetHudWidget::HandleFdcLine(double SimT, const FString& Call, const FString& Msg)
{
	// The line itself reaches the log through PumpTranscript (one ordered
	// source - header note); this handler only drives the latch.
	// Danger-close latch: the FDC read-back appends ", DANGER CLOSE"
	// (10-mission.js:439) and the acknowledgment quips speak it (07:174).
	if (!bDangerClose && Msg.Contains(TEXT("DANGER CLOSE")))
	{
		bDangerClose = true;
		if (Strip) { Strip->SetDangerClose(true); }
	}
}

void UShitfireNetHudWidget::HandleSysLine(double SimT, const FString& Msg, bool bError)
{
	if (bError)
	{
		// 'syserr' rows are transcript-only in the browser (no log() site);
		// surfaced to the dev log so failures are never silent.
		UE_LOG(LogShitfire, Warning, TEXT("net syserr @%.1f: %s"), SimT, *Msg);
	}
	// plain sys rows render through PumpTranscript, in transcript order
}

void UShitfireNetHudWidget::HandleShotCue(double SimT)
{
	if (Strip) { Strip->FlashCue(TEXT("● SHOT"), ShitfireUi::Amber); }
	if (Comms) { Comms->Flash(ShitfireUi::Amber); }
}

void UShitfireNetHudWidget::HandleSplashCue(double SimT)
{
	if (Strip) { Strip->FlashCue(TEXT("● SPLASH"), ShitfireUi::WarnRed); }
	if (Comms) { Comms->Flash(ShitfireUi::WarnRed); }
}

void UShitfireNetHudWidget::HandleStateChange(const FString& OldState, const FString& NewState, double SimT)
{
	if (Strip)
	{
		Strip->SetStateText(NewState);
	}
	// Mission closed, next call starts clean - drop the latch.
	if (bDangerClose && NewState == TEXT("OBSERVING"))
	{
		bDangerClose = false;
		if (Strip) { Strip->SetDangerClose(false); }
	}
}

void UShitfireNetHudWidget::HandleAarReady(const FShitfireAarSummary& InAar)
{
	if (!Aar)
	{
		return;
	}
	if (Comms && Comms->IsInputFocused())
	{
		Comms->BlurInput();
	}
	Aar->ShowFromHost(Host.Get(), InAar, MissionChapterId);
	// The reward screen owns the keyboard (UX9); cursor released like
	// document.exitPointerLock() (13-aar.js:454).
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	Aar->TakeKeyboardFocus();
}

void UShitfireNetHudWidget::HandleAarDismissed()
{
	ApplyGameOnlyInput();   // [ESC] -> back to the OP
}

void UShitfireNetHudWidget::HandleTx(const FString& Raw)
{
	UShitfireSimHost* H = Host.Get();
	if (H && H->IsMissionLive())
	{
		// The core books the obs echo (and any same-frame sys notice, in
		// its own order) inside submitMessage; pumping right after renders
		// the echo synchronously - no local echo, so nothing can invert.
		H->SubmitMessage(Raw);
		PumpTranscript();
	}
	else if (Comms)
	{
		Comms->AppendLine(EShitfireLogRole::Sys, 0.0, FString(),
			TEXT("No mission on the net."));
	}
}

void UShitfireNetHudWidget::HandleInputFocusChanged(bool bFocused)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;   // test window path: Slate focus only, no input modes to swap
	}
	if (bFocused)
	{
		// Browser: [ENTER] exits pointer lock and focuses the box
		// (16-input.js:181); game keys go quiet while it holds focus.
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
	else if (!(Aar && Aar->IsShowing()))
	{
		ApplyGameOnlyInput();
	}
}

void UShitfireNetHudWidget::ApplyGameOnlyInput()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void UShitfireNetHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Lazy chapter discovery (game path) - same reasoning as the pawn's lazy
	// FindWorldActor (ObserverPawn.cpp:165-183): spawn order is unknowable.
	if (!bMissionContextSet && GetWorld())
	{
		for (TActorIterator<AShitfireWorldActor> It(GetWorld()); It; ++It)
		{
			SetMissionContext((*It)->ChapterId);
			break;
		}
	}

	PumpTranscript();
}

void UShitfireNetHudWidget::PumpTranscript()
{
	// ONE ordered source for the log (header note): every net row the
	// browser log()s - obs / fdc / sys / caller / note - drained off the
	// TLOG by watermark, in transcript order. parse / impact / spot / aar
	// rows are transcript-only; syserr rows are UE_LOG'd by HandleSysLine.
	UShitfireSimHost* H = Host.Get();
	if (!H || !H->IsMissionLive() || !Comms)
	{
		return;
	}
	const std::vector<shitfire::review::TlogEntry>& Es = H->GetSimCore()->tlog().entries();
	const int32 Size = static_cast<int32>(Es.size());
	if (Size < TlogUiSeen)
	{
		TlogUiSeen = 0;   // fresh mission (relaunch resets the TLOG)
	}
	for (int32 i = TlogUiSeen; i < Size; i++)
	{
		const shitfire::review::TlogEntry& E = Es[static_cast<std::size_t>(i)];
		const FString Call = UTF8_TO_TCHAR(E.call.c_str());
		const FString Msg = UTF8_TO_TCHAR(E.msg.c_str());
		if (E.kind == "obs")
		{
			Comms->AppendLine(EShitfireLogRole::Obs, E.t, Call, Msg);
		}
		else if (E.kind == "fdc")
		{
			Comms->AppendLine(EShitfireLogRole::Fdc, E.t, Call, Msg);
		}
		else if (E.kind == "sys")
		{
			Comms->AppendLine(EShitfireLogRole::Sys, E.t, FString(), Msg);
		}
		else if (E.kind == "caller")
		{
			Comms->AppendLine(EShitfireLogRole::Caller, E.t, Call, Msg);
		}
		else if (E.kind == "note")
		{
			Comms->AppendLine(EShitfireLogRole::Note, E.t, FString(), Msg);
		}
	}
	TlogUiSeen = Size;
}
