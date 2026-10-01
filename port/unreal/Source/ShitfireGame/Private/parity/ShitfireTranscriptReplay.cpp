#include "parity/ShitfireTranscriptReplay.h"
#include "ShitfireFixtures.h"
#include "ShitfireGame.h"
#include "bridge/ShitfireSimHost.h"

#include "shitfire/sim_core.h"

#include <algorithm>
#include <cmath>

namespace ShitfireParity
{
	/* ---- recorded transcript ------------------------------------------- */

	bool LoadTranscript(const FString& RelPath, FTranscript& Out)
	{
		Out.RelPath = RelPath;
		const TSharedPtr<FJsonObject> J = ShitfireFixtures::LoadJson(RelPath);
		if (!J.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Meta = J->GetObjectField(TEXT("meta"));
		Meta->TryGetStringField(TEXT("chapter"), Out.Chapter);   // null -> stays ""
		Out.ScenarioType = Meta->GetStringField(TEXT("scenarioType"));
		Out.Difficulty = Meta->GetStringField(TEXT("difficulty"));
		Out.SeedMission = Meta->GetNumberField(TEXT("seedMission"));
		Out.SeedTerrain = Meta->GetNumberField(TEXT("seedTerrain"));
		Out.Entries.Reset();
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("tlog")))
		{
			const TSharedPtr<FJsonObject> E = V->AsObject();
			FRecEntry R;
			R.T = E->GetNumberField(TEXT("t"));
			R.Kind = E->GetStringField(TEXT("kind"));
			E->TryGetStringField(TEXT("msg"), R.Msg);
			if (R.Kind == TEXT("impact"))
			{
				R.DTgt = E->GetNumberField(TEXT("dTgt"));
				if (!E->TryGetStringField(TEXT("shell"), R.Shell))
				{
					R.Shell = TEXT("he");
				}
			}
			else if (R.Kind == TEXT("aar"))
			{
				double S = -1;
				if (E->TryGetNumberField(TEXT("stars"), S)) { R.Stars = static_cast<int32>(S); }
				E->TryGetStringField(TEXT("outcome"), R.Outcome);
				E->TryGetNumberField(TEXT("rounds"), R.Rounds);
				E->TryGetNumberField(TEXT("dur"), R.Dur);
			}
			Out.Entries.Add(MoveTemp(R));
		}
		return Out.Entries.Num() > 0;
	}

	/* ---- clock helpers (replay_test.cpp:104-124, verbatim ports) -------- */

	double JsRound1(double X) { return std::floor(X * 10 + 0.5) / 10; }

	TArray<int64> GridCandidates(double TRec)
	{
		TArray<int64> Out;
		const int64 Center = static_cast<int64>(std::floor(TRec * 1000 + 0.5));
		for (int64 M = ((Center - 64) / 16) * 16; M <= Center + 64; M += 16)
		{
			if (M < 0) { continue; }
			if (JsRound1(static_cast<double>(M) / 1000.0) == JsRound1(TRec))
			{
				Out.Add(M);
			}
		}
		return Out;
	}

	int64 DeliveryFrameMs(double T)
	{
		const double Raw = T * 1000.0;
		int64 M = static_cast<int64>(std::ceil(Raw / 16.0 - 1e-9)) * 16;
		if (static_cast<double>(M) < Raw - 1e-9) { M += 16; }
		return M;
	}

	/* ---- gated-flavor replay --------------------------------------------- */

	namespace
	{
		void ReplaceAll(std::string& S, const std::string& From, const std::string& To)
		{
			std::size_t Pos = 0;
			while ((Pos = S.find(From, Pos)) != std::string::npos)
			{
				S.replace(Pos, From.size(), To);
				Pos += To.size();
			}
		}

		std::string NormalizeCallsigns(std::string S)
		{
			ReplaceAll(S, "HACKSAW FIRES", "HELLHOUND FIRES");
			ReplaceAll(S, "SUNLAMP ACTUAL", "HELLHOUND FIRES");
			ReplaceAll(S, "HACKSAW", "HELLHOUND");
			ReplaceAll(S, "SUNLAMP", "HELLHOUND");
			return S;
		}
	}

	FReplayFlavor::FReplayFlavor(const FTranscript& Tr) : Rng(0x5F3759DFu)
	{
		namespace sf = shitfire;
		const sf::doctrine::QuipSet Quips = sf::doctrine::makeQuips();
		const std::map<sf::doctrine::FlavorGate, const std::vector<std::string>*> Gates = {
			{sf::doctrine::FlavorGate::FfeAck, &Quips.ffeAck.lines},
			{sf::doctrine::FlavorGate::CompleteTail, &Quips.completeTail.lines},
			{sf::doctrine::FlavorGate::ReadbackTail, &Quips.readbackTail.lines},
			{sf::doctrine::FlavorGate::SnideRound, &Quips.snideRound.lines},
			{sf::doctrine::FlavorGate::CorrSnark, &Quips.corrSnark.lines},
		};
		for (const FRecEntry& E : Tr.Entries)
		{
			if (E.Kind != TEXT("fdc")) { continue; }
			const std::string Msg = NormalizeCallsigns(TCHAR_TO_UTF8(*E.Msg));
			for (const auto& G : Gates)
			{
				std::vector<std::string> Normalized;
				for (const std::string& L : *G.second)
				{
					Normalized.push_back(NormalizeCallsigns(L));
				}
				if (std::find(Normalized.begin(), Normalized.end(), Msg) != Normalized.end())
				{
					Times[G.first].push_back(E.T);
					break;
				}
			}
		}
	}

	double FReplayFlavor::next() { return Rng.next(); }

	bool FReplayFlavor::coin(double /*P*/, shitfire::doctrine::FlavorGate Gate, double PredictedAirTime)
	{
		if (Gate == shitfire::doctrine::FlavorGate::CareerCallback) { return false; }
		const auto It = Times.find(Gate);
		if (It == Times.end()) { return false; }
		auto& Used = Consumed[Gate];
		const double TPred = JsRound1(
			static_cast<double>(DeliveryFrameMs(PredictedAirTime)) / 1000.0);
		for (std::size_t i = 0; i < It->second.size(); i++)
		{
			if (Used.count(i)) { continue; }
			if (std::fabs(It->second[i] - TPred) <= 0.75)
			{
				Used.insert(i);
				return true;
			}
			if (It->second[i] > TPred + 0.75) { break; }
		}
		return false;
	}

	void FReplayFlavor::ResetRun()
	{
		Consumed.clear();
		Rng = shitfire::Mulberry32(0x5F3759DFu);
	}

	/* ---- one attempt's verdict ------------------------------------------ */

	void FAttemptResult::Fail(double T, const FString& What)
	{
		bPass = false;
		if (T < FirstFailT) { FirstFailT = T; }
		if (Errors.Num() < 10) { Errors.Add(What); }
	}

	FAttemptResult CompareRun(const FTranscript& Tr,
	                          const std::vector<shitfire::review::TlogEntry>& Got)
	{
		namespace sf = shitfire;
		FAttemptResult R;

		// impacts: count, order, msg, dTgt within 0.01 m, shell, t +/- 0.5 s
		TArray<const FRecEntry*> RecImpacts;
		for (const FRecEntry& E : Tr.Entries)
		{
			if (E.Kind == TEXT("impact")) { RecImpacts.Add(&E); }
		}
		std::vector<const sf::review::TlogEntry*> GotImpacts;
		for (const auto& E : Got)
		{
			if (E.kind == "impact") { GotImpacts.push_back(&E); }
		}
		if (RecImpacts.Num() != static_cast<int32>(GotImpacts.size()))
		{
			R.Fail(RecImpacts.Num() > 0 ? RecImpacts[0]->T : 0,
				FString::Printf(TEXT("impact count: recorded %d, got %d"),
					RecImpacts.Num(), static_cast<int32>(GotImpacts.size())));
		}
		const int32 NImp = FMath::Min(RecImpacts.Num(), static_cast<int32>(GotImpacts.size()));
		for (int32 i = 0; i < NImp; i++)
		{
			const FRecEntry& Rec = *RecImpacts[i];
			const sf::review::TlogEntry& Mine = *GotImpacts[static_cast<std::size_t>(i)];
			const FString MineMsg = UTF8_TO_TCHAR(Mine.msg.c_str());
			if (!Rec.Msg.Equals(MineMsg))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d msg: recorded '%s', got '%s'"),
					i, *Rec.Msg, *MineMsg));
			}
			const double Diff = std::fabs(Rec.DTgt - Mine.dTgt.value_or(-1.0));
			R.WorstDTgtDiff = FMath::Max(R.WorstDTgtDiff, Diff);
			if (Diff > 0.01)   // the gate tolerance; Tier-0 holds this at 0.000
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d dTgt: recorded %.3f, got %.3f"),
					i, Rec.DTgt, Mine.dTgt.value_or(-1.0)));
			}
			const FString MineShell = UTF8_TO_TCHAR(Mine.shell.value_or("he").c_str());
			if (!Rec.Shell.Equals(MineShell))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d shell: recorded %s, got %s"),
					i, *Rec.Shell, *MineShell));
			}
			const double Skew = std::fabs(Rec.T - Mine.t);
			R.WorstSkew = FMath::Max(R.WorstSkew, Skew);
			if (Skew > 0.5)
			{
				R.Fail(Rec.T, FString::Printf(TEXT("impact #%d time skew %.3f s"), i, Skew));
			}
		}

		// the AAR: verdict, stars, outcome, rounds, dur
		TArray<const FRecEntry*> RecAars;
		for (const FRecEntry& E : Tr.Entries)
		{
			if (E.Kind == TEXT("aar")) { RecAars.Add(&E); }
		}
		std::vector<const sf::review::TlogEntry*> GotAars;
		for (const auto& E : Got)
		{
			if (E.kind == "aar") { GotAars.push_back(&E); }
		}
		if (RecAars.Num() != static_cast<int32>(GotAars.size()))
		{
			R.Fail(RecAars.Num() > 0 ? RecAars[0]->T : 1e299,
				FString::Printf(TEXT("aar count: recorded %d, got %d"),
					RecAars.Num(), static_cast<int32>(GotAars.size())));
		}
		const int32 NAar = FMath::Min(RecAars.Num(), static_cast<int32>(GotAars.size()));
		for (int32 i = 0; i < NAar; i++)
		{
			const FRecEntry& Rec = *RecAars[i];
			const sf::review::TlogEntry& Mine = *GotAars[static_cast<std::size_t>(i)];
			const FString MineMsg = UTF8_TO_TCHAR(Mine.msg.c_str());
			if (!Rec.Msg.Equals(MineMsg))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar verdict: recorded '%s', got '%s'"),
					*Rec.Msg, *MineMsg));
			}
			const int32 MineStars = Mine.stars.has_value() ? static_cast<int32>(*Mine.stars) : -1;
			if (Rec.Stars != MineStars)
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar stars: recorded %d, got %d"),
					Rec.Stars, MineStars));
			}
			const FString MineOutcome = UTF8_TO_TCHAR(Mine.outcome.value_or("").c_str());
			if (!Rec.Outcome.Equals(MineOutcome))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar outcome: recorded '%s', got '%s'"),
					*Rec.Outcome, *MineOutcome));
			}
			if (Rec.Rounds != Mine.rounds.value_or(-1.0))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar rounds: recorded %.0f, got %.0f"),
					Rec.Rounds, Mine.rounds.value_or(-1.0)));
			}
			if (Rec.Dur != Mine.dur.value_or(-1.0))
			{
				R.Fail(Rec.T, FString::Printf(TEXT("aar dur: recorded %.0f, got %.0f"),
					Rec.Dur, Mine.dur.value_or(-1.0)));
			}
		}
		return R;
	}

	/* ---- the driver (ShitfireSimHostReplayTest.cpp b997e7d, lifted) ----- */

	FReplayOutcome ReplayThroughHost(UShitfireSimHost& Host, const FTranscript& Tr,
	                                 FReplayFlavor& Flavor,
	                                 TFunctionRef<void()> OnAttemptStart)
	{
		FReplayOutcome Out;
		Out.Best.bPass = false;
		Out.Best.Errors.Add(TEXT("no attempt ran"));
		if (Tr.Entries.Num() == 0)
		{
			return Out;
		}
		const double LastRecT = Tr.Entries.Last().T;

		// send list = parse entries (raw player transmissions)
		TArray<const FRecEntry*> Sends;
		for (const FRecEntry& E : Tr.Entries)
		{
			if (E.Kind == TEXT("parse")) { Sends.Add(&E); }
		}

		// candidate schedule search (replay_test.cpp:604-662, bounded backtrack)
		const TArray<int64> StartCands = GridCandidates(Tr.Entries[0].T);
		TArray<TArray<int64>> SendCands;
		for (const FRecEntry* S : Sends) { SendCands.Add(GridCandidates(S->T)); }

		for (int32 Si = 0; Si < StartCands.Num() && !Out.bPassed; Si++)
		{
			TArray<int32> Pick;
			Pick.Init(0, SendCands.Num());
			for (int32 Attempt = 0; Attempt < 64; Attempt++)
			{
				// materialize strictly-increasing send times after the start frame
				TArray<int64> SendMs;
				SendMs.Init(0, SendCands.Num());
				int64 Prev = StartCands[Si];
				bool bFeasible = true;
				for (int32 i = 0; i < SendCands.Num(); i++)
				{
					int64 Chosen = -1;
					for (int32 C = Pick[i]; C < SendCands[i].Num(); C++)
					{
						if (SendCands[i][C] > Prev)
						{
							Chosen = SendCands[i][C];
							Pick[i] = C;
							break;
						}
					}
					if (Chosen < 0) { bFeasible = false; break; }
					SendMs[i] = Chosen;
					Prev = Chosen;
				}
				if (!bFeasible) { break; }

				// ---- one attempt through the HOST ---------------------------
				Flavor.ResetRun();
				OnAttemptStart();
				Host.SetFlavorRngForNextLaunch(&Flavor);
				FShitfireMissionParams P;
				P.ChapterId = Tr.Chapter;
				P.SkirmishType = Tr.ScenarioType;
				P.Difficulty = Tr.Difficulty;
				P.MissionSeed = Tr.SeedMission;
				P.TerrainSeed = Tr.SeedTerrain;
				P.StartMs = StartCands[Si];
				Host.LaunchMission(P);
				Host.SetTimeScale(1000.0);
				for (int32 i = 0; i < Sends.Num(); i++)
				{
					Host.QueueMessageAtSimMs(SendMs[i], Sends[i]->Msg);
				}

				// drive: 16 ms real ticks = 16 s sim each at 1000x
				int32 Guard = 0;
				while (Host.GetSimNow() < LastRecT + 12.0 && Guard++ < 5000)
				{
					Host.Tick(0.016f);
				}
				// trailing drain (replay_test.cpp:579-593 horizon)
				while (Host.GetPendingSimEventCount() > 0 &&
				       Host.GetSimNow() <= LastRecT + 240.0 && Guard++ < 20000)
				{
					Host.Tick(0.016f);
				}

				const FAttemptResult R = CompareRun(Tr, Host.GetSimCore()->tlog().entries());
				Out.Attempts++;
				if (R.bPass)
				{
					Out.Best = R;
					Out.bPassed = true;
					break;
				}
				if (!Out.Best.bPass &&
				    (Out.Best.Errors.Num() == 1 || R.Errors.Num() < Out.Best.Errors.Num() ||
				     R.FirstFailT > Out.Best.FirstFailT))
				{
					Out.Best = R;
				}
				// bump the latest send at or before the first failure
				const int64 FailMs = DeliveryFrameMs(R.FirstFailT);
				int32 Bump = -1;
				for (int32 i = SendMs.Num() - 1; i >= 0; i--)
				{
					if (SendMs[i] <= FailMs && Pick[i] + 1 < SendCands[i].Num())
					{
						Bump = i;
						break;
					}
				}
				if (Bump < 0) { break; }
				Pick[Bump]++;
				for (int32 i = Bump + 1; i < Pick.Num(); i++) { Pick[i] = 0; }
			}
		}
		return Out;
	}
}
