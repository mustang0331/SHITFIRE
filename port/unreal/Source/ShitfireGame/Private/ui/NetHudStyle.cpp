#include "ui/NetHudStyle.h"

#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

namespace ShitfireUi
{
	FSlateFontInfo Mono(int32 Size, int32 LetterSpacing)
	{
		static UFont* MonoFont = LoadObject<UFont>(nullptr,
			TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
		FSlateFontInfo Info = MonoFont
			? FSlateFontInfo(MonoFont, Size)
			: FCoreStyle::GetDefaultFontStyle("Mono", Size);
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}
}
