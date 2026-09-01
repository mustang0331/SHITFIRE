#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

/* The SHITFIRE radio-message parser, ported from src/js/11-parser.js in its
   entirety, plus the TLOG classification record from 12-cff-handling.js:907.
   Branch ORDER inside parseMessage is load-bearing (the repo keeps a
   parser-order gate, tools/parser-order.js, because of it): the .cpp ports
   every branch in source order with the JS line cited at each one.

   All bodies live in parser_message.cpp (port/README.md: nothing
   computational inline in a core header). No exceptions are thrown or
   caught; every std::regex pattern is a compile-time-known valid ECMAScript
   pattern (the browser parser was verified to use only lookahead + matchAll
   — no lookbehind, no named groups).

   Caller contract carried over from the browser: onPlayerMessage refuses
   transmissions longer than 1000 characters before parsing
   (12-cff-handling.js:889, BUILD4/NET9 stack-overflow guard). parseMessage
   recurses (correction :114, routed :145) and inherits that cap. */
namespace shitfire { namespace doctrine {

/* One entry of Scenario.kps as the parser sees it: id is the bare 4-digit
   string ("1001", "1002", ... — 05-units-scenario.js:784; the "AB" prefix is
   display-only), name is the doctrine nickname in UPPERCASE ("BREWERY").
   parseMessage lowercases the name itself (11-parser.js:404). */
struct KnownPointRef {
  std::string id;
  std::string name;
};

/* The exact sim state the parse layer reads — nothing else in parseMessage
   touches mission or scenario state (verified: the TLOG parse entry is
   written at 12-cff-handling.js:907 immediately after parseMessage, before
   any state-dependent dispatch).

   WAVE-2 RECONCILIATION SEAM: defaults below reproduce the browser for the
   whole recorded corpus. declEastDeg is the constant 7 in every chapter
   (00-config.js:162 — the only assignment in the source). kps defaults to
   empty; it only affects parsing when the observer shifts from a known point
   BY NAME ("shift BREWERY", 11-parser.js:400-405), and no transmission in
   the 33-mission corpus does (cross-checked against the JS parser). The
   scenario porter's generated KP list plugs in here. */
struct ParserContext {
  double declEastDeg = 7.0;
  std::vector<KnownPointRef> kps;
};

/* eom refinement (11-parser.js:229) — right/add signed, any = either seen. */
struct EomRefine {
  double right = 0;
  double add = 0;
  bool any = false;
};

/* Correction triple (11-parser.js:476, G23) — deviation / range / vertical. */
struct CorrectionValues {
  double right = 0;
  double add = 0;
  double vert = 0;
};

/* The JS parse-result object as a struct. JS returns a different field set
   per branch; here every field exists and a branch fills only what its JS
   counterpart set. Conventions for "field absent / null" in JS:
     - std::string  "" = absent/null   (method, warno, imm, shell, ...)
     - std::optional nullopt = null    (dirMils, vertM, minutes)
     - inner        nullptr unless the branch sets it (correction, routed)
   JS numbers are double throughout (port/README.md porting rules). */
struct ParsedMessage {
  std::string type;                      // every branch
  std::string method;                    // cff: "grid" | "shift" | "polar"
  std::string warno;                     // cff/warno: "none" | "adjust" | "ffe"
  bool ffe = false;                      // adjust branch (:501)
  std::string raw;                       // normalized text t, where JS binds raw: t
  std::vector<std::string> toks;         // normalized tokens, where JS binds toks
  std::unique_ptr<ParsedMessage> inner;  // correction (:115) / routed (:148)
  std::string tgtNum;                    // routed/suppresstgt/prioritytgt/firetgt
  bool on = false;                       // amc (:150 off, :535 on)
  double sec = 0;                        // tot (:168)
  std::string kind;                      // shell (:186) / fuze (:204) / sheaf (:225)
  bool cancel = false;                   // sheaf (:225)
  EomRefine refine;                      // eom (:240)
  bool record = false;                   // eom (:240)
  std::string bda;                       // eom (:240)
  std::string digits;                    // cff-grid (:396) / posrep (:258) / plantgt (:296) / planfpf (:313)
  std::string imm;                       // cff-grid (:396): "suppress" | "smoke" | "" (null)
  std::string desc;                      // cff branches
  std::string kpId;                      // cff-shift (:429)
  double sRight = 0;                     // cff-shift (:414)
  double sAdd = 0;                       // cff-shift (:414)
  std::optional<double> dirMils;         // shift (:430) / polar (:456) / direction (:469) / planfpf (:313)
  double distM = 0;                      // polar (:457)
  std::optional<double> vertM;           // shift (:431) / polar (:458)
  CorrectionValues corr;                 // adjust (:476)
  bool any = false;                      // adjust (:477)
  double stray = 0;                      // adjust (:498) — can go negative, JS number
  bool rep = false;                      // adjust (:502) — REPEAT riding on a correction
  std::string shell;                     // repeat (:509): shell word or "" (null)
  std::optional<double> minutes;         // suppresstgt (:272)
  std::string name;                      // planseries/fireseries/plangroup/firegroup
  std::vector<std::string> tgts;         // planseries/plangroup
  double gun = 0;                        // fpfadj (:350)
  bool repeat = false;                   // fpfadj (:350)
  double right = 0;                      // fpfadj (:351)
  double add = 0;                        // fpfadj (:352)
};

/* normalize — 11-parser.js:36-98. Voice-transcript repair passes: STT proword
   repair, number words to digits, hundred/thousand fold, single-digit-run
   merge, announced-MAGNETIC direction fold (uses ctx.declEastDeg),
   announced-DEGREES fold, phonetic target-number fold. Returns the repaired
   text as space-joined lowercase tokens. */
std::string normalize(const std::string& s, const ParserContext& ctx);

/* parseMessage — 11-parser.js:100-548, every branch in source order. */
ParsedMessage parseMessage(const std::string& raw, const ParserContext& ctx);

/* The classification record the TLOG stores for every observer transmission
   — 12-cff-handling.js:907-908:
     TLOG.add('parse', '', raw, { ptype: p.type, method: p.method || null,
       warno: p.warno || null, ffe: !!p.ffe });
   "" here stands for the recorded null. This is the per-message Level-3
   parity surface (PARITY_TESTING.md level 3: "identical parse classification
   per message").
   Not ported (wave-2 FDC scope, they precede this record and log no parse
   entry): the "note:" intercept (12-cff-handling.js:865) and the >1000-char
   oversize refusal (:889) — both log syserr/nothing instead. */
struct TlogParseClass {
  std::string ptype;
  std::string method;  // "" = recorded null
  std::string warno;   // "" = recorded null
  bool ffe = false;
};
TlogParseClass tlogParseClass(const ParsedMessage& p);

/* formatNotes — 11-parser.js:550-566. fdcCallFull is the full net callsign
   from fdcCall() (01-math-terrain.js:176-181, e.g. "HELLHOUND FIRES"); the
   short vocative form is derived here exactly as fdcShort() does
   (01-math-terrain.js:182). Which callsign is active is chapter state — the
   caller (wave-2 FDC) supplies it. */
std::vector<std::string> formatNotes(const ParsedMessage& p,
                                     const std::string& fdcCallFull);

} }
