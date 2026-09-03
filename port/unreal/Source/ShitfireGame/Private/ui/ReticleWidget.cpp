#include "ui/ReticleWidget.h"
#include "observer/ObserverPawn.h"
#include "observer/ObserverConfig.h"

#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	// ctx.strokeStyle/fillStyle 'rgba(15,20,12,0.9)' (09-camera.js:73-74).
	// FromSRGBColor so the on-screen shade matches the browser's sRGB value.
	const FLinearColor ReticleColor = FLinearColor::FromSRGBColor(FColor(15, 20, 12, 230));

	// '11px Consolas, monospace' / '10px' (09-camera.js:76,124). Slate sizes
	// are points (~4/3 px at scale 1): 8pt ~= 11 px, 7pt ~= 10 px - nearest
	// match with the engine's shipped mono face (no Consolas in-engine).
	constexpr int32 FontSizeMain = 8;
	constexpr int32 FontSizeSmall = 7;

	FSlateFontInfo MonoFont(int32 Size)
	{
		static UFont* Mono = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
		if (Mono)
		{
			return FSlateFontInfo(Mono, Size);
		}
		return FCoreStyle::GetDefaultFontStyle("Mono", Size);
	}

	// One straight segment - the ctx.moveTo/lineTo/stroke idiom.
	void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo,
	          const FVector2D& A, const FVector2D& B, float Thickness)
	{
		const TArray<FVector2f> Pts = { FVector2f(A), FVector2f(B) };
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts,
			ESlateDrawEffect::None, ReticleColor, /*bAntialias*/ true, Thickness);
	}

	// fillText with canvas semantics: X is the anchor per Align
	// (-1 left / 0 center / +1 right), Y is the alphabetic BASELINE.
	void TextAt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo,
	            const FString& S, double X, double Y, int32 Align, int32 Size)
	{
		const FSlateFontInfo Font = MonoFont(Size);
		const TSharedRef<FSlateFontMeasure> Measure =
			FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FVector2D Sz = Measure->Measure(S, Font);
		double Left = X;
		if (Align == 0) { Left = X - Sz.X * 0.5; }
		else if (Align > 0) { Left = X - Sz.X; }
		const double Top = Y - Sz.Y * 0.8;   // baseline ~80% down the cell
		FSlateDrawElement::MakeText(Out, Layer,
			Geo.ToPaintGeometry(FVector2f(Sz), FSlateLayoutTransform(FVector2f(float(Left), float(Top)))),
			S, Font, ESlateDrawEffect::None, ReticleColor);
	}

	// Solid annulus (triangle list) with per-edge alpha - one gradient stop
	// pair of the browser's radial-gradient field stop.
	void Annulus(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo,
	             const FVector2D& C, double R0, double R1, uint8 Alpha0, uint8 Alpha1)
	{
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
		const FSlateResourceHandle Handle =
			FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush);
		const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();

		constexpr int32 Segs = 96;
		TArray<FSlateVertex> Verts;
		TArray<SlateIndex> Idx;
		Verts.Reserve((Segs + 1) * 2);
		Idx.Reserve(Segs * 6);
		for (int32 i = 0; i <= Segs; i++)
		{
			const double A = 2.0 * PI * double(i) / double(Segs);
			const FVector2f Dir(float(FMath::Cos(A)), float(FMath::Sin(A)));
			const FVector2f P0 = FVector2f(C) + Dir * float(R0);
			const FVector2f P1 = FVector2f(C) + Dir * float(R1);
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				RT, P0, FVector2f(0.5f, 0.5f), FColor(0, 0, 0, Alpha0)));
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				RT, P1, FVector2f(0.5f, 0.5f), FColor(0, 0, 0, Alpha1)));
			if (i < Segs)
			{
				const int32 B = i * 2;
				Idx.Append({ SlateIndex(B), SlateIndex(B + 1), SlateIndex(B + 2),
				             SlateIndex(B + 2), SlateIndex(B + 1), SlateIndex(B + 3) });
			}
		}
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, Verts, Idx, nullptr, 0, 0);
	}
}

