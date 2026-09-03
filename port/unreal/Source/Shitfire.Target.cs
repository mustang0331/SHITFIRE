// SHITFIRE Unreal port - game target. Conventions copied from the engine's
// own 5.8 TP_Blank template (BuildSettingsVersion.V7 / Unreal5_8 include order).
using UnrealBuildTool;
using System.Collections.Generic;

public class ShitfireTarget : TargetRules
{
	public ShitfireTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ShitfireGame");
	}
}
