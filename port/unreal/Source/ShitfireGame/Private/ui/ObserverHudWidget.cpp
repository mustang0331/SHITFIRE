#include "ui/ObserverHudWidget.h"
#include "ui/MilCardWidget.h"
#include "ui/ReticleWidget.h"
#include "observer/ObserverPawn.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/InvalidationBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

namespace
{
	// shitfire.html CSS colors for the observer-owned chrome
	const FLinearColor HudGreen = FLinearColor::FromSRGBColor(FColor(0xcf, 0xe0, 0xbd)); // body text family
	const FLinearColor BinoInfoCol = FLinearColor::FromSRGBColor(FColor(0xe6, 0xee, 0xcd)); // #binoinfo (:176)
	const FLinearColor LaseCol = FLinearColor::FromSRGBColor(FColor(0xff, 0xe9, 0xa8));  // #lase (:177)
	const FLinearColor HintCol = FLinearColor::FromSRGBColor(FColor(0x9f, 0xb0, 0x8c));  // .ref green

	FSlateFontInfo MonoFont(int32 Size)
	{
		static UFont* Mono = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
		return Mono ? FSlateFontInfo(Mono, Size) : FCoreStyle::GetDefaultFontStyle("Mono", Size);
	}
}

void UObserverHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HudCanvas"));
	WidgetTree->RootWidget = Canvas;
	SetVisibility(ESlateVisibility::HitTestInvisible);   // pointer-events: none

	auto MakeText = [&](const TCHAR* Name, int32 Size, const FLinearColor& Color) {
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		T->SetFont(MonoFont(Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		return T;
	};

	// ---- topbar right half: #heading 15px + #tool (shitfire.html:360-361) --
	{
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopBar"));
		HeadingText = MakeText(TEXT("Heading"), 11, HudGreen);
		HeadingText->SetText(FText::FromString(TEXT("MAG 0000")));
		ToolText = MakeText(TEXT("Tool"), 10, HudGreen);
		ToolText->SetText(FText::FromString(TEXT("EYES")));
		Top->AddChildToHorizontalBox(HeadingText);
		if (UHorizontalBoxSlot* S = Top->AddChildToHorizontalBox(ToolText))
		{
			S->SetPadding(FMargin(18.0f, 1.0f, 0.0f, 0.0f));
		}
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Top))
		{
			S->SetAnchors(FAnchors(1.0f, 0.0f));
			S->SetAlignment(FVector2D(1.0, 0.0));
			S->SetPosition(FVector2D(-12.0, 8.0));
			S->SetAutoSize(true);
		}
	}

	// ---- #crosshair (shitfire.html:34-36): 6px dot, centre, .85 white ------
	{
		Crosshair = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Crosshair"));
		Crosshair->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f));
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Crosshair))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5, 0.5));
			S->SetPosition(FVector2D::ZeroVector);
			S->SetSize(FVector2D(6.0, 6.0));
		}
	}

	// ---- #bino overlay: reticle in an invalidation box (research 05) -------
	{
		BinoRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BinoRoot"));
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(BinoRoot))
		{
			S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			S->SetOffsets(FMargin(0.0f));
		}

		ReticleBox = WidgetTree->ConstructWidget<UInvalidationBox>(UInvalidationBox::StaticClass(), TEXT("ReticleBox"));
		Reticle = CreateWidget<UReticleWidget>(this, UReticleWidget::StaticClass());
		ReticleBox->SetContent(Reticle);
		if (UOverlaySlot* S = BinoRoot->AddChildToOverlay(ReticleBox))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
		BinoRoot->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- #binoinfo: heading + lase readout, centre, bottom 15% (:175-177) --
	{
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BinoInfo"));
		BinoHdgText = MakeText(TEXT("BinoHdg"), 10, BinoInfoCol);
		BinoHdgText->SetText(FText::FromString(TEXT("MAG 0000")));
		BinoHdgText->SetJustification(ETextJustify::Center);
		LaseText = MakeText(TEXT("Lase"), 10, LaseCol);
		LaseText->SetJustification(ETextJustify::Center);
		Info->AddChildToVerticalBox(BinoHdgText);
		if (UVerticalBoxSlot* S = Info->AddChildToVerticalBox(LaseText))
		{
			S->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));      // margin-top: 6px
			S->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(BinoHdgText->Slot))
		{
			S->SetHorizontalAlignment(HAlign_Center);
		}
		// bottom: 15% => element bottom pinned at 85% of viewport height
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Info))
		{
			S->SetAnchors(FAnchors(0.5f, 0.85f));
			S->SetAlignment(FVector2D(0.5, 1.0));
			S->SetPosition(FVector2D::ZeroVector);
			S->SetAutoSize(true);
		}
		Info->SetVisibility(ESlateVisibility::Collapsed);
		// canvas sibling of BinoRoot (not a child) so the readout paints
		// ABOVE the field stop, like the DOM order #vignette -> #binoinfo
		// (shitfire.html:366-368); SetBinoMode toggles both.
		BinoInfoWidget = Info;
	}

	// ---- #milcard: bottom right 16px, width 340 (:276-282) -----------------
	{
		MilCardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MilCardBox"));
		MilCardBox->SetWidthOverride(340.0f);
		MilCard = CreateWidget<UMilCardWidget>(this, UMilCardWidget::StaticClass());
		MilCardBox->AddChild(MilCard);
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(MilCardBox))
		{
			S->SetAnchors(FAnchors(1.0f, 1.0f));
			S->SetAlignment(FVector2D(1.0, 1.0));
			S->SetPosition(FVector2D(-16.0, -16.0));
			S->SetAutoSize(true);
		}
		MilCardBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- transient sys-hint line, bottom left ------------------------------
	{
		USizeBox* HintBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HintBox"));
		HintBox->SetWidthOverride(560.0f);
		HintText = MakeText(TEXT("Hint"), 9, HintCol);
		HintText->SetAutoWrapText(true);
		HintBox->AddChild(HintText);
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(HintBox))
		{
			S->SetAnchors(FAnchors(0.0f, 1.0f));
			S->SetAlignment(FVector2D(0.0, 1.0));
			S->SetPosition(FVector2D(12.0, -12.0));
			S->SetAutoSize(true);
		}
	}
}

