// gcc main.c -o main -lm && ./main 2>dreggn
// plot 'dreggn' u 1:2 w l lw 5, '' u 1:3 w l lw 5, '' u 1:4 w l lw 5, '' u 1:($2+$3+$4) w l lw 5
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../../../../tools/shared/lu.h" 
#include "../../../../tools/shared/matrices.h"

// discretisation of quadrature scheme
#define CIE_SAMPLES 95
#define CIE_LAMBDA_MIN 360.0
#define CIE_LAMBDA_MAX 830.0
#define CIE_FINE_SAMPLES ((CIE_SAMPLES - 1) * 3 + 1)
#define RGB2SPEC_EPSILON 1e-4
#define MOM_EPS 1e-3

#include "../../../../tools/shared/cie1931.h"

/// Precomputed tables for fast spectral -> RGB conversion
double lambda_tbl[CIE_FINE_SAMPLES],
       rgb_tbl[3][CIE_FINE_SAMPLES],
       rgb_to_xyz[3][3],
       xyz_to_rgb[3][3],
       xyz_whitepoint[3];

double sigmoid(double x) {
  return 0.5 * x / sqrt(1.0 + x * x) + 0.5;
}

double sqrd(double x) { return x * x; }

void clamp_coeffs(double coeffs[10])
{
  for(int c=0;c<3;c++)
  {
    double max = fmax(fmax(fabs(coeffs[3*c+0]), fabs(coeffs[3*c+1])), fabs(coeffs[3*c+2]));
    if (max > 1000) {
      for (int j = 0; j < 3; ++j)
        coeffs[3*c+j] *= 1000 / max;
    }
  }
  coeffs[9] = fmax(fmin(10.0, coeffs[9]), 0.1);
}

void init_tables()//Gamut gamut)
{
  memset(rgb_tbl, 0, sizeof(rgb_tbl));
  memset(xyz_whitepoint, 0, sizeof(xyz_whitepoint));

  const double *illuminant = 0;

#if 0
  switch (gamut) {
    case SRGB:
#endif
      // TODO: is this what i want?
      illuminant = cie_d65;
      memcpy(xyz_to_rgb, xyz_to_srgb, sizeof(double) * 9);
      memcpy(rgb_to_xyz, srgb_to_xyz, sizeof(double) * 9);
#if 0
      break;

    case ERGB:
      illuminant = cie_e;
      memcpy(xyz_to_rgb, xyz_to_ergb, sizeof(double) * 9);
      memcpy(rgb_to_xyz, ergb_to_xyz, sizeof(double) * 9);
      break;

    case XYZ:
      // illuminant = cie_e;
      // illuminant = cie_c; // for munsell data compatibility
      illuminant = cie_d65; // for hue constancy table with d65 white
      memcpy(xyz_to_rgb, xyz_to_xyz, sizeof(double) * 9);
      memcpy(rgb_to_xyz, xyz_to_xyz, sizeof(double) * 9);
      break;

    case ProPhotoRGB:
      illuminant = cie_d50;
      memcpy(xyz_to_rgb, xyz_to_prophoto_rgb, sizeof(double) * 9);
      memcpy(rgb_to_xyz, prophoto_rgb_to_xyz, sizeof(double) * 9);
      break;

    case ACES2065_1:
      illuminant = cie_d60;
      memcpy(xyz_to_rgb, xyz_to_aces2065_1, sizeof(double) * 9);
      memcpy(rgb_to_xyz, aces2065_1_to_xyz, sizeof(double) * 9);
      break;

    case ACES_AP1:
      illuminant = cie_d60;
      memcpy(xyz_to_rgb, xyz_to_aces_ap1, sizeof(double) * 9);
      memcpy(rgb_to_xyz, aces_ap1_to_xyz, sizeof(double) * 9);
      break;

    case REC2020:
      illuminant = cie_d65;
      memcpy(xyz_to_rgb, xyz_to_rec2020, sizeof(double) * 9);
      memcpy(rgb_to_xyz, rec2020_to_xyz, sizeof(double) * 9);
      break;
  }
#endif
for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    double h, lambda, weight;
    // if(!use_bad_cmf) {
      h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (CIE_FINE_SAMPLES - 1.0);

      lambda = CIE_LAMBDA_MIN + i * h;
      weight = 3.0 / 8.0 * h;
      if (i == 0 || i == CIE_FINE_SAMPLES - 1)
        ;
      else if ((i - 1) % 3 == 2)
        weight *= 2.f;
      else
        weight *= 3.f;
#if 0
    }
    else
    {
      h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (double)CIE_FINE_SAMPLES;
      lambda = CIE_LAMBDA_MIN + (i+0.5) * h;
      weight = h;
    }
#endif
    double xyz[3] = {
      cie_interp(cie_x, lambda),
      cie_interp(cie_y, lambda),
      cie_interp(cie_z, lambda) };
    const double I = cie_interp(illuminant, lambda);

#if 0 // output table for shader code
    double out[3] = {0.0};
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j)
        out[k] += xyz_to_rgb[k][j] * xyz[j];
    fprintf(stderr, "vec3(%g, %g, %g), // %g nm\n", out[0], out[1], out[2], lambda);
