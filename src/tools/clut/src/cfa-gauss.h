#pragma once

static inline void
cfa_gauss_init(
    int num,
    double *p)
{
  for(int k=0;k<num/2;k++)
    p[k] = (k+0.5)*2.0/num;
  for(int k=num/2;k<num;k++)
    p[k] = 0.1;
}

static inline void
cfa_gauss_init_bounds(
    int num,
    double *lb,
    double *ub)
{
  for(int k=0;k<num;k++) lb[k] = k < num/2 ? 0.0 : -1000.0;
  for(int k=0;k<num;k++) ub[k] = k < num/2 ? 1.0 :  1000.0;
}

static inline double
cfa_gauss_smoothness(
    int num,
    const double *p)
{
  // does not seem to do anything.
  double err = 0.0;
  // for(int k=0;k<3;k++)
  //   for(int i=num+1;i<num;i++)
  //     err += (p[num*k+i]-p[num*k+i-1])*(p[num*k+i]-p[num*k+i-1]) / num;
  return err;
}

// fake gaussian distribution, normalised to (-inf,inf) or, equivalently,
// [-4,4]. it also samples this domain. close to a real gaussian with
// sigma=0.833 (i.e. sqrt(log(2))).
static inline float
fakegaussian(const float x)
{
  // we don't care about normalisation:
  // const float norm_c1 = 2.0f * 0x1.1903a6p+0;
  const int i1 = 0x3f800000u, i2 = 0x40000000u;
  const int k0 = i1 - x*x * (i2 - i1);
  const int k = k0 > 0 ? k0 : 0;
  float ret;
  memcpy(&ret, &k, sizeof(float));
  return ret; // /norm_c1;
}

static inline double
cfa_gauss_all(
    int           num,         // number of params (==10 at this time so the hardcoded gauss widths are good)
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  if(wavelength < 380.0 || wavelength >= 720.0) return 0.0;
  double res = 0.0;
  const double *x = p;
  // const double x[] = { 0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95 };
  const double *y = p + num/2;
  for(int i=0;i<num/2;i++)
  {
    const double sigma = 22.0 * 20.0 / num; // simulate IR filter cutoff starting at 680nm (x is <= 1.0)
    double z = (wavelength-(380.0 + (680.0-380.0)*x[i]))/sigma * 0.833;
    res += y[i] * y[i] * fakegaussian(z);
    // res += sqrt(p[i] * p[i]) * exp(-(wavelength-x[i])*(wavelength-x[i])/(20.0*20.0));
  }
  return res;
}

static inline double
cfa_gauss_red(int num, const double *p, double wavelength)
{ return cfa_gauss_all(num, p, wavelength); }
static inline double
cfa_gauss_green(int num, const double *p, double wavelength)
{ return cfa_gauss_all(num, p, wavelength); }
static inline double
cfa_gauss_blue(int num, const double *p, double wavelength)
{ return cfa_gauss_all(num, p, wavelength); }
