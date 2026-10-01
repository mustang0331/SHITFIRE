// Shared helpers for the BUILDER-UI automation gates (P4): mount a C++ UMG
// widget in a real top-level Slate window (so list virtualization, focus and
// keyboard routing run the SHIPPING code paths, not mocks), type through the
// Slate application like a keyboard would, and save widget screenshots.
// Private/Tests only - never game code.
#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"

namespace ShitfireUiTest
{
	/** Any live world for CreateWidget's outer chain (PIE first, else the
	    editor world). Widgets here never require gameplay - just an outer. */
	inline UWorld* AnyWorld()
	{
		UWorld* Fallback = nullptr;
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (!Ctx.World())
			{
				continue;
			}
			if (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game)
			{
				return Ctx.World();
			}
			if (Ctx.WorldType == EWorldType::Editor)
			{
				Fallback = Ctx.World();
			}
		}
		return Fallback;
	}

	/** New OS-level window holding Content at a fixed size. */
	inline TSharedRef<SWindow> OpenWindow(const FVector2D& ClientSize,
	                                      const TSharedRef<SWidget>& Content,
	                                      const FString& Title)
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(FText::FromString(Title))
			.ClientSize(ClientSize)
			.AutoCenter(EAutoCenter::PrimaryWorkArea)
			.SupportsMaximize(false)
			.SupportsMinimize(false);
		Window->SetContent(Content);
		FSlateApplication::Get().AddWindow(Window);
		return Window;
	}

	/** Renders the widget through the Slate widget renderer (independent of
	    window presentation) and writes a PNG. Returns the absolute path. */
	inline FString SaveWidgetScreenshot(const TSharedRef<SWidget>& Widget, const FString& BaseName)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("Screenshots/ShitfireUI"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = Dir / (BaseName + TEXT(".png"));

		TArray<FColor> Pixels;
		FIntVector Size(0, 0, 0);
		if (!FSlateApplication::Get().TakeScreenshot(Widget, Pixels, Size) ||
			Size.X <= 0 || Size.Y <= 0)
		{
			return FString();
		}
		for (FColor& C : Pixels)
		{
			C.A = 255;
		}
		const FImageView View(Pixels.GetData(), Size.X, Size.Y);
		return FImageUtils::SaveImageByExtension(*Path, View) ? Path : FString();
	}

	/** Type one character through the full Slate input path. */
	inline void TypeChar(TCHAR C)
	{
		FCharacterEvent E(C, FModifierKeysState(), 0, false);
		FSlateApplication::Get().ProcessKeyCharEvent(E);
	}

	/** Press+release a key through the full Slate input path. */
	inline void PressKey(const FKey& Key)
	{
		FKeyEvent Down(Key, FModifierKeysState(), 0, false, 0, 0);
		FSlateApplication::Get().ProcessKeyDownEvent(Down);
		FKeyEvent Up(Key, FModifierKeysState(), 0, false, 0, 0);
		FSlateApplication::Get().ProcessKeyUpEvent(Up);
	}

	/** Type a whole line and press Enter (the browser's typed path). */
	inline void TypeLineAndEnter(const FString& Line)
	{
		for (int32 i = 0; i < Line.Len(); i++)
		{
			TypeChar(Line[i]);
		}
		PressKey(EKeys::Enter);
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