#endif
    lambda_tbl[i] = lambda;
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j)
        rgb_tbl[k][i] += xyz_to_rgb[k][j] * xyz[j] * I * weight;

    for (int k = 0; k < 3; ++k)
      xyz_whitepoint[k] += xyz[k] * I * weight;
  }
}


// loss function: the three primaries should be met, and their sum should be as white as illuminant E.
void eval_residual(
    const double *coeff,   // 3x 3 sigmoid spectrum coefficients
    const double *rgb,     // 3x rgb target values for the primaries
    double       *residual)// output 3x rgb residuals + 1 "deviation of sum from illuminant E" channel, i.e. 10 elements.
{
  double out[10] = { 0.0 };
  residual[9] = 1.0/600.0;

  for(int c=0;c<3;c++) for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    // the optimiser doesn't like nanometers.
    // we'll do the normalised lambda thing and later convert when we write out.
    double lambda;
    // if(use_bad_cmf) lambda = (i+.5)/(double)CIE_FINE_SAMPLES;
    // else            
    lambda = i/(double)CIE_FINE_SAMPLES;
    double cf[3] = {coeff[3*c+0], coeff[3*c+1], coeff[3*c+2]};

    /* Polynomial */
    double x = 0.0;
    for (int i = 0; i < 3; ++i)
      x = x * lambda + cf[i];

    /* Sigmoid */
    double s = coeff[9] * sigmoid(x);

    /* Integrate against precomputed curves */
    for (int j = 0; j < 3; ++j)
      out[3*c+j] += rgb_tbl[j][i] * s;
    // sum of all our sigmoid spectra should be close to constant 1 (illuminant E)
    residual[9] -= s/600.0;
  }

  for(int c=0;c<3;c++) for (int j=0;j<3;j++)
    residual[3*c+j] = rgb[3*c+j] - out[3*c+j];
}

void eval_jacobian(
    const double *coeffs,  // [9]
    const double *rgb,     // [9]
    double **jac)          // [10][10]
{
  double r0[10], r1[10], tmp[10];

  for (int i = 0; i < 10; ++i)
  {
    memcpy(tmp, coeffs, sizeof(double) * 10);
    tmp[i] -= RGB2SPEC_EPSILON;
    eval_residual(tmp, rgb, r0);

    memcpy(tmp, coeffs, sizeof(double) * 10);
    tmp[i] += RGB2SPEC_EPSILON;
    eval_residual(tmp, rgb, r1);

    for(int j=0;j<10;j++) assert(r1[j] == r1[j]);
    for(int j=0;j<10;j++) assert(r0[j] == r0[j]);

    for (int j = 0; j < 10; ++j)
      jac[j][i] = (r1[j] - r0[j]) * 1.0 / (2 * RGB2SPEC_EPSILON);
  }
  // for (int j = 0; j < 10; ++j) jac[j][9] = 0.0;
  // jac[9][9] = 1.0;//??
}

double gauss_newton(const double rgb[9], double coeffs[10])
{
  int it = 400;//15;
  double r = 0;
  for (int i = 0; i < it; ++i)
  {
    double J0[10*10], *J[10];
    for(int k=0;k<10;k++) J[k] = J0+10*k;

    double residual[10];

    clamp_coeffs(coeffs);
    eval_residual(coeffs, rgb, residual);
    eval_jacobian(coeffs, rgb, J);

    int P[11];
    int rv = LUPDecompose(J, 10, 1e-15, P);
    if (rv != 1) {
      fprintf(stdout, "RGB %g %g %g -> %g %g %g\n", rgb[0], rgb[1], rgb[2], coeffs[0], coeffs[1], coeffs[2]);
      fprintf(stdout, "row: coeffs vs. columns: 3x rgb + w\n");
      for(int j=0;j<10;j++)
      {
        for(int k=0;k<10;k++) fprintf(stdout, "%g ", J0[j*10+k]);
        fprintf(stdout, "\n");
      }
      return 666.0;
    }

    double x[10];
    LUPSolve(J, P, residual, 10, x);

    r = 0.0;
    for (int j = 0; j < 10; ++j) {
      coeffs[j] -= x[j];
      r += residual[j] * residual[j];
    }
    fprintf(stdout, "residual %d %g\n", i, r);

    if (r < 1e-6)
      break;
  }
  return sqrt(r);
}

int main(int argc, char **argv)
{
  init_tables();
  double rgbm[] = {
    1, 0, 0,
    0, 1, 0,
    0, 0, 1};
  double coeffs[] = {
    0, 1, 0,
    0, 1, 0,
    0, 1, 0,
    1,
  };
  double resid = gauss_newton(rgbm, coeffs);
  fprintf(stdout, "coeffs %g %g %g -- %g %g %g -- %g %g %g\n",
      coeffs[0], coeffs[1], coeffs[2],
      coeffs[3], coeffs[4], coeffs[5],
      coeffs[6], coeffs[7], coeffs[8]);
  fprintf(stdout, "scale %g\n", coeffs[9]);
  for(int i=0;i<=300;i++)
  {
    double lambda = i / 300.0;
    fprintf(stderr, "%g, ", i+400.0);
    for(int c=0;c<3;c++)
    {
      double x = 0.0;
      for (int i = 0; i < 3; ++i)
        x = x * lambda + coeffs[3*c+i];
      double s = sigmoid(x);
      fprintf(stderr, "%g, ", s);
    }
    fprintf(stderr, "\n");
  }
}
