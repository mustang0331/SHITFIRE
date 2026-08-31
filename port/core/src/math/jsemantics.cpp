#include "shitfire/math/jsemantics.h"
#include <cmath>

namespace shitfire { namespace js {

std::uint32_t toUint32(double v) {
  /* ECMA-262 ToUint32: NaN/±Inf → 0; truncate toward zero; mod 2^32.
     std::fmod is exact for doubles, so every step is exact. */
  if (!std::isfinite(v)) return 0;
  double t = std::trunc(v);
  t = std::fmod(t, 4294967296.0);
  if (t < 0) t += 4294967296.0;
  return static_cast<std::uint32_t>(t);   /* t in [0, 2^32), integral */
}

std::int32_t toInt32(double v) {
  /* ToInt32 = ToUint32 reinterpreted as signed (same low 32 bits). */
  return static_cast<std::int32_t>(toUint32(v));
}

double round(double v) {
  /* JS Math.round per the porting rules: half toward +infinity. */
  return std::floor(v + 0.5);
}

std::int32_t imul(std::int32_t a, std::int32_t b) {
  /* Unsigned multiply wraps mod 2^32 (defined behaviour); low 32 bits back
     as signed — exactly Math.imul. */
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) *
                                   static_cast<std::uint32_t>(b));
}

} }
