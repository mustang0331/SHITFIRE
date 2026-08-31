#pragma once
#include <cstdint>

namespace shitfire {

/* port of src/js/01-math-terrain.js:2 (oracle-v1) — mulberry32.
   One JS closure = one class: the captured `a` is the uint32 state here.
   Bit-exact contract (PARITY_TESTING.md): both the pre-division u32 stream
   and the double stream (u32 / 2^32) must match the browser to the bit.
   The u32 comparison is the diagnostic when the double stream drifts. */
class Mulberry32 {
public:
  explicit Mulberry32(std::uint32_t seed);

  /* One draw, returning the pre-division value ((t ^ t>>>14) >>> 0).
     Advances the state. */
  std::uint32_t nextU32();

  /* One draw, returning what the JS closure returns: nextU32() / 4294967296.0.
     Advances the state. */
  double next();

private:
  std::uint32_t a_;
};

}
