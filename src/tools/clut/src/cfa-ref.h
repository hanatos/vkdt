#pragma once
#include "spectrum.h"

static double ref_spec[1000][4];

static inline void
cfa_ref_init(
    int num,
    double *p)
{ // this is horrible:
  static int chan = 0;
  if(chan == 0)
    spectrum_load("data/Canon_EOS_5D_Mark_II", ref_spec);
}

static inline double
cfa_ref_smoothness(
    int num,
    const double *p)
{
  return 0.0;
}

static inline double
cfa_ref_all(
    int           c,           // channel 1..3
    int           num,         // number of params
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  float step = 5.0;
  int l = (wavelength - 380.0)/step;
  if(l < 0 || l > 80) return 0.0;
  return ref_spec[l][c];
}

static inline double
cfa_ref_red(int num, const double *p, double wavelength)
{ return cfa_ref_all(1, num, p, wavelength); }
static inline double
cfa_ref_green(int num, const double *p, double wavelength)
{ return cfa_ref_all(2, num, p, wavelength); }
static inline double
cfa_ref_blue(int num, const double *p, double wavelength)
{ return cfa_ref_all(3, num, p, wavelength); }
