// UShitfireNetHudWidget - the BUILDER-UI net layer: status strip (top),
// comms log + typed input (bottom left), AAR overlay (full screen). Composes
// the three P4 widgets, binds the UShitfireSimHost delegate surface, and
// owns the glue the browser kept in globals:
//
//   - log feed: ONE ordered source - the TLOG, by watermark (PumpTranscript,
//     each widget tick + right after a submit). The delegate surface alone
//     cannot rebuild the browser's log: obs rows are deliberately not
//     broadcast (ShitfireSimHost.cpp fan-out note - "the observer UI reads
//     them straight off tlog()"), and any hybrid feed inverts same-frame
//     pairs (verified: the core books the mid-transmission sys notice
//     BEFORE the obs echo; a submit-time local echo rendered them swapped).
//     OnFdcLine/OnSysLine still drive the latch/diagnostics below;
//     OnShotCue/OnSplashCue/OnStateChange/OnAarReady drive everything else;
//   - the obs echo of a typed send appears synchronously: the submit
//     handler pumps the transcript right after SubmitMessage books it;
//   - syserr rows: NOT displayed - the browser never log()s them either
//     (every 'syserr' site is a bare TLOG.add, e.g. 12-cff-handling.js:892);
//     they go to UE_LOG so they are not silent;
//   - danger close: latched when FDC traffic speaks "DANGER CLOSE" (the
//     readback appends it, 10-mission.js:439; the quip pool airs it,
//     07-voice-spot-audio.js:174), cleared when the state returns to
//     OBSERVING - the sim keeps no queryable latch;
//   - input modes: game input suppressed while the box has focus (browser:
//     keydown returns early when e.target === txInput, 16-input.js:110-113),
//     mouse-look released like exitPointerLock (16-input.js:181).
//
// Mounted by UObserverHudWidget (the sanctioned minimal edit) as its own
// viewport layer at Z 1 - the observer HUD is HitTestInvisible top-to-bottom
// and the radio box must take clicks/keys.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "bridge/ShitfireSimHost.h"
#include "NetHudWidget.generated.h"

class AObserverPawn;
class UShitfireAarWidget;
class UShitfireCommsWidget;
class UShitfireStatusStripWidget;

UCLASS()
class SHITFIREGAME_API UShitfireNetHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindHost(UShitfireSimHost* InHost);
	void BindObserver(AObserverPawn* Pawn);

	/** Chapter id ('' = skirmish) for the strip + AAR header. The game path
	    discovers it lazily off AShitfireWorldActor; tests set it directly. */
	void SetMissionContext(const FString& ChapterId);

	/** [ENTER] seam target (ObserverPawn.h:79-82). */
	void FocusInput();

	/** Observer-local sys line (AObserverPawn::Hint replacement - the
	    browser's log(...,'sys') guard lines, display-only, never the TLOG:
	    the core TLOG is the parity record and UI lines must not perturb it). */
	void AppendLocalSys(const FString& Msg);

	/** Drain new TLOG rows into the log (obs/fdc/sys/caller/note, exact
	    transcript order). Runs every widget tick and after each submit;
	    public so tests can pump synchronously. */
	void PumpTranscript();

	UShitfireCommsWidget* GetComms() const { return Comms; }
	UShitfireStatusStripWidget* GetStrip() const { return Strip; }
	UShitfireAarWidget* GetAar() const { return Aar; }

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Idempotent tree build - see UShitfireLogLineWidget::BuildTree note. */
	void BuildTree();
	bool bTreeBuilt = false;

	UFUNCTION()
	void HandleFdcLine(double SimT, const FString& Call, const FString& Msg);
	UFUNCTION()
	void HandleSysLine(double SimT, const FString& Msg, bool bError);
	UFUNCTION()
	void HandleShotCue(double SimT);
	UFUNCTION()
	void HandleSplashCue(double SimT);
	UFUNCTION()
	void HandleStateChange(const FString& OldState, const FString& NewState, double SimT);
	UFUNCTION()
	void HandleAarReady(const FShitfireAarSummary& Aar);

	void HandleTx(const FString& Raw);
	void HandleInputFocusChanged(bool bFocused);
	void HandleAarDismissed();
	void ApplyGameOnlyInput();

	TWeakObjectPtr<UShitfireSimHost> Host;

	UPROPERTY() TObjectPtr<UShitfireStatusStripWidget> Strip;
	UPROPERTY() TObjectPtr<UShitfireCommsWidget> Comms;
	UPROPERTY() TObjectPtr<UShitfireAarWidget> Aar;

	FString MissionChapterId;
	bool bMissionContextSet = false;
	bool bDangerClose = false;
	int32 TlogUiSeen = 0;   // transcript watermark (independent of the host's)
};
