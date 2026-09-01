#include "shitfire/doctrine/parser_message.h"
#include "shitfire/doctrine/lexicon_strings.h"
#include "shitfire/math/jsemantics.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <regex>

/* src/js/11-parser.js ported branch-for-branch IN SOURCE ORDER (the repo's
   parser-order gate exists because this order is load-bearing). Every branch
   cites its JS line. Regexes are the JS patterns verbatim under std::regex's
   ECMAScript grammar — the source uses only plain classes, alternation, one
   lookahead (:411) and matchAll (:322,:334), all supported. JS String.replace
   with /g maps to std::regex_replace default (replace-all); without /g,
   std::regex_constants::format_first_only.

   The corpus is ASCII (verified over all 33 level3 transcripts), so the
   ASCII-only lowering/upper below and std::regex's byte-wise \s\b\w match JS
   Unicode semantics on every real transmission. */
namespace shitfire { namespace doctrine {

namespace {

using svec = std::vector<std::string>;
const auto kFirstOnly = std::regex_constants::format_first_only;

bool includes(const std::string& t, const char* sub) {
  return t.find(sub) != std::string::npos;
}

std::string toLowerAscii(std::string s) {
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return s;
}

std::string toUpperAscii(std::string s) {
  for (char& c : s) if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  return s;
}

bool isDigitsStr(const std::string& w) {          // JS /^\d+$/
  if (w.empty()) return false;
  for (char c : w) if (c < '0' || c > '9') return false;
  return true;
}

bool isSingleDigit(const std::string& w) {        // JS /^\d$/
  return w.size() == 1 && w[0] >= '0' && w[0] <= '9';
}

/* JS parseInt on a pure-digit capture: exact mathematical value correctly
   rounded to double — strtod on decimal digits does precisely that. */
double parseDigits(const std::string& w) {
  return std::strtod(w.c_str(), nullptr);
}

/* JS String(n) for the fold results — integral, bounded by the source's own
   round/mod order to [0, 6400], so plain integer digits. */
std::string jsIntString(double v) {
  return std::to_string(static_cast<long long>(v));
}

/* JS String.prototype.trim truthiness check (ASCII whitespace; a rest that
   is all whitespace re-parses to 'empty' by tokenization anyway, so the JS
   and C++ paths converge even on exotic input). */
bool isBlank(const std::string& s) {
  for (char c : s)
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v')
      return false;
  return true;
}

svec splitSpace(const std::string& t) {           // t.split(' ').filter(Boolean)
  svec out;
  std::string cur;
  for (char c : t) {
    if (c == ' ') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
    else cur.push_back(c);
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

svec splitWs(const std::string& t) {              // .split(/\s+/).filter(Boolean)
  svec out;
  std::string cur;
  for (char c : t) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
      if (!cur.empty()) { out.push_back(cur); cur.clear(); }
    } else cur.push_back(c);
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

std::string joinSpace(const svec& v) {
  std::string out;
  for (std::size_t i = 0; i < v.size(); i++) { if (i) out += ' '; out += v[i]; }
  return out;
}

std::ptrdiff_t indexOf(const svec& v, const char* w) {   // Array.indexOf
  for (std::size_t i = 0; i < v.size(); i++) if (v[i] == w) return std::ptrdiff_t(i);
  return -1;
}

bool hasToken(const svec& v, const char* w) {            // Array.includes
  return indexOf(v, w) >= 0;
}

bool inList(const std::string& w, std::initializer_list<const char*> list) {
  for (const char* e : list) if (w == e) return true;
  return false;
}

/* JS t.replace(matchedText, ' ') — STRING replace: the first literal
   occurrence of the matched text (:181, :197, :221 pass mShell[0]/mFuze[0]/
   mSheaf[0] as a string, not a regex). */
std::string replaceFirstLiteral(const std::string& t, const std::string& what,
                                const char* with) {
  const std::size_t pos = t.find(what);
  if (pos == std::string::npos) return t;
  std::string out = t;
  out.replace(pos, what.size(), with);
  return out;
}

std::size_t countMatches(const std::string& t, const std::regex& re) {
  std::size_t n = 0;
  for (std::sregex_iterator it(t.begin(), t.end(), re), end; it != end; ++it) n++;
  return n;
}

/* Lookup tables — 11-parser.js:4-7 (DIGIT_WORDS / TENS_WORDS), :24-35
   (STT_FIX), :89 (PHON). Plain pair lists; only ever probed by key. */
struct WordMap { const char* from; const char* to; };

const WordMap kDigitWords[] = {
  {"zero","0"}, {"oh","0"}, {"o","0"}, {"one","1"}, {"two","2"}, {"three","3"},
  {"four","4"}, {"five","5"}, {"six","6"}, {"seven","7"}, {"eight","8"},
  {"nine","9"}, {"niner","9"},
};
const WordMap kTensWords[] = {
  {"ten","10"}, {"twenty","20"}, {"thirty","30"}, {"forty","40"}, {"fifty","50"},
  {"sixty","60"}, {"seventy","70"}, {"eighty","80"}, {"ninety","90"},
};
/* F5 — speech-recognition repairs for the CORRECTION prowords; conservative
   list, every entry a word with no other meaning on a fire net. NET3 added
   affect->effect ("FIRE FOR AFFECT" fired an adjust round). Deliberately NOT
   included: "at", "ad", "fire" variants (see 11-parser.js:20-23). */
const WordMap kSttFix[] = {
  {"write","right"}, {"wright","right"}, {"rite","right"}, {"writ","right"},
  {"dropped","drop"}, {"draw","drop"}, {"drawn","drop"}, {"drops","drop"},
  {"dropp","drop"},
  {"lft","left"}, {"laft","left"},
  {"ads","add"}, {"adds","add"}, {"added","add"},
  {"danjer","danger"},
  {"affect","effect"},
};
const WordMap kPhon[] = {
  {"alpha","a"}, {"alfa","a"}, {"bravo","b"}, {"charlie","c"}, {"delta","d"},
};

const char* mapLookup(const WordMap* m, std::size_t n, const std::string& w) {
  for (std::size_t i = 0; i < n; i++) if (w == m[i].from) return m[i].to;
  return nullptr;
}
#define MAP_LOOKUP(tbl, w) mapLookup(tbl, sizeof(tbl) / sizeof((tbl)[0]), w)

/* "four hundred" -> 400 fold, string form: parseInt(prev) * 100/1000 with
   String() — strip parseInt's leading zeros, append the zeros. Exact for any
   length (JS parseInt loses precision past 2^53; no real digit run gets
   there, and the string form is the exact value). 11-parser.js:43-44. */
std::string scaleDigits(const std::string& d, int zeros) {
  std::size_t nz = 0;
  while (nz < d.size() && d[nz] == '0') nz++;
  if (nz == d.size()) return "0";                  // parseInt("000")*100 -> "0"
  return d.substr(nz) + std::string(std::size_t(zeros), '0');
}

}  // namespace

/* ============================================================ normalize
   11-parser.js:36-98. */
std::string normalize(const std::string& s, const ParserContext& ctx) {
  /* :37 — toLowerCase + replace(/[^a-z0-9]+/g,' ') + trim/split/filter, as
     one tokenizing walk: every byte outside [a-z0-9] after ASCII lowering is
     a separator (equivalent output for any byte input). */
  svec toks;
  {
    std::string cur;
    for (unsigned char uc : s) {
      char c = (uc >= 'A' && uc <= 'Z') ? char(uc - 'A' + 'a') : char(uc);
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) cur.push_back(c);
      else if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) toks.push_back(cur);
  }
  /* :38 — STT_FIX pass. */
  for (auto& w : toks) if (const char* r = MAP_LOOKUP(kSttFix, w)) w = r;
  /* :39 — digit words, then tens words. */
  for (auto& w : toks) {
    if (const char* d = MAP_LOOKUP(kDigitWords, w)) w = d;
    else if (const char* tn = MAP_LOOKUP(kTensWords, w)) w = tn;
  }
  /* :41-45 — "four hundred" -> 400, "two thousand" -> 2000. */
  svec out;
  for (auto& w : toks) {
    if ((w == "hundred" || w == "thousand") && !out.empty() && isDigitsStr(out.back()))
      out.back() = scaleDigits(out.back(), w == "hundred" ? 2 : 3);
    else out.push_back(w);
  }
  /* :48-55 — merge runs of single digits: "2 4 5 5 2 3" -> "245523". */
  svec merged;
  for (std::size_t i = 0; i < out.size(); i++) {
    if (isSingleDigit(out[i])) {
      std::string num = out[i];
      while (i + 1 < out.size() && isSingleDigit(out[i + 1])) num += out[++i];
      merged.push_back(num);
    } else merged.push_back(out[i]);
  }
  /* :69-78 — NET12: a direction announced MAGNETIC is folded to grid, once
     (GRID = MAG + declination). Splice(i+1, j-i) == erase [i+1, j+1). JS op
     order kept: fmod BEFORE Math.round; decl term evaluated left-to-right. */
  for (std::size_t i = 1; i < merged.size(); i++) {
    if (isDigitsStr(merged[i]) && merged[i - 1] == "direction") {
      std::size_t j = i + 1;
      if (j < merged.size() && merged[j] == "mils") j++;
      if (j < merged.size() && merged[j] == "magnetic") {
        const double v = std::fmod(
            parseDigits(merged[i]) + ctx.declEastDeg / 360.0 * 6400.0, 6400.0);
        merged[i] = jsIntString(js::round(v));
        merged.erase(merged.begin() + std::ptrdiff_t(i + 1),
                     merged.begin() + std::ptrdiff_t(j + 1));
      }
    }
  }
  /* :83-88 — NET29(c): an announced DEGREES value is honoured, once, for
     every branch downstream. JS op order kept: Math.round FIRST, then % 6400;
     parseInt(x) * 6400 / 360 evaluated left-to-right. */
  for (std::size_t i = 0; i + 1 < merged.size(); i++) {
    if (isDigitsStr(merged[i]) &&
        (merged[i + 1] == "degree" || merged[i + 1] == "degrees")) {
      const double v =
          std::fmod(js::round(parseDigits(merged[i]) * 6400.0 / 360.0), 6400.0);
      merged[i] = jsIntString(v);
      merged.erase(merged.begin() + std::ptrdiff_t(i + 1));
    }
  }
  /* :89-96 — NET20: fold a phonetic-letter pair immediately followed by a
     4-digit run into the written target number ("alpha bravo 7001" ->
     "ab7001"); gated on the digits so qfp avenue names are untouched. */
  svec folded;
  for (std::size_t i = 0; i < merged.size(); i++) {
    const char* p1 = MAP_LOOKUP(kPhon, merged[i]);
    const char* p2 = (i + 1 < merged.size()) ? MAP_LOOKUP(kPhon, merged[i + 1]) : nullptr;
    const bool four = (i + 2 < merged.size()) && merged[i + 2].size() == 4 &&
                      isDigitsStr(merged[i + 2]);
    if (p1 && p2 && four) {
      folded.push_back(std::string(p1) + p2 + merged[i + 2]);
      i += 2;
    } else folded.push_back(merged[i]);
  }
  return joinSpace(folded);   // :97
}

/* ============================================================ parseMessage
   11-parser.js:100-548. One return object per branch; p starts zeroed and a
   branch fills exactly the fields its JS counterpart set. */
ParsedMessage parseMessage(const std::string& raw, const ParserContext& ctx) {
  ParsedMessage p;
  const std::string t = normalize(raw, ctx);     // :101
  svec toks = splitSpace(t);                     // :102
  std::smatch m;

  if (toks.empty()) { p.type = "empty"; return p; }                     // :103
  if (includes(t, "say again")) { p.type = "sayagain"; return p; }      // :104

  /* :105-116 — NET17: a leading CORRECTION wraps the re-parsed remainder.
     Matched after SAY AGAIN, before everything else. The remainder is cut
     from the ORIGINAL raw text (case-insensitive, first occurrence). */
  static const std::regex reCorrLead(
      R"(^(?:hellhound\s+|hacksaw\s+|fires\s+|this\s+is\s+|mustang\s+|\d+\s+)*correction\b)");
  if (std::regex_search(t, reCorrLead)) {
    static const std::regex reCorrStrip(R"(^[\s\S]*?correction[,.\s]*)",
                                        std::regex::ECMAScript | std::regex::icase);
    const std::string rest = std::regex_replace(raw, reCorrStrip, "", kFirstOnly);
    ParsedMessage inner;
    if (!isBlank(rest)) inner = parseMessage(rest, ctx);
    else inner.type = "empty";
    p.type = "correction";
    p.inner = std::make_unique<ParsedMessage>(std::move(inner));
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :117-123 — G25: the two SAFETY prowords, matched EARLY so no future
     keyword can shadow them. */
  if (includes(t, "check firing")) { p.type = "checkfire"; p.raw = t; p.toks = toks; return p; }
  if (includes(t, "cease loading")) { p.type = "ceaseload"; p.raw = t; p.toks = toks; return p; }

  /* :124-149 — SUGG5: traffic led by a target number routes to that mission.
     Anchored at the start; tested after the safety prowords. BUILD4: repeated
     leading target-number prefixes are consumed ITERATIVELY (stacked prefixes
     carry no meaning beyond the first), then exactly one recursion. */
  static const std::regex reRoute(
      R"(^target\s+(?:number\s+)?([a-z]{2})\s*(\d{4})\b[\s,]*(.+)$)");
  if (std::regex_search(t, m, reRoute)) {
    std::string rest = m[3].str();
    static const std::regex reRouteMore(
        R"(^target\s+(?:number\s+)?[a-z]{2}\s*\d{4}\b[\s,]*(.+)$)");
    std::smatch mMore;
    while (std::regex_search(rest, mMore, reRouteMore)) rest = mMore[1].str();
    ParsedMessage inner = parseMessage(rest, ctx);
    if (inner.type != "unknown" && inner.type != "empty") {
      p.type = "routed";
      p.tgtNum = toUpperAscii(m[1].str() + m[2].str());
      p.inner = std::make_unique<ParsedMessage>(std::move(inner));
      p.raw = t; p.toks = toks;
      return p;
    }
  }

  /* :150-152 — G24: CANCEL AT MY COMMAND before AT MY COMMAND (it contains
     it; the bare AT MY COMMAND lives far below, after the CFF branches). */
  if (includes(t, "cancel at my command")) { p.type = "amc"; p.on = false; p.raw = t; p.toks = toks; return p; }
  if (includes(t, "do not load")) { p.type = "donotload"; p.raw = t; p.toks = toks; return p; }
  if (includes(t, "cannot observe")) { p.type = "cannotobserve"; p.raw = t; p.toks = toks; return p; }

  /* :153-166 — NET24: REQUEST SPLASH; standalone RECORD AS TARGET (riding on
     END OF MISSION it is RREMS and belongs to the eom branch below — indexOf,
     not includes, so tools/parser-order.js can locate the real eom branch by
     its includes-marker); near-bare MARK. */
  if (includes(t, "request splash")) { p.type = "reqsplash"; p.raw = t; p.toks = toks; return p; }
  if (includes(t, "record as target") && t.find("end of mission") == std::string::npos) {
    p.type = "recordtgt"; p.raw = t; p.toks = toks; return p;
  }
  static const std::regex reMark(
      R"(^(?:hellhound\s+|hacksaw\s+|fires\s+|this\s+is\s+|mustang\s+|\d+\s+)*mark(?:\s+center(?:\s+of)?\s+sector)?(?:\s+(?:over|out))?$)");
  if (std::regex_search(t, reMark)) { p.type = "mark"; p.raw = t; p.toks = toks; return p; }

  /* :167-169 — time on target, with or without the seconds value. */
  static const std::regex reTot(R"(\btime on target\s+(\d+)\b)");
  if (std::regex_search(t, m, reTot)) {
    p.type = "tot"; p.sec = parseDigits(m[1].str()); p.raw = t; p.toks = toks; return p;
  }
  if (includes(t, "time on target")) { p.type = "tot"; p.sec = 0; p.raw = t; p.toks = toks; return p; }

  /* :170-190 — 12h: standalone SHELL selection ("SHELL SMOKE, OVER"). Claim
     only the bare transmission; anything more falls through to the CFF flow. */
  static const std::regex reShellSel(R"(\bshell\s+(he|smoke|wp|white phosphorus|illum\w*)\b)");
  static const std::regex reLRAD(R"(\b(?:left|right|add|drop)\b)");
  if (std::regex_search(t, m, reShellSel) && !includes(t, "grid") &&
      !std::regex_search(t, reLRAD)) {
    static const std::regex reShellStrip(
        R"(\b(?:over|out|break|hellhound|hacksaw|fires|this|is|mustang|\d+)\b)");
    const std::string r =
        std::regex_replace(replaceFirstLiteral(t, m[0].str(), " "), reShellStrip, " ");
    if (splitWs(r).size() <= 2) {
      const std::string k = m[1].str();
      p.type = "shell";
      p.kind = (k == "wp" || k == "white phosphorus") ? "smoke"
             : (k.rfind("illum", 0) == 0) ? "illum" : k;
      p.raw = t; p.toks = toks;
      return p;
    }
  }

  /* :191-206 — G16: standalone FUZE, same bare-transmission discipline. */
  static const std::regex reFuzeSel(
      R"(\bfuze\s+(vt|victor tango|variable time|time|delay|quick|pd|point detonating)\b)");
  if (std::regex_search(t, m, reFuzeSel) && !includes(t, "grid") &&
      !std::regex_search(t, reLRAD)) {
    static const std::regex reFuzeStrip(
        R"(\b(?:over|out|break|hellhound|hacksaw|fires|this|is|mustang|shell|he|fire|for|effect|\d+)\b)");
    const std::string r =
        std::regex_replace(replaceFirstLiteral(t, m[0].str(), " "), reFuzeStrip, " ");
    if (splitWs(r).size() <= 2) {
      const std::string k = m[1].str();
      p.type = "fuze";
      p.kind = (k == "victor tango" || k == "variable time") ? "vt"
             : (k == "point detonating" || k == "quick") ? "pd" : k;
      p.raw = t; p.toks = toks;
      return p;
    }
  }

  /* :207-215 — G10/NET29(e): FIRE UNIT STATUS, tolerant of the STT variants
     and of normalize() splitting "unit's" into "unit s". */
  static const std::regex reUnitStatus(R"(\b(?:fire|firing) unit(?:'?s|\s+s)? status\b)");
  static const std::regex reReqStatus(R"(\brequest (?:your )?status\b)");
  if (std::regex_search(t, reUnitStatus) || std::regex_search(t, reReqStatus)) {
    p.type = "unitstatus"; p.raw = t; p.toks = toks; return p;
  }

  /* :216-226 — G15: bare sheaf transmission only; real description text
     riding along flows on to the CFF queue instead. */
  static const std::regex reSheafSel(
      R"(\b(?:(cancel)\s+)?(converged|open|parallel|linear|circular|special)\s+sheaf\b)");
  if (std::regex_search(t, m, reSheafSel) && !includes(t, "fire") && !includes(t, "grid")) {
    static const std::regex reSheafStrip(
        R"(\b(?:over|out|break|hellhound|hacksaw|fires|this|is|mustang|\d+)\b)");
    const std::string r =
        std::regex_replace(replaceFirstLiteral(t, m[0].str(), " "), reSheafStrip, " ");
    if (splitWs(r).size() <= 2) {
      p.type = "sheaf"; p.kind = m[2].str(); p.cancel = m[1].matched;
      p.raw = t; p.toks = toks;
      return p;
    }
  }

  /* :227-241 — end of mission; RREMS refinement and "record as target" may
     ride along; bda keeps only the surveillance text. */
  if (includes(t, "end of mission")) {
    EomRefine ref;
    static const std::regex reLRnum(R"(\b(left|right)\s+(\d+))");
    static const std::regex reADnum(R"(\b(add|drop)\s+(\d+))");
    std::smatch rm;
    if (std::regex_search(t, rm, reLRnum)) {
      ref.right = (rm[1] == "right" ? 1 : -1) * parseDigits(rm[2].str());
      ref.any = true;
    }
    if (std::regex_search(t, rm, reADnum)) {
      ref.add = (rm[1] == "add" ? 1 : -1) * parseDigits(rm[2].str());
      ref.any = true;
    }
    static const std::regex reCutEom(R"(.*end of mission)");
    static const std::regex reRefStrip(R"(\b(left|right|add|drop|up|down)\s+\d+)");
    static const std::regex reRecStrip(R"(record as target)");
    static const std::regex reOverEnd(R"(\bover\b\s*$)");
    static const std::regex reSpaces(R"(\s{2,})");
    static const std::regex reTrimSC(R"(^[\s,]+|[\s,]+$)");
    std::string bda = std::regex_replace(t, reCutEom, "", kFirstOnly);
    bda = std::regex_replace(bda, reRefStrip, "");
    bda = std::regex_replace(bda, reRecStrip, "");
    bda = std::regex_replace(bda, reOverEnd, "", kFirstOnly);
    bda = std::regex_replace(bda, reSpaces, " ");
    bda = std::regex_replace(bda, reTrimSC, "");
    p.type = "eom"; p.refine = ref;
    p.record = includes(t, "record as target");
    p.bda = bda;
    return p;                       // JS returns no raw/toks here
  }

  const bool ffe = includes(t, "fire for effect");   // :242

  /* :243-259 — G9/NET10: POSITION REPORT, tested BEFORE grid extraction so
     "position grid 245 523" is never claimed as a fire mission at the
     observer's own position. Without the word GRID the digit run must be a
     full 6+. */
  static const std::regex rePos(
      R"(\b(?:pos\s*rep|my\s+position|observer\s+position|position)\s+(?:is\s+)?(grid\s+)?((?:\d+\s*)+))");
  if (std::regex_search(t, m, rePos) && !includes(t, "adjust fire") &&
      !includes(t, "fire for effect")) {
    std::string pd;
    for (char c : m[2].str()) if (c >= '0' && c <= '9') pd.push_back(c);  // .replace(/\s+/g,'')
    if (m[1].matched || pd.size() >= 6) {
      p.type = "posrep"; p.digits = pd; p.raw = t; p.toks = toks; return p;
    }
  }

  /* :260-265 — G14/G26: the IMMEDIATE mission types. "" plays JS null. */
  const std::string imm = includes(t, "immediate suppression") ? "suppress"
                        : includes(t, "immediate smoke")       ? "smoke" : "";

  /* :266-273 — G27: SUPPRESS on a recorded target, by number and duration;
     tested before location parsing because it carries a target number. */
  static const std::regex reSup(R"(\bsuppress\s+(?:target\s+)?([a-z]{2})\s*(\d{4})\b)");
  if (std::regex_search(t, m, reSup) && imm.empty()) {
    static const std::regex reMin(R"((\d+)\s*(?:minute|minutes|min)\b)");
    std::smatch mMin;
    p.type = "suppresstgt";
    p.tgtNum = toUpperAscii(m[1].str() + m[2].str());
    if (std::regex_search(t, mMin, reMin)) p.minutes = parseDigits(mMin[1].str());
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :274-279 — SUGG2: priority target, same target-number shape. */
  static const std::regex rePri(R"(\bpriority\s+target\s+([a-z]{2})\s*(\d{4})\b)");
  if (std::regex_search(t, m, rePri) && imm.empty()) {
    p.type = "prioritytgt"; p.tgtNum = toUpperAscii(m[1].str() + m[2].str());
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :280-285 — SUGG2: fire a recorded target by number. */
  static const std::regex reFireTgt(R"(\bfire\s+target\s+([a-z]{2})\s*(\d{4})\b)");
  if (std::regex_search(t, m, reFireTgt) && imm.empty()) {
    p.type = "firetgt"; p.tgtNum = toUpperAscii(m[1].str() + m[2].str());
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :286-297 — SUGG8: PLAN TARGET files a numbered target without firing.
     Tested before grid extraction (it carries a grid). */
  static const std::regex rePlanTgt(R"(\bplan\s+(?:a\s+)?target\b)");
  if (std::regex_search(t, rePlanTgt)) {
    const std::ptrdiff_t gi8 = indexOf(toks, "grid");
    std::string d8;
    if (gi8 >= 0)
      for (std::size_t i = std::size_t(gi8) + 1; i < toks.size() && isDigitsStr(toks[i]); i++)
        d8 += toks[i];
    p.type = "plantgt"; p.digits = d8; p.raw = t; p.toks = toks;
    return p;
  }

  /* :298-315 — SUGG1: the final protective fire, three shapes, all before
     location parsing. */
  static const std::regex reFireFpf(R"(\b(?:fire|repeat)\s+(?:the\s+)?fpf\b)");
  if (std::regex_search(t, reFireFpf)) { p.type = "firefpf"; p.raw = t; p.toks = toks; return p; }
  static const std::regex reFpf(R"(\bfpf\b)");
  if (std::regex_search(t, reFpf) && hasToken(toks, "grid")) {
    const std::ptrdiff_t gi2 = indexOf(toks, "grid");
    std::string d2;
    for (std::size_t i = std::size_t(gi2) + 1; i < toks.size() && isDigitsStr(toks[i]); i++)
      d2 += toks[i];
    static const std::regex reDir34(R"(\bdirection\s+(\d{3,4})\b)");
    std::smatch mDir;
    p.type = "planfpf"; p.digits = d2;
    if (std::regex_search(t, mDir, reDir34)) p.dirMils = parseDigits(mDir[1].str());
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :316-328 — SUGG4: series of targets — plan/establish files, fire brings
     it down. Target list via matchAll. */
  static const std::regex reTgtNum(R"(\b([a-z]{2})\s*(\d{4})\b)");
  static const std::regex reSer(R"(\b(?:plan|establish)\s+series\s+([a-z]+)\b)");
  if (std::regex_search(t, m, reSer)) {
    p.type = "planseries"; p.name = toUpperAscii(m[1].str());
    for (std::sregex_iterator it(t.begin(), t.end(), reTgtNum), end; it != end; ++it)
      p.tgts.push_back(toUpperAscii((*it)[1].str() + (*it)[2].str()));
    p.raw = t; p.toks = toks;
    return p;
  }
  static const std::regex reSerF(R"(\bfire\s+series\s+([a-z]+)\b)");
  if (std::regex_search(t, m, reSerF)) {
    p.type = "fireseries"; p.name = toUpperAscii(m[1].str());
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :329-340 — SUGG3: group of targets, same grammar; the difference is
     simultaneity, and that difference is the lesson. */
  static const std::regex reGrp(R"(\b(?:plan|establish)\s+group\s+([a-z0-9]+)\b)");
  if (std::regex_search(t, m, reGrp)) {
    p.type = "plangroup"; p.name = toUpperAscii(m[1].str());
    for (std::sregex_iterator it(t.begin(), t.end(), reTgtNum), end; it != end; ++it)
      p.tgts.push_back(toUpperAscii((*it)[1].str() + (*it)[2].str()));
    p.raw = t; p.toks = toks;
    return p;
  }
  static const std::regex reGrpF(R"(\bfire\s+group\s+([a-z0-9]+)\b)");
  if (std::regex_search(t, m, reGrpF)) {
    p.type = "firegroup"; p.name = toUpperAscii(m[1].str());
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :341-343 — SUGG6: SAY TIME OF FLIGHT. */
  static const std::regex reTof(
      R"(\b(?:say|request|send)\s+(?:your\s+|the\s+)?time\s+of\s+flight\b)");
  if (std::regex_search(t, reTof)) { p.type = "tofquery"; p.raw = t; p.toks = toks; return p; }

  /* :344-354 — SUGG1: flank-piece-in FPF adjustment ("NUMBER n, RIGHT x...").
     Single digit at a word boundary, so an MTO readback's "TARGET NUMBER
     7001" can never be mistaken for a piece. Falls through when the gun
     number has no correction or REPEAT riding with it. */
  static const std::regex reGun(R"(\bnumber\s+(\d)\b)");
  if (std::regex_search(t, m, reGun)) {
    static const std::regex reR(R"(\bright\s+(\d+)\b)");
    static const std::regex reL(R"(\bleft\s+(\d+)\b)");
    static const std::regex reA(R"(\badd\s+(\d+)\b)");
    static const std::regex reD(R"(\bdrop\s+(\d+)\b)");
    static const std::regex reRep(R"(\brepeat\b)");
    std::smatch gR, gL, gA, gD;
    const bool hR = std::regex_search(t, gR, reR);
    const bool hL = std::regex_search(t, gL, reL);
    const bool hA = std::regex_search(t, gA, reA);
    const bool hD = std::regex_search(t, gD, reD);
    const bool rep = std::regex_search(t, reRep);
    if (rep || hR || hL || hA || hD) {
      p.type = "fpfadj"; p.gun = parseDigits(m[1].str()); p.repeat = rep;
      p.right = hR ? parseDigits(gR[1].str()) : hL ? -parseDigits(gL[1].str()) : 0;
      p.add = hA ? parseDigits(gA[1].str()) : hD ? -parseDigits(gD[1].str()) : 0;
      p.raw = t; p.toks = toks;
      return p;
    }
  }

  /* :355-398 — grid extraction. NET16: cap the digit run at the first token
     prefix totalling a valid grid length (6/8/10) so a description that
     opens with a number is not swallowed; an unsplittable run still falls
     through whole and is refused exactly as before. An immediate call also
     accepts a bare 6/8-digit run with the word "grid" dropped. */
  const std::ptrdiff_t gi = indexOf(toks, "grid");
  std::string digits;
  std::ptrdiff_t gridEnd = -1;
  if (gi >= 0) {
    svec runs;
    for (std::size_t i = std::size_t(gi) + 1; i < toks.size() && isDigitsStr(toks[i]); i++)
      runs.push_back(toks[i]);
    std::size_t take = runs.size();
    for (std::size_t i = 0, len = 0; i < runs.size(); i++) {
      len += runs[i].size();
      if (len == 6 || len == 8 || len == 10) { take = i + 1; break; }
      if (len > 10) break;
    }
    for (std::size_t i = 0; i < take; i++) digits += runs[i];
    gridEnd = gi + 1 + std::ptrdiff_t(take);
  }
  if (digits.empty() && !imm.empty()) {          // :383-387
    for (std::size_t i = 0; i < toks.size(); i++)
      if (isDigitsStr(toks[i]) && (toks[i].size() == 6 || toks[i].size() == 8)) {
        digits = toks[i];
        gridEnd = std::ptrdiff_t(i) + 1;
        break;
      }
  }
  if (!digits.empty()) {                         // :388-398
    p.warno = !imm.empty() ? "ffe" : (ffe ? "ffe" : (includes(t, "adjust fire") ? "adjust" : "none"));
    svec descToks;                               // description after the TAKEN grid tokens
    for (std::size_t i = std::size_t(gridEnd);
         i < toks.size() && !inList(toks[i], {"over", "out", "break"}); i++)
      descToks.push_back(toks[i]);
    p.type = "cff"; p.method = "grid"; p.digits = digits; p.imm = imm;
    p.desc = descClean(joinSpace(descToks));     // F7
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :399-433 — shift from known point; named KPs normalized to their id
     first (:402-405 — the KP name is interpolated into the pattern raw,
     exactly as the JS does; scenario KP names are plain uppercase words).
     NET15: two-letter prefix anchored DIRECTLY to the digits by lookahead. */
  std::string tS = t;
  for (const KnownPointRef& k : ctx.kps) {
    if (k.name.empty()) continue;
    const std::regex reKp("\\b(?:known point\\s+|kp\\s+)?" + toLowerAscii(k.name) + "\\b");
    tS = std::regex_replace(tS, reKp, "known point " + k.id, kFirstOnly);
  }
  static const std::regex reShift(
      R"(\b(?:from|shift)\s+(?:target\s+)?(?:known point\s+|kp\s+|a\s?b\s+|[a-z]{2}(?=\d{4}\b))?(\d{4})\b)");
  if (std::regex_search(tS, m, reShift)) {
    p.warno = ffe ? "ffe" : (includes(t, "adjust fire") ? "adjust" : "none");
    p.kpId = m[1].str();
    static const std::regex reLRnum2(R"(\b(left|right)\s+(\d+))");
    static const std::regex reADnum2(R"(\b(add|drop)\s+(\d+))");
    static const std::regex reDirN(R"(\bdirection\s+(\d+)\b)");
    static const std::regex reUDnum(R"(\b(up|down)\s+(\d+))");
    std::smatch sm;
    if (std::regex_search(t, sm, reLRnum2))
      p.sRight = (sm[1] == "right" ? 1 : -1) * parseDigits(sm[2].str());
    if (std::regex_search(t, sm, reADnum2))
      p.sAdd = (sm[1] == "add" ? 1 : -1) * parseDigits(sm[2].str());
    if (std::regex_search(t, sm, reDirN)) p.dirMils = parseDigits(sm[1].str());
    /* :420-425 — kw strip set (G23 includes up/down or the vertical becomes
       the target description). */
    svec descToks;
    for (const std::string& w : toks)
      if (!inList(w, {"hellhound", "hacksaw", "fires", "this", "is", "mustang",
                      "adjust", "fire", "for", "effect", "shift", "from", "known",
                      "point", "kp", "a", "b", "ab", "left", "right", "add",
                      "drop", "direction", "over", "out", "break", "danger",
                      "close", "up", "down"}) &&
          !isDigitsStr(w))
        descToks.push_back(w);
    p.desc = joinSpace(descToks);
    if (std::regex_search(t, sm, reUDnum))       // :428 — G23 shift vertical
      p.vertM = (sm[1] == "up" ? 1 : -1) * parseDigits(sm[2].str());
    p.type = "cff"; p.method = "shift";
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :434-463 — polar. NET3 tolerance: RANGE as a synonym for DISTANCE and a
     natural connective between proword and number. */
  static const std::regex reDirTol(R"(\bdirection\s+(?:for\s+|of\s+)?(\d+)\b)");
  static const std::regex reDistTol(R"(\b(?:distance|range)\s+(?:to\s+|of\s+|at\s+)?(\d+)\b)");
  std::smatch mDir, mDist;
  if (std::regex_search(t, mDir, reDirTol) && std::regex_search(t, mDist, reDistTol)) {
    p.warno = ffe ? "ffe" : (includes(t, "adjust fire") ? "adjust" : "none");
    std::ptrdiff_t di = indexOf(toks, "distance");
    if (di < 0) di = indexOf(toks, "range");
    di++;
    while (di < std::ptrdiff_t(toks.size()) &&
           inList(toks[std::size_t(di)], {"to", "of", "at"}))
      di++;
    while (di < std::ptrdiff_t(toks.size()) && isDigitsStr(toks[std::size_t(di)])) di++;
    svec descToks;
    for (std::size_t i = std::size_t(di);
         i < toks.size() && !inList(toks[i], {"over", "out", "break"}); i++)
      descToks.push_back(toks[i]);
    static const std::regex reUDnum2(R"(\b(up|down)\s+(\d+))");
    std::smatch pUD;
    if (std::regex_search(t, pUD, reUDnum2))     // :455 — G23 polar vertical
      p.vertM = (pUD[1] == "up" ? 1 : -1) * parseDigits(pUD[2].str());
    p.type = "cff"; p.method = "polar";
    p.dirMils = parseDigits(mDir[1].str());
    p.distM = parseDigits(mDist[1].str());
    /* :459-461 — strip the vertical out of the description (it follows the
       distance, so "distance 2100, down 40, troops" would read as
       "down 40 troops"). */
    static const std::regex reUDstrip(R"(\b(up|down)\s+\d+\s*)");
    p.desc = descClean(std::regex_replace(joinSpace(descToks), reUDstrip, ""));  // F7
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :464-469 — standalone OT direction (same connective tolerance). JS
     returns ONLY type + dirMils here — no raw/toks. */
  static const std::regex reLRADcap(R"(\b(left|right|add|drop)\b)");
  std::smatch mDirOnly;
  if (std::regex_search(t, mDirOnly, reDirTol) && !std::regex_search(t, reLRADcap)) {
    p.type = "direction";
    p.dirMils = parseDigits(mDirOnly[1].str());
    return p;
  }

  /* :470-502 — the correction, now carrying its VERTICAL (G23), plus the F5
     stray-number count (NET19: the observer's own callsign digits are
     stripped before counting — identifying yourself is never wrong). */
  CorrectionValues corr;
  bool any = false;
  static const std::regex reLRnum3(R"(\b(left|right)\s+(\d+))");
  static const std::regex reADnum3(R"(\b(add|drop)\s+(\d+))");
  static const std::regex reUDnum3(R"(\b(up|down)\s+(\d+))");
  std::smatch mLR, mAD, mUD;
  const bool hLR = std::regex_search(t, mLR, reLRnum3);
  const bool hAD = std::regex_search(t, mAD, reADnum3);
  const bool hUD = std::regex_search(t, mUD, reUDnum3);
  if (hLR) { corr.right = (mLR[1] == "right" ? 1 : -1) * parseDigits(mLR[2].str()); any = true; }
  if (hAD) { corr.add = (mAD[1] == "add" ? 1 : -1) * parseDigits(mAD[2].str()); any = true; }
  if (hUD) { corr.vert = (mUD[1] == "up" ? 1 : -1) * parseDigits(mUD[2].str()); any = true; }
  static const std::regex reCallsignNum(R"(\bmustang\s+\d+\b)");
  static const std::regex reAnyNum(R"(\b\d+\b)");
  const std::string tNums = std::regex_replace(t, reCallsignNum, "mustang");
  const double stray = double(countMatches(tNums, reAnyNum)) -
                       (hLR ? 1 : 0) - (hAD ? 1 : 0) - (hUD ? 1 : 0);
  static const std::regex reRepeatWord(R"(\brepeat\b)");
  if (any || ffe) {                              // :501-502
    p.type = "adjust"; p.corr = corr; p.any = any; p.ffe = ffe; p.stray = stray;
    p.rep = std::regex_search(t, reRepeatWord);  // G13 — REPEAT riding on a correction
    p.raw = t; p.toks = toks;
    return p;
  }

  /* :503-512 — NET6: a shell word riding on REPEAT travels with it. JS
     returns ONLY type + shell here — no raw/toks. */
  if (std::regex_search(t, reRepeatWord)) {
    static const std::regex reRepShell(R"(\b(he|smoke|illum\w*|wp|white phosphorus)\b)");
    std::smatch mRs;
    p.type = "repeat";
    if (std::regex_search(t, mRs, reRepShell)) {
      const std::string k = mRs[1].str();
      p.shell = (k == "wp" || k == "white phosphorus") ? "smoke"
              : (k.rfind("illum", 0) == 0) ? "illum" : k;
    }
    return p;
  }

  /* :513-530 — G7: the observer transmitting his OT FACTOR; tested after
     grid/shift/polar so a real call is never claimed. */
  if (includes(t, "factor")) { p.type = "otfactor"; p.raw = t; p.toks = toks; return p; }

  /* :531-535 — G24: AT MY COMMAND tested HERE, after the CFF branches — it
     is a legal element of transmission 3. */
  if (includes(t, "at my command")) { p.type = "amc"; p.on = true; p.raw = t; p.toks = toks; return p; }

  /* :536-545 — the bare FIRE proword, deliberately strict: only when FIRE is
     the whole transmission apart from callsigns/prowords ('12' in the JS
     strip set is redundant beside the digit filter; kept for fidelity). */
  {
    svec core;
    for (const std::string& w : toks)
      if (!inList(w, {"over", "out", "break", "hellhound", "hacksaw", "fires",
                      "this", "is", "mustang", "12"}) &&
          !isDigitsStr(w))
        core.push_back(w);
    if (core.size() == 1 && core[0] == "fire") { p.type = "fire"; p.raw = t; p.toks = toks; return p; }
  }

  /* :546 — G22: a bare WARNING ORDER is doctrinal Transmission 1. */
  static const std::regex reAdjustFire(R"(\badjust fire\b)");
  if (std::regex_search(t, reAdjustFire)) {
    p.type = "warno"; p.warno = "adjust"; p.raw = t; p.toks = toks; return p;
  }

  p.type = "unknown"; p.raw = t; p.toks = toks;  // :547
  return p;
}

/* ============================================================ TLOG record
   12-cff-handling.js:907-908 — the classification the transcripts store. */
TlogParseClass tlogParseClass(const ParsedMessage& p) {
  return TlogParseClass{ p.type, p.method, p.warno, p.ffe };
}

/* ============================================================ formatNotes
   11-parser.js:550-566. Note strings byte-identical to the source (the
   \xE2\x80\x94 escapes are the em dash, kept out of the literal so the file
   stays ASCII for MSVC's source-charset handling). */
std::vector<std::string> formatNotes(const ParsedMessage& p,
                                     const std::string& fdcCallFull) {
  std::vector<std::string> notes;
  const std::string& t = p.raw;
  static const std::regex reShortCall(R"(\s+(?:FIRES|ACTUAL)$)");  // 01-math-terrain.js:182
  const std::string shortCall =
      std::regex_replace(fdcCallFull, reShortCall, "", kFirstOnly);
  if (t.find(toLowerAscii(shortCall)) == std::string::npos || !includes(t, "mustang"))
    notes.push_back("Open with both callsigns: \"" + fdcCallFull + ", THIS IS MUSTANG 12\".");
  if (p.warno == "none")
    notes.push_back("No warning order \xE2\x80\x94 say ADJUST FIRE or FIRE FOR EFFECT.");
  /* NET29(a) — the 8-digit-on-artillery note is deleted in the source;
     nothing to port. */
  if (p.desc.empty())
    notes.push_back("No target description (\"infantry in the open\", etc.).");
  if (!hasToken(p.toks, "over"))
    notes.push_back("End each transmission with OVER.");
  return notes;
}

} }
