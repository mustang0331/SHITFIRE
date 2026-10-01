// Load the committed browser-oracle fixtures from the repo
// (ENGINE_PORT/fixtures) via UE's Json module. Used by the SHITFIRE.Parity
// automation tests, the boot golden-ray self-test (dev builds) and the
// -ShitfireParityGate runner (every config, Shipping included - the gate is
// what proves the /fp:precise lib survives a Shipping link). Packaged
// non-dev builds don't carry fixtures; -ParityFixtures=<dir> points at them.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace ShitfireFixtures
{
	/** Absolute path of ENGINE_PORT/fixtures (repo-relative to the project,
	    or the -ParityFixtures= override). */
	FString FixturesDir();

	/** Load + parse one fixture (path relative to FixturesDir). nullptr on failure. */
	TSharedPtr<FJsonObject> LoadJson(const FString& RelPath);
}
