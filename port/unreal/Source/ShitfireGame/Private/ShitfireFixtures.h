// Dev/test-only helper: load the committed browser-oracle fixtures from the
// repo (ENGINE_PORT/fixtures) via UE's Json module. Used by the
// SHITFIRE.Parity.Smoke automation test and the boot golden-ray self-test.
// Never compiled into Shipping; packaged non-dev builds don't carry fixtures.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace ShitfireFixtures
{
	/** Absolute path of ENGINE_PORT/fixtures (repo-relative to the project). */
	FString FixturesDir();

	/** Load + parse one fixture (path relative to FixturesDir). nullptr on failure. */
	TSharedPtr<FJsonObject> LoadJson(const FString& RelPath);
}
