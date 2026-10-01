// UShitfireStatusStripWidget - the topbar status strip. Port of the browser
// #topbar's LEFT half (#status "STATE ..." - head.html:21-32,:357-362; the
// right half, #heading/#tool, is the observer's, ObserverHudWidget.cpp:52).
// Extended per P4 scope with the net facts the browser kept in prose: firing
// asset, ammo books, adjusting-round count, danger-close and strict-net
// flags.
//
// Sources: mission state via OnStateChange/GetMissionStateString; ammo +
// adjust rounds polled off SimCore parts (mission_machine.h ammoLeft /
// activeMission - the same door the observer pawn uses for the heightfield,
// ObserverPawn.cpp:185-193); asset + strict from the campaign chapter table
// (campaign_data.h ChapterSpec.asset/.strict); danger close latched by the
// net HUD from FDC traffic (the readback speaks ", DANGER CLOSE" -
// 10-mission.js:439 - and the sim keeps no queryable latch).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StatusStripWidget.generated.h"

class UShitfireSimHost;
class UTextBlock;

UCLASS()
class SHITFIREGAME_API UShitfireStatusStripWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHost(UShitfireSimHost* InHost);

	/** Chapter id ('' = skirmish) -> asset name + strict flag off the table. */
	void SetChapterContext(const FString& ChapterId);

	void SetStateText(const FString& StateString);   // 'STATE ' + setState vocab
	void SetDangerClose(bool bOn);

	/** SHOT/SPLASH emphasis: transient tag next to the state, ~1.2 s. */
	void FlashCue(const FString& Tag, const FLinearColor& Color);

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Idempotent tree build - see UShitfireLogLineWidget::BuildTree note. */
	void BuildTree();
	bool bTreeBuilt = false;

	void PollSim();

	TWeakObjectPtr<UShitfireSimHost> Host;

	UPROPERTY() TObjectPtr<UTextBlock> StateText;
	UPROPERTY() TObjectPtr<UTextBlock> CueText;      // flash tag
	UPROPERTY() TObjectPtr<UTextBlock> AssetText;
	UPROPERTY() TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY() TObjectPtr<UTextBlock> AdjText;
	UPROPERTY() TObjectPtr<UTextBlock> DangerText;
	UPROPERTY() TObjectPtr<UTextBlock> StrictText;

	double NowSeconds = 0.0;
	double NextPollAt = 0.0;
	double CueClearAt = -1.0;
	FString LastAmmoStr, LastAdjStr;
};
