#include "parity/ShitfireParityGate.h"
#include "ShitfireFixtures.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireSimHost.h"
#include "parity/ShitfireTranscriptReplay.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/math/prng.h"
#include "shitfire/sim_core.h"
#include "shitfire/terrain/heightfield.h"

namespace ShitfireParityGate
{
	TArray<FString> DefaultTranscripts()
	{
		return {
			TEXT("level3/mission_05_ch1-2-bunker-bracket.json"),
			TEXT("level3/mission_13_ch3-1-convoy-pit-stop.json"),
			TEXT("level3/mission_14_ch3-2-danger-close-creep.json"),
			TEXT("level3/mission_20_ch4-2-60mm-precision.json"),
			TEXT("level3/mission_32_fail-fratricide.json"),
		};
	}

	bool IsRequested()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("ShitfireParityGate"));
	}

	void RegisterIfRequested()
	{
		if (IsRequested())
		{
			// Run RIGHT HERE, at module startup (FEngineLoop::PreInit ->
			// LoadStartupModules), not on OnPostEngineInit: a Shipping build
			// requires cooked content, and the uncooked project crashes inside
			// package serialization during engine init before that delegate
			// fires (2026-10-01, FBufferReaderBase::Serialize access violation
			// with no report written). Nothing the gate needs is engine-side:
			// CoreUObject + Engine classes are registered, Json is loaded, the
			// sim host runs without a world or GEngine.
			Run();
		}
	}

	namespace
	{
		const TCHAR* ConfigName()
		{
#if UE_BUILD_SHIPPING
			return TEXT("Shipping");
#elif UE_BUILD_TEST
			return TEXT("Test");
#elif UE_BUILD_DEVELOPMENT
			return TEXT("Development");
#elif UE_BUILD_DEBUG
			return TEXT("Debug");
#else
			return TEXT("Unknown");
#endif
		}

		/** SHITFIRE.Parity.Smoke's two checks, framework-free. */
		bool RunSmoke(TSharedRef<FJsonObject> Out, FString& Why)
		{
			bool bOk = true;

			// ---- PRNG: seed 1337, first 1000 draws, u32-exact ----------------
			{
				const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(TEXT("level1/prng_streams.json"));
				if (!J.IsValid())
				{
					Why = TEXT("level1/prng_streams.json did not load");
					return false;
				}
				const TSharedPtr<FJsonObject> Stream =
					J->GetObjectField(TEXT("streams"))->GetObjectField(TEXT("1337"));
				const TArray<TSharedPtr<FJsonValue>>& U32s = Stream->GetArrayField(TEXT("u32"));
				if (U32s.Num() < 1000)
				{
					Why = TEXT("prng fixture has fewer than 1000 u32 draws");
					return false;
				}
				shitfire::Mulberry32 Rng(1337u);
				int32 Mismatches = 0;
				int32 FirstBad = -1;
				for (int32 i = 0; i < 1000; i++)
				{
					const uint32 Expected = static_cast<uint32>(U32s[i]->AsNumber());
					const uint32 Got = Rng.nextU32();
					if (Got != Expected)
					{
						if (FirstBad < 0) { FirstBad = i; }
						Mismatches++;
					}
				}
				Out->SetNumberField(TEXT("prngMismatches"), Mismatches);
				Out->SetNumberField(TEXT("prngFirstBad"), FirstBad);
				if (Mismatches > 0)
				{
					bOk = false;
					Why = FString::Printf(TEXT("mulberry32(1337) u32 stream: %d mismatches, first at draw %d"),
						Mismatches, FirstBad);
				}
			}

			// ---- H(x,z): one full row of the seed-1337 procedural island -----
			{
				const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(TEXT("level1/hxz_proc_1337.json"));
				if (!J.IsValid())
				{
					Why = TEXT("level1/hxz_proc_1337.json did not load");
					return false;
				}
				const TSharedPtr<FJsonObject> Grid = J->GetObjectField(TEXT("grid"));
				const int32 N = static_cast<int32>(Grid->GetNumberField(TEXT("n")));
				const double Step = Grid->GetNumberField(TEXT("step"));
				const double XFrom = Grid->GetNumberField(TEXT("xFrom"));
				const double ZFrom = Grid->GetNumberField(TEXT("zFrom"));
				const TArray<TSharedPtr<FJsonValue>>& Values = J->GetArrayField(TEXT("values"));
				if (Values.Num() != N * N)
				{
					Why = TEXT("hxz fixture grid is not n*n");
					return false;
				}
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
					const double Err = FMath::Abs(Hf.H(X, Z) - Expected);
					MaxErr = FMath::Max(MaxErr, Err);
					if (Err > 1e-9) { Mismatches++; }   // PARITY_TESTING tolerance
				}
				Out->SetNumberField(TEXT("hRowMismatches"), Mismatches);
				Out->SetNumberField(TEXT("hRowMaxErrM"), MaxErr);
				if (Mismatches > 0)
				{
					bOk = false;
					if (Why.IsEmpty())
					{
						Why = FString::Printf(TEXT("H(x,z) row %d: %d samples outside 1e-9 (max |err| %.3e m)"),
							RowJ, Mismatches, MaxErr);
					}
				}
			}
			return bOk;
		}

		TArray<FString> TranscriptList()
		{
			FString Csv;
			if (FParse::Value(FCommandLine::Get(), TEXT("ParityTranscripts="), Csv) && !Csv.IsEmpty())
			{
				TArray<FString> Parts;
				Csv.ParseIntoArray(Parts, TEXT(","), true);
				for (FString& P : Parts) { P.TrimStartAndEndInline(); }
				return Parts;
			}
			return DefaultTranscripts();
		}

		FString ReportDir()
		{
			return FPaths::ConvertRelativePathToFull(
				FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ParityGate")));
		}
	}

	void Run()
	{
		const double T0 = FPlatformTime::Seconds();
		// Breadcrumb first: proves the module loaded and the gate started even
		// if the process dies later (a Shipping build has no log to say so).
		FFileHelper::SaveStringToFile(
			FString::Printf(TEXT("started %s\n"), *FDateTime::UtcNow().ToIso8601()),
			*FPaths::Combine(ReportDir(), FString::Printf(TEXT("parity-gate-%s.started"), ConfigName())));
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("config"), ConfigName());
		Report->SetStringField(TEXT("fixturesDir"), ShitfireFixtures::FixturesDir());
		Report->SetStringField(TEXT("startedUtc"), FDateTime::UtcNow().ToIso8601());

		int32 ExitCode = 0;
		FString Summary;

		// ---- 1. smoke ---------------------------------------------------------
		TSharedRef<FJsonObject> Smoke = MakeShared<FJsonObject>();
		FString SmokeWhy;
		const bool bSmokeOk = RunSmoke(Smoke, SmokeWhy);
		Smoke->SetBoolField(TEXT("pass"), bSmokeOk);
		if (!bSmokeOk) { Smoke->SetStringField(TEXT("why"), SmokeWhy); }
		Report->SetObjectField(TEXT("smoke"), Smoke);
		if (!bSmokeOk)
		{
			ExitCode = SmokeWhy.Contains(TEXT("did not load")) ? 1 : 2;
			Summary = TEXT("SMOKE FAIL: ") + SmokeWhy;
		}

		// ---- 2. transcripts through a standalone host -------------------------
		TArray<TSharedPtr<FJsonValue>> Rows;
		int32 Passed = 0, Failed = 0;
		if (ExitCode == 0)
		{
			// UGameInstanceSubsystem is UCLASS(Within = GameInstance): the host
			// needs a GameInstance outer (the automation test's arrangement).
			// GEngine does not exist yet at module startup - transient package.
			UGameInstance* HostOuter = NewObject<UGameInstance>(GetTransientPackage());
			HostOuter->AddToRoot();
			UShitfireSimHost* Host = NewObject<UShitfireSimHost>(HostOuter);
			Host->AddToRoot();
			Host->InitializeStandalone();

			for (const FString& Rel : TranscriptList())
			{
				TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				Row->SetStringField(TEXT("transcript"), Rel);
				ShitfireParity::FTranscript Tr;
				if (!ShitfireParity::LoadTranscript(Rel, Tr))
				{
					Row->SetBoolField(TEXT("pass"), false);
					Row->SetStringField(TEXT("why"), TEXT("fixture did not load"));
					Rows.Add(MakeShared<FJsonValueObject>(Row));
					Failed++;
					ExitCode = 1;
					continue;
				}
				ShitfireParity::FReplayFlavor Flavor(Tr);
				const double TStart = FPlatformTime::Seconds();
				const ShitfireParity::FReplayOutcome R =
					ShitfireParity::ReplayThroughHost(*Host, Tr, Flavor, []() {});
				Row->SetBoolField(TEXT("pass"), R.bPassed);
				Row->SetNumberField(TEXT("attempts"), R.Attempts);
				Row->SetNumberField(TEXT("worstDTgtDiffM"), R.Best.WorstDTgtDiff);
				Row->SetNumberField(TEXT("worstSkewS"), R.Best.WorstSkew);
				Row->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - TStart);
				if (!R.bPassed)
				{
					TArray<TSharedPtr<FJsonValue>> Errs;
					for (const FString& E : R.Best.Errors) { Errs.Add(MakeShared<FJsonValueString>(E)); }
					Row->SetArrayField(TEXT("errors"), Errs);
					Failed++;
					ExitCode = 3;
				}
				else
				{
					Passed++;
				}
				Rows.Add(MakeShared<FJsonValueObject>(Row));
				Host->SetFlavorRngForNextLaunch(nullptr);
				Host->ShutdownMission();
			}
			Host->RemoveFromRoot();
			HostOuter->RemoveFromRoot();
			if (Summary.IsEmpty())
			{
				Summary = FString::Printf(TEXT("%s: smoke %s; transcripts %d/%d exact"),
					ExitCode == 0 ? TEXT("PASS") : TEXT("FAIL"),
					bSmokeOk ? TEXT("ok") : TEXT("FAIL"), Passed, Passed + Failed);
			}
		}
		Report->SetArrayField(TEXT("transcripts"), Rows);
		Report->SetNumberField(TEXT("passed"), Passed);
		Report->SetNumberField(TEXT("failed"), Failed);
		Report->SetNumberField(TEXT("exitCode"), ExitCode);
		Report->SetStringField(TEXT("summary"), Summary);
		Report->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - T0);

		// ---- report file (the record - a Shipping build has no log) ----------
		FString Json;
		const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Report, W);
		const FString Path = FPaths::Combine(ReportDir(),
			FString::Printf(TEXT("parity-gate-%s.json"), ConfigName()));
		FFileHelper::SaveStringToFile(Json, *Path);
		FFileHelper::SaveStringToFile(Summary + TEXT("\n"),
			*FPaths::Combine(ReportDir(), FString::Printf(TEXT("parity-gate-%s.txt"), ConfigName())));

		UE_LOG(LogShitfire, Display, TEXT("ParityGate [%s]: %s -> %s (exit %d)"),
			ConfigName(), *Summary, *Path, ExitCode);

		// Forced exit on purpose: nothing beyond this point is the gate's
		// business, no map has loaded, and TerminateProcess carries the code
		// (WindowsPlatformMisc.cpp RequestExitWithStatus, Force path).
		if (GLog) { GLog->Flush(); }
		FPlatformMisc::RequestExitWithStatus(true, static_cast<uint8>(ExitCode), TEXT("ShitfireParityGate"));
	}
}
