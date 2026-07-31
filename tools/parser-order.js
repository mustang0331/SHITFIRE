/* tools/parser-order.js — parseMessage branch-precedence guard.
   Run: cscript //Nologo //E:JScript tools\parser-order.js
   READ-ONLY against SHITFIRE.html, per CLAUDE.md's dev-tooling rules.
   Run it whenever a branch is added to parseMessage.

   Parser precedence audit - after today's ~10 new branches, the residual risk in
   parseMessage is ORDER, not logic: every branch was harness-tested in isolation,
   but several are only correct BECAUSE of where they sit (posrep before grid,
   safety prowords first, at-my-command after the location methods, bare FIRE
   after everything containing the word "fire"). This reads the SHIPPED source and
   asserts the order, so a future insertion in the wrong place fails loudly. */
var fails = 0;
function check(label, cond, detail) {
  WScript.Echo((cond ? "  PASS  " : "  FAIL  ") + label + (detail ? "   [" + detail + "]" : ""));
  if (!cond) fails++;
}
var fso = new ActiveXObject("Scripting.FileSystemObject");
var s = fso.OpenTextFile("c:\\Users\\conno\\OneDrive\\Documents\\SHITFIRE\\SHITFIRE.html", 1).ReadAll();

var start = s.indexOf('function parseMessage(');
var end = s.indexOf('\nfunction ', start + 10);
var body = s.substring(start, end);
WScript.Echo("parseMessage body: " + body.length + " chars");

/* markers, in the order they MUST test */
var order = [
  ["say again",              "t.includes('say again')"],
  ["check firing (safety)",  "t.includes('check firing')"],
  ["cease loading (safety)", "t.includes('cease loading')"],
  ["cancel at my command",   "t.includes('cancel at my command')"],
  ["do not load",            "t.includes('do not load')"],
  ["cannot observe",         "t.includes('cannot observe')"],
  ["time on target",         "time on target"],
  ["end of mission",         "t.includes('end of mission')"],
  ["POS REP (before grid!)", "pos\\s*rep"],
  ["immediate types",        "immediate suppression"],
  ["suppress by number",     "suppresstgt"],
  ["grid extraction",        "toks.indexOf('grid')"],
  ["shift",                  "known point\\s+"],
  ["polar",                  "\\b(?:distance|range)\\s+"],   // NET3 — range is a distance synonym
  ["standalone direction",   "type: 'direction'"],
  ["corrections (incl HOB)", "type: 'adjust'"],
  ["repeat",                 "type: 'repeat'"],
  ["OT factor",              "type: 'otfactor'"],
  ["at my command (late!)",  "t.includes('at my command')"],
  ["bare FIRE (last)",       "type: 'fire'"],
  ["bare warning order",     "type: 'warno'"],
  ["unknown (fallthrough)",  "type: 'unknown'"]
];
WScript.Echo("");
WScript.Echo("=== branch order as shipped ===");
var prev = -1, prevName = "(start)";
for (var i = 0; i < order.length; i++) {
  var idx = body.indexOf(order[i][1]);
  WScript.Echo("  " + (idx >= 0 ? String(idx) : "MISSING") + "  " + order[i][0]);
  check("  '" + order[i][0] + "' present and after '" + prevName + "'",
        idx > prev, "at " + idx + ", prev " + prev);
  if (idx > prev) { prev = idx; prevName = order[i][0]; }
}

WScript.Echo("");
WScript.Echo("=== the four load-bearing orderings, stated ===");
check("safety prowords are tested before everything except say-again",
      body.indexOf("check firing") < body.indexOf("end of mission"));
check("POS REP is tested before grid extraction",
      body.indexOf("pos\\s*rep") < body.indexOf("toks.indexOf('grid')"));
check("at-my-command is tested AFTER the location methods (it is a legal T3 element)",
      body.indexOf("t.includes('at my command')") > body.indexOf("type: 'adjust'"));
check("bare FIRE is tested after every branch containing the word 'fire'",
      body.indexOf("type: 'fire'") > body.indexOf("t.includes('at my command')"));

WScript.Echo("");
WScript.Echo(fails === 0 ? "ALL CHECKS PASSED" : (fails + " CHECK(S) FAILED"));
