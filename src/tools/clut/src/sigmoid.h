#pragma once
#include "core/core.h"
#include <math.h>

static inline double
normalise1(double *col)
{
  const double b = col[0] + col[1] + col[2];
  if(fabs(b) < 1e-8) return b;
  for(int k=0;k<3;k++) col[k] /= b;
  return b;
}

static inline double
poly(const double *c, double lambda, int num)
{
  double r = 0.0;
  for(int i=0;i<num;i++)
    r = r * lambda + c[i];
  return r;
}

static inline double
sigmoid(double x)
{
  return 0.5 * x / sqrt(1.0 + x * x) + 0.5;
}

static inline double
inv_sigmoid(double s)
{
  s = CLAMP(s, 1e-4, 1.0-1e-4);
  return (2.0*s - 1.0)/(2.0*sqrt(s*(1.0-s)));
}

static inline double
ddx_sigmoid(double x)
{
  return 0.5 * pow(x*x+1.0, -3.0/2.0);
}
