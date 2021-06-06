#pragma once

#if 0 // eric's non-C2 quad to tri mapping:
void quad2tri(double *x, double *y)
{
  if(*y>*x) { *x *= 0.5f; *y -= *x; }
  else      { *y *= 0.5f; *x -= *y; }
}
// the inverse:
void tri2quad(double *x, double *y)
{ // same condition as for quad, diagonal is unchanged
  if(*y>*x) { *y += *x; *x *= 2.0; }
  else      { *x += *y; *y *= 2.0; }
}
#else
#include <math.h>
// std C2 mapping with sqrt
void quad2tri(double *x, double *y)
{
  const double sx = sqrt(*x);
  *x = 1.0-sx;
  *y = *y * sx;
}
void tri2quad(double *x, double *y)
{
  const double t0 = *x, t1 = *y;
  *x = (1.0-t0)*(1.0-t0);
  *y = t1 / (1.0-t0);
}
#endif
