// this uses the coefficient cube optimiser from the paper:
//
// Wenzel Jakob and Johannes Hanika. A low-dimensional function space for
// efficient spectral upsampling. Computer Graphics Forum (Proceedings of
// Eurographics), 38(2), March 2019. 

// creates spectra.lut (c0*1e5 y l s)/(x y) and abney.lut (x y)/(s l)
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
} Gamut;

double sigmoid(double x) {
  return 0.5 * x / sqrt(1.0 + x * x) + 0.5;
}

double sqrd(double x) { return x * x; }

void cvt_c0yl_c012(const double *c0yl, double *coeffs)
{
  coeffs[0] = c0yl[0];
  coeffs[1] = c0yl[2] * -2.0 * c0yl[0];
  coeffs[2] = c0yl[1] + c0yl[0] * c0yl[2] * c0yl[2];
}

void cvt_c012_c0yl(const double *coeffs, double *c0yl)
{
  // account for normalising lambda:
  double c0 = CIE_LAMBDA_MIN, c1 = 1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);
  double A = coeffs[0], B = coeffs[1], C = coeffs[2];

  double A2 = (double)(A*(sqrd(c1)));
  double B2 = (double)(B*c1 - 2.0*A*c0*(sqrd(c1)));
  double C2 = (double)(C - B*c0*c1 + A*(sqrd(c0*c1)));

  if(fabs(A2) < 1e-12)
  {
    c0yl[0] = c0yl[1] = c0yl[2] = 0.0;
    return;
  }
  // convert to c0 y dom-lambda:
  c0yl[0] = A2;                           // square slope stays
  c0yl[2] = B2 / (-2.0*A2);               // dominant wavelength
  c0yl[1] = C2 - B2*B2 / (4.0 * A2);      // y
}

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
#if 0 // DEBUG vis
  if(fabs(A2) < 1e-12)
  {
    out[0] = out[1] = out[2] = 0.0;
    return;
  }
  // convert to c0 y dom-lambda:
  out[0] = A2;                           // square slope stays
  out[1] = C2 - B2*B2 / (4.0 * A2);      // y
  out[2] = B2 / (-2.0*A2);               // dominant wavelength
  out[2] = (out[2] - c0)*c1;             // normalise to [0,1] range for vis
#endif
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
#if 0
  // clamp dom lambda to visible range:
  // this will cause the fitter to diverge on the ridge.
  double c0yl[3];
  c0yl[0] = coeffs[0];
  if(fabs(coeffs[0]) < 1e-12) return;

  c0yl[2] = coeffs[1] / (-2.0*coeffs[0]);
  c0yl[1] = coeffs[2] - coeffs[1]*coeffs[1] / (4.0 * coeffs[0]);

  c0yl[2] = CLAMP(c0yl[2], 0.0, 1.0);

  coeffs[0] = c0yl[0];
  coeffs[1] = c0yl[2] * -2.0 * c0yl[0];
  coeffs[2] = c0yl[1] + c0yl[0] * c0yl[2] * c0yl[2];
#endif
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
  float *out, *lsbuf, *max_b;
  int max_w, max_h, res;
}
parallel_shared_t;

