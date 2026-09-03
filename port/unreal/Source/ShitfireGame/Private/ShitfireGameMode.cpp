#include "ShitfireGameMode.h"
#include "ShitfireGame.h"
#include "ShitfireObserverPawn.h"

// Engine-free sim core (prebuilt /fp:precise lib via ShitfireCoreLib).
#include "shitfire/math/prng.h"

AShitfireGameMode::AShitfireGameMode()
{
	DefaultPawnClass = AShitfireObserverPawn::StaticClass();
}

void AShitfireGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Linkage proof: one draw through the prebuilt core lib. Seed 1337's
	// first double is 0.6270739405881613 in the oracle fixture
	// (ENGINE_PORT/fixtures/level1/prng_streams.json); the real parity gate
	// is the SHITFIRE.Parity.Smoke automation test.
	shitfire::Mulberry32 Rng(1337u);
	UE_LOG(LogShitfire, Log, TEXT("shitfire_core linked: mulberry32(1337) first draw = %.17g"), Rng.next());
}
