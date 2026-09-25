#pragma once
static uint32_t seed = 1337;
static inline double
xrand()
{ // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed / 4294967296.0;
}
