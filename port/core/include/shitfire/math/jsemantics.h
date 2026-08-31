#pragma once
#include <cstdint>

/* JavaScript numeric semantics (ECMA-262 conversions + Math.round / Math.imul).
   All bodies live in jsemantics.cpp — nothing computational is inline in any
   core header, so an Unreal module including these can never recompile the
   math under /fp:fast (see port/README.md, porting rules). */
namespace shitfire { namespace js {

/* ECMA-262 ToInt32: NaN/±Inf → 0, truncate toward zero, wrap mod 2^32,
   reinterpret the low 32 bits as signed. Use for any unknown-range double a
   JS bitwise op would touch. Never raw static_cast<int>(double) — that is UB
   out of range. */
std::int32_t toInt32(double v);

/* ECMA-262 ToUint32 (JS `>>> 0`): same wrap, reinterpreted unsigned. */
std::uint32_t toUint32(double v);

/* JS Math.round per porting rules: std::floor(x + 0.5) — half rounds toward
   +infinity. NEVER std::round (half away from zero: round(-2.5) differs). */
double round(double v);

/* JS Math.imul: low 32 bits of the integer product, as signed int32. */
std::int32_t imul(std::int32_t a, std::int32_t b);

} }
