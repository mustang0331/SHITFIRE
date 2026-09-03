// SHITFIRE Unreal port - editor target.
using UnrealBuildTool;
using System.Collections.Generic;

public class ShitfireEditorTarget : TargetRules
{
	public ShitfireEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ShitfireGame");
	}
}
