// this uses the coefficient cube optimiser from the paper:
//
// Wenzel Jakob and Johannes Hanika. A low-dimensional function space for
// efficient spectral upsampling. Computer Graphics Forum (Proceedings of
// Eurographics), 38(2), March 2019. 

// creates spectra.lut for upsampling from XYZ such that 1/3 1/3 is equal energy
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "shared/lu.h"
#include "shared/matrices.h"
#include "core/clip.h"
#include "core/inpaint.h"
#include "core/threads.h"
#include "shared/q2t.h"
#include "core/half.h"
#include "core/core.h"
#include "shared/xrand.h"

int use_bad_cmf = 0;
// for fast gpu conversion, we use the abbridged versions of the cie functions:
#define BAD_SAMPLES 30
#define BAD_FINE_SAMPLES 30
#define BAD_LAMBDA_MIN 400.0
#define BAD_LAMBDA_MAX 700.0
// discretisation of quadrature scheme
#define CIE_SAMPLES 95
#define CIE_LAMBDA_MIN 360.0
#define CIE_LAMBDA_MAX 830.0
#define CIE_FINE_SAMPLES ((CIE_SAMPLES - 1) * 3 + 1)
#define RGB2SPEC_EPSILON 1e-4
#define MOM_EPS 1e-3

#include "shared/cie1931.h"


/// Precomputed tables for fast spectral -> RGB conversion
double lambda_tbl[CIE_FINE_SAMPLES],
       rgb_tbl[3][CIE_FINE_SAMPLES],
       rgb_to_xyz[3][3],
       xyz_to_rgb[3][3],
       xyz_whitepoint[3];

/// Currently supported gamuts
typedef enum Gamut {
  SRGB,
  ProPhotoRGB,
  ACES2065_1,
  ACES_AP1,
  REC2020,
  ERGB,
  XYZ,
  XYZ_D65,
} Gamut;

double sigmoid(double x) {
  return 0.5 * x / sqrt(1.0 + x * x) + 0.5;
}

double sqrd(double x) { return x * x; }

void quantise_coeffs(double coeffs[3], float out[3])
{
  // account for normalising lambda:
  double c0 = CIE_LAMBDA_MIN, c1 = 1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);
  double A = coeffs[0], B = coeffs[1], C = coeffs[2];

  const double A2 = (A*(sqrd(c1)));
  const double B2 = (B*c1 - 2*A*c0*(sqrd(c1)));
  const double C2 = (C - B*c0*c1 + A*(sqrd(c0*c1)));
  out[0] = (float)A2;
  out[1] = (float)B2;
  out[2] = (float)C2;
}

void init_coeffs(double coeffs[3])
{
  coeffs[0] = 0.0;
  coeffs[1] = 1.0;
  coeffs[2] = 0.0;
}

void clamp_coeffs(double coeffs[3])
{
  double max = fmax(fmax(fabs(coeffs[0]), fabs(coeffs[1])), fabs(coeffs[2]));
  if (max > 1000) {
    for (int j = 0; j < 3; ++j)
      coeffs[j] *= 1000 / max;
  }
}

int check_gamut(double rgb[3])
{
  double xyz[3] = {0.0};
  for(int j=0;j<3;j++)
    for(int i=0;i<3;i++)
      xyz[i] += rgb_to_xyz[i][j] * rgb[j];
  double x = xyz[0] / (xyz[0] + xyz[1] + xyz[2]);
  double y = xyz[1] / (xyz[0] + xyz[1] + xyz[2]);
  return dt_spectrum_outside(x, y);
}

