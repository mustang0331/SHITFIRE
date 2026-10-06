// SHITFIRE.Units.HumanoidImportGate - the unit bundle never hands out a
// humanoid whose rendered geometry disagrees with the rig it is fit to.
//
// Background (2026-10-06): the 2026-10-01 bench frame showed a "wreck pile"
// on the beach and a "vehicle" in the sky. Both were the Quaternius soldier:
// its Interchange import left the vertex buffer at 272 x 222 x 69 METRES
// (lying along -Y) and a ref-pose Root scale of 170.6, while the bones sit at
// 2 m - so MeshScale (240 / 200 cm) drew 300 m figures and the head part
// attached with SnapToTargetIncludingScale rode the root scale into the sky.
// Nothing in placement was wrong; the asset was. This test pins the gate
// that turns that silent failure into an Error + box-tier fallback, and it
// keeps passing once the asset is re-imported cleanly (then the humanoid
// tier is simply on).
//
// Run headless (GPU not required):
//   UnrealEditor-Cmd.exe <proj> -ExecCmds="Automation RunTests SHITFIRE.Units"
//       -unattended -nullrhi -nosound -testexit="Automation Test Queue Empty"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/SkeletalMesh.h"
#include "units/ShitfireUnitAssets.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireUnitsHumanoidImportGateTest,
	"SHITFIRE.Units.HumanoidImportGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireUnitsHumanoidImportGateTest::RunTest(const FString& Parameters)
{
	// ---- the gate itself, on the shipped asset ----------------------------
	USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, FShitfireUnitAssets::SoldierBodyPath());
	if (!Body)
	{
		// No asset at all is the documented box-tier path; nothing to gate.
		AddWarning(FString::Printf(TEXT("soldier body not found at %s - box tier"),
			FShitfireUnitAssets::SoldierBodyPath()));
		const FShitfireUnitAssets A = FShitfireUnitAssets::Load();
		TestFalse(TEXT("no asset -> no humanoid"), A.bHumanoidReal);
		return true;
	}

	const FVector Size = Body->GetBounds().BoxExtent * 2.0;
	const FVector RootScale = Body->GetRefSkeleton().GetNum() > 0
		? Body->GetRefSkeleton().GetRefBonePose()[0].GetScale3D()
		: FVector::OneVector;
	AddInfo(FString::Printf(
		TEXT("soldier body bounds %.0f x %.0f x %.0f cm, root ref-pose scale (%.1f, %.1f, %.1f), rig %.0f cm"),
		Size.X, Size.Y, Size.Z, RootScale.X, RootScale.Y, RootScale.Z,
		FShitfireUnitAssets::SoldierNativeHeightCm));

	FString Why;
	const bool bSane = FShitfireUnitAssets::HumanoidImportSane(Body, Why);

	// The gate's own arithmetic, independent of the asset's current state.
	const bool bBoundsFit = Size.GetMax() <=
		FShitfireUnitAssets::SoldierNativeHeightCm * FShitfireUnitAssets::HumanoidBoundsSlack;
	const bool bRootUnscaled = RootScale.GetAbsMax() <= FShitfireUnitAssets::HumanoidMaxRootScale;
	TestEqual(TEXT("gate verdict == (bounds fit && root unscaled)"), bSane, bBoundsFit && bRootUnscaled);
	if (!bSane)
	{
		TestTrue(TEXT("rejection names the failed check"), !Why.IsEmpty());
		AddWarning(FString::Printf(
			TEXT("humanoid tier OFF - asset needs re-import: %s"), *Why));
		// Load() reports the rejection as an Error on purpose; expected here.
		AddExpectedError(TEXT("soldier body REJECTED"), EAutomationExpectedErrorFlags::Contains, 1);
	}

	// ---- Load() honours the verdict ----------------------------------------
	const FShitfireUnitAssets A = FShitfireUnitAssets::Load();
	TestEqual(TEXT("Load(): humanoid tier iff the import is sane"), A.bHumanoidReal, bSane);
	TestEqual(TEXT("Load(): SoldierMesh null iff rejected"), A.SoldierMesh != nullptr, bSane);
	if (A.bHumanoidReal)
	{
		// What the actor will do with it (ShitfireUnitsActor.cpp MakeFigure):
		// MeshScale = TroopHeightM * 100 / SoldierNativeHeightCm must leave a
		// figure that fits the gate's envelope, i.e. a few metres, not 300.
		const double MeshScale = (FShitfireUnitAssets::TroopHeightM * 100.0) /
			FShitfireUnitAssets::SoldierNativeHeightCm;
		const double DrawnMaxCm = Size.GetMax() * MeshScale;
		TestTrue(FString::Printf(TEXT("drawn troop extent %.0f cm is under 10 m"), DrawnMaxCm),
			DrawnMaxCm < 1000.0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
