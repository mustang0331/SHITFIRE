# shitfire_core — the engine-free sim core. Static lib, C++20, double-only.
#
# /fp:precise is the load-bearing flag (UNREAL_RESEARCH.md §3a): UE compiles
# its modules /fp:fast, so every computational body lives in these .cpp files,
# compiled once, here, with per-op IEEE semantics matching V8. Nothing
# computational may ever move inline into a header.
#
# Hard rules: no exceptions thrown/caught, no file I/O, no engine or platform
# headers — standard library only. nlohmann/json exists only in core-tests.

# Sources are globbed so parallel porter agents add files without editing this
# shared list (P2 prep). CONFIGURE_DEPENDS reruns the glob on build.
file(GLOB_RECURSE SHITFIRE_CORE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(shitfire_core STATIC ${SHITFIRE_CORE_SOURCES})

target_include_directories(shitfire_core PUBLIC include)
target_compile_features(shitfire_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(shitfire_core PRIVATE /fp:precise /W4 /permissive-)

  # Exception model — DELIBERATE, not a default (ADVERSARY_SIGNOFF.md F1).
  # Core sources contain zero throw/try/catch (F1 grep, every hit read), so
  # the lib is pinned exceptions-OFF to make the README's "compiles
  # exceptions-off" a build fact and to match the EH model of the UE modules
  # it links into: UBT compiles non-editor modules with _HAS_EXCEPTIONS=0 and
  # no /EH flag at all (UE_5.8 VCToolChain.cs:437-451, AddExceptionArguments —
  # only editor/UHT builds get /EHsc). /EHs-c- overrides the /EHsc that CMake
  # puts in the default MSVC flags (last flag wins). PRIVATE on purpose:
  # core-tests keep /EHsc for Catch2 and link this lib exactly the way an
  # exceptions-on UE *editor* module does — both mixes stay under test.
  # Sinks may still not throw through core frames (events.h sink contract).
  target_compile_options(shitfire_core PRIVATE /EHs-c-)
  target_compile_definitions(shitfire_core PRIVATE _HAS_EXCEPTIONS=0)
endif()
