#pragma once
#include "xrand.h"

static inline void
cfa_legendre_init(
    int num,
    double *p)
{
  for(int i=0;i<num;i++) p[i] = 1e-4 + 1e-3*xrand();
}

static inline double
cfa_legendre_smoothness(
    int num,
    const double *p)
{
  double err = 0.0;
  return err;
}

static inline double
cfa_legendre_poly(
    int n,
    double x,
    double Pnm2,
    double Pnm1)
{
  if(n == 0) return 1.0;
  if(n == 1) return x;
  return ((2.0*n-1.0)*x*Pnm1 - (n-1.0)*Pnm2) / n;
}

static inline double
cfa_legendre_all(
    int           num,         // number of params
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  if(wavelength < 380.0 || wavelength > 780.0) return 0.0;
  const double x = (wavelength-380.0)/(780.0-380.0);
  double Pm2 = cfa_legendre_poly(0, x, 0, 0);
  double Pm1 = cfa_legendre_poly(1, x, 0, 0);
  double f = 0.0;
  f += Pm2 * p[0];
  f += Pm1 * p[1];
  for(int n=2;n<num;n++)
  {
    double P = cfa_legendre_poly(n, x, Pm2, Pm1);
    f += P * p[n];
    Pm2 = Pm1;
    Pm1 = P;
  }
  return exp(f); // make all positive
}

static inline double
cfa_legendre_red(int num, const double *p, double wavelength)
{ return cfa_legendre_all(num, p, wavelength); }
static inline double
cfa_legendre_green(int num, const double *p, double wavelength)
{ return cfa_legendre_all(num, p, wavelength); }
static inline double
cfa_legendre_blue(int num, const double *p, double wavelength)
{ return cfa_legendre_all(num, p, wavelength); }
