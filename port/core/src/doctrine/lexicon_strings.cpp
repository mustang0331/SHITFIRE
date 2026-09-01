#include "shitfire/doctrine/lexicon_strings.h"
#include <regex>

/* Ported from src/js/01-math-terrain.js:132-158. Every std::regex below is
   the JS pattern verbatim (std::regex ECMAScript grammar; the source uses
   nothing beyond plain classes/alternation here). JS String.replace with a
   /g regex maps to std::regex_replace's default replace-all. */
namespace shitfire { namespace doctrine {

namespace {

/* DC_RE — 01-math-terrain.js:157. Shared by saidDangerClose and descClean
   (the source reuses DC_RE.source with the g flag at :134). */
const std::regex& dcRe() {
  static const std::regex re(R"(\bdanger\s*(?:close|closed|clothes|cloths|clove|cloze|glose)\b)");
  return re;
}

}

/* 01-math-terrain.js:132-143 — replacement chain in source order. */
std::string descClean(const std::string& d) {
  static const std::regex reOtFactor(R"(\bot\s+factor\s+\d+(?:\.\d+)?\b)");   // :133 NET8c
  static const std::regex reMoe(R"(\b(?:at my command|when ready|do not load|cannot observe)\b)");  // :135
  static const std::regex reSheaf(R"(\b(?:converged|open|parallel|linear|circular|special)\s+sheaf\b)");  // :137 G15/G16
  static const std::regex reFuze(R"(\b(?:shell he\b\s*,?\s*)?fuze\s+(?:vt|victor tango|variable time|time|delay|quick|pd|point detonating)\b)");  // :138
  static const std::regex reShell(R"(\bshell\s+(?:he|smoke|wp|white phosphorus|illum\w*)\b)");  // :141 12h
  static const std::regex reIllum(R"(\billum(?:ination)?\b)");                // :142
  static const std::regex reSpaces(R"(\s{2,})");                              // :143
  static const std::regex reTrim(R"(^[\s,]+|[\s,]+$)");                       // :143

  std::string s = d;
  s = std::regex_replace(s, reOtFactor, " ");
  s = std::regex_replace(s, dcRe(), " ");        // :134 — strip EVERY occurrence
  s = std::regex_replace(s, reMoe, " ");
  s = std::regex_replace(s, reSheaf, " ");
  s = std::regex_replace(s, reFuze, " ");
  s = std::regex_replace(s, reShell, " ");
  s = std::regex_replace(s, reIllum, " ");
  s = std::regex_replace(s, reSpaces, " ");
  s = std::regex_replace(s, reTrim, "");
  return s;
}

/* 01-math-terrain.js:158 — DC_RE.test(raw). */
bool saidDangerClose(const std::string& raw) {
  return std::regex_search(raw, dcRe());
}

} }