void UReticleWidget::RequestRepaint()
{
	if (const TSharedPtr<SWidget> W = GetCachedWidget())
	{
		W->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UReticleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Ret = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!Observer.IsValid())
	{
		return Ret;
	}

	// drawReticle (09-camera.js:55-132). Local Slate units stand in for CSS
	// px: local height * DPI scale = viewport px height, and the projection
	// maps mils to viewport px, so mils-to-local math is scale-invariant -
	// tick m lands on a target subtending m mils at every DPI.
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const double W = Size.X, H = Size.Y;
	if (W < 2.0 || H < 2.0)
	{
		return Ret;
	}
	const double CX = W / 2.0, CY = H / 2.0;

	// pxPerMil = innerHeight / (binoFovNow() * 17.7778) (:60) - the browser's
	// rounded literal, verbatim (ObserverConfig.h).
	const double Fov = Observer->VerticalFovDeg();
	const double PPM = H / (Fov * ShitfireObserver::MilsPerDegLiteral);

	// G4 - arm extent fits the FIELD, snapped down to a multiple of 5 (:71-72)
	const double Ext = FMath::Max(10.0, FMath::Floor(FMath::Min(42.0,
		(FMath::Min(H, W) / 2.0 - 30.0) / PPM) / 5.0) * 5.0);

	const int32 MarksLayer = Ret + 1;

	// axes with the 3-mil centre gap (:79-84)
	Line(OutDrawElements, MarksLayer, AllottedGeometry,
		{ CX - Ext * PPM, CY }, { CX - 3.0 * PPM, CY }, 1.4f);
	Line(OutDrawElements, MarksLayer, AllottedGeometry,
		{ CX + 3.0 * PPM, CY }, { CX + Ext * PPM, CY }, 1.4f);
	Line(OutDrawElements, MarksLayer, AllottedGeometry,
		{ CX, CY - Ext * PPM }, { CX, CY - 3.0 * PPM }, 1.4f);
	Line(OutDrawElements, MarksLayer, AllottedGeometry,
		{ CX, CY + 3.0 * PPM }, { CX, CY + Ext * PPM }, 1.4f);

	// 5-mil ticks, 10-mil labels (:85-97)
	for (int32 M = int32(-Ext); M <= int32(Ext); M += 5)
	{
		if (M == 0) { continue; }
		const double L = (M % 10 == 0) ? 9.0 : 5.0;
		const double PX = CX + M * PPM, PY = CY + M * PPM;
		Line(OutDrawElements, MarksLayer, AllottedGeometry, { PX, CY - L }, { PX, CY + L }, 1.4f);
		Line(OutDrawElements, MarksLayer, AllottedGeometry, { CX - L, PY }, { CX + L, PY }, 1.4f);
		if (M % 10 == 0)
		{
			const FString Label = FString::FromInt(FMath::Abs(M));
			TextAt(OutDrawElements, MarksLayer, AllottedGeometry, Label, PX, CY + 24.0, 0, FontSizeMain);
			TextAt(OutDrawElements, MarksLayer, AllottedGeometry, Label, CX + 26.0, PY + 4.0, 0, FontSizeMain);
		}
	}

	// fine 1-mil ticks near centre (:99-107)
	for (int32 M = -10; M <= 10; M++)
	{
		if (M % 5 == 0) { continue; }
		const double PX = CX + M * PPM, PY = CY + M * PPM;
		Line(OutDrawElements, MarksLayer, AllottedGeometry, { PX, CY - 2.5 }, { PX, CY + 2.5 }, 1.0f);
		Line(OutDrawElements, MarksLayer, AllottedGeometry, { CX - 2.5, PY }, { CX + 2.5, PY }, 1.0f);
	}

	// stadia - the 5 m TRUCK at 1000/2000/3000 m = 5 / 2.5 / 1.67 mils
	// (:108-125; the DO-NOT-MIL-A-MAN decision lives in the browser comment)
	const double StX = CX - (Ext - 4.0) * PPM;
	const double StY = CY + Ext * 0.58 * PPM;
	const double StadiaMils[3] = { 5.0, 2.5, 5.0 / 3.0 };
	const TCHAR* StadiaLabels[3] = { TEXT("1000 M"), TEXT("2000 M"), TEXT("3000 M") };
	for (int32 i = 0; i < 3; i++)
	{
		const double Y = StY + i * 14.0;
		Line(OutDrawElements, MarksLayer, AllottedGeometry,
			{ StX, Y }, { StX + StadiaMils[i] * PPM, Y }, 1.6f);
		TextAt(OutDrawElements, MarksLayer, AllottedGeometry, StadiaLabels[i],
			StX + 6.0 * PPM + 4.0, Y + 4.0, -1, FontSizeMain);
	}
	TextAt(OutDrawElements, MarksLayer, AllottedGeometry, TEXT("5M TRUCK"),
		StX, StY - 12.0, -1, FontSizeSmall);

	// current optical power (:127-129)
	TextAt(OutDrawElements, MarksLayer, AllottedGeometry,
		FString::Printf(TEXT("%dX"), Observer->BinoPower()),
		CX + (Ext - 2.0) * PPM, CY - Ext * 0.62 * PPM, 1, FontSizeMain);

	// centre dot r=1.6 (:131) - a 3.2 box reads identically at this size
	FSlateDrawElement::MakeBox(OutDrawElements, MarksLayer,
		AllottedGeometry.ToPaintGeometry(FVector2f(3.2f, 3.2f),
			FSlateLayoutTransform(FVector2f(float(CX - 1.6), float(CY - 1.6)))),
		FCoreStyle::Get().GetBrush("GenericWhiteBox"), ESlateDrawEffect::None, ReticleColor);

	// layered field stop ABOVE the marks, matching the browser DOM order
	// (#vignette follows #reticle, shitfire.html:365-368). Gradient stops
	// from the CSS radial-gradient (:172-173): transparent 26% ->
	// rgba(0,0,0,.13) 38% -> .30 45% -> #000 61% of the farthest-corner
	// radius; the warm/cool chromatic fringe (:170-171, alpha .06-.07) is
	// omitted - cosmetic, and vertex-color rings can't do hue blends cleanly.
	const int32 StopLayer = MarksLayer + 1;
	const FVector2D C(CX, CY);
	const double R = FMath::Sqrt(CX * CX + CY * CY);   // farthest corner
	Annulus(OutDrawElements, StopLayer, AllottedGeometry, C, 0.26 * R, 0.38 * R, 0, 33);
	Annulus(OutDrawElements, StopLayer, AllottedGeometry, C, 0.38 * R, 0.45 * R, 33, 77);
	Annulus(OutDrawElements, StopLayer, AllottedGeometry, C, 0.45 * R, 0.61 * R, 77, 255);
	Annulus(OutDrawElements, StopLayer, AllottedGeometry, C, 0.61 * R, R * 1.05, 255, 255);

	return StopLayer;
}
