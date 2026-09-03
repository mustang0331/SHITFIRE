// UMilCardWidget - the static mil-relation reference card, content ported
// verbatim from shitfire.html:400-414 (oracle-v1), colors from its CSS
// (#milcard, shitfire.html:276-282). Toggled with [R] while glassing; the
// LRF-fault drill (noLaser chapters) force-opens it. Pure static text -
// built once in C++, no tick, no state.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MilCardWidget.generated.h"

class UVerticalBox;

UCLASS()
class SHITFIREGAME_API UMilCardWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
};
