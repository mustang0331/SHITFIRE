// ShitfireParity - the in-engine transcript replay machinery, shared by
// the SHITFIRE.Parity.SimHostReplay automation test (dev/editor builds) and
// the -ShitfireParityGate command-line runner (every config, including
// Shipping, where the automation framework does not exist - Launch.Build.cs
// loads AutomationController "in any configuration besides shipping").
//
// Everything here is a port of the Tier-0 comparator
// (port/core-tests/level3/replay_test.cpp), lifted verbatim out of
// Private/Tests/ShitfireSimHostReplayTest.cpp (b997e7d) so one copy drives
// both consumers:
//   - recorded TLOG stamps are Math.round(t*10)/10 of 16 ms frame times, so
//     each send has up to ~6 grid candidates (GridCandidates);
//   - gated flavor lines (fdc_quips.h FlavorGate) must reproduce their
//     recorded PRESENCE because four gates air before say() calls whose
//     return time schedules impacts (FReplayFlavor);
//   - CompareRun asserts impacts (count/order/msg/shell/|dTgt| <= 0.01 m/
//     skew <= 0.5 s) and the AAR (verdict/stars/outcome/rounds/dur exact).
// Tolerances come from ENGINE_PORT/PARITY_TESTING.md and are never loosened.
#pragma once

#include "CoreMinimal.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/doctrine/fdc_quips.h"
#include "shitfire/math/prng.h"
#include "shitfire/review/tlog.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class UShitfireSimHost;

namespace ShitfireParity
{
	/* ---- recorded transcript (UE-Json flavor of the Tier-0 loader) ------ */

	struct FRecEntry
	{
		double T = 0;
		FString Kind;
		FString Msg;
		double DTgt = 0;            // impact
		FString Shell;              // impact ('he' when absent, tlog.h)
		int32 Stars = -1;           // aar
		FString Outcome;            // aar
		double Rounds = 0;          // aar
		double Dur = 0;             // aar
	};

	struct FTranscript
	{
		FString RelPath;            // fixture path, e.g. level3/mission_05_...json
		FString Chapter;            // "" = skirmish
		FString ScenarioType;
		FString Difficulty;
		double SeedMission = 1;
		double SeedTerrain = 1337;
		TArray<FRecEntry> Entries;
	};

	/** Load one level3 fixture (path relative to ENGINE_PORT/fixtures). */
	bool LoadTranscript(const FString& RelPath, FTranscript& Out);

	/* ---- clock helpers (replay_test.cpp:104-124, verbatim ports) -------- */

	double JsRound1(double X);
	TArray<int64> GridCandidates(double TRec);
	int64 DeliveryFrameMs(double T);

	/* ---- gated-flavor replay (replay_test.cpp:126-311, trimmed) ---------
	   Presence of the five gated flavor lines is answered from the recording
	   (fdc_quips.h FlavorGate note); pool selection stays free flavor. */

	class FReplayFlavor final : public shitfire::doctrine::IFlavorRng
	{
	public:
		explicit FReplayFlavor(const FTranscript& Tr);

		double next() override;
		bool coin(double P, shitfire::doctrine::FlavorGate Gate, double PredictedAirTime) override;

		void ResetRun();

	private:
		shitfire::Mulberry32 Rng;
		std::map<shitfire::doctrine::FlavorGate, std::vector<double>> Times;
		std::map<shitfire::doctrine::FlavorGate, std::set<std::size_t>> Consumed;
	};

	/* ---- one attempt's verdict ------------------------------------------ */

	struct FAttemptResult
	{
		bool bPass = true;
		double FirstFailT = 1e300;
		double WorstDTgtDiff = 0;
		double WorstSkew = 0;
		TArray<FString> Errors;

		void Fail(double T, const FString& What);
	};

	FAttemptResult CompareRun(const FTranscript& Tr,
	                          const std::vector<shitfire::review::TlogEntry>& Got);

	/* ---- the driver ------------------------------------------------------ */

	struct FReplayOutcome
	{
		bool bPassed = false;
		int32 Attempts = 0;
		FAttemptResult Best;
	};

	/** Drive one transcript end-to-end through Host at TimeScale 1000 with
	    the bounded candidate-schedule search (<= start candidates x 64
	    bounded-backtrack attempts). Host must be initialized
	    (InitializeStandalone or a live subsystem). OnAttemptStart runs before
	    each attempt's LaunchMission (reset any collectors there). The LAST
	    attempt's mission is left live so the caller can inspect
	    Host.GetSimCore()->tlog() and its delegates; the caller shuts it down
	    and clears the flavor RNG (SetFlavorRngForNextLaunch(nullptr)). */
	FReplayOutcome ReplayThroughHost(UShitfireSimHost& Host, const FTranscript& Tr,
	                                 FReplayFlavor& Flavor,
	                                 TFunctionRef<void()> OnAttemptStart);
}
