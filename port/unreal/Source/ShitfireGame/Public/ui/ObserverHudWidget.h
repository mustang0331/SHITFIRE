// UObserverHudWidget - the observer's HUD chrome, C++-built UMG (no widget
// Blueprints - project hard rule). Ports the observer-owned slice of
// shitfire.html's fixed overlay: #heading/#tool (topbar right half),
// #crosshair, #bino (reticle + field stop + #binohdg + #lase readout) and
// #milcard. The comms log, PTT banner, state line and mission clock belong
// to BUILDER-BRIDGE/UI and are absent on purpose.
//
// The transient hint line stands in for the browser's log(...,'sys') calls
// until the comms log exists - BUILDER-UI: route AObserverPawn::Hint into
// the real log and delete it.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObserverHudWidget.generated.h"

class AObserverPawn;
class UImage;
class UInvalidationBox;
class UMilCardWidget;
class UOverlay;
class UReticleWidget;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class SHITFIREGAME_API UObserverHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetObserver(AObserverPawn* InObserver);

	/** setBinos' UI half (09-camera.js:138-141): bino overlay vs crosshair. */
	void SetBinoMode(bool bBinos);

	/** refreshTool (09-camera.js:168-175) - EYES / BINOS nX. Vision modes and
	    the NO DISP flag are other subsystems' state; appended when they exist. */
	void RefreshTool();

	void SetHeadingText(const FText& Text);   // #heading + #binohdg together (17-loop-boot.js:128-129)
	void SetLaseText(const FText& Text);      // #lase (09-camera.js:207-221)

	void SetMilCardOn(bool bOn);              // #milcard.on (09-camera.js:190)
	bool IsMilCardOn() const;

	void ShowHint(const FText& Text, double Seconds);

	/** drawReticle moments (09:141,157; 16:221 resize) - repaint the cached box. */
	void InvalidateReticle();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TWeakObjectPtr<AObserverPawn> Observer;

	UPROPERTY() TObjectPtr<UTextBlock> HeadingText;
	UPROPERTY() TObjectPtr<UTextBlock> ToolText;
	UPROPERTY() TObjectPtr<UImage> Crosshair;
	UPROPERTY() TObjectPtr<UOverlay> BinoRoot;
	UPROPERTY() TObjectPtr<UVerticalBox> BinoInfoWidget;
	UPROPERTY() TObjectPtr<UInvalidationBox> ReticleBox;
	UPROPERTY() TObjectPtr<UReticleWidget> Reticle;
	UPROPERTY() TObjectPtr<UTextBlock> BinoHdgText;
	UPROPERTY() TObjectPtr<UTextBlock> LaseText;
	UPROPERTY() TObjectPtr<USizeBox> MilCardBox;
	UPROPERTY() TObjectPtr<UMilCardWidget> MilCard;
	UPROPERTY() TObjectPtr<UTextBlock> HintText;

	FVector2D LastSize = FVector2D::ZeroVector;
	double HintClearAt = -1.0;
};
