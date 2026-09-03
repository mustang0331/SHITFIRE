// SHITFIRE.Parity.Smoke - proves the External-module link and /fp isolation
// INSIDE the engine process: the PRNG stream (integer ops) and one H(x,z)
// row (transcendental-heavy terrain math) both run through the prebuilt
// /fp:precise shitfire_core.lib while this module compiles /fp:fast.
// If UE's flags ever leaked into core math, the H row is where it shows.
//
// Development config now; running it under a Shipping build is a P4 gate
// item (port plan). Full parity lives in port/core-tests (Tier 0).
//
// Run headless:
//   UnrealEditor-Cmd.exe <proj> -ExecCmds="Automation RunTests SHITFIRE.Parity"
//       -unattended -nullrhi -nosound -testexit="Automation Test Queue Empty"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ShitfireFixtures.h"
#include "ShitfireGame.h"

// Engine-free sim core (prebuilt lib - see ShitfireCoreLib.Build.cs).
#include "shitfire/math/prng.h"
#include "shitfire/terrain/heightfield.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShitfireParitySmokeTest,
	"SHITFIRE.Parity.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::ProductFilter)

bool FShitfireParitySmokeTest::RunTest(const FString& Parameters)
{
	// ---- PRNG: seed 1337, first 1000 draws, u32-exact --------------------
	{
		const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(TEXT("level1/prng_streams.json"));
		if (!TestNotNull(TEXT("prng_streams.json loads"), J.Get()))
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Stream =
			J->GetObjectField(TEXT("streams"))->GetObjectField(TEXT("1337"));
		const TArray<TSharedPtr<FJsonValue>>& U32s = Stream->GetArrayField(TEXT("u32"));
		if (!TestTrue(TEXT("fixture has >= 1000 u32 draws"), U32s.Num() >= 1000))
		{
			return false;
		}

		shitfire::Mulberry32 Rng(1337u);
		int32 Mismatches = 0;
		int32 FirstBad = -1;
		for (int32 i = 0; i < 1000; i++)
		{
			// u32 values are integers <= 2^32-1: exactly representable in the
			// JSON double they were parsed into.
			const uint32 Expected = static_cast<uint32>(U32s[i]->AsNumber());
			const uint32 Got = Rng.nextU32();
			if (Got != Expected)
			{
				if (FirstBad < 0) { FirstBad = i; }
				Mismatches++;
			}
		}
		TestEqual(TEXT("mulberry32(1337) u32 stream, first 1000 draws exact"), Mismatches, 0);
		if (Mismatches > 0)
		{
			AddError(FString::Printf(TEXT("first PRNG mismatch at draw %d"), FirstBad));
		}
	}

	// ---- H(x,z): one full row of the seed-1337 procedural island ---------
	{
		const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(TEXT("level1/hxz_proc_1337.json"));
		if (!TestNotNull(TEXT("hxz_proc_1337.json loads"), J.Get()))
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Grid = J->GetObjectField(TEXT("grid"));
		const int32 N = static_cast<int32>(Grid->GetNumberField(TEXT("n")));
		const double Step = Grid->GetNumberField(TEXT("step"));
		const double XFrom = Grid->GetNumberField(TEXT("xFrom"));
		const double ZFrom = Grid->GetNumberField(TEXT("zFrom"));
		const TArray<TSharedPtr<FJsonValue>>& Values = J->GetArrayField(TEXT("values"));
		if (!TestEqual(TEXT("fixture grid is n*n"), Values.Num(), N * N))
		{
			return false;
		}

		// Row j = N/2 crosses the island interior (z = zFrom + j*step = 25 m).
		const int32 RowJ = N / 2;
		const double Z = ZFrom + RowJ * Step;
		const shitfire::HeightField Hf(shitfire::TerrainConfig{},
		                               J->GetNumberField(TEXT("terrainSeed")));
		int32 Mismatches = 0;
		double MaxErr = 0.0;
		for (int32 i = 0; i < N; i++)
		{
			const double X = XFrom + i * Step;
			const double Expected = Values[RowJ * N + i]->AsNumber();
			const double Got = Hf.H(X, Z);
			const double Err = FMath::Abs(Got - Expected);
			MaxErr = FMath::Max(MaxErr, Err);
			if (Err > 1e-9) { Mismatches++; }   // PARITY_TESTING tolerance
		}
		TestEqual(TEXT("H(x,z) row within 1e-9 of oracle"), Mismatches, 0);
		AddInfo(FString::Printf(TEXT("H row %d: max |err| = %.3e m over %d samples"),
			RowJ, MaxErr, N));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
