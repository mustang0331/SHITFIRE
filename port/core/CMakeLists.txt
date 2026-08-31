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
endif()
