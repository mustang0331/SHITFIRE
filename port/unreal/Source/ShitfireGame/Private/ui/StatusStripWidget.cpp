#include "ui/StatusStripWidget.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireSimHost.h"
#include "ui/NetHudStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/campaign/campaign_data.h"
#include "shitfire/sim/events.h"
#include "shitfire/sim/mission_machine.h"
#include "shitfire/sim_core.h"

void UShitfireStatusStripWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildTree();
}

TSharedRef<SWidget> UShitfireStatusStripWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UShitfireStatusStripWidget::BuildTree()
{
	if (bTreeBuilt)
	{
		return;
	}
	bTreeBuilt = true;

	// #topbar (head.html:21-26): full-width scrim band. The browser's
	// gradient-to-transparent is approximated as a flat 72%-alpha strip (UMG
	// borders are single brushes; a gradient needs an asset or a paint pass -
	// deliberately not spent here).
	UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Scrim"));
	Scrim->SetBrushColor(ShitfireUi::ScrimBg);
	Scrim->SetPadding(FMargin(14.f, 6.f, 14.f, 10.f));   // padding: 6px 14px 14px
	WidgetTree->RootWidget = Scrim;
	SetVisibility(ESlateVisibility::HitTestInvisible);    // pointer-events: none

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	Scrim->SetContent(Row);

	auto Add = [&](const TCHAR* Name, const FLinearColor& Color, float PadLeft) {
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		T->SetFont(ShitfireUi::Mono(10, 80));   // 13px, .08em (head.html:22-23)
		T->SetColorAndOpacity(FSlateColor(Color));
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(T))
		{
			S->SetPadding(FMargin(PadLeft, 0.f, 0.f, 0.f));
		}
		return T;
	};

	StateText = Add(TEXT("State"), ShitfireUi::TopbarText, 0.f);
	StateText->SetText(FText::FromString(TEXT("STATE OBSERVING")));   // #status default (:358)
	CueText = Add(TEXT("Cue"), ShitfireUi::Amber, 12.f);
	CueText->SetVisibility(ESlateVisibility::Collapsed);
	AssetText = Add(TEXT("Asset"), ShitfireUi::HeadText, 24.f);
	AmmoText = Add(TEXT("Ammo"), ShitfireUi::BodyText, 24.f);
	AdjText = Add(TEXT("Adj"), ShitfireUi::BodyText, 18.f);
	DangerText = Add(TEXT("Danger"), ShitfireUi::WarnRed, 24.f);
	DangerText->SetText(FText::FromString(TEXT("DANGER CLOSE")));
	DangerText->SetVisibility(ESlateVisibility::Collapsed);
	StrictText = Add(TEXT("Strict"), ShitfireUi::Amber, 18.f);
	StrictText->SetText(FText::FromString(TEXT("STRICT NET")));
	StrictText->SetVisibility(ESlateVisibility::Collapsed);
}

void UShitfireStatusStripWidget::SetHost(UShitfireSimHost* InHost)
{
	Host = InHost;
	NextPollAt = 0.0;
}

void UShitfireStatusStripWidget::SetChapterContext(const FString& ChapterId)
{
	BuildTree();
	// ChapterSpec.asset '' | 'mortar60' | 'sunlamp' (campaign_data.h:69);
	// callsigns are the FdcCallsigns strings (fdc_say.h:26-31).
	const shitfire::campaign::ChapterSpec* Ch =
		shitfire::campaign::findChapter(TCHAR_TO_UTF8(*ChapterId));
	FString Asset = TEXT("HELLHOUND — 155MM BATTERY");
	bool bStrict = false;
	if (Ch)
	{
		bStrict = Ch->strict;
		if (Ch->asset == "mortar60") { Asset = TEXT("HACKSAW — 60MM MORTAR"); }
		else if (Ch->asset == "sunlamp") { Asset = TEXT("SUNLAMP — ORBITAL"); }
	}
	if (AssetText) { AssetText->SetText(FText::FromString(Asset)); }
	if (StrictText)
	{
		StrictText->SetVisibility(bStrict
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UShitfireStatusStripWidget::SetStateText(const FString& StateString)
{
	BuildTree();
	if (StateText)
	{
		// setState (10-mission.js:6): '#status'.textContent = 'STATE ' + s
		StateText->SetText(FText::FromString(TEXT("STATE ") + StateString));
	}
}

void UShitfireStatusStripWidget::SetDangerClose(bool bOn)
{
	BuildTree();
	if (DangerText)
	{
		DangerText->SetVisibility(bOn
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UShitfireStatusStripWidget::FlashCue(const FString& Tag, const FLinearColor& Color)
{
	BuildTree();
	if (CueText)
	{
		CueText->SetText(FText::FromString(Tag));
		CueText->SetColorAndOpacity(FSlateColor(Color));
		CueText->SetVisibility(ESlateVisibility::HitTestInvisible);
		CueClearAt = NowSeconds + 1.2;
	}
}

void UShitfireStatusStripWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	NowSeconds += InDeltaTime;

	if (CueClearAt >= 0.0 && NowSeconds >= CueClearAt)
	{
		CueClearAt = -1.0;
		if (CueText) { CueText->SetVisibility(ESlateVisibility::Collapsed); }
	}

	if (NowSeconds >= NextPollAt)
	{
		NextPollAt = NowSeconds + 0.25;
		PollSim();
	}
}

void UShitfireStatusStripWidget::PollSim()
{
	UShitfireSimHost* H = Host.Get();
	shitfire::SimCore* Sim = (H && H->IsMissionLive()) ? H->GetSimCore() : nullptr;
	if (!Sim)
	{
		return;
	}
	shitfire::sim::MissionMachine* M = Sim->parts().machine;
	if (!M)
	{
		return;
	}

	// Ammo books (SUGG7, mission_machine.h ammoLeft) - the browser only ever
	// spoke these on a unit status report; the strip keeps them in view.
	const FString Ammo = FString::Printf(TEXT("HE %d · SMK %d · ILL %d"),
		static_cast<int32>(M->ammoLeft(shitfire::sim::ShellKind::He)),
		static_cast<int32>(M->ammoLeft(shitfire::sim::ShellKind::Smoke)),
		static_cast<int32>(M->ammoLeft(shitfire::sim::ShellKind::Illum)));
	if (Ammo != LastAmmoStr)
	{
		LastAmmoStr = Ammo;
		if (AmmoText) { AmmoText->SetText(FText::FromString(Ammo)); }
	}

	const shitfire::sim::Mission* Act = M->activeMission();
	const FString Adj = (Act && !Act->done)
		? FString::Printf(TEXT("ADJ %d"), Act->adjustRounds)
		: FString();
	if (Adj != LastAdjStr)
	{
		LastAdjStr = Adj;
		if (AdjText) { AdjText->SetText(FText::FromString(Adj)); }
	}
}
