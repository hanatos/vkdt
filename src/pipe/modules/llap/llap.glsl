#include "config.h"

float gamma_from_i(uint i)
{
  // linear, next to no samples in blacks
  return (i)/(NUM_GAMMA - 1.0f); // have one sample at exactly 0 and exactly 1
  // bias everything to more samples near the blacks. this means we'll keep
  // blacks from completely drowning for high values of clarity
  float linear = i/(NUM_GAMMA-1.0f);
  const float delta = 6.0/29.0;
  if(linear <= delta*delta*delta) return linear / (3.0*delta*delta) + 4.0/29.0;
  return pow(linear, 1.0/3.0);
  // return pow((i)/(NUM_GAMMA - 1.0f), 1.0/2.0f);
}

int gamma_hi_from_v(float v)
{
  int hi = 1;
  for(;hi<NUM_GAMMA-1 && gamma_from_i(hi) <= v;hi++);
  return hi;
}

