#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "shitfire/math/grid.h"
#include "shitfire/math/mils.h"
#include <string>

/* Level 1 / grid helpers — all 127 recorded cases EXACT: strings byte-equal,
   numbers compared by IEEE bit pattern (every op involved is exact IEEE
   arithmetic — add/sub/mul/div/floor/fmod — so bit equality is the honest
   standard, not a luxury). */
TEST_CASE("worldToEN / enToWorld / gridOf / radToMils: 127 cases exact", "[grid]") {
  const auto j = fixtures::load("level1/grid_roundtrip.json");
  const auto& cases = j.at("cases");

  shitfire::MapConfig map;
  map.originE = cases.at("map").at("originE").get<double>();
  map.originN = cases.at("map").at("originN").get<double>();

  long long total = 0, bad = 0;
  std::string firstBad;

  for (const auto& c : cases.at("worldToEN")) {
    const shitfire::EN got = shitfire::worldToEN(
        c.at("x").get<double>(), c.at("z").get<double>(), map);
    if (fixtures::bits(got.e) != fixtures::bits(c.at("e").get<double>()) ||
        fixtures::bits(got.n) != fixtures::bits(c.at("n").get<double>())) {
      if (firstBad.empty()) firstBad = "worldToEN " + c.dump();
      bad++;
    }
    total++;
  }

  for (const auto& c : cases.at("enToWorld")) {
    const shitfire::WorldXZ got = shitfire::enToWorld(
        c.at("e").get<double>(), c.at("n").get<double>(), map);
    if (fixtures::bits(got.x) != fixtures::bits(c.at("x").get<double>()) ||
        fixtures::bits(got.z) != fixtures::bits(c.at("z").get<double>())) {
      if (firstBad.empty()) firstBad = "enToWorld " + c.dump();
      bad++;
    }
    total++;
  }

  for (const auto& c : cases.at("gridOf")) {
    const std::string got = shitfire::gridOf(
        c.at("x").get<double>(), c.at("z").get<double>(), map);
    if (got != c.at("grid").get<std::string>()) {
      if (firstBad.empty()) firstBad = "gridOf " + c.dump() + " got " + got;
      bad++;
    }
    total++;
  }

  for (const auto& c : cases.at("radToMils")) {
    const double got = shitfire::radToMils(c.at("rad").get<double>());
    if (fixtures::bits(got) != fixtures::bits(c.at("mils").get<double>())) {
      if (firstBad.empty()) firstBad = "radToMils " + c.dump();
      bad++;
    }
    total++;
  }

  INFO("first mismatch: " << firstBad);
  CHECK(total == 127);
  REQUIRE(bad == 0);
}