void parallel_run(uint32_t item, void *data)
{
  const parallel_shared_t *const d = data;
  const int j = item / d->res;
  const int i = item - j * d->res;
  const int lsres = d->res;
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
  // normalise to max(rgb)=1
  rgb[0] = x;
  rgb[1] = y;
  rgb[2] = 1.0-x-y;
  if(check_gamut(rgb)) return;

  int ii = (int)fmin(d->max_w - 1, fmax(0, x * d->max_w + 0.5));
  int jj = (int)fmin(d->max_h - 1, fmax(0, y * d->max_h + 0.5));
  double m = fmax(0.001, 0.5*d->max_b[ii + d->max_w * jj]);
  double rgbm[3] = {rgb[0] * m, rgb[1] * m, rgb[2] * m};
  double resid = gauss_newton(rgbm, coeffs);

  double c0yl[3];
  cvt_c012_c0yl(coeffs, c0yl);

  (void)resid;
  int idx = j*d->res + i;
  d->out[5*idx + 0] = coeffs[0];
  d->out[5*idx + 1] = coeffs[1];
  d->out[5*idx + 2] = coeffs[2];

  float white[2] = {.3127266, .32902313}; // D65
  // something circular (should be elliptical, says munsell) but smooth:
  float sat = pow(3.0 * ((x-white[0])*(x-white[0])+(y-white[1])*(y-white[1])), 0.25);

  // bin into lambda/saturation buffer
  float satc = (lsres-2) * sat; // keep two columns for gamut limits
  // normalise to extended range:
  float norm = (c0yl[2] - 400.0)/(700.0-400.0);
  // float lamc = 1.0/(1.0+exp(-2.0*(2.0*norm-1.0))) * lsres / 2; // center deriv=1
  // float fx = norm*norm*norm+norm;
  float fx = norm-0.5;
  // fx = fx*fx*fx+fx; // worse
  float lamc = (0.5 + 0.5 * fx / sqrt(fx*fx+0.25)) * lsres / 2;
  int lami = fmaxf(0, fminf(lsres/2-1, lamc));
  int sati = satc;
  if(c0yl[0] > 0) lami += lsres/2;
  lami = fmaxf(0, fminf(lsres-1, lami));
  sati = fmaxf(0, fminf(lsres-3, sati));
  float olamc = d->lsbuf[5*(lami*lsres + sati)+3];
  float osatc = d->lsbuf[5*(lami*lsres + sati)+4];
  float odist = 
    (olamc - lami - 0.5f)*(olamc - lami - 0.5f)+
    (osatc - sati - 0.5f)*(osatc - sati - 0.5f);
  float  dist = 
    ( lamc - lami - 0.5f)*( lamc - lami - 0.5f)+
    ( satc - sati - 0.5f)*( satc - sati - 0.5f);
  if(dist < odist)
  {
    d->lsbuf[5*(lami*lsres + sati)+0] = x;
    d->lsbuf[5*(lami*lsres + sati)+1] = y;
    d->lsbuf[5*(lami*lsres + sati)+2] = 1.0-x-y;
    d->lsbuf[5*(lami*lsres + sati)+3] = lamc;
    d->lsbuf[5*(lami*lsres + sati)+4] = satc;
  }
  d->out[5*idx + 3] = (lami+0.5f) / (float)lsres;
  d->out[5*idx + 4] = (sati+0.5f) / (float)lsres;
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
    max_b = calloc(sizeof(float), max_w*max_h);
    uint16_t *half = calloc(sizeof(float), max_w*max_h);
    fread(half, header.wd*header.ht, sizeof(uint16_t), f);
    fclose(f);
    for(int k=0;k<header.wd*header.ht;k++)
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

#if 0
  const int N = 50;
  for(int i=0;i<N;i++)
  {
    double theta = i/(double)N*2.0*M_PI;//xrand()*2.0*M_PI;
    double sat = 0.75;//xrand();
    double dist = max_dist(theta) * sat;
    double x = cos(theta) * dist;
    double y = sin(theta) * dist;

    double rgb[3];
    double coeffs[3];
    init_coeffs(coeffs);
    // normalise to max(rgb)=1
    rgb[0] = x;
    rgb[1] = y;
    rgb[2] = 1.0-x-y;
    // if(check_gamut(rgb)) continue;

    int ii = (int)fmin(max_w - 1, fmax(0, x * max_w + 0.5));
    int jj = (int)fmin(max_h - 1, fmax(0, y * max_h + 0.5));
    double m = fmax(0.001, 0.5*max_b[ii + max_w * jj]);
    double rgbm[3] = {rgb[0] * m, rgb[1] * m, rgb[2] * m};
    double resid = gauss_newton(rgbm, coeffs);

    double c0yl[3];
    cvt_c012_c0yl(coeffs, c0yl);
    fprintf(stdout, "%g %g %g\n", theta, sat, c0yl[2]);
    // turingbot says:
    // 485.798+((7.18167-(3.77891*fmod(-0.102169+col2,col1)/(-1.02793+col1)))*log(col2)-(cos((-1.81813)*(0.516929+col1))+((round(col2)+3.48652)*(-0.0856938+col2)*tan(col1+0.256979))))
    //
    // for sat == 0.5 it finds:
    // for random theta:
    //  479.698+(1.69192*log2((abs(1.07426-tan(((-0.00382351)/tan(1.00703*col1-0.967258))+(1.01348*(-0.0135525+col1))))+0.16693)*erf(0.907124+col1))-(0.0326317+tan(0.232961+col1)))
    // for uniform theta:
    // 488.9+(5.4483*asinh((((-0.131495)/abs(tan(-0.977603+col1)))+tan(-3.4558-(0.000227752+col1))-2.92341)/1.58603)+(asinh(-2.51662*tan(3.9132+col1))/1.46824))
    //
    //sat == 0.75
    // (abs(tan(-1.034-col1+col2)+4.23188)/0.301905)-asinh(0.168976*(tan(0.99956+col1)+tan(-0.660586-col1)+cos(-1.94464*col1)+tan(-0.019656+col1)))+469.578
    // TODO: plot theta vs hue angle or wavelength vs wavelength!
    // TODO: this way we won't see all the tan trying to compensate

    // (18.86-((tan(0.14787+1.02656*col1)/0.324046)+1.2005*atanh(cos(2.30529*(-0.726312+col1)))-((4.31693+(col1-cos(2.08053*col1)))*(1.13487-col2))))*(erf(col2)-(0.00470529/(-0.051686+col2)))+468.909
  }
  exit(0);
#endif
#if 0
  // go through dt_spectrum_clip points as start and know the wavelength that
  // goes with it.
  // wedge saturation from that to zero (i.e. linearly blend with E)
  // then find lambda 
  const float E[] = {1./3., 1./3.};
  const int p_cnt = sizeof(dt_spectrum_clip)/2/sizeof(dt_spectrum_clip[0]);
  for(int i=0;i<p_cnt;i++)
  {
    double lambda = 420.0 + 5.0*i;
    fprintf(stdout, "%g ", lambda);
    int ns = 20;
    for(int s=0;s<ns;s++)
    {
      // TODO: also do it for purple line/u-shapes!
      double sat = s/(ns-1.0);
      double x = sat * dt_spectrum_clip[2*i+0] + (1.0-sat)*E[0];
      double y = sat * dt_spectrum_clip[2*i+1] + (1.0-sat)*E[1];

      double rgb[3];
      double coeffs[3];
      init_coeffs(coeffs);
      // normalise to max(rgb)=1
      rgb[0] = x;
      rgb[1] = y;
      rgb[2] = 1.0-x-y;
      // if(check_gamut(rgb)) continue;

      int ii = (int)fmin(max_w - 1, fmax(0, x * max_w + 0.5));
      int jj = (int)fmin(max_h - 1, fmax(0, y * max_h + 0.5));
      double m = fmax(0.001, 0.5*max_b[ii + max_w * jj]);
      double rgbm[3] = {rgb[0] * m, rgb[1] * m, rgb[2] * m};
      double resid = gauss_newton(rgbm, coeffs);

      double c0yl[3];
      cvt_c012_c0yl(coeffs, c0yl);
      fprintf(stdout, "%g ", c0yl[2]);
    }
    fprintf(stdout, "\n");
  }
  exit(0);
#endif

  printf("optimising ");

  int lsres = res; // /4;
  float *lsbuf = calloc(sizeof(float), 5*lsres*lsres);

  size_t bufsize = 5*res*res;
  float *out = calloc(sizeof(float), bufsize);

  parallel_shared_t par = (parallel_shared_t) {
    .out   = out,
    .lsbuf = lsbuf,
    .max_b = max_b,
    .max_w = max_w,
    .max_h = max_h,
    .res   = res,
  };

  threads_global_init();
#if 0
  const int nt = threads_num();
  int taskid = -1;
  for(int i=0;i<nt;i++)
    taskid = threads_task(res*res, taskid, &par, parallel_run, 0);
  threads_wait(taskid);
#else
  for(int k=0;k<res*res;k++)
    parallel_run(k, &par);
#endif

  { // scope write abney map on (lambda, saturation)
    dt_inpaint_buf_t inpaint_buf = {
      .dat = lsbuf,
      .wd  = lsres,
      .ht  = lsres,
      .cpp = 5,
    };
    dt_inpaint(&inpaint_buf);

    // determine gamut boundaries for rec709, rec2020, and spectral:
    // walk each row and find first time it goes outside.
    // record this in special 1d tables
    float *bound_rec709   = calloc(sizeof(float), lsres);
    float *bound_rec2020  = calloc(sizeof(float), lsres);
    float *bound_spectral = calloc(sizeof(float), lsres);
    for(int j=0;j<lsres;j++)
    {
      bound_rec2020[j] = (lsres-3.5)/lsres; // init to max because it touches and then is never inited below
      int active = 7;
      for(int i=0;i<lsres-2;i++)
      {
        int idx = j*lsres + i;
        double xyz[] = {lsbuf[5*idx], lsbuf[5*idx+1], 1.0-lsbuf[5*idx]-lsbuf[5*idx+1]};
        double rec709 [3] = {0.0};
        double rec2020[3] = {0.0};
        const float xy[] = {xyz[0], xyz[1]};
        const float d65[] = {.3127266, .32902313};
        for (int k = 0; k < 3; ++k)
          for (int l = 0; l < 3; ++l)
            rec709[k] += xyz_to_srgb[k][l] * xyz[l];
        for (int k = 0; k < 3; ++k)
          for (int l = 0; l < 3; ++l)
            rec2020[k] += xyz_to_rec2020[k][l] * xyz[l];
        if((active & 1) && (rec709 [0] < 0 || rec709 [1] < 0 || rec709 [2] < 0))
        {
          bound_rec709[j] = (i-.5f)/(float)lsres;
          active &= ~1;
        }
        if((active & 2) && (rec2020[0] < 0 || rec2020[1] < 0 || rec2020[2] < 0))
        {
          bound_rec2020[j] = (i-.5f)/(float)lsres;
          active &= ~2;
        }
        if((active & 4) && dt_spectrum_saturation(xy, d65) > 0.98)
        {
          bound_spectral[j] = (i-.5f)/(float)lsres;
          active &= ~4;
        }
        if(!active) break;
      }
      // clamp so that everything is smaller than what we think is the spectral locus
      bound_rec2020[j] = fminf(bound_rec2020[j], bound_spectral[j]);
      bound_rec709 [j] = fminf(bound_rec709 [j], bound_spectral[j]);
    }

    // write 2 channel half lut:
    uint32_t size = 2*sizeof(uint16_t)*lsres*lsres;
    uint16_t *b16 = malloc(size);
    // also write pfm for debugging purposes
    FILE *pfm = 0;//fopen("abney.pfm", "wb");
    if(pfm) fprintf(pfm, "PF\n%d %d\n-1.0\n", lsres, lsres);
    for(int j=0;j<lsres;j++)
    {
      for(int i=0;i<lsres-2;i++)
      {
        int ki = j*lsres + i, ko = j*lsres + i;
        b16[2*ko+0] = float_to_half(lsbuf[5*ki+0]);
        b16[2*ko+1] = float_to_half(lsbuf[5*ki+1]);
        float q[] = {lsbuf[5*ki], lsbuf[5*ki+1], 1.0f-lsbuf[5*ki]-lsbuf[5*ki+1]};
        if(pfm) fwrite(q, sizeof(float), 3, pfm);
      }
      b16[2*(j*lsres+lsres-2)+0] = float_to_half(bound_rec709  [j]);
      b16[2*(j*lsres+lsres-2)+1] = float_to_half(bound_spectral[j]);
      b16[2*(j*lsres+lsres-1)+0] = float_to_half(bound_rec2020 [j]);
      b16[2*(j*lsres+lsres-1)+1] = float_to_half(bound_spectral[j]);
      float q[] = {bound_rec709[j], bound_rec2020[j], bound_spectral[j]};
      if(pfm) fwrite(q, sizeof(float), 3, pfm);
      if(pfm) fwrite(q, sizeof(float), 3, pfm);
    }
    header_t head = (header_t) {
      .magic    = 1234,
      .version  = 2,
      .channels = 2,
      .datatype = 0,
      .wd       = lsres,
      .ht       = lsres,
    };
    FILE *f = fopen("abney.lut", "wb");
    if(f)
    {
      fwrite(&head, sizeof(head), 1, f);
      fwrite(b16, size, 1, f);
      fclose(f);
    }
    free(b16);
    free(bound_rec709);
    free(bound_rec2020);
    free(bound_spectral);
    if(pfm) fclose(pfm);
  }

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
      double coeffs[3] = {out[5*k+0], out[5*k+1], out[5*k+2]};
      float q[] = {0, 0, 0, out[5*k+4]}; // c0yl works in half, but doesn't interpolate upon lookup :(
      quantise_coeffs(coeffs, q);
      if(f)   fwrite(q, sizeof(float), 4, f);
      // if(pfm) fwrite(q, sizeof(float), 3, pfm);
    }
    if(f) fclose(f);
    // if(pfm) fclose(pfm);
  }
  free(out);
  threads_global_cleanup();
  printf("\n");
}
