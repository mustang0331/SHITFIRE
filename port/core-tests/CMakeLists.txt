# Parity tests against the P0 oracle fixtures (tag oracle-v1).
# Configure with -DSHITFIRE_FIXTURES=<abs path to ENGINE_PORT/fixtures>.

if(NOT DEFINED SHITFIRE_FIXTURES)
  message(FATAL_ERROR
    "Pass -DSHITFIRE_FIXTURES=<absolute path to ENGINE_PORT/fixtures> "
    "(the committed oracle-v1 fixture directory).")
endif()
file(TO_CMAKE_PATH "${SHITFIRE_FIXTURES}" SHITFIRE_FIXTURES_CMAKE)

# Vendored Catch2 v3 amalgamated build (BSL-1.0) — provides main().
add_library(catch2_amalgamated STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/catch2/catch_amalgamated.cpp)
target_include_directories(catch2_amalgamated PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../vendor)
target_compile_features(catch2_amalgamated PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(catch2_amalgamated PRIVATE /W3 /bigobj)
endif()

add_executable(shitfire_core_tests
  support/fixtures.cpp
)
target_link_libraries(shitfire_core_tests PRIVATE shitfire_core catch2_amalgamated)
target_include_directories(shitfire_core_tests PRIVATE
  support
  ${CMAKE_CURRENT_SOURCE_DIR}/../vendor)
target_compile_definitions(shitfire_core_tests PRIVATE
  SHITFIRE_FIXTURES_DIR="${SHITFIRE_FIXTURES_CMAKE}")
if(MSVC)
  target_compile_options(shitfire_core_tests PRIVATE /fp:precise /W4 /bigobj)
endif()

# One ctest entry per fixture area, selected by Catch2 tag. Labels gate the
# tiers: level1 must be green (PRNG bit-exact) before anything merges;
# level2/level3 arrive in P2+. Tests land with their fixture areas.
