// gcc main.c -o main -lm && ./main 2>dreggn
// plot 'dreggn' u 1:2 w l lw 5, '' u 1:3 w l lw 5, '' u 1:4 w l lw 5, '' u 1:($2+$3+$4) w l lw 5
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../../../../core/solve.h"
#include "../../matrices.h"

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

  illuminant = cie_d65;
  double M0[] = matrix_xyz_to_rec709;
  double M1[] = matrix_rec709_to_xyz;
  memcpy(xyz_to_rgb, M0, sizeof(double) * 9);
  memcpy(rgb_to_xyz, M1, sizeof(double) * 9);
  for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    double h, lambda, weight;
      h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (CIE_FINE_SAMPLES - 1.0);

      lambda = CIE_LAMBDA_MIN + i * h;
      weight = 3.0 / 8.0 * h;
      if (i == 0 || i == CIE_FINE_SAMPLES - 1)
        ;
      else if ((i - 1) % 3 == 2)
        weight *= 2.f;
      else
        weight *= 3.f;
    double xyz[3] = {
      cie_interp(cie_x, lambda),
      cie_interp(cie_y, lambda),
      cie_interp(cie_z, lambda) };
    const double I = cie_interp(illuminant, lambda);

    lambda_tbl[i] = lambda;
    for (int k = 0; k < 3; ++k)
      for (int j = 0; j < 3; ++j)
        rgb_tbl[k][i] += xyz_to_rgb[k][j] * xyz[j] * I * weight;

    for (int k = 0; k < 3; ++k)
      xyz_whitepoint[k] += xyz[k] * I * weight;
  }
}

// loss function: the three primaries should be met, and their sum should be as white as illuminant E.
void eval_f(
    double *coeff,   // 3x 3 sigmoid spectrum coefficients
    double *f,
    int m,           // = 9 coefs
    int n,           // = 1
    void *data)
{
  // n = 1 so we evaluate the loss here only
  double target[] = {
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
  };
  f[0] = 0.0;
  double rgb[9] = {0.0};

  for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    double s[3];
    // the optimiser doesn't like nanometers.
    // we'll do the normalised lambda thing and later convert when we write out.
    double lambda = i/(double)CIE_FINE_SAMPLES;
    for(int c=0;c<3;c++)
    {
      double cf[3] = {coeff[3*c+0], coeff[3*c+1], coeff[3*c+2]};

      /* Polynomial */
      double x = 0.0;
      for (int i = 0; i < 3; ++i)
        x = x * lambda + cf[i];

      /* Sigmoid */
      s[c] = sigmoid(x);

      /* Integrate against precomputed curves */
      for (int j = 0; j < 3; ++j)
        rgb[3*c+j] += rgb_tbl[j][i] * s[c];
    }
    // sum of all our sigmoid spectra should be close to constant 1 (illuminant E)
    f[0] += fabs(1.0-(s[0]+s[1]+s[2]))/100.0;
  }
  for(int c=0;c<3;c++)
    for (int j = 0; j < 3; ++j)
      f[0] += (rgb[3*c+j] - target[3*c+j])*(rgb[3*c+j] - target[3*c+j]);
}

void eval_J(
    double *coeffs,  // [9]
    double *J,
    int m,  // = 9
    int n,  // = 1
    void *data)
{
  double f0[1], f1[1], tmp[9];

  for (int i = 0; i < 9; ++i)
  {
    memcpy(tmp, coeffs, sizeof(double) * 9);
    tmp[i] -= RGB2SPEC_EPSILON;
    eval_f(tmp, f0, m, n, data);

    memcpy(tmp, coeffs, sizeof(double) * 9);
    tmp[i] += RGB2SPEC_EPSILON;
    eval_f(tmp, f1, m, n, data);

    J[i] = (f1[0] - f0[0]) * 1.0 / (2 * RGB2SPEC_EPSILON);
  }
}

int main(int argc, char **argv)
{
  init_tables();
  double target[] = { 0.0 };
  double coeffs[] = {
    10,   5, 0,
   -10,  10, 0,
    10, -10, 0,
  };
  double lb[] = {
    -1000.0, -1000.0, -1000.0,
    -1000.0, -1000.0, -1000.0,
    -1000.0, -1000.0, -1000.0,
  };
  double ub[] = {
    1000.0, 1000.0, 1000.0,
    1000.0, 1000.0, 1000.0,
    1000.0, 1000.0, 1000.0,
  };
  double resid = dt_adam(&eval_f, &eval_J, coeffs, target, 9, 1, lb, ub, 100000, 0, 1e-8, 0.9, 0.999, 0.2, 0);
  fprintf(stdout, "coeffs %g %g %g -- %g %g %g -- %g %g %g\n",
      coeffs[0], coeffs[1], coeffs[2],
      coeffs[3], coeffs[4], coeffs[5],
      coeffs[6], coeffs[7], coeffs[8]);
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
