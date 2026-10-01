// ShitfireUi - the shared palette + font helpers for the BUILDER-UI net
// layer (comms log, status strip, AAR). Every color is the browser CSS
// value from src/shell/head.html (cited per constant); the mono font is the
// same engine DroidSansMono the observer HUD uses (ObserverHudWidget.cpp:32)
// so the two HUD layers read as one instrument.
//
// Browser px -> Slate pt follows the observer's ratio (~0.73: CSS 15px ->
// 11pt, 13px -> 10pt) so the layers match on screen.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

namespace ShitfireUi
{
	// ---- panels (head.html:13 .panel, :52-56 #commshead) -------------------
	inline const FLinearColor PanelBg = FLinearColor(FLinearColor::FromSRGBColor(FColor(12, 16, 10)).R,
		FLinearColor::FromSRGBColor(FColor(12, 16, 10)).G,
		FLinearColor::FromSRGBColor(FColor(12, 16, 10)).B, 0.80f);
	inline const FLinearColor PanelBorder = FLinearColor::FromSRGBColor(FColor(0x39, 0x44, 0x2e));
	inline const FLinearColor HeadBg = FLinearColor(FLinearColor::FromSRGBColor(FColor(57, 68, 46)).R,
		FLinearColor::FromSRGBColor(FColor(57, 68, 46)).G,
		FLinearColor::FromSRGBColor(FColor(57, 68, 46)).B, 0.5f);
	inline const FLinearColor HeadText = FLinearColor::FromSRGBColor(FColor(0x9f, 0xb0, 0x8c));

	// ---- log line roles (head.html:60-66,:78) ------------------------------
	inline const FLinearColor ObsCol = FLinearColor::FromSRGBColor(FColor(0xff, 0xd9, 0x8a));   // .obs
	inline const FLinearColor FdcCol = FLinearColor::FromSRGBColor(FColor(0x9f, 0xd0, 0xff));   // .fdc
	inline const FLinearColor SysCol = FLinearColor::FromSRGBColor(FColor(0x8f, 0xa3, 0x87));   // .sys
	inline const FLinearColor CallerCol = FLinearColor::FromSRGBColor(FColor(0xff, 0x9d, 0x7a)); // .caller (NET1)
	inline const FLinearColor NoteCol = FLinearColor::FromSRGBColor(FColor(0xc9, 0xb8, 0x77));  // .note (NET7)
	inline const FLinearColor BodyText = FLinearColor::FromSRGBColor(FColor(0xcf, 0xe0, 0xbd)); // .panel color

	// ---- input row (head.html:68-71) ---------------------------------------
	inline const FLinearColor PlaceholderCol = FLinearColor::FromSRGBColor(FColor(0x7d, 0x74, 0x55));

	// ---- topbar / strip (head.html:21-32) ----------------------------------
	inline const FLinearColor TopbarText = FLinearColor::FromSRGBColor(FColor(0xdf, 0xe8, 0xc8));
	inline const FLinearColor ScrimBg = FLinearColor(FLinearColor::FromSRGBColor(FColor(8, 11, 7)).R,
		FLinearColor::FromSRGBColor(FColor(8, 11, 7)).G,
		FLinearColor::FromSRGBColor(FColor(8, 11, 7)).B, 0.72f);
	inline const FLinearColor WarnRed = FLinearColor::FromSRGBColor(FColor(0xff, 0x6b, 0x57));  // #msnclock.warn
	inline const FLinearColor Amber = FLinearColor::FromSRGBColor(FColor(0xe8, 0xc8, 0x69));    // #msnclock

	// ---- AAR (head.html:202-217,:265) --------------------------------------
	inline const FLinearColor AarPass = FLinearColor::FromSRGBColor(FColor(0xa8, 0xe0, 0x8a));
	inline const FLinearColor AarFail = FLinearColor::FromSRGBColor(FColor(0xe0, 0x8a, 0x7a));
	inline const FLinearColor StarGold = FLinearColor::FromSRGBColor(FColor(0xff, 0xe9, 0xa8)); // #titlebtns/.aarstars
	inline const FLinearColor DiagText = FLinearColor::FromSRGBColor(FColor(0xd8, 0xb0, 0x8a)); // #aarbox ul
	inline const FLinearColor FootText = FLinearColor::FromSRGBColor(FColor(0x8f, 0xa3, 0x87)); // .foot
	inline const FLinearColor RowSep = FLinearColor(FLinearColor::FromSRGBColor(FColor(0xcf, 0xe0, 0xbd)).R,
		FLinearColor::FromSRGBColor(FColor(0xcf, 0xe0, 0xbd)).G,
		FLinearColor::FromSRGBColor(FColor(0xcf, 0xe0, 0xbd)).B, 0.10f);  // tr+tr border
	inline const FLinearColor AarFootBg = FLinearColor::FromSRGBColor(FColor(0x0f, 0x13, 0x0c));
	inline const FLinearColor AarScrim = FLinearColor(0.f, 0.f, 0.f, 0.72f);   // #aar :184

	/** DroidSansMono like the observer HUD (ObserverHudWidget.cpp:30-34).
	    LetterSpacing is FSlateFontInfo units (1/1000 em) - CSS .08em = 80. */
	SHITFIREGAME_API FSlateFontInfo Mono(int32 Size, int32 LetterSpacing = 0);
}
