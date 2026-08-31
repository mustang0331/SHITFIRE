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

# Test sources are globbed so parallel porter agents add files without editing
# this shared list (P2 prep).
file(GLOB SHITFIRE_TEST_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/support/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/level1/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/level2/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/level3/*.cpp)
add_executable(shitfire_core_tests ${SHITFIRE_TEST_SOURCES})
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
# tiers: level1 must be green (PRNG bit-exact) before anything merges.
# P2 entries are pre-registered here (shared-file contention avoidance): an
# entry whose tagged tests have not landed yet fails until its porter lands
# them — expected transient during a P2 wave, never at a wave boundary.
foreach(t prng hash_noise terrain_proc terrain_dem raylos grid
          spotburst corrections parser_corpus)
  add_test(NAME level1.${t} COMMAND shitfire_core_tests "[${t}]")
  set_tests_properties(level1.${t} PROPERTIES LABELS "level1")
endforeach()
foreach(t scenario_gen chapters)
  add_test(NAME level2.${t} COMMAND shitfire_core_tests "[${t}]")
  set_tests_properties(level2.${t} PROPERTIES LABELS "level2")
endforeach()
add_test(NAME level3.replay COMMAND shitfire_core_tests "[replay]")
set_tests_properties(level3.replay PROPERTIES LABELS "level3")
