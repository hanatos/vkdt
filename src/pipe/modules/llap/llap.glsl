#include "config.h"

float gamma_from_i(uint i)
{
  // linear, next to no samples in blacks
  // return (i+0.5f)/6.0f;
  return (i)/(NUM_GAMMA - 1.0f); // have one sample at exactly 0 and exactly 1
  // return pow((i+0.5f)/6.0f, 2.0f);
}

int gamma_hi_from_v(float v)
{
  int hi = 1;
  for(;hi<NUM_GAMMA-1 && gamma_from_i(hi) <= v;hi++);
  return hi;
}

