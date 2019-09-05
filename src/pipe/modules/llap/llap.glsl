#include "config.h"
#ifdef LLAP_HQ
#define gamma_cnt 10
#else
#define gamma_cnt 6
#endif

float gamma_from_i(int i)
{
  // linear, next to no samples in blacks
  // return (i+0.5f)/6.0f;
  return (i)/(gamma_cnt - 1.0f); // have one sample at exactly 0 and exactly 1
  // return pow((i+0.5f)/6.0f, 2.0f);
}

int gamma_hi_from_v(float v)
{
  int hi = 1;
  for(;hi<gamma_cnt-1 && gamma_from_i(hi) <= v;hi++);
  return hi;
}

