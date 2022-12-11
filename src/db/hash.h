#pragma once

// quick hacky uncryptographic 64-bit hash from https://stackoverflow.com/questions/13325125/lightweight-8-byte-hash-function-algorithm
static inline uint64_t
hash64_l(const char *str, size_t l)
{
  uint64_t mix = 0;
  const uint64_t mulp = 2654435789u;
  mix ^= 104395301u;
  for(size_t i=0;i<l&&*str;i++) mix += (*str++ * mulp) ^ (mix >> 23);
  return mix ^ (mix << 37);
}

static inline uint64_t
hash64(const char *str)
{
  return hash64_l(str, -1ul);
}
