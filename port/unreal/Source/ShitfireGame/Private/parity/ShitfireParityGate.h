// -ShitfireParityGate: the P4 parity gate as a command-line mode of the
// GAME itself, so it runs in every build configuration - above all
// SHIPPING, where UE's automation framework is absent (Launch.Build.cs:
// AutomationController loads "in any configuration besides shipping") and
// where the /fp:fast risk the whole external-lib design exists for would
// otherwise go untested end to end (UNREAL_RESEARCH.md 3a: parity green in
// DebugGame, silently divergent in Shipping).
//
// What it runs, in order:
//   1. smoke - mulberry32(1337) first 1000 draws u32-exact and one H(x,z)
//      row of the seed-1337 island within 1e-9 (SHITFIRE.Parity.Smoke's
//      checks) through the linked shitfire_core.lib;
//   2. transcripts - each listed level3 oracle transcript driven through a
//      standalone UShitfireSimHost at TimeScale 1000 with the bounded
//      schedule search (parity/ShitfireTranscriptReplay.h): impacts to
//      0.01 m, AAR exact.
//
// It runs at GAME MODULE STARTUP (FShitfireGameModule::StartupModule, inside
// FEngineLoop::PreInit) - before engine init, before any content - because a
// Shipping build requires cooked content and this uncooked project crashes in
// package serialization during engine init (so a later hook never fires).
// The exit code therefore means exactly one thing. The report is a JSON file
// under Saved/ParityGate/ (in Shipping that resolves to
// %LOCALAPPDATA%\Shitfire\Saved\ParityGate\) because a Shipping build has no
// log (bUseLoggingInShipping needs a unique build environment, which an
// installed Launcher engine refuses). A *.started breadcrumb lands first.
//
// Exit codes: 0 all green; 1 fixtures unreachable; 2 smoke failed;
// 3 at least one transcript failed.
//
// Command line:
//   <exe> -ShitfireParityGate [-ParityTranscripts=rel1,rel2,...]
//         [-ParityFixtures=<abs path to ENGINE_PORT/fixtures>]
//         -nullrhi -unattended -nosplash -nosound
#pragma once

#include "CoreMinimal.h"

namespace ShitfireParityGate
{
	/** The transcripts the gate (and SHITFIRE.Parity.SimHostReplay) drive by
	    default: five oracle missions spanning adjust+FFE bracketing, a moving
	    convoy with a pit stop, danger-close creeping fire, 60mm precision
	    grids, and the fratricide fail path. All five replay on the first
	    candidate schedule in Tier-0 (ADVERSARY_SIGNOFF.md section 4). */
	TArray<FString> DefaultTranscripts();

	/** True when -ShitfireParityGate is on the command line. */
	bool IsRequested();

	/** Module startup hook: runs the gate immediately when requested. */
	void RegisterIfRequested();

	/** The gate itself. Writes the report, then exits the process with the code. */
	void Run();
}
