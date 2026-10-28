// SHITFIRE.Art.MaterialVisTag - no material under /Game is without an optic
// signature (row ART-a, PORT3). Asset half of the gate; the runtime half is
// ShitfireVis::AuditWorld at the end of AShitfireWorldActor::BeginPlay.
//
// Why a test and not a fallback: the browser derives untagged materials from
// daytime luminance (08-burst-optics.js:155-171), and its own first pass
// measured that derivation WORSE than the naked eye at night - enemy body
// and jungle landed on the same tube brightness (04-world.js:93-118). A pack
// material that slips in untagged would make the figures un-countable under
// NVG with nothing failing. So: every UMaterialInterface the registry knows
// under /Game must resolve `VisNvg` and `VisThermal`, the master material
// and the preview collection must exist, and the sweep must have found the
// contract's own materials (an empty sweep is a broken registry, not a pass).
//
// Run headless (GPU not required):
//   UnrealEditor-Cmd.exe <proj> -ExecCmds="Automation RunTests SHITFIRE.Art"
//       -unattended -nullrhi -nosound -testexit="Automation Test Queue Empty"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "ShitfireVis.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireMaterialVisTagTest,
	"SHITFIRE.Art.MaterialVisTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireMaterialVisTagTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	// Commandlet runs start the scan asynchronously; a sweep over a half-
	// scanned registry would pass on whatever it had not seen yet.
	Registry.SearchAllAssets(/*bSynchronousSearch=*/true);
	Registry.WaitForCompletion();

	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UMaterialInterface::StaticClass()->GetClassPathName(),
		Assets, /*bSearchSubClasses=*/true);

	int32 Swept = 0, Untagged = 0;
	for (const FAssetData& A : Assets)
	{
		const FString Path = A.GetObjectPathString();
		if (!Path.StartsWith(TEXT("/Game/")))
		{
			continue;   // engine / plugin content is outside the contract
		}
		Swept++;
		UMaterialInterface* Mat = Cast<UMaterialInterface>(A.GetAsset());
		if (!Mat)
		{
			Untagged++;
			AddError(FString::Printf(TEXT("could not load material %s"), *Path));
			continue;
		}
		FString Why;
		if (!ShitfireVis::IsTagged(Mat, &Why))
		{
			Untagged++;
			AddError(FString::Printf(TEXT("untagged material: %s"), *Why));
		}
	}
	AddInfo(FString::Printf(TEXT("swept %d materials under /Game, %d untagged"), Swept, Untagged));
	TestTrue(TEXT("the sweep found materials (registry scanned, content present)"), Swept > 0);
	TestEqual(TEXT("untagged materials under /Game"), Untagged, 0);

	// The contract's own assets (Tools/gen_assets.py).
	UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, ShitfireVis::MasterPath);
	TestNotNull(TEXT("M_ShitfireMaster exists"), Master);
	if (Master)
	{
		TestTrue(TEXT("M_ShitfireMaster is tagged"), ShitfireVis::IsTagged(Master));
	}
	UMaterialParameterCollection* MPC =
		LoadObject<UMaterialParameterCollection>(nullptr, ShitfireVis::CollectionPath);
	TestNotNull(TEXT("MPC_Vision exists"), MPC);
	if (MPC)
	{
		bool bHasMode = false;
		for (const FCollectionScalarParameter& P : MPC->ScalarParameters)
		{
			bHasMode |= (P.ParameterName == FName(ShitfireVis::ModeParam));
		}
		TestTrue(TEXT("MPC_Vision declares VisMode"), bHasMode);
	}

	// The browser's untagged derivation, pinned on one known colour: the
	// flag pole 0x8A8A80 (05-units-scenario.js:22) -> lum 0.538.
	const FShitfireVis Pole = ShitfireVis::Derived(
		FLinearColor::FromSRGBColor(FColor(0x8A, 0x8A, 0x80)));
	TestTrue(TEXT("Derived(): nvg is the sRGB luminance (flag pole 0.538)"),
		FMath::IsNearlyEqual(Pole.Nvg, 0.538f, 0.005f));
	TestTrue(TEXT("Derived(): thermal is the wash-0 cold grey 0.05 + 0.13 lum"),
		FMath::IsNearlyEqual(Pole.Thermal.R, 0.05f + 0.13f * 0.538f, 0.002f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
