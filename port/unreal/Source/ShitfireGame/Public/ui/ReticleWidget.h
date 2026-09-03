// UReticleWidget - the binocular mil reticle, a line-for-line port of
// drawReticle() (src/js/09-camera.js:55-132, oracle-v1) into UMG NativePaint,
// plus the layered field stop (#vignette, shitfire.html:168-173).
//
// px-per-mil is computed at PAINT time from the observer's CURRENT doctrine
// FOV (60/power), so the graduations are true mils at 4X, 7X and 14X alike -
// the browser's G4 guarantee. The widget sits inside a UInvalidationBox
// (UNREAL_RESEARCH.md / research 05: static chrome pays paint cost only when
// invalidated); the pawn invalidates it on power change, binos raise and
// viewport resize - the only times the drawing changes.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReticleWidget.generated.h"

class AObserverPawn;

UCLASS()
class SHITFIREGAME_API UReticleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetObserver(AObserverPawn* InObserver) { Observer = InObserver; }

	/** Mark the paint cache dirty (power change / binos raise). Under 5.8's
	    global invalidation the enclosing SInvalidationPanel reuses cached
	    draw elements until a child is dirtied - UInvalidationBox::
	    InvalidateCache is deprecated ("InvalidationCache is not used"), so
	    the child dirties ITSELF; a stale-mils reticle is the silent failure
	    this guards. Resizes dirty layout on their own. */
	void RequestRepaint();

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TWeakObjectPtr<AObserverPawn> Observer;
};
