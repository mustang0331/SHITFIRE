#pragma once
#include <string>

/* Doctrine string utilities ported from src/js/01-math-terrain.js — the two
   parser-adjacent pieces the P1 math porter deliberately left (they are radio
   lexicon, not math, so they live in doctrine/). Bodies in
   lexicon_strings.cpp: nothing computational is inline in a core header
   (port/README.md layout rules). */
namespace shitfire { namespace doctrine {

/* descClean — 01-math-terrain.js:132-143 (F7). Strips the method-of-
   engagement prowords out of a TARGET DESCRIPTION so the readback never
   echoes "DANGER CLOSE, DANGER CLOSE TROOPS IN THE OPEN". Description means
   what the target IS, not how to engage it. Input is normalized (lowercase)
   text; output is the cleaned description with runs of whitespace collapsed
   and leading/trailing space-or-comma trimmed. */
std::string descClean(const std::string& d);

/* DC_RE + saidDangerClose — 01-math-terrain.js:157-158 (F6). The DANGER
   CLOSE proword matched with tolerance for what speech recognition actually
   produces ("DANGER CLOTHES" appears in two recorded play transcripts). A
   deliberately NARROW fuzzy match — an explicit variant list, never a
   similarity metric, because a false positive here would let a genuinely
   undeclared danger-close call through the safety gate.
   Case-sensitive exactly like the source regex (no /i flag): callers pass
   normalized lowercase text. */
bool saidDangerClose(const std::string& raw);

} }
