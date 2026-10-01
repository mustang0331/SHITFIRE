// UShitfireAarWidget - the after-action review overlay. Port of the browser
// AAR panel's content structure (src/js/13-aar.js showAAR :239-470; markup +
// CSS head.html:179-217): scrim, chapter header, star row, verdict, the
// commendations, the metrics table, adjustment diagnosis, format coaching,
// outro, sticky foot. The shot PLOT (renderShotPlot) is P6 scope - excluded.
//
// Content sources (the analysis payload):
//   - the OnAarReady summary (verdict/stars/outcome/effPct/...);
//   - the TLOG aar row for the rest of the recorded schema (firstMiss,
//     aimErr0, tInit, corrEff, trace, notes count, noDisp - tlog.h:76-94);
//   - the live Mission struct (mission_machine.h:91-149) for the fields the
//     browser reads off its `mission` global (wasted, refined/recorded,
//     otDirSent, usedCheckFire, MTO readback, coaching note TEXT);
//   - the chapter table (par/strict/asset/outro) + scenario state
//     (difficulty/tgtClass/posture/effScale) for bands and medals.
//
// Dismiss: [ESC] (browser 16-input.js:115 closes #aar) or [ENTER] (port
// addition - PIE eats ESC in-editor; noted) -> hidden, OnDismissed fires,
// the net HUD restores game input. Nothing pauses underneath, exactly like
// the browser.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "bridge/ShitfireSimHost.h"
#include "AarPanelWidget.generated.h"

class UBorder;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class SHITFIREGAME_API UShitfireAarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleDelegate OnDismissed;

	/** Build + show. Host may be mid-shutdown-safe null-guarded throughout. */
	void ShowFromHost(UShitfireSimHost* Host, const FShitfireAarSummary& Summary,
	                  const FString& ChapterId);

	void Dismiss();
	bool IsShowing() const;

	/** Grab keyboard focus so ESC/ENTER land here (call after ShowFromHost). */
	void TakeKeyboardFocus();

	/** Test peek: the rendered verdict line. */
	FString VerdictLineText() const;
	FString StarsLineText() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** Idempotent tree build - see UShitfireLogLineWidget::BuildTree note. */
	void BuildTree();
	bool bTreeBuilt = false;

	UTextBlock* AddText(const FString& S, int32 Size, const FLinearColor& Color,
	                    int32 LetterSpacing, const FMargin& Pad, bool bWrap = true);
	void AddLabel(const FString& S);                       // .mlabel
	void AddBullets(const TArray<FString>& Lines, const FLinearColor& Color);
	void AddKV(const FString& Label, const FString& Value, bool bFirst);

	UPROPERTY() TObjectPtr<UScrollBox> Scroll;
	UPROPERTY() TObjectPtr<UVerticalBox> Content;
	UPROPERTY() TObjectPtr<UTextBlock> FootText;
	UPROPERTY() TObjectPtr<UTextBlock> VerdictText;
	UPROPERTY() TObjectPtr<UTextBlock> StarsText;

	bool bTableStarted = false;
};
