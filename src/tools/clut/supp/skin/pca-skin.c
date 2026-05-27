// construct pca basis from a few measured skin reflectance samples
// CFLAGS=-I../../../ LDFLAGS=-lm make pca-skin
#define NIST_SC 100  // sample count
#define NIST_WC 141  // wavelength count. we cropped the dataset to the visible domain.
#define NIST_LO 382  // lowest wavelength in the data
#define NIST_HI 802  // highest wavelength in the data
#define NIST_PP 3    // increment (this data is somewhat redundant here)
// Lu, Yan; Xiao, Kaida; Pointer, Michael; He, Ruili; Zhou, Sicong; Nasseraldin, Ahmed; et al. (2025). The International Skin Spectra Archive (ISSA): a multicultural human skin phenotype and colour spectra collection. figshare. Dataset. https://doi.org/10.6084/m9.figshare.28228571
#define ISSA_LO 400
#define ISSA_HI 700
#define ISSA_PP 10    // 10nm steps
#define ISSA_WC 31    // 31 measurements from [lo,hi]
#define ISSA_SC 15256 // wow.
#include "../../src/spectrum.h"
#include "../../../shared/cie1931.h"
#include "pipe/modules/matrices.h"
#include "core/mat3.h"
#define SC ISSA_SC
#define WC ISSA_WC
#define LO ISSA_LO
#define HI ISSA_HI
#define PP ISSA_PP
#define SVD_SIZE WC
#include "../svd.h"
#include <stdlib.h>
#include <assert.h>

static inline float inv(float x)
{
  return 1.0f/x;
}

// from pysr:
// static inline float model0(
//     float x0, float x1, float x2)
// {
//   return (((x0 * 0.0005281622) + x2) + -0.4059381) * (cos(x0 * -0.02818263) + inv(x2 + -0.16066551));
// }
// static inline float model0(
//     float x0, float x1, float x2)
// {
//     return (((x1 * 73.693) + -25.198) * cos(sqrt(cos(sqrt(x0) + (-1.2088 + cos((cos(sqrt(x0 * (x2 * 1.0464))) + (inv(x2) + cos(sqrt(x0) * 0.84716))) * -1.7138))) + sqrt(x0)))) + 1.139;
// }
// static inline float model0(
//     float x0, float x1, float x2)
// {
//   return (((inv(x2 + -0.16396938) + -1.4226682) + cos(inv(cos((x0 * 0.0002451296) + cos((sqrt(x0 + 1.3435133) * 0.98599696) + x2))) + sqrt(x0 + -354.61697))) * (x1 + 1.2300199)) * ((x0 * 0.00035998685) + (x2 + -0.3569576));
// }
// static inline float model1(
//     float x0, float x1, float x2)
// {
// return ((cos(sqrt(cos((cos(cos((x1 * sqrt(x0)) + -0.31618)) * -2.0409) + ((sqrt(x0 + cos(x2 * -74.983)) + 0.37491) + cos(((inv(x2) + cos(cos(cos(cos(sqrt(x0) * (cos(x2 * (cos(sqrt(x0) * -0.79734) * 1.7006)) * x1)))) * -1.8829)) * -1.7493) + (cos(sqrt(x0 * 0.92002)) + cos((sqrt(x0) * 1.4262) * (x2 + 0.20909)))))) + sqrt(x0))) * ((x1 * 73.769) + -25.089)) + 0.83845) + x2;
// }
#if 0
// NIST
static inline float model0(
    float x0, float x1, float x2)
{
return (cos(sqrt(sqrt(x0) + cos((cos(((inv(x2) + cos(sqrt(x0) * 0.84688807)) + cos(sqrt((x2 * x0) * 1.0487038))) * -1.7175605) + sqrt(x0)) + 17.654617))) * ((x1 * 73.69266) + -25.19251)) + 1.1371331;
}
static inline float model1(
    float x0, float x1, float x2)
{
return (x2 + x2) + ((inv(x1 + -0.27487227) * (x2 + -0.265104)) + (((x1 * 73.78014) + -25.122435) * (cos(sqrt(sqrt(x0 * 0.93578607) + cos(sqrt(x0 + 0.78704065) + ((cos(cos((x2 * -1.5042135) + (sqrt(x0) * x1))) * -1.6661059) + cos((((cos(sqrt(((inv(x2) * -3.16674) + (x0 * x2)) * 1.2282598)) + cos(inv(x2))) * -1.672805) + cos((cos(sqrt(x2)) * sqrt(x0 + -0.5936513)) * 1.1182858)) + -1.7370629))))) + 0.3030856)));
}
#endif
#if 1 // ISSA
static inline float model0(
    float x0, float x1, float x2)
{
  x0 = (x0-400.0)/300.0;
  return cos(cos((((x1 + cos(((((x0 + (x0 + x0)) + 2.704) * ((cos(x1 * ((x1 * x0) + 1.4979)) + (x2 + x0)) + (inv(((x2 + (x1 + cos((x2 * ((x0 + 5.9355) + (x0 * x0))) + (x0 + 0.13752)))) + cos(x1 + 0.56375)) * (((x1 + -1.1419) + x0) * 1.5704)) * 0.014532))) + x0) + ((x0 + (x0 * 2.9844)) + x2))) * 0.027581) + 0.42599) * (cos(x1) + (x0 + 0.89106))) + 0.68856) ;
}
static inline float model1(
    float x0, float x1, float x2)
{
  x0 = (x0-400.0)/300.0;
  return  ((x2 + ((((x0 * -0.017046) + (((x2 + -0.11702) + ((x0 * ((((x2 * 0.85605) + (x0 + -0.015761)) * x1) * 5.3044e-05)) + inv(inv((x2 * x0) * (((x1 + 0.80815) * x1) * 0.84805)) + (x1 * ((x1 + (((x2 + x1) + 0.19267) * (x1 + x1))) * 0.30424))))) + (x2 * 0.84117))) + -1.46) * 1.656)) * (x1 * x1)) + x2;
}
#endif

