// UShitfireCommsWidget - the radio net panel: comms log + typed input row.
// Port of the browser's #comms panel (src/shell/head.html:46-77,:373-392)
// minus drag/resize/persistence (G6 is mouse-era chrome; the port pins the
// panel at the CSS default: left 12, bottom 12, 600x261) and minus the NET7
// dev NOTE button (dev-mode only; note-kind rows still render if the TLOG
// carries them).
//
// Log rows ride a Slate SListView (entry virtualization - research 05
// section 1's "ListView for the log"). The UMG UListView WRAPPER is
// deliberately not used: in editor builds its RebuildWidget refuses any
// native entry class outright (ListViewBase.cpp:208-215 UE 5.8,
// Error_NonBPEntryWidget - a designer aid) and this project is C++-only by
// hard rule. SListView is the same virtualizing widget one level down;
// rows are TSharedPtr structs, cheaper than per-line UObjects.
//
// Auto-scroll: newest at bottom, pinned there unless the user scrolls up
// (browser :196 logEl.scrollTop = scrollHeight on every append - the pin
// is a port nicety the browser lacked).
//
// The typed input is THE mandatory path (browser #txinput): Enter focuses
// it (AObserverPawn::OnFocusTextRequested -> UShitfireNetHudWidget ->
// FocusInput), Enter submits (trim, clear, empty = no-op, box keeps focus -
// the browser's txform submit handler, 16-input.js:101-108), Escape blurs
// (16-input.js:112). Input is capped at 300 chars like the browser's
// maxlength (BUILD4, head.html:386-389).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/Views/SListView.h"
#include "CommsLogWidget.generated.h"

class UBorder;
class UEditableTextBox;
class UNativeWidgetHost;
class UTextBlock;

/** Log line roles = the browser's CSS classes (head.html:60-66,:78). */
enum class EShitfireLogRole : uint8
{
	Obs,      /* .obs - observer echo */
	Fdc,      /* .fdc */
	Sys,      /* .sys */
	Caller,   /* .caller (NET1) */
	Note,     /* .note (NET7) */
};

/** One rendered log line (SListView item). */
struct FShitfireLogLine
{
	EShitfireLogRole Role = EShitfireLogRole::Sys;
	double SimT = 0.0;
	FString Call;    // "" = no call prefix (sys lines)
	FString Msg;
};

UCLASS()
class SHITFIREGAME_API UShitfireCommsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fired on Enter with a non-empty trimmed transmission. */
	DECLARE_DELEGATE_OneParam(FOnTx, const FString& /*Raw*/);
	FOnTx OnTx;

	/** Fired when the box takes/loses keyboard focus - the net HUD applies
	    the PC input-mode swap there (game input suppressed while typing). */
	DECLARE_DELEGATE_OneParam(FOnFocusChanged, bool /*bFocused*/);
	FOnFocusChanged OnFocusChanged;

	/** Append one line; newest at bottom; auto-scrolls while pinned. */
	void AppendLine(EShitfireLogRole Role, double SimT, const FString& Call, const FString& Msg);

	/** [ENTER] seam (ObserverPawn.h:79-82): focus the box. */
	void FocusInput();
	/** Escape / focus-away: drop keyboard focus (browser txInput.blur()). */
	void BlurInput();
	bool IsInputFocused() const;

	/** SHOT/SPLASH emphasis - border flash, decays over ~0.8 s. */
	void Flash(const FLinearColor& Color);

	// ---- test peeks -------------------------------------------------------
	int32 NumLines() const { return Lines.Num(); }
	FString LineMsgAt(int32 Index) const;
	EShitfireLogRole LineRoleAt(int32 Index) const;
	/** True when the SListView has a LIVE row widget for this line - only
	    visible rows have one (virtualization). */
	bool HasRowWidgetFor(int32 Index) const;
	UEditableTextBox* GetInputBox() const { return InputBox; }
	bool IsPinnedToBottom() const { return bPinnedToBottom; }

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** Idempotent tree build. NativeOnInitialized only fires with a player
	    context (UserWidget.cpp:171-176, UE 5.8) - automation windows have
	    none, so the RebuildWidget override (TakeWidget-driven, BEFORE the
	    slate tree is read) builds there. */
	void BuildTree();
	bool bTreeBuilt = false;

	UFUNCTION()
	void HandleTextChanged(const FText& Text);
	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	TSharedRef<ITableRow> GenerateLogRow(TSharedPtr<FShitfireLogLine> Line,
	                                     const TSharedRef<STableViewBase>& Owner);
	void HandleListScrolled(double InScrollOffset);

	UPROPERTY() TObjectPtr<UBorder> OuterBorder;   // 1px #39442e frame
	UPROPERTY() TObjectPtr<UNativeWidgetHost> LogHost;
	UPROPERTY() TObjectPtr<UEditableTextBox> InputBox;
	UPROPERTY() TObjectPtr<UTextBlock> PromptText;

	TArray<TSharedPtr<FShitfireLogLine>> Lines;
	TSharedPtr<SListView<TSharedPtr<FShitfireLogLine>>> LogListView;

	bool bPinnedToBottom = true;
	bool bInputFocused = false;

	// flash decay state (accumulated widget time - no UWorld in tests)
	double NowSeconds = 0.0;
	double FlashUntil = -1.0;
	FLinearColor FlashColor = FLinearColor::White;
};
