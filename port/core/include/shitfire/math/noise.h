#pragma once

namespace shitfire {

/* port of src/js/01-math-terrain.js:10 (oracle-v1) — integer hash → [0,1).
   Bit-exact for any double inputs: ECMA ToInt32 conversions, 32-bit-wrapping
   multiplies/adds, one exact divide by 2^32. */
double hash2(double ix, double iz, double seed);

/* port of src/js/01-math-terrain.js:16 (oracle-v1) — smoothstep-interpolated
   value noise over hash2 lattice points. Pure IEEE ops over hash2. */
double vnoise(double x, double z, double seed);

/* port of src/js/01-math-terrain.js:24 (oracle-v1) — fractal Brownian motion:
   `oct` octaves of vnoise, halving amplitude / doubling frequency. */
double fbm(double x, double z, double seed, int oct);

/* port of src/js/01-math-terrain.js:32-34 (oracle-v1) */
double clamp(double v, double a, double b);
double lerp(double a, double b, double t);
double smoothstep(double a, double b, double v);

}