void init_tables(Gamut gamut)
{
  memset(rgb_tbl, 0, sizeof(rgb_tbl));
  memset(xyz_whitepoint, 0, sizeof(xyz_whitepoint));

  const double *illuminant = 0;

  switch (gamut) {
    case SRGB:
      illuminant = cie_d65;
      memcpy(xyz_to_rgb, xyz_to_srgb, sizeof(double) * 9);
      memcpy(rgb_to_xyz, srgb_to_xyz, sizeof(double) * 9);
      break;

    case ERGB:
      illuminant = cie_e;
      memcpy(xyz_to_rgb, xyz_to_ergb, sizeof(double) * 9);
      memcpy(rgb_to_xyz, ergb_to_xyz, sizeof(double) * 9);
      break;

    case XYZ_D65:
      illuminant = cie_d65; // for hue constancy table with d65 white
      memcpy(xyz_to_rgb, xyz_to_xyz, sizeof(double) * 9);
      memcpy(rgb_to_xyz, xyz_to_xyz, sizeof(double) * 9);
      break;

    case XYZ:
      illuminant = cie_e;
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

  for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    double h, lambda, weight;
    if(!use_bad_cmf)
    {
      h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (CIE_FINE_SAMPLES - 1.0);

      lambda = CIE_LAMBDA_MIN + i * h;
      weight = 3.0 / 8.0 * h;
      if (i == 0 || i == CIE_FINE_SAMPLES - 1)
        ;
      else if ((i - 1) % 3 == 2)
        weight *= 2.f;
      else
        weight *= 3.f;
    }
    else
    {
      h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (double)CIE_FINE_SAMPLES;
      lambda = CIE_LAMBDA_MIN + (i+0.5) * h;
      weight = h;
    }
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

void eval_residual(const double *coeff, const double *rgb, double *residual)
{
  double out[3] = { 0.0, 0.0, 0.0 };

  for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    // the optimiser doesn't like nanometers.
    // we'll do the normalised lambda thing and later convert when we write out.
    double lambda;
    if(use_bad_cmf) lambda = (i+.5)/(double)CIE_FINE_SAMPLES;
    else            lambda = i/(double)CIE_FINE_SAMPLES;
    double cf[3] = {coeff[0], coeff[1], coeff[2]};

    /* Polynomial */
    double x = 0.0;
    for (int i = 0; i < 3; ++i)
      x = x * lambda + cf[i];

    /* Sigmoid */
    double s = sigmoid(x);

    /* Integrate against precomputed curves */
    for (int j = 0; j < 3; ++j)
      out[j] += rgb_tbl[j][i] * s;
  }
  memcpy(residual, rgb, sizeof(double) * 3);

  for (int j = 0; j < 3; ++j)
    residual[j] -= out[j];
}

void eval_jacobian(const double *coeffs, const double *rgb, double **jac)
{
  double r0[3], r1[3], tmp[3];

  for (int i = 0; i < 3; ++i) {
    memcpy(tmp, coeffs, sizeof(double) * 3);
    tmp[i] -= RGB2SPEC_EPSILON;
    eval_residual(tmp, rgb, r0);

    memcpy(tmp, coeffs, sizeof(double) * 3);
    tmp[i] += RGB2SPEC_EPSILON;
    eval_residual(tmp, rgb, r1);

    for(int j=0;j<3;j++) assert(r1[j] == r1[j]);
    for(int j=0;j<3;j++) assert(r0[j] == r0[j]);

    for (int j = 0; j < 3; ++j)
      jac[j][i] = (r1[j] - r0[j]) * 1.0 / (2 * RGB2SPEC_EPSILON);
  }
}

double gauss_newton(const double rgb[3], double coeffs[3])
{
  int it = 40;//15;
  double r = 0;
  for (int i = 0; i < it; ++i) {
    double J0[3], J1[3], J2[3], *J[3] = { J0, J1, J2 };

    double residual[3];

    clamp_coeffs(coeffs);
    eval_residual(coeffs, rgb, residual);
    eval_jacobian(coeffs, rgb, J);

    int P[4];
    int rv = LUPDecompose(J, 3, 1e-15, P);
    if (rv != 1) {
      fprintf(stdout, "RGB %g %g %g -> %g %g %g\n", rgb[0], rgb[1], rgb[2], coeffs[0], coeffs[1], coeffs[2]);
      fprintf(stdout, "J0 %g %g %g\n", J0[0], J0[1], J0[2]);
      fprintf(stdout, "J1 %g %g %g\n", J1[0], J1[1], J1[2]);
      fprintf(stdout, "J2 %g %g %g\n", J2[0], J2[1], J2[2]);
      return 666.0;
    }

    double x[3];
    LUPSolve(J, P, residual, 3, x);

    r = 0.0;
    for (int j = 0; j < 3; ++j) {
      coeffs[j] -= x[j];
      r += residual[j] * residual[j];
    }

    if (r < 1e-6)
      break;
  }
  return sqrt(r);
}

// this is the piecewise fit
double max_dist(double theta)
{
  double x = theta <= 1.7355 ? theta + 2*M_PI : theta;
  if(x > 1.7355)
    return 0.230142/(cos(x-0.0236733-8.96303)+((-4.61744e-06)/(-1.73848+x)));
  if(x > 3.604)
    return 0.0583482*x/(-0.0635246+cos(cos(-0.0382566+x)+0.0321751));
  if(x > 5.805)
    return 0.699391/(sin((2.00266-(-0.0155152*cos(-2.00539*x)))*x)+1.99541);
  else return 0.0; // XXX
}

typedef struct parallel_shared_t
{
  float *out, *max_b;
  float *lsbuf;
  int max_w, max_h, res;
}
parallel_shared_t;

void parallel_run(uint32_t item, void *data)
{
  const parallel_shared_t *const d = data;
  const int j = item / d->res;
  const int i = item - j * d->res;
  if(i == 0)
  {
    printf(".");
    fflush(stdout);
  }

  double x = (i) / (double)d->res;
  double y = (j) / (double)d->res;
  quad2tri(&x, &y);
  double rgb[3];
  double coeffs[3];
  init_coeffs(coeffs);
  rgb[0] = x;
  rgb[1] = y;
  rgb[2] = 1.0-x-y;
  if(check_gamut(rgb)) return;

  int ii = (int)fmin(d->max_w - 1, fmax(0, x * d->max_w + 0.5));
  int jj = (int)fmin(d->max_h - 1, fmax(0, y * d->max_h + 0.5));
  double m = fmax(0.05, 0.5*d->max_b[ii + d->max_w * jj]);
  double rgbm[3] = {rgb[0] * m, rgb[1] * m, rgb[2] * m};
  double resid = gauss_newton(rgbm, coeffs);

  (void)resid;
  int idx = j*d->res + i;
  d->out[4*idx + 0] = coeffs[0];
  d->out[4*idx + 1] = coeffs[1];
  d->out[4*idx + 2] = coeffs[2];
  d->out[4*idx + 3] = m;
}


int main(int argc, char **argv)
{
  Gamut gamut = XYZ;
  init_tables(gamut);

  const int res = 512;//atoi(argv[1]); // resolution of 2d lut

  typedef struct header_t
  {
    uint32_t magic;
    uint16_t version;
    uint8_t  channels;
    uint8_t  datatype;
    uint32_t wd;
    uint32_t ht;
  }
  header_t;

  // read max macadam brightness lut
  int max_w, max_h;
  float *max_b = 0;
  {
    FILE *f = fopen("macadam.lut", "rb");
    header_t header;
    if(!f) goto mac_error;
    if(fread(&header, sizeof(header_t), 1, f) != 1) goto mac_error;
    max_w = header.wd;
    max_h = header.ht;
    if(header.channels != 1) goto mac_error;
    if(header.version != 2) goto mac_error;
    max_b = calloc(sizeof(float), max_w*(uint64_t)max_h);
    uint16_t *half = calloc(sizeof(float), max_w*(uint64_t)max_h);
    fread(half, header.wd*(uint64_t)header.ht, sizeof(uint16_t), f);
    fclose(f);
    for(uint64_t k=0;k<header.wd*(uint64_t)header.ht;k++)
      max_b[k] = half_to_float(half[k]);
    free(half);
    if(0)
    {
mac_error:
      if(f) fclose(f);
      fprintf(stderr, "could not read macadam.lut!\n");
      exit(2);
    }
  }

  printf("optimising ");

  size_t bufsize = 4*res*res;
  float *out = calloc(sizeof(float), bufsize);

  parallel_shared_t par = (parallel_shared_t) {
    .out   = out,
    .max_b = max_b,
    .max_w = max_w,
    .max_h = max_h,
    .res   = res,
  };

  // threads_global_init();
#if 0 // note that with proper atomics this is slower than single thread:
  const int nt = threads_num();
  int taskid = -1;
  for(int i=0;i<nt;i++)
    taskid = threads_task("mkabney", res*res, taskid, &par, parallel_run, 0);
  threads_wait(taskid);
#else
  for(int k=0;k<res*res;k++)
    parallel_run(k, &par);
#endif

  { // write spectra map: (x,y) |--> sigmoid coeffs + saturation
    header_t head = (header_t) {
      .magic    = 1234,
      .version  = 2,
      .channels = 4,
      .datatype = 1,  // 32-bit float
      .wd       = res,
      .ht       = res,
    };
    // FILE *pfm = fopen(argv[2], "wb"); // also write pfm for debugging purposes
    // if(pfm) fprintf(pfm, "PF\n%d %d\n-1.0\n", res, res);
    FILE *f = fopen("spectra.lut", "wb");
    if(f) fwrite(&head, sizeof(head), 1, f);
    for(int k=0;k<res*res;k++)
    {
      double coeffs[4] = {out[4*k+0], out[4*k+1], out[4*k+2], out[4*k+3]};
      float q[4] = {0,0,0, coeffs[3]};
      quantise_coeffs(coeffs, q);
      if(f)   fwrite(q, sizeof(float), 4, f);
      // if(pfm) fwrite(q, sizeof(float), 3, pfm);
    }
    fprintf(f,
        "spectral upsampling table for Jakob 2019 style sigmoid spectra.\n"
        "lookup using triangle-to-quad xy chromaticities.\n"
        "this is meant to be used for emissions, i.e. querying at the equal energy white\n"
        "coordinates (1/3,1/3) will yield the equal energy spectrum.\n"
        "the fourth channel is the brightness b=X+Y+Z of the corresponding colour coordinate.\n"
        "this is not 1.0/constant because the bounded spectra are subject to MacAdams limit.");
    if(f) fclose(f);
    // if(pfm) fclose(pfm);
  }
  free(out);
  free(max_b);
  // threads_global_cleanup();
  printf("\n");
}
