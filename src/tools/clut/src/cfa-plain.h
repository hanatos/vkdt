#pragma once

static inline double
cfa_plain_smoothness(
    int num,
    const double *p)
{
  double err = 0.0;
  for(int k=0;k<3;k++)
    for(int i=1;i<num;i++)
      err += 100*(p[num*k+i]-p[num*k+i-1])*(p[num*k+i]-p[num*k+i-1]) / num;
  return err;
}

static inline double
cfa_plain_all(
    int           num,         // number of params
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  float step = (730.0f - 380.0f) / (num - 1.0f);
  int l = (wavelength - 360.0)/step;
  if(l < 0 || l >= 36) return 0.0;
  return p[l] * p[l]; // square to force non-negativity
  // return 0.32 * p[l]; // XXX pass through
}

static inline double
cfa_plain_red(int num, const double *p, double wavelength)
{ return cfa_plain_all(num, p, wavelength); }
static inline double
cfa_plain_green(int num, const double *p, double wavelength)
{ return cfa_plain_all(num, p, wavelength); }
static inline double
cfa_plain_blue(int num, const double *p, double wavelength)
{ return cfa_plain_all(num, p, wavelength); }
