#include "shitfire/math/prng.h"

namespace shitfire {

/* port of src/js/01-math-terrain.js:2 (oracle-v1)
   JS:
     a |= 0; a = a + 0x6D2B79F5 | 0;
     let t = Math.imul(a ^ a >>> 15, 1 | a);
     t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
     return ((t ^ t >>> 14) >>> 0) / 4294967296;
   Every op is 32-bit-clean, so direct uint32 arithmetic is bit-identical:
   int32 adds that overflow in JS are ToInt32'd right back to the same low
   32 bits uint32 wrap-around produces. */

Mulberry32::Mulberry32(std::uint32_t seed) : a_(seed) {}

std::uint32_t Mulberry32::nextU32() {
  a_ += 0x6D2B79F5u;
  std::uint32_t t = (a_ ^ (a_ >> 15)) * (1u | a_);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  return t ^ (t >> 14);
}

double Mulberry32::next() {
  return static_cast<double>(nextU32()) / 4294967296.0;
}

}
