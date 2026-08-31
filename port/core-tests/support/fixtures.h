#pragma once
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

/* Fixture loading for the parity tests. The fixture root comes in at compile
   time from -DSHITFIRE_FIXTURES (see core-tests/CMakeLists.txt). Test-only
   code: exceptions and file I/O are fine here, never in core/. */
namespace fixtures {

/* Load ENGINE_PORT/fixtures/<relPath> (e.g. "level1/prng_streams.json").
   Throws on missing file / bad JSON — Catch2 reports it as a test failure. */
nlohmann::json load(const std::string& relPath);

/* Raw IEEE-754 bit pattern of a double, for bit-exact comparison. */
std::uint64_t bits(double d);

}
