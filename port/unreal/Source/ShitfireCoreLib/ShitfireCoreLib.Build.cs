// ShitfireCoreLib - the /fp:precise guarantee lives here.
//
// ModuleType.External (UNREAL_RESEARCH.md 3a): UBT compiles NOTHING in this
// module. It only exposes the engine-free sim core's headers and links the
// prebuilt static lib that CMake produced under /fp:precise. UE compiles its
// own modules /fp:fast (reassociation + FMA contraction) - a parity breaker -
// but build flags cannot touch machine code that is already compiled. The
// core's headers carry no computational inline bodies (hard rule, enforced
// upstream in port/core), so nothing numeric ever recompiles under UE flags.
//
// WORKFLOW (also in port/README.md): whenever port/core sources change,
// rebuild the lib via CMake BEFORE running UBT:
//   cmake --build port/build --config RelWithDebInfo
// UBT does not know about the CMake build; a stale .lib means stale sim math.
using System.IO;
using UnrealBuildTool;

public class ShitfireCoreLib : ModuleRules
{
	public ShitfireCoreLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// ModuleDirectory = port/unreal/Source/ShitfireCoreLib -> PortDir = port/
		string PortDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", ".."));

		PublicIncludePaths.Add(Path.Combine(PortDir, "core", "include"));

		// RelWithDebInfo for every UE configuration: /MD runtime matches UE's
		// Development and Shipping; sim math must be byte-identical across
		// configs anyway - that is the whole point of the external lib.
		string LibPath = Path.Combine(PortDir, "build", "core", "RelWithDebInfo", "shitfire_core.lib");
		if (!File.Exists(LibPath))
		{
			throw new BuildException(
				"shitfire_core.lib not found at " + LibPath +
				" - build it first: cmake --build port/build --config RelWithDebInfo");
		}
		PublicAdditionalLibraries.Add(LibPath);
	}
}
