#include <catch2/catch_amalgamated.hpp>
#include "fixtures.h"
#include "shitfire/doctrine/parser_message.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

/* Level 1 / parser corpus — front-loads the Level-3 replay assertion
   "identical parse classification per message" (PARITY_TESTING.md, level 3,
   step 3). Every observer transmission recorded in every level3 transcript
   fixture is re-classified through the ported parser, and the result must
   reproduce the recorded TLOG parse entry (12-cff-handling.js:907) exactly:
   ptype, method, warno, ffe. The parse entry's own msg field is the raw
   transmission as the player sent it (the obs entry is the uppercased echo),
   so the parse entries are both the input and the expected output.

   State inputs: the parse layer reads exactly two pieces of sim state.
   CONFIG.NAV.declEastDeg is the constant 7 in every chapter (00-config.js:162
   — ParserContext's default). Scenario.kps only matters for shifts from a
   known point BY NAME (11-parser.js:400-405); each mission's KP names are
   recovered from its own "Known points registered:" boot-brief sys line
   (printed by 15-campaign-menu.js:616-617), so no scenario-generation stub is
   needed. (No transmission in this corpus actually exercises the named-KP
   path; the context is built anyway so the test stays honest if the corpus
   grows one.) */
TEST_CASE("level3 transcript corpus reclassifies identically", "[parser_corpus]") {
  namespace fs = std::filesystem;
  const fs::path dir = fs::path(SHITFIRE_FIXTURES_DIR) / "level3";
  std::vector<std::string> files;
  for (const auto& e : fs::directory_iterator(dir)) {
    const std::string name = e.path().filename().string();
    if (name.rfind("mission_", 0) == 0 && e.path().extension() == ".json")
      files.push_back(name);
  }
  std::sort(files.begin(), files.end());
  REQUIRE(files.size() == 33);   // the oracle-v1 transcript library

  long total = 0, matched = 0, obsCount = 0;
  std::map<std::string, std::pair<long, long>> byType;   // ptype -> {matched, seen}
  std::ostringstream misses;
  long missCount = 0;

  static const std::regex reKp("AB(\\d{4}) \"([A-Z]+)\"");

  for (const std::string& fname : files) {
    const auto j = fixtures::load("level3/" + fname);

    shitfire::doctrine::ParserContext ctx;   // declEastDeg = 7 (00-config.js:162)
    for (const auto& e : j.at("tlog")) {
      if (e.at("kind") != "sys") continue;
      const std::string msg = e.at("msg").get<std::string>();
      if (msg.rfind("Known points registered:", 0) == 0) {
        for (std::sregex_iterator it(msg.begin(), msg.end(), reKp), end; it != end; ++it)
          ctx.kps.push_back({ (*it)[1].str(), (*it)[2].str() });
        break;
      }
    }

    for (const auto& e : j.at("tlog")) {
      const std::string kind = e.at("kind").get<std::string>();
      if (kind == "obs") { obsCount++; continue; }
      if (kind != "parse") continue;
      total++;
      const std::string msg = e.at("msg").get<std::string>();
      const std::string wantP = e.at("ptype").get<std::string>();
      const std::string wantM = e.at("method").is_null() ? "" : e.at("method").get<std::string>();
      const std::string wantW = e.at("warno").is_null() ? "" : e.at("warno").get<std::string>();
      const bool wantF = e.at("ffe").get<bool>();

      const auto parsed = shitfire::doctrine::parseMessage(msg, ctx);
      const auto got = shitfire::doctrine::tlogParseClass(parsed);

      byType[wantP].second++;
      const bool ok = got.ptype == wantP && got.method == wantM &&
                      got.warno == wantW && got.ffe == wantF;
      if (ok) { matched++; byType[wantP].first++; }
      else if (missCount++ < 20) {
        misses << "  " << fname << " t=" << e.at("t").get<double>()
               << " msg=\"" << msg << "\"\n    got  "
               << got.ptype << "/" << (got.method.empty() ? "-" : got.method)
               << "/" << (got.warno.empty() ? "-" : got.warno)
               << "/" << (got.ffe ? "ffe" : "-") << "\n    want "
               << wantP << "/" << (wantM.empty() ? "-" : wantM)
               << "/" << (wantW.empty() ? "-" : wantW)
               << "/" << (wantF ? "ffe" : "-") << "\n";
      }
    }
  }

  /* Every obs transmission gets exactly one parse entry (onPlayerMessage
     logs them back-to-back, 12-cff-handling.js:895+907; the note:/oversize
     paths that skip the pair also skip the obs echo or log syserr). */
  REQUIRE(obsCount == total);
  REQUIRE(total == 227);   // pinned with the oracle-v1 fixtures

  std::ostringstream rep;
  rep << "parser corpus: " << matched << "/" << total << " messages match\n";
  for (const auto& kv : byType)
    rep << "  " << kv.first << ": " << kv.second.first << "/" << kv.second.second << "\n";
  std::cout << rep.str();   // visible on a direct run; INFO below covers failures

  INFO(rep.str() << "first mismatches:\n" << misses.str());
  REQUIRE(matched == total);
}
