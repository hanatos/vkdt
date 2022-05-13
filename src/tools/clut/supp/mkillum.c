// finds the illumination spectrum of a cc24 target shot.
// requires known cfa spectrum curves.
//
// read 24 picked colour spots from a cc24 image (camera rgb)
// read cfa spectrum curves
// read cc24 spectrum curves
// find illum spectrum such that:
// illum * cc24 * cfa = picked rgb colour
// illum is 36-bin 10nm spacing spectrum and saved to illum.txt

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "core/core.h"
#include "core/solve.h"
#include "spectrum.h"
#include "cc24.h"
#include "cie1931.h"
#include "xrand.h"

static uint32_t cfa_cnt;
static double   cfa_buf[100][4];

static double picked[24][3];
static double illum[36] = {0.0}; // same size as cc24

void integrate(
    double res[24][3],
    double *ill)
{
  int j = 0;
  int cnt = 0;
  for(int s=0;s<24;s++)
    res[s][0] = res[s][1] = res[s][2] = 0.0;
  for(int i=0;i<cc24_nwavelengths;i++)
  {
    while(cc24_wavelengths[i] > cfa_buf[j][0] && j < cfa_cnt-1) j++;
    if(cc24_wavelengths[i] == cfa_buf[j][0])
    {
      for(int s=0;s<24;s++)
      {
        res[s][0] += cfa_buf[j][1] * cc24_spectra[s][i] * ill[i];
        res[s][1] += cfa_buf[j][2] * cc24_spectra[s][i] * ill[i];
        res[s][2] += cfa_buf[j][3] * cc24_spectra[s][i] * ill[i];
      }
      cnt++;
    }
  }
  for(int s=0;s<24;s++)
    for(int k=0;k<3;k++)
      res[s][k] *= (cc24_wavelengths[cc24_nwavelengths-1] - cc24_wavelengths[0]) / (cnt-1.0);
}

void loss(
    double *p, // parameters: illum spectrum
    double *x, // output data, write here
    int     m, // number of parameters
    int     n, // number of data points (=1)
    void   *data)
{
  double res[24][3];
  double err = 0.0;
  integrate(res, p);
  // mean squared error:
  for(int i=0;i<24;i++)
  {
    err += (res[i][0] - picked[i][0])*(res[i][0] - picked[i][0])/24.0;
    err += (res[i][1] - picked[i][1])*(res[i][1] - picked[i][1])/24.0;
    err += (res[i][2] - picked[i][2])*(res[i][2] - picked[i][2])/24.0;
  }
  // smoothness term:
  for(int i=1;i<36;i++) err += 1. * (p[i]-p[i-1])*(p[i]-p[i-1]) / 36.0;
  x[0] = err;
}

void loss_dif(
    double *p,   // parameters: illum spectrum
    double *jac, // output: derivative dx / dp (n x m entries, n-major, i.e. (dx[0]/dp[0], dx[0]/dp[1], ..)
    int     m,   // number of parameters (=36)
    int     n,   // number of data points (=1)
    void   *data)
{
  for(int j=0;j<m;j++)
  {
    double X1, X2;
    double p2[m];
    const double h = 1e-10 + xrand()*1e-3;//1e-10;
    memcpy(p2, p, sizeof(p2));
    p2[j] += h;
    loss(p2, &X1, m, n, data);
    memcpy(p2, p, sizeof(p2));
    p2[j] -= h;
    loss(p2, &X2, m, n, data);
    jac[j] = (X1 - X2) / (2.0*h);
    // fprintf(stderr, "J %d = %g\n", j, jac[j]);
  }
}

int main(int argc, char *argv[])
{
  const char *model = "Canon EOS 5D Mark II";
  const char *pick  = "picked.txt";
  for(int k=1;k<argc;k++)
  {
    if(!strcmp(argv[k], "--picked") && k+1 < argc) pick = argv[++k];
    else model = argv[k];
  }

  cfa_cnt = spectrum_load(model, cfa_buf);

  FILE *f = fopen(pick, "rb");
  if(!f) exit(1);
  fscanf(f, "param:pick:01:picked:%lf:%lf:%lf",
      &picked[0][0], &picked[0][1], &picked[0][2]);
  for(int i=1;i<24;i++)
    fscanf(f, ":%lf:%lf:%lf",
        &picked[i][0], &picked[i][1], &picked[i][2]);
  fclose(f);

  double lb[36], ub[36];
  for(int i=0;i<36;i++) lb[i] = 0;
  for(int i=0;i<36;i++) ub[i] = 10000;
  uint32_t num_it = 10000;
  double target = 0.0;
  (void)dt_adam(
        loss, loss_dif,
        illum, &target, cc24_nwavelengths, 1,
        lb, ub, num_it, 0, 1e-8, 0.9, 0.99, 0.001, 0);

  f = fopen("illum.txt", "wb");
  for(int i=0;i<cc24_nwavelengths;i++)
    fprintf(f, "%g %g\n", cc24_wavelengths[i], illum[i]);
  fclose(f);

  exit(0);
}