void UObserverHudWidget::SetObserver(AObserverPawn* InObserver)
{
	Observer = InObserver;
	if (Reticle) { Reticle->SetObserver(InObserver); }
}

void UObserverHudWidget::SetBinoMode(bool bBinos)
{
	// setBinos (09-camera.js:138-139): binoEl on/off, crosshair the inverse.
	if (BinoRoot) { BinoRoot->SetVisibility(bBinos ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (BinoInfoWidget) { BinoInfoWidget->SetVisibility(bBinos ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (Crosshair) { Crosshair->SetVisibility(bBinos ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
}

void UObserverHudWidget::RefreshTool()
{
	// refreshTool (09-camera.js:168-175). VISION mode tags and the NO DISP
	// test-aid flag belong to subsystems that don't exist in P3; the string
	// grows those suffixes when they do.
	if (ToolText && Observer.IsValid())
	{
		ToolText->SetText(FText::FromString(Observer->BinosUp()
			? FString::Printf(TEXT("BINOS %dX"), Observer->BinoPower())
			: FString(TEXT("EYES"))));
	}
}

void UObserverHudWidget::SetHeadingText(const FText& Text)
{
	// one string, both readouts (17-loop-boot.js:128-129)
	if (HeadingText) { HeadingText->SetText(Text); }
	if (BinoHdgText) { BinoHdgText->SetText(Text); }
}

void UObserverHudWidget::SetLaseText(const FText& Text)
{
	if (LaseText) { LaseText->SetText(Text); }
}

void UObserverHudWidget::SetMilCardOn(bool bOn)
{
	if (MilCardBox)
	{
		MilCardBox->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UObserverHudWidget::IsMilCardOn() const
{
	return MilCardBox && MilCardBox->GetVisibility() != ESlateVisibility::Collapsed;
}

void UObserverHudWidget::ShowHint(const FText& Text, double Seconds)
{
	if (HintText)
	{
		HintText->SetText(Text);
		HintClearAt = GetWorld() ? GetWorld()->GetTimeSeconds() + Seconds : -1.0;
	}
}

void UObserverHudWidget::InvalidateReticle()
{
	// dirty the child, not the box: InvalidateCache is deprecated no-op
	// under global invalidation (see UReticleWidget::RequestRepaint)
	if (Reticle) { Reticle->RequestRepaint(); }
}

void UObserverHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// resize redraws the reticle (16-input.js:216-221)
	const FVector2D Size = MyGeometry.GetLocalSize();
	if (!Size.Equals(LastSize, 0.5))
	{
		LastSize = Size;
		InvalidateReticle();
	}

	if (HintClearAt >= 0.0 && GetWorld() && GetWorld()->GetTimeSeconds() >= HintClearAt)
	{
		HintClearAt = -1.0;
		if (HintText) { HintText->SetText(FText::GetEmpty()); }
	}
}
