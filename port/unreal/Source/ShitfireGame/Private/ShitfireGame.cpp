#include "ShitfireGame.h"
#include "Modules/ModuleManager.h"
#include "parity/ShitfireParityGate.h"

DEFINE_LOG_CATEGORY(LogShitfire);

// The game module: the default impl plus one hook - the -ShitfireParityGate
// command-line mode (parity/ShitfireParityGate.h) registers itself on
// OnPostEngineInit here, because the automation framework that the dev
// gates ride does not exist in Shipping builds.
class FShitfireGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		ShitfireParityGate::RegisterIfRequested();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FShitfireGameModule, ShitfireGame, "ShitfireGame");