int main(int argc, char *argv[])
{
#if 0 // load NIST data
  FILE *fin = fopen("nist-skin.csv", "rb");
  if(!fin) exit(4);
  double X[NIST_SC*NIST_WC]   = {0.0}; // raw data
  double XtX[NIST_WC*NIST_WC] = {0.0}; // covariance matrix
  for(int l=0;l<NIST_WC;l++)
  { // for all wavelengths
    double lambda;
    fscanf(fin, "%lf,", &lambda); // could assert that lambda is really what we expect
    for(int s=0;s<NIST_SC;s++)
    { // read average spectrum of specimen s into X
      fscanf(fin, "%*f,%*f,%*f,%lf,", X+NIST_WC*s+l);
    }
    // drop the rest of the line
    fscanf(fin, "%*[^\n]");
    fgetc(fin); // \n
  }
#endif
#if 1 // load ISSA data
  FILE *fin = fopen("issa-400-700.csv", "rb");
  if(!fin) exit(4);
  double *X = calloc(sizeof(double),ISSA_SC*ISSA_WC);
  double *XtX = calloc(sizeof(double),ISSA_WC*ISSA_WC);
  for(int s=0;s<ISSA_SC;s++)
  { // for all samples/rows
    double lambda;
    int l=0;
    char line[2000] = {0};
    fscanf(fin, "%[^\n]\n", line);
    char *p = line;
    while(l<ISSA_WC)
    {
      X[ISSA_WC*s + l] = strtod(p, &p); // returns zero if it fails
      l++;
      if(*p = ',') { p++; continue; }
      if(!*p) break; // eol
    }
  }
#endif

  double (*crcb)[3] = calloc(sizeof(double),SC*3);
  double cfa_spec[100][4];
  int sp_cnt = spectrum_load("cie_observer", cfa_spec);
{ // output training data for pysr:
  FILE *fy = fopen("sr-y.dat", "wb");
  FILE *fx = fopen("sr-x.dat", "wb");
  srand48(666);
  for(int s=0;s<SC;s++)
  {
    float xyz[3] = {0.0f};
    for(int l=0;l<WC;l++)
    {
      double lambda = LO + PP*l;
      double em = cie_interp(cie_d65, lambda)/9.67579; // d65 normalised such that y=1
      double val = em * X[WC*s + l]; // "emissive" spectrum
      double cfa[3];
      for(int c=0;c<3;c++)
      {
        cfa[c] = spectrum_interp(cfa_spec, sp_cnt, c, lambda);
        xyz[c] += cfa[c] * val;
        assert(xyz[c] == xyz[c]);
      }
    }
    for(int c=0;c<3;c++) xyz[c] *= (HI-LO)*(double)PP/(double)WC;
    float M[] = matrix_xyz_to_rec2020;
    float bt2020[3] = {0.0f};
    mat3mulv(bt2020, M, xyz);
    float b = bt2020[0] + bt2020[1] + bt2020[2];
    float cr = bt2020[0]/b;
    float cb = bt2020[2]/b;
    crcb[s][0] = cr;
    crcb[s][1] = cb;
    crcb[s][2] = b;
        assert(b == b);
    // now output normalised spectrum as training data y
    // and output feature vector (lambda, cr, cb) as x
    for(int l=0;l<WC;l++)
    {
      double lambda = LO + PP*l;
      double em = cie_interp(cie_d65, lambda)/9.67579; // d65 normalised such that y=1
      double val = em * X[WC*s + l];
        assert(val == val);
        assert(b > 0);
      // if(drand48() < 0.05)
      {
        fprintf(fx, "%g %f %f\n", l/(WC-1.0), cr, cb); // use normalised lambda
        fprintf(fy, "%f\n", 1000.0*val/b);
      }
    }
  }
  fclose(fy);
  fclose(fx);
}
  // exit(0); // XXX
  // [..pysr doing it's thing here..]
{ // feed the same training data into our fitted model evaluation
  // plot for [i=0:3] 'test.dat' u 1:2+3*i w l lw 4, for [i=0:3] '' u 1:3+3*i w l lw 1, for[i=0:3] '' u 1:4+3*i w lp lw 2
  FILE *ft = fopen("test.dat", "wb");
  for(int l=0;l<WC;l++)
  {
    double lambda = LO + PP*l;
    double em = cie_interp(cie_d65, lambda)/9.67579; // d65 normalised such that y=1
    fprintf(ft, "%g ", lambda);
    for(int s=0;s<SC;s++)
    {
      double valr = em * X[WC*s + l];
      double val0 = 1e-3*model0(lambda, crcb[s][0], crcb[s][1])*crcb[s][2];
      double val1 = 1e-3*model1(lambda, crcb[s][0], crcb[s][1])*crcb[s][2];
      fprintf(ft, "%g %g %g ", valr, val0, val1);
    }
    fprintf(ft, "\n");
  }
  fclose(ft);
  double mse0 = 0.0, mse1 = 0.0, varr = 0.0, varb = 0.0, cov = 0.0, meanr = 0.0, meanb = 0.0;
  for(int s=0;s<SC;s++)
  {
    float xyz_0[3] = {0.0f};
    float xyz_1[3] = {0.0f};
    for(int l=0;l<WC;l++)
    {
      double lambda = LO + PP*l;
      double val0 = model0(lambda, crcb[s][0], crcb[s][1]);
      double val1 = model1(lambda, crcb[s][0], crcb[s][1]);
      double cfa[3];
      for(int c=0;c<3;c++)
      {
        cfa[c] = spectrum_interp(cfa_spec, sp_cnt, c, lambda);
        xyz_0[c] += cfa[c] * val0;
        xyz_1[c] += cfa[c] * val1;
      }
    }
    float M[] = matrix_xyz_to_rec2020;
    float bt2020_0[3] = {0.0f};
    float bt2020_1[3] = {0.0f};
    mat3mulv(bt2020_0, M, xyz_0);
    mat3mulv(bt2020_1, M, xyz_1);
    float b0 = bt2020_0[0] + bt2020_0[1] + bt2020_0[2];
    float cr0 = bt2020_0[0]/b0;
    float cb0 = bt2020_0[2]/b0;
    float b1 = bt2020_1[0] + bt2020_1[1] + bt2020_1[2];
    float cr1 = bt2020_1[0]/b1;
    float cb1 = bt2020_1[2]/b1;
    meanr += cr0;
    meanb += cb0;
    varr += cr0*cr0;
    varb += cb0*cb0;
    cov  += cr0*cb0;
    cr0 -= crcb[s][0];
    cb0 -= crcb[s][1];
    cr1 -= crcb[s][0];
    cb1 -= crcb[s][1];
    mse0 += cr0*cr0 + cb0*cb0;
    mse1 += cr1*cr1 + cb1*cb1;
    // fprintf(stderr, "specimen %d errors %g %g -- %g %g\n", s, cr0, cb0, cr1, cb1);
  }
  fprintf(stderr, "mse %g %g mean vec2(%g, %g) cov mat2(%g, %g, %g, %g)\n", mse0/SC, mse1/SC,
      meanr/SC, meanb/SC,
      varr/SC-(meanr/SC*meanr/SC),
      cov/SC-(meanr/SC*meanb/SC),
      cov/SC-(meanr/SC*meanb/SC),
      varb/SC-(meanb/SC*meanb/SC));
}
exit(0);

  // compute PCA:
  double *avg = calloc(sizeof(double),WC);
  for(int i=0;i<WC;i++) for(int s=0;s<SC;s++)
    avg[i] += X[WC*s+i]/SC;
  for(int j=0;j<WC;j++)
    for(int i=0;i<WC;i++)
      for(int s=0;s<SC;s++)
          XtX[WC*j+i] += (X[WC*s+i] - avg[i])*(X[WC*s+j] - avg[j]);
  double *S = calloc(sizeof(double),WC);
  double (*V)[WC] = calloc(sizeof(double),WC*WC);
  dsvd((double (*)[WC])XtX, WC, WC, S, V);
    
#if 0 // DEBUG: now output the results
    for(int i=0;i<WC;i++)
      fprintf(stdout, "%g %g %g %g %g %g\n",
          avg[i], XtX[WC*i+0], XtX[WC*i+1], XtX[WC*i+2], XtX[WC*i+8], XtX[WC*i+14]);
    // singular values. looks like energy decays very quickly.
    for(int i=0;i<WC;i++) fprintf(stderr, "%g ", S[i]);
    fprintf(stderr, "\n");
    exit(1);
#endif
#if 0
    {
  FILE *f = fopen("skin_data.h", "wb");
  if(!f) exit(3);
  fprintf(f, "#pragma once\n");

    // write out header for parametric model: first couple of vectors + s
    const int maxd = 36;
    fprintf(f, "static const double skin_evec_%s[%d][%d] = {{\n", maxd, WC);
    for(int k=0;k<maxd;k++)
    {
      for(int i=0;i<WC;i++)
        fprintf(f, "%g, ", XtX[WC*i + k]);
      if(k < maxd-1) fprintf(f, "},{\n");
    }
    fprintf(f, "}};\n");
    fprintf(f, "static const double skin_avg[%d] = {\n", WC);
    for(int i=0;i<WC;i++)
      fprintf(f, "%g, ", avg[i]);
    fprintf(f, "};\n");
    fprintf(f, "static const double skin_s[%d] = {\n", maxd);
    for(int k=0;k<maxd;k++)
      fprintf(f, "%g, ", S[k]);
    fprintf(f, "};\n");
  }
  fclose(f);
}
#endif

#if 1
{ // plot gnuplot happy:
  FILE *f = 0;
  f = fopen("plot-skin-s.dat", "wb"); 
  for(int k=0;k<WC;k++)
    fprintf(f, "%d %g\n", LO+PP*k, S[k]);
  fclose(f);

  f = fopen("plot-skin-avg.dat", "wb"); 
  for(int k=0;k<WC;k++)
    fprintf(f, "%d %g\n", LO+PP*k, avg[k]);
  fclose(f);

  f = fopen("plot-skin-evec.dat", "wb"); 
  for(int k=0;k<WC;k++)
  {
    fprintf(f, "%d ", LO+PP*k);
    for(int s=0;s<WC;s++)
      fprintf(f, "%g ", XtX[WC*k + s]);
    fprintf(f, "\n");
  }
  fclose(f);
}
#endif

  exit(0);
}
