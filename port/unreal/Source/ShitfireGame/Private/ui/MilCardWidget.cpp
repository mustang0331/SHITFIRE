#include "ui/MilCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

namespace
{
	// #milcard CSS (shitfire.html:276-282)
	const FLinearColor CardBg = FLinearColor::FromSRGBColor(FColor(12, 16, 9, 219));      // rgba(12,16,9,.86)
	const FLinearColor CardBorder = FLinearColor::FromSRGBColor(FColor(0x4d, 0x5a, 0x3c)); // #4d5a3c
	const FLinearColor CardText = FLinearColor::FromSRGBColor(FColor(0xcf, 0xe0, 0xbd));   // #cfe0bd
	const FLinearColor HeadText = FLinearColor::FromSRGBColor(FColor(0xff, 0xe9, 0xa8));   // #ffe9a8 (b)
	const FLinearColor RefText = FLinearColor::FromSRGBColor(FColor(0x9f, 0xb0, 0x8c));    // #9fb08c (.ref)

	FSlateFontInfo CardFont(int32 Size)
	{
		static UFont* Mono = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
		return Mono ? FSlateFontInfo(Mono, Size) : FCoreStyle::GetDefaultFontStyle("Mono", Size);
	}
}

void UMilCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The browser card is 11.5px text in a bordered panel; the .ref lines are
	// italic - the mono face has no italic, so the dimmer green carries the
	// distinction alone. Content is shitfire.html:401-414 verbatim (HTML
	// entities decoded).
	// 'border: 1px solid #4d5a3c' - an outer 1px border box around the fill.
	UBorder* Outline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CardOutline"));
	Outline->SetBrushColor(CardBorder);
	Outline->SetPadding(FMargin(1.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CardPanel"));
	Panel->SetBrushColor(CardBg);
	Panel->SetPadding(FMargin(13.0f, 10.0f));
	Outline->SetContent(Panel);

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardBox"));
	Panel->SetContent(Box);

	auto AddLine = [&](const TCHAR* S, const FLinearColor& Color, int32 Size, float TopPad)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(S));
		T->SetFont(CardFont(Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S2 = Box->AddChildToVerticalBox(T))
		{
			S2->SetPadding(FMargin(0.0f, TopPad, 0.0f, 0.0f));
		}
	};

	AddLine(TEXT("MIL RELATION — RANGE BY RETICLE"), HeadText, 9, 0.0f);
	AddLine(TEXT("RANGE (m) = SIZE (m) ÷ MILS × 1000"), CardText, 8, 4.0f);
	AddLine(TEXT("DEVIATION (m) = MILS × OT FACTOR (OT range ÷ 1000)"), CardText, 8, 2.0f);
	AddLine(TEXT("KNOWN SIZES: hut 3 m high · truck 5 m long · hangar 6 m high · radio mast 26 m · fuel drum 2 m"),
		RefText, 8, 4.0f);
	AddLine(TEXT("Example: mast subtends 5 mils → 26 ÷ 5 × 1000 = 5200 m"), RefText, 8, 2.0f);
	AddLine(TEXT("DO NOT MIL A MAN — figures are drawn enlarged at range so you can see them. Mil hard structures. Deviation in mils is unaffected."),
		RefText, 8, 2.0f);
	AddLine(TEXT("Reticle graduations are TRUE MILS at 4X, 7X and 14X alike — the scale is derived from the field of view, so zoom does not change the answer."),
		RefText, 8, 2.0f);
	AddLine(TEXT("G-M ANGLE: your HUD reads MAG. The grid, the map and the fire net read GRID. GRID = MAG + 124 mils (declination 7° E). Send OT DIRECTION in GRID."),
		RefText, 8, 2.0f);
	AddLine(TEXT("[R] closes this card"), RefText, 8, 4.0f);

	WidgetTree->RootWidget = Outline;
}
