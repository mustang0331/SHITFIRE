// STUB — not Microsoft's mscoree.h.
// Exists only so UnrealBuildTool's NetFxSDK probe succeeds on a machine
// without the .NET Framework SDK (SwarmInterface/Lightmass — which this
// fully-dynamic-lighting project never compiles or uses; the engine module
// is precompiled in the Launcher build). See port/README.md. If anything
// ever actually compiles against this header it will fail loudly here.
#error SwarmInterface should never be compiled from source in this project
