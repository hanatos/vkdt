#include "cfa_data.h"

static inline void
cfa_pca_init(
    int num,
    double *p)
{
  for(int k=0;k<num;k++)
    p[k] = k <= 4 ? 0.1 : 0.0;
}

static inline double
cfa_pca_smoothness(
    int num,
    const double *p)
{
  return 0.0; // pca has no idea what smoothness is.
}

static inline double
cfa_pca_red(
    int           num,         // number of params (max 10 at this time)
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  int l = (wavelength - 380.0)/5.0;
  if(l < 0 || l >= 81) return 0.0;
  double res = p[0] * cfa_avg_red[l];
  for(int i=0;i<num-1;i++)
    res += cfa_evec_red[i][l] * p[i+1];
  return fmax(0.0, res);
}

static inline double
cfa_pca_green(
    int           num,         // number of params (max 10 at this time)
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  int l = (wavelength - 380.0)/5.0;
  if(l < 0 || l >= 81) return 0.0;
  double res = p[0] * cfa_avg_green[l];
  for(int i=0;i<num-1;i++)
    res += cfa_evec_green[i][l] * p[i+1];
  return fmax(0.0, res);
}

static inline double
cfa_pca_blue(
    int           num,         // number of params (max 10 at this time)
    const double *p,           // parameters
    double        wavelength)  // evaluate at this wavelength
{
  int l = (wavelength - 380.0)/5.0;
  if(l < 0 || l >= 81) return 0.0;
  double res = p[0] * cfa_avg_blue[l];
  for(int i=0;i<num-1;i++)
    res += cfa_evec_blue[i][l] * p[i+1];
  return fmax(0.0, res);
}
