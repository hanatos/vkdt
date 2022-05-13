#pragma once
#include "sigmoid.h"

static inline double
cfa_sigmoid_smoothness(
    int num,
    const double *p)
{
  double err = 0.0;
  return err;
}

static inline double
cfa_sigmoid_all(
    int           num,         // number of params
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  if(wavelength < 380.0 || wavelength > 780.0) return 0.0;
  double pl = poly(p, (wavelength-380.0)/(780.0-380.0), num);
  return sigmoid(pl);
}

static inline double
cfa_sigmoid_red(int num, const double *p, double wavelength)
{ return cfa_sigmoid_all(num, p, wavelength); }
static inline double
cfa_sigmoid_green(int num, const double *p, double wavelength)
{ return cfa_sigmoid_all(num, p, wavelength); }
static inline double
cfa_sigmoid_blue(int num, const double *p, double wavelength)
{ return cfa_sigmoid_all(num, p, wavelength); }
