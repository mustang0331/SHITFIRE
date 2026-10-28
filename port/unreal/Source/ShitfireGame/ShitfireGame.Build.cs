// ShitfireGame - the runtime shell: world assembly, rendering, input, UI.
// ALL gameplay logic is C++ (no Blueprint logic - UNREAL_RESEARCH.md 8).
// Sim math NEVER lives here: this module compiles under UE's /fp:fast; every
// scored number comes from the prebuilt shitfire_core.lib via ShitfireCoreLib.
using UnrealBuildTool;

public class ShitfireGame : ModuleRules
{
	public ShitfireGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Niagara",
			"Slate", "SlateCore",      // observer HUD/reticle NativePaint (FSlateDrawElement)
			"ApplicationCore",         // observer demo input injection (IPlatformInputDeviceMapper)
			"RealtimeMeshComponent",   // terrain/road/ocean meshes (port/unreal/Plugins)
			"ShitfireCoreLib"          // the external /fp:precise sim core
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json",                    // fixture parsing for parity smoke + golden-ray self-test
			"AssetRegistry"            // SHITFIRE.Art.MaterialVisTag sweeps every /Game material
		});
	}
}
