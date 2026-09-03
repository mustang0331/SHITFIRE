// SHITFIRE.Observer.LaseGate - the P3 observer gate: for every ray in the
// committed browser-oracle fixture (ENGINE_PORT/fixtures/level1/raylos.json),
// park the REAL observer pawn's camera on the fixture ray (sim -> UE through
// the coords shim), run the production lase path (AObserverPawn::ComputeLase:
// camera world transform -> ShitfireCoords -> core groundHit -> gridOf /
// otFactor), and require the answer the browser recorded.
//
// This validates the pawn's ray CONSTRUCTION and the coords shim end-to-end -
// the sim math itself is already gated bit-for-bit by port/core-tests
// (raylos_test.cpp: same hit/miss, dist within 0.01 m over 500 rays) and the
// boot golden-ray self-test. Same tolerance discipline here, same fixture.
//
// Run headless:
//   UnrealEditor-Cmd.exe <proj> -ExecCmds="Automation RunTests SHITFIRE.Observer"
//       -unattended -nullrhi -nosound -testexit="Automation Test Queue Empty"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ShitfireFixtures.h"
#include "ShitfireGame.h"
#include "observer/ObserverPawn.h"
#include "bridge/ShitfireCoords.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/config/sim_config.h"
#include "shitfire/math/grid.h"
#include "shitfire/sim/spotburst_math.h"
#include "shitfire/terrain/heightfield.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireObserverLaseGateTest,
	"SHITFIRE.Observer.LaseGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireObserverLaseGateTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(TEXT("level1/raylos.json"));
	if (!TestNotNull(TEXT("raylos.json loads"), J.Get()))
	{
		return false;
	}
	const double MaxDist = J->GetNumberField(TEXT("maxDist"));
	const shitfire::HeightField Hf(shitfire::TerrainConfig{},
	                               J->GetNumberField(TEXT("terrainSeed")));
	const TArray<TSharedPtr<FJsonValue>>& Rays = J->GetArrayField(TEXT("rays"));
	if (!TestEqual(TEXT("fixture carries 500 rays"), Rays.Num(), 500))
	{
		return false;
	}

	// Any live world does - editor world when run from the editor (no
	// BeginPlay there), the game world under -game. No UnrealEd dependency.
	UWorld* World = nullptr;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (Ctx.World() && (Ctx.WorldType == EWorldType::Editor ||
			Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE))
		{
			World = Ctx.World();
			break;
		}
	}
	if (!TestNotNull(TEXT("a usable world exists"), World))
	{
		return false;
	}

	// The production pawn, spawned cold (no BeginPlay in a non-playing world -
	// exactly right: ComputeLase must not depend on play state).
	AObserverPawn* Pawn = World->SpawnActor<AObserverPawn>();
	if (!TestNotNull(TEXT("observer pawn spawns"), Pawn))
	{
		return false;
	}
	Pawn->SwayMilAmplitude = 0.0;   // no sway on a test bench (0 disables - 00-config.js:39)

	UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (!TestNotNull(TEXT("pawn camera exists"), Camera))
	{
		World->DestroyActor(Pawn);
		return false;
	}

	const shitfire::MapConfig Map{};
	int32 Mismatch = 0, FirstBad = -1, Hits = 0, GridMismatch = 0, OtMismatch = 0;
	double MaxDistErr = 0.0;

	for (int32 i = 0; i < Rays.Num(); i++)
	{
		const TSharedPtr<FJsonObject> Ray = Rays[i]->AsObject();
		const auto& O = Ray->GetArrayField(TEXT("o"));
		const auto& D = Ray->GetArrayField(TEXT("d"));

		// Park the camera ON the fixture ray through the coords shim: origin
		// sim meters -> UE cm; direction sim -> UE axis shuffle, then a
		// rotator, then let the pawn read its own components back.
		Pawn->SetActorLocation(ShitfireCoords::SimToUE(
			O[0]->AsNumber(), O[1]->AsNumber(), O[2]->AsNumber()));
		const FVector UEDir = ShitfireCoords::SimDirToUE(
			D[0]->AsNumber(), D[1]->AsNumber(), D[2]->AsNumber());
		Camera->SetWorldRotation(UEDir.Rotation());

		const FObserverLaseHit Got = Pawn->ComputeLase(Hf, MaxDist);

		const TSharedPtr<FJsonObject>* HitObj = nullptr;
		const bool bExpectHit = Ray->TryGetObjectField(TEXT("hit"), HitObj);
		if (Got.bHit != bExpectHit)
		{
			if (FirstBad < 0) { FirstBad = i; }
			Mismatch++;
			continue;
		}
		if (!bExpectHit)
		{
			continue;
		}
		Hits++;

		const double ExpDist = (*HitObj)->GetNumberField(TEXT("dist"));
		const double Err = FMath::Abs(Got.DistM - ExpDist);
		MaxDistErr = FMath::Max(MaxDistErr, Err);
		if (Err > 0.01)   // raylos_test.cpp / PARITY_TESTING tolerance
		{
			if (FirstBad < 0) { FirstBad = i; }
			Mismatch++;
		}

		// grid + OT factor derived from the FIXTURE's recorded hit, compared
		// against what the pawn's readout would print for its own hit.
		const FString ExpGrid = UTF8_TO_TCHAR(shitfire::gridOf(
			(*HitObj)->GetNumberField(TEXT("x")),
			(*HitObj)->GetNumberField(TEXT("z")), Map).c_str());
		if (Got.Grid != ExpGrid)
		{
			if (FirstBad < 0) { FirstBad = i; }
			GridMismatch++;
		}
		if (Got.OtFactor != int32(shitfire::sim::otFactor(ExpDist)))
		{
			if (FirstBad < 0) { FirstBad = i; }
			OtMismatch++;
		}
	}

	World->DestroyActor(Pawn);

	TestEqual(TEXT("hit/miss + distance parity over 500 fixture rays"), Mismatch, 0);
	TestEqual(TEXT("6-digit grid parity on every hit"), GridMismatch, 0);
	TestEqual(TEXT("OT factor parity on every hit"), OtMismatch, 0);
	AddInfo(FString::Printf(
		TEXT("lase gate: %d rays (%d hits), max |dist err| = %.3e m, first bad ray = %d"),
		Rays.Num(), Hits, MaxDistErr, FirstBad));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
