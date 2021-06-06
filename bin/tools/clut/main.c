// #define USE_LEVMAR
#include "inpaint.h"
#include "../../modules/o-pfm/half.h"
#include "q2t.h"
#include "clip.h"
#include "solve.h"
#include <strings.h>
#include "../../modules/i-raw/adobe_coeff.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>
#ifdef USE_LEVMAR
#include "levmar-2.6/levmar.h"
#endif

#define MIN(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })
#define MAX(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
   _a > _b ? _a : _b; })
#define CLAMP(a,m,M) (MIN(MAX((a), (m)), (M)))

#define CIE_SAMPLES 95
#define CIE_LAMBDA_MIN 360.0
#define CIE_LAMBDA_MAX 830.0
#define CIE_FINE_SAMPLES 95

typedef struct header_t
{ // header of .lut files
  uint32_t magic;
  uint16_t version;
  uint8_t  channels;
  uint8_t  datatype;
  uint32_t wd;
  uint32_t ht;
}
header_t;
static int num_coeff = 6;
static uint32_t seed = 1337;
static const double srgb_to_xyz[] = {
  0.412453, 0.357580, 0.180423,
  0.212671, 0.715160, 0.072169,
  0.019334, 0.119193, 0.950227 
};
static const double xyz_to_rec2020[] = {
  1.7166511880, -0.3556707838, -0.2533662814,
 -0.6666843518,  1.6164812366,  0.0157685458,
  0.0176398574, -0.0427706133,  0.9421031212
};
static const double rec2020_to_xyz[] = {
  0.6369580483, 0.1446169036, 0.1688809752,
  0.2627002120, 0.6779980715, 0.0593017165,
  0.0000000000, 0.0280726930, 1.0609850577,
};

static inline double
xrand()
{ // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed / 4294967296.0;
}

// bilinear lookup
static inline void
fetch_coeff(
    const double *xy,       // cie xy chromaticities
    const float  *spectra,  // loaded spectral coeffs, 4-strided
    const int     wd,       // width of texture
    const int     ht,       // height of texture
    double       *out)      // bilinear lookup will end up here
{
  out[0] = out[1] = out[2] = 0.0;
  if(xy[0] < 0 || xy[1] < 0 || xy[0] > 1.0 || xy[1] > 1.0) return;
  double tc[] = {xy[0], xy[1]};
  tri2quad(tc+0, tc+1);
  double xf = tc[0]*wd, yf = tc[1]*ht;
  int x0 = (int)CLAMP(xf,   0, wd-1), y0 = (int)CLAMP(yf,   0, ht-1);
  int x1 = (int)CLAMP(x0+1, 0, wd-1), y1 = (int)CLAMP(y0+1, 0, ht-1);
  int dx = x1 - x0, dy = y1 - y0;
  double u = xf - x0, v = yf - y0;
  const float *c = spectra + 4*(y0*wd + x0);
  out[0] = out[1] = out[2] = 0.0;
  for(int k=0;k<3;k++) out[k] += (1.0-u)*(1.0-v)*c[k];
  for(int k=0;k<3;k++) out[k] += (    u)*(1.0-v)*c[k + 4*dx];
  for(int k=0;k<3;k++) out[k] += (1.0-u)*(    v)*c[k + 4*wd*dy];
  for(int k=0;k<3;k++) out[k] += (    u)*(    v)*c[k + 4*(wd*dy+dx)];
}

// nearest neighbour lookup
static inline void
fetch_coeffi(
    const double *xy,
    const float  *spectra,
    const int     wd,
    const int     ht,
    double       *out)
{
  out[0] = out[1] = out[2] = 0.0;
  if(xy[0] < 0 || xy[1] < 0 || xy[0] > 1.0 || xy[1] > 1.0) return;
  double tc[] = {xy[0], xy[1]};
  tri2quad(tc+0, tc+1);
  int xi = (int)CLAMP(tc[0]*wd+0.5, 0, wd-1), yi = (int)CLAMP(tc[1]*ht+0.5, 0, ht-1);
  const float *c = spectra + 4*(yi*wd + xi);
  out[0] = c[0]; out[1] = c[1]; out[2] = c[2];
}

// sample a cubic b-spline around (0.5, 0.5, 0.5)
static inline void
sample_rgb(double *rgb, double delta, int clamp)
{
#if 0 // just straight simple full cube
  for(int k=0;k<3;k++)
    rgb[k] = xrand();
#else
  for(int k=0;k<3;k++)
  {
    rgb[k] = 0.5;
    for(int i=0;i<3;i++)
      rgb[k] += delta*(2.0*xrand()-1.0);
    if(clamp) rgb[k] = CLAMP(rgb[k], 0.0, 1.0);
  }
#endif
}

static inline double
normalise1(double *col)
{
  const double b = col[0] + col[1] + col[2];
  for(int k=0;k<3;k++) col[k] /= b;
  return b;
}

static inline double
poly(const double *c, double lambda, int num)
{
  double r = 0.0;
  for(int i=0;i<num;i++)
    r = r * lambda + c[i];
  return r;
}

static inline double
sigmoid(double x)
{
  return 0.5 * x / sqrt(1.0 + x * x) + 0.5;
}

static inline double
ddx_sigmoid(double x)
{
  return 0.5 * pow(x*x+1.0, -3.0/2.0);
}

static inline double
eval_ref(
    const double (*cfa_spec)[4],
    const int      channel,
    const int      cnt,
    const double  *cf)
{
  double out = 0.0;
  for(int i=0;i<cnt;i++)
  {
    double lambda = cfa_spec[i][0];
    double s = sigmoid(poly(cf, lambda, 3));
    double t = cfa_spec[i][1+channel];
    out += s*t;
  }
  return out * (cfa_spec[cnt-1][0] - cfa_spec[0][0]) / (double) cnt;
}

static inline double
eval(
    const double *cp, // spectrum with np coeffs, use normalised lambda (for optimiser)
    const double *cf, // spectrum with nf coeffs, use lambda in nanometers
    int np,
    int nf)
{
  double out = 0.0;
  for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    double lambda0 = (i+.5)/(double)CIE_FINE_SAMPLES;
    double lambda1 = lambda0 * (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) + CIE_LAMBDA_MIN;
    double s = sigmoid(poly(cf, lambda1, nf)); // from map, use real lambda
    double t = sigmoid(poly(cp, lambda0, np)); // optimising this, use normalised for smaller values (assumption of hessian approx)
    out += s * t;
  }
  return out * (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (double)CIE_FINE_SAMPLES;
}

static inline double
ddp_eval(
    const double *cp,  // parameters, cfa spectrum
    const double *cf,  // coefficients of data point
    int           np,  // number of parameter coeffs
    int           nf,  // number of data point coeffs
    double       *jac) // output: gradient dx/dp{0..np-1} (i.e. np elements)
{
  double out = 0.0;
  double ddp_poly[20];
  double ddp_t[20];
  for(int j=0;j<np;j++) jac[j] = 0.0;
  for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    double lambda0 = (i+.5)/(double)CIE_FINE_SAMPLES;
    double lambda1 = lambda0 * (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) + CIE_LAMBDA_MIN;

    double x = poly(cf, lambda1, nf); // from map, use real lambda
    double y = poly(cp, lambda0, np); // optimising this, use normalised for smaller values (assumption of hessian approx)
    double s = sigmoid(x);
    double t = sigmoid(y);

    double ddx_sig = ddx_sigmoid(y);
    // ddp t = ddx_sigmoid(poly(cp, lambda0, np)) * { ddp_poly(cp, lambda0, np) }
    // where ddp_poly = (lambda0^{num_coeff-1}, lambda0^{num_coeff-2}, .., lambda0, 1)
    ddp_poly[0] = 1.0;
    ddp_poly[1] = lambda0;
    ddp_poly[2] = lambda0 * lambda0;
    for(int j=3;j<np;j++) ddp_poly[j] = (j&1? ddp_poly[j/2] * ddp_poly[j/2+1] : ddp_poly[j/2]*ddp_poly[j/2]);
    for(int j=0;j<np;j++) ddp_t[j] = ddx_sig * ddp_poly[np-j-1];

    // now out = sum(s * t)
    // ddp out = sum ddp s*t = sum s * ddp t
    out += s * t;
    for(int j=0;j<np;j++) jac[j] += s * ddp_t[j];
  }
  // apply final scale:
  for(int j=0;j<np;j++) jac[j] *= (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (double)CIE_FINE_SAMPLES;

#if 0 // debug by taking finite differences to vanilla eval(): this matches very well
  const double eps = 1e-8;
  double jac2[20] = {0.0};
  for(int j=0;j<np;j++)
  {
    double p[20];
    memcpy(p, cp, sizeof(double)*np);
    p[j] += eps;
    double x2 = eval(p, cf, np, nf);
    memcpy(p, cp, sizeof(double)*np);
    p[j] -= eps;
    double x1 = eval(p, cf, np, nf);
    jac2[j] = (x2-x1)/(2.0*eps);
    // XXX
    // if(fabs(jac[j]) < 1e-4) jac[j] = 0.0;
  }
  for(int j=0;j<np;j++)
    if(fabs((jac[j] - jac2[j])/fmax(1e-3, fmax(fabs(jac[j]),fabs(jac2[j])))) > 2e-2)
      fprintf(stderr, "XXX[%d]  (%g %g) err %g\n", j, jac[j], jac2[j],
    fabs((jac[j] - jac2[j])/fmax(1e-3, fmax(fabs(jac[j]),fabs(jac2[j])))));
  for(int j=0;j<np;j++) jac[j] = jac2[j]; // XXX replace parts with differences!
#endif
  // also return regular value, we need it for the chain rule
  return out * (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (double)CIE_FINE_SAMPLES;
}

static inline double 
mat3_det(const double *const restrict a)
{
#define A(y, x) a[(y - 1) * 3 + (x - 1)]
  return
    A(1, 1) * (A(3, 3) * A(2, 2) - A(3, 2) * A(2, 3)) -
    A(2, 1) * (A(3, 3) * A(1, 2) - A(3, 2) * A(1, 3)) +
    A(3, 1) * (A(2, 3) * A(1, 2) - A(2, 2) * A(1, 3));
}

static inline double
mat3_inv(const double *const restrict a, double *const restrict inv)
{
  const double det = mat3_det(a);
  if(!(det != 0.0)) return 0.0;
  const double invdet = 1.0 / det;

  inv[3*0+0] =  invdet * (A(3, 3) * A(2, 2) - A(3, 2) * A(2, 3));
  inv[3*0+1] = -invdet * (A(3, 3) * A(1, 2) - A(3, 2) * A(1, 3));
  inv[3*0+2] =  invdet * (A(2, 3) * A(1, 2) - A(2, 2) * A(1, 3));

  inv[3*1+0] = -invdet * (A(3, 3) * A(2, 1) - A(3, 1) * A(2, 3));
  inv[3*1+1] =  invdet * (A(3, 3) * A(1, 1) - A(3, 1) * A(1, 3));
  inv[3*1+2] = -invdet * (A(2, 3) * A(1, 1) - A(2, 1) * A(1, 3));

  inv[3*2+0] =  invdet * (A(3, 2) * A(2, 1) - A(3, 1) * A(2, 2));
  inv[3*2+1] = -invdet * (A(3, 2) * A(1, 1) - A(3, 1) * A(1, 2));
  inv[3*2+2] =  invdet * (A(2, 2) * A(1, 1) - A(2, 1) * A(1, 2));
  return det;
#undef A
}

static inline void
mat3_mulv(
    const double *const restrict a,
    const double *const restrict v,
    double *const restrict res)
{
  res[0] = res[1] = res[2] = 0.0f;
  for(int j=0;j<3;j++)
    for(int i=0;i<3;i++)
      res[j] += a[3*j+i] * v[i];
}

// objective function for nelder-mead optimiser:
double nm_objective(
    double *p,
    double *target,
    int     n,
    void   *data)
{
  double *cf = data;
  int num = n/2;
  double resid = 0.0;
  for(int i=0;i<num;i++)
  {
    double x[3] = {
      eval(p+0*num_coeff, cf + 3*i, num_coeff, 3),
      eval(p+1*num_coeff, cf + 3*i, num_coeff, 3),
      eval(p+2*num_coeff, cf + 3*i, num_coeff, 3)};
    normalise1(x);
    for(int k=0;k<2;k++) resid += (x[k]-target[2*i+k])*(x[k]-target[2*i+k]);
  }
  return resid;
}

// jacobian for levmar:
void lm_jacobian(
    double *p,   // parameters: num_coeff * 3 for camera cfa spectra
    double *jac, // output: derivative dx / dp (n x m entries, n-major, i.e. (dx[0]/dp[0], dx[0]/dp[1], ..)
    int     m,   // number of parameters (=num_coeff*3)
    int     n,   // number of data points
    void   *data)
{
  double *cf = data;
  memset(jac, 0, sizeof(jac[0])*m*n);
  double *Je = alloca(3*m*sizeof(double)); // dxdp
  int num = n/2;
  for(int i=0;i<num;i++) // for all rgb data points
  {
    memset(Je, 0, sizeof(double)*3*m);
    // get derivative dx[3*i+{0,1,2}] / dp[3 * num_coeff]
    double rgb[3];
    rgb[0] = ddp_eval(p+0*num_coeff, cf + 3*i, num_coeff, 3, Je + 0*m);
    rgb[1] = ddp_eval(p+1*num_coeff, cf + 3*i, num_coeff, 3, Je + 1*m+1*num_coeff);
    rgb[2] = ddp_eval(p+2*num_coeff, cf + 3*i, num_coeff, 3, Je + 2*m+2*num_coeff);
    double b = rgb[0]+rgb[1]+rgb[2];
    double ib = 1.0/(b*b);
    // account for normalise1:
    // we got n = rgb/sum(rgb), so
    // ddr r/(r+g+b)  => ddx x/(x+c) =   c / (x+c)^2
    // ddr g/(r+g+b)  => ddx a/(x+b) = - a / (x+b)^2
    //      /  ddr r, ddg r, ddb r \
    // Jn = |  ddr g, ddg g, ddb g |
    //      \  ddr b, ddg b, ddb b /
    double Jn[] = {
      (rgb[1]+rgb[2])*ib, -rgb[0]*ib, -rgb[0]*ib,
      -rgb[1]*ib, (rgb[0]+rgb[2])*ib, -rgb[1]*ib,
      -rgb[2]*ib, -rgb[2]*ib, (rgb[0]+rgb[1])*ib,
    };
    // ddp n(rgb) = ddx n(rgb) * ddp rgb
    //                Jn         `-----' =Je see above
    //                3x3        (3*num_coeff)x3

    // fprintf(stderr, "Je = ");
    // for(int jj=0;jj<3;jj++)
    //   for(int ii=0;ii<3*num_coeff;ii++) fprintf(stderr, "%g %c", Je[3*num_coeff*jj + ii], (ii==3*num_coeff-1) ? '\n': ' ');
    // matches very well it seems:
    // TODO: use non-zero padded Je and adjust loop accordingly!
    for(int j=0;j<m;j++) // parameter number
      for(int k=0;k<2;k++) // rgb colour channel
        for(int l=0;l<3;l++)
          jac[(2*i + k)*m + j] += Jn[3*k+l] * Je[m*l + j];
  }
}

// callback for levmar:
void lm_callback(
    double *p, // parameers: num_coeff * 3 for camera cfa spectra
    double *x, // output data, write here
    int     m, // number of parameters
    int     n, // number of data points, i.e. colour spots
    void   *data)
{
  double *cf = data;
  int num = n/2;
  for(int i=0;i<num;i++)
  {
    double rgb[3] = {
      eval(p+0*num_coeff, cf + 3*i, num_coeff, 3),
      eval(p+1*num_coeff, cf + 3*i, num_coeff, 3),
      eval(p+2*num_coeff, cf + 3*i, num_coeff, 3)};
    normalise1(rgb);
    x[2*i+0] = rgb[0];
    x[2*i+1] = rgb[1];
  }
}

void lm_jacobian_dif(
    double *p,   // parameters: num_coeff * 3 for camera cfa spectra
    double *jac, // output: derivative dx / dp (n x m entries, n-major, i.e. (dx[0]/dp[0], dx[0]/dp[1], ..)
    int     m,   // number of parameters (=num_coeff*3)
    int     n,   // number of data points
    void   *data)
{
  for(int j=0;j<m;j++)
  {
    double X1[n];
    double X2[n];
    double cfa2[m];
    const double h = 1e-10;
    memcpy(cfa2, p, sizeof(cfa2));
    cfa2[j] += h;
    lm_callback(cfa2, X1, m, n, data);
    memcpy(cfa2, p, sizeof(cfa2));
    cfa2[j] -= h;
    lm_callback(cfa2, X2, m, n, data);
    for(int k=0;k<n;k++)
      jac[m*k + j] = (X1[k] - X2[k]) / (2.0*h);
  }
#if 0 // DEBUG
  double J2[m*n];
  lm_jacobian(p, J2, m, n, data);
  for(int j=0;j<n;j++) for(int i=0;i<m;i++)
  {
    int k = m*j + i;
    if(fabs((J2[k] - jac[k])/fmax(1e-4, fmax(fabs(J2[k]),fabs(jac[k])))) > 1e-2)
      fprintf(stderr, "[p %d d %d] %g %g\n", i, j, J2[k], jac[k]);
  }
  memcpy(jac, J2, sizeof(double)*m*n);
#endif
}

#if 0// debug the lm_jacobian callback as compared to the lm_callback.
static inline void
debug_jacobian(
    double *cfa,  // 3 * num_coeff cfa sigmoid polynomial coefficients
    double *data, // data points (two chroma value per index)
    int     num)  // number of data points (sizeof data = 2*num)
{
  double J1[3*num_coeff * 2*num];
  double J2[3*num_coeff * 2*num];
  lm_jacobian(cfa, J1, 3*num_coeff, 2*num, data);
  for(int j=0;j<3*num_coeff;j++)
  {
    double X1[2*num];
    double X2[2*num];
    double cfa2[3*num_coeff];
    const double h = 1e-10;
    memcpy(cfa2, cfa, sizeof(cfa2));
    cfa2[j] += h;
    lm_callback(cfa2, X1, 3*num_coeff, 2*num, data);
    memcpy(cfa2, cfa, sizeof(cfa2));
    cfa2[j] -= h;
    lm_callback(cfa2, X2, 3*num_coeff, 2*num, data);
    for(int k=0;k<2*num;k++)
      J2[3*num_coeff*k + j] = (X1[k] - X2[k]) / (2.0*h);
  }
  // now compare J1 and J2
  for(int k=0;k<3*num_coeff*2*num;k++)
    fprintf(stderr, "(%g %g)\n", J1[k], J2[k]); // seems to match well
}
#endif

static inline int
load_reference_cfa_spectra(
    const char *model,
    double cfa_spec[100][4])
{
  char filename[256];
  snprintf(filename, sizeof(filename), "%s.txt", model);
  int len = strlen(filename), cfa_spec_cnt = 0;
  for(int i=0;i<len;i++) if(filename[i]==' ') filename[i] = '_';
  FILE *fr = fopen(filename, "rb");
  if(fr)
  {
    while(!feof(fr))
    {
      if(4 != fscanf(fr, "%lg %lg %lg %lg",
          cfa_spec[cfa_spec_cnt] + 0, cfa_spec[cfa_spec_cnt] + 1,
          cfa_spec[cfa_spec_cnt] + 2, cfa_spec[cfa_spec_cnt] + 3))
        cfa_spec_cnt--; // do nothing, we'll ignore comments and stuff gone wrong
      fscanf(fr, "*[^\n]"); fgetc(fr);
      cfa_spec_cnt++;
    }
    fclose(fr);
  }
  else fprintf(stderr, "[fit] can't open reference response curves! `%s'\n", filename);
  return cfa_spec_cnt;
}

// print xyz and rgb pairs for vector plots:
static inline void
write_sample_points(
    const char     *basename,
    const double   *cfa,
    const float    *spectra,
    const header_t *sh,
    const double  (*cfa_spec)[4],
    const int       cfa_spec_cnt,
    const double   *xyz_to_cam,
    const double   *cam_to_xyz,
    float          *chroma,
    const int       cwd,
    const int       cht)
{
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s_points.dat", basename);
  FILE *f0 = fopen(filename, "wb");
  if(!f0) return;
  for(int i=0;i<2000;i++)
  { // pick a random srgb colour inside gamut
    double rgb[3] = {0.0}, xyz[3] = {0.0}, xyz2[3], xyz3[3], cf[3], cam_rgb[3], cam_rgb_spec[3], cam_rgb_rspec[3];
    sample_rgb(rgb, 0.9, 0);
    mat3_mulv(srgb_to_xyz, rgb, xyz);
    mat3_mulv(xyz_to_cam, xyz, cam_rgb); // convert to camera by matrix
    normalise1(xyz);                     // convert to chromaticity

    fetch_coeff(xyz, spectra, sh->wd, sh->ht, cf);
    if(cf[0] == 0.0) continue; // discard out of spectral locus

    for(int k=0;k<3;k++) // camera rgb by processing spectrum * cfa spectrum
      cam_rgb_spec[k] = eval(cfa+num_coeff*k, cf, num_coeff, 3);
    mat3_mulv(cam_to_xyz, cam_rgb_spec, xyz2); // spectral camera to xyz via matrix

    for(int k=0;k<3;k++) // also compute a reference
      cam_rgb_rspec[k] = eval_ref(cfa_spec, k, cfa_spec_cnt, cf);
    mat3_mulv(cam_to_xyz, cam_rgb_rspec, xyz3);

    double rec2020_from_mat[3] = {0.0};
    mat3_mulv(xyz_to_rec2020, xyz2, rec2020_from_mat);

    normalise1(cam_rgb);
    normalise1(cam_rgb_spec);
    normalise1(cam_rgb_rspec);
    normalise1(xyz2);
    normalise1(xyz3);

    // also write exactly the thing we'll do runtime: camera rgb -> matrix and camera rgb -> lut
    double tc[2] = {cam_rgb_spec[0], cam_rgb_spec[2]}; // normalised already
    double rec2020[4] = {0.0}, xyz_spec[3] = {0.0};
    fetch_coeff(tc, chroma, cwd, cht, rec2020); // fetch rb
    // double norm = rec2020[2];
    rec2020[2] = rec2020[1]; // convert to rgb
    rec2020[1] = 1.0-rec2020[0]-rec2020[2];
    mat3_mulv(rec2020_to_xyz, rec2020, xyz_spec);
    normalise1(rec2020_from_mat);
    // for(int k=0;k<3;k++) rec2020[k] *= norm;
    normalise1(rec2020);
    normalise1(xyz_spec);

    fprintf(f0, "%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g\n",
        xyz[0], xyz[1], xyz2[0], xyz2[1], xyz3[0], xyz3[1],
        cam_rgb[0], cam_rgb[1], cam_rgb[2],
        cam_rgb_spec[0], cam_rgb_spec[1], cam_rgb_spec[2],
        cam_rgb_rspec[0], cam_rgb_rspec[1], cam_rgb_rspec[2],
        // rec2020[0], rec2020[2], rec2020_from_mat[0], rec2020_from_mat[2]);
        xyz_spec[0], xyz_spec[1], xyz2[0], xyz2[1]);
  }
  fclose(f0);
}

#if 0
static inline void
write_identity_lut(
    int wd,
    int ht)
{
  header_t hout = {
    .magic    = 1234,
    .version  = 2,
    .channels = 2,
    .datatype = 0,
    .wd       = wd,
    .ht       = ht,
  };
  FILE *f = fopen("id.lut", "wb");
  fwrite(&hout, sizeof(hout), 1, f);
  uint16_t *b16 = calloc(sizeof(uint16_t), wd*ht*2);
  for(int j=0;j<ht;j++)
  for(int i=0;i<wd;i++)
  {
    const int k=j*wd+i;
    double rb[2] = {(i+0.5)/wd, (j+0.5)/ht};
    quad2tri(rb, rb+1);
    b16[2*k+0] = float_to_half(rb[0]);
    b16[2*k+1] = float_to_half(rb[1]);
  }
  fwrite(b16, sizeof(uint16_t), wd*ht*2, f);
  fclose(f);
  free(b16);
}
#endif

// create 2.5D chroma lut
static inline float*
create_chroma_lut(
    int            *wd_out,
    int            *ht_out,
    const float    *spectra,
    const header_t *sh,
    const double   *cfa,
    const double  (*cfa_spec)[4],
    const int       cfa_spec_cnt,
    const double   *xyz_to_cam,
    const double   *cam_to_xyz)
{
  const int ssf = 0; // DEBUG output lut based on measured curve

  // to avoid interpolation artifacts we only want to place straight pixel
  // center values of our spectra.lut in the output:
  int swd = sh->wd, sht = sh->ht; // sampling dimensions
  int wd  = swd, ht = sht; // output dimensions
  float *buf = calloc(sizeof(float)*4, wd*ht+1);

  // do two passes over the data
  // get illum E white point (lowest saturation) in camera rgb and quad param:
  const double white_xyz[3] = {1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f};
  double white_cam_rgb[3];
  mat3_mulv(xyz_to_cam, white_xyz, white_cam_rgb);
  normalise1(white_cam_rgb); // should not be needed
  tri2quad(white_cam_rgb, white_cam_rgb+2);

  // first pass: get rough idea about max deviation from white and the saturation we got there
  double *angular_ds = calloc(sizeof(double), 360*2);
  int sample_wd = swd, sample_ht = sht;
  for(int j=0;j<sample_ht;j++) for(int i=0;i<sample_wd;i++)
  {
    double xy[2] = {(i+0.5)/sample_wd, (j+0.5)/sample_ht};
    quad2tri(xy+0, xy+1);
    double cf[3]; // look up the coeffs for the sampled colour spectrum
    fetch_coeffi(xy, spectra, sh->wd, sh->ht, cf); // nearest
    if(cf[0] == 0) continue; // discard out of spectral locus
    double cam_rgb_spec[3] = {0.0}; // camera rgb by processing spectrum * cfa spectrum
    for(int k=0;k<3;k++)
      if(ssf) cam_rgb_spec[k] = eval_ref(cfa_spec, k, cfa_spec_cnt, cf);
      else    cam_rgb_spec[k] = eval(cfa+num_coeff*k, cf, num_coeff, 3);
    normalise1(cam_rgb_spec);
    double u0 = cam_rgb_spec[0], u1 = cam_rgb_spec[2];
    tri2quad(&u0, &u1);
    float fxy[] = {xy[0], xy[1]}, white[] = {1.0f/3.0f, 1.0f/3.0f};
    float sat = spectrum_saturation(fxy, white);
    // find angular max dist + sat
    int bin = CLAMP(180.0/M_PI * (M_PI + atan2(u1-white_cam_rgb[2], u0-white_cam_rgb[0])), 0, 359);
    double dist2 =
      (u1-white_cam_rgb[2])*(u1-white_cam_rgb[2])+
      (u0-white_cam_rgb[0])*(u0-white_cam_rgb[0]);
    if(dist2 > angular_ds[2*bin])
    {
      angular_ds[2*bin+0] = dist2;
      angular_ds[2*bin+1] = sat;
    }
  }

  double white_norm = 1.0;
  {
  double coeff[3] = {0.0};
  coeff[2] = 100000.0;
  double white_cam_rgb[3] = {0.0};
  white_cam_rgb[0] = eval(cfa+num_coeff*0, coeff, num_coeff, 3);
  white_cam_rgb[1] = eval(cfa+num_coeff*1, coeff, num_coeff, 3);
  white_cam_rgb[2] = eval(cfa+num_coeff*2, coeff, num_coeff, 3);
  white_norm = normalise1(white_cam_rgb);
  }

  // 2nd pass:
// #pragma omp parallel for schedule(dynamic) collapse(2) default(shared)
  for(int j=0;j<sht;j++) for(int i=0;i<swd;i++)
  {
    double xy[2] = {(i+0.5)/swd, (j+0.5)/sht};
    quad2tri(xy+0, xy+1);
    const double xyz[3] = {xy[0], xy[1], 1.0-xy[0]-xy[1]};

    double cf[3]; // look up the coeffs for the sampled colour spectrum
    fetch_coeff(xy, spectra, sh->wd, sh->ht, cf); // interpolate
    // fetch_coeffi(xy, spectra, sh->wd, sh->ht, cf); // nearest
    if(cf[0] == 0) continue; // discard out of spectral locus

    double cam_rgb_spec[3] = {0.0}; // camera rgb by processing spectrum * cfa spectrum
    for(int k=0;k<3;k++)
      if(ssf) cam_rgb_spec[k] = eval_ref(cfa_spec, k, cfa_spec_cnt, cf);
      else    cam_rgb_spec[k] = eval(cfa+num_coeff*k, cf, num_coeff, 3);
    double norm = normalise1(cam_rgb_spec);

    float fxy[] = {xy[0], xy[1]}, white[2] = {1.0f/3.0f, 1.0f/3.0f};
    float sat = spectrum_saturation(fxy, white);

    // convert tri t to quad u:
    double u0 = cam_rgb_spec[0], u1 = cam_rgb_spec[2];
    tri2quad(&u0, &u1);

    int bin = CLAMP(180.0/M_PI * (M_PI + atan2(u1-white_cam_rgb[2], u0-white_cam_rgb[0])), 0, 359);
    double dist2 =
      (u1-white_cam_rgb[2])*(u1-white_cam_rgb[2])+
      (u0-white_cam_rgb[0])*(u0-white_cam_rgb[0]);
    if(dist2 < angular_ds[2*bin] && sat > angular_ds[2*bin+1])
      continue; // discard higher xy sat for lower rgb sat
    if(dist2 < 0.8*0.8*angular_ds[2*bin] && sat > 0.95*angular_ds[2*bin+1])
      continue; // be harsh to values straddling our bounds

    // sort this into rb/sum(rgb) map in camera rgb
    int ii = CLAMP(u0 * wd + 0.5, 0, wd-1);
    int jj = CLAMP(u1 * ht + 0.5, 0, ht-1);
    double rec2020[3];
    mat3_mulv(xyz_to_rec2020, xyz, rec2020);
    normalise1(rec2020);

    buf[4*(jj*wd + ii)+0] = rec2020[0];
    buf[4*(jj*wd + ii)+1] = rec2020[2];
    buf[4*(jj*wd + ii)+2] = norm/white_norm; // store relative norm too!
  }
  free(angular_ds);

  *wd_out = wd;
  *ht_out = ht;
  return buf;
}

static inline void
write_chroma_lut(
    const char  *basename,
    const float *buf,
    const int    wd,
    const int    ht)
{
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s.pfm", basename);
  FILE *f = fopen(filename, "wb");
  fprintf(f, "PF\n%d %d\n-1.0\n", wd, ht);
  for(int k=0;k<wd*ht;k++)
  {
    float col[3] = {buf[4*k], buf[4*k+1], 1.0-buf[4*k]-buf[4*k+1]};
    fwrite(col, sizeof(float), 3, f);
  }
  fclose(f);
  header_t hout = {
    .magic    = 1234,
    .version  = 2,
    .channels = 2,
    .datatype = 0,
    .wd       = wd,
    .ht       = ht,
  };
  snprintf(filename, sizeof(filename), "%s.lut", basename);
  f = fopen(filename, "wb");
  fwrite(&hout, sizeof(hout), 1, f);
  uint16_t *b16 = calloc(sizeof(uint16_t), wd*ht*2);
  for(int k=0;k<wd*ht;k++)
  {
    b16[2*k+0] = float_to_half(buf[4*k+0]);
    b16[2*k+1] = float_to_half(buf[4*k+1]);
  }
  fwrite(b16, sizeof(uint16_t), wd*ht*2, f);
  fclose(f);
  free(b16);
}

static inline void
print_cfa_coeffs(double *cfa)
{
  fprintf(stderr, "[fit] red   cfa coeffs ");
  for(int k=0;k<num_coeff;k++) fprintf(stderr, "%2.8g ", cfa[k]);
  fprintf(stderr, "\n[fit] green cfa coeffs ");
  for(int k=0;k<num_coeff;k++) fprintf(stderr, "%2.8g ", cfa[num_coeff+k]);
  fprintf(stderr, "\n[fit] blue  cfa coeffs ");
  for(int k=0;k<num_coeff;k++) fprintf(stderr, "%2.8g ", cfa[2*num_coeff+k]);
  fprintf(stderr, "\n");
}

static inline void
write_camera_curves(
    const char   *basename,
    const double *cfa)
{ // plot 'dat' u 1:2 w l lw 4, '' u 1:3 w l lw 4, '' u 1:4 w l lw 4
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s_curves.dat", basename);
  FILE *f = fopen(filename, "wb");
  if(!f) return;
  for(int i=0;i<CIE_SAMPLES;i++)
  { // plot the camera curves:
    double lambda0 = (i+.5)/(double)CIE_FINE_SAMPLES;
    double lambda1 = lambda0 * (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) + CIE_LAMBDA_MIN;
    double s0 = sigmoid(poly(cfa+0*num_coeff, lambda0, num_coeff));
    double s1 = sigmoid(poly(cfa+1*num_coeff, lambda0, num_coeff));
    double s2 = sigmoid(poly(cfa+2*num_coeff, lambda0, num_coeff));
    fprintf(f, "%g %g %g %g\n", lambda1, s0, s1, s2);
  }
  fclose(f);
}

int main(int argc, char *argv[])
{
  // warm up random number generator
  for(int k=0;k<10;k++) xrand();
  // load spectra.lut:
  header_t header;
  float *spectra = 0;
  {
    FILE *f = fopen("../../data/spectra.lut", "rb");
    if(!f) goto error;
    if(fread(&header, sizeof(header_t), 1, f) != 1) goto error;
    if(header.channels != 4) goto error;
    if(header.version != 2) goto error;
    spectra = calloc(4*sizeof(float), header.wd * header.ht);
    fread(spectra, header.wd*header.ht, 4*sizeof(float), f);
    fclose(f);
    if(0)
    {
error:
      if(f) fclose(f);
      fprintf(stderr, "could not read spectra.lut!\n");
      exit(2);
    }
  }

  // these influence the problem size and the mode of operation of the fitter:
  // low quality mode:
  num_coeff     = 6;   // number of coefficients in the sigmoid/polynomial cfa spectra
  int num_it    = 500; // iterations per batch
  int num       = 10;  // number of data points (spectrum/xy) per batch
  int batch_cnt = 50;  // number of batches
  // initial thoughts: using 100 batches and 1000 iterations (num=50)
  // improves the fit a bit, only not in the reds. probably not
  // converged yet, but indeed it seems more data serves the process.

  int der = 0, hq = 0;
  const char *model = "identity";
  for(int k=1;k<argc;k++)
  {
    if     (!strcmp(argv[k], "--hq" )) hq  = 1; // high quality mode
    else if(!strcmp(argv[k], "--der")) der = 1; // use analytic jacobian
    else model = argv[k];
  }

  if(hq && !der)
  {
    fprintf(stderr, "[fit] setting high quality mode, this can be slow..\n");
    num_coeff = 6;
    num_it    = 1000; // 100k unfortunately it does improve from 10k, not by much but notably. this is 10x the cost :(
    batch_cnt = 100;
    num       = 70;
  }
  else if(hq && der)
  { // these need way more iterations because they often bail out early
    fprintf(stderr, "[fit] setting high quality mode and analytic jacobian. this can be slow..\n");
    num_coeff = 6;
    num_it    = 1000;
    batch_cnt = 100;
    num       = 70;
  }

  double xyz_to_cam[9];
  float adobe_mat[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  fprintf(stderr, "[fit] using matrix for `%s'\n", model);
  if(strcmp(model, "identity") && dt_dcraw_adobe_coeff(model, &adobe_mat))
  {
    fprintf(stderr, "[fit] could not find this camera model (%s)! check your spelling?\n", model);
    exit(3);
  }
  for(int i=0;i<9;i++) xyz_to_cam[i] = adobe_mat[i];

  double cam_to_xyz[9] = {0.0};
  mat3_inv(xyz_to_cam, cam_to_xyz);
  // xyz -> camera rgb matrix. does it contain white balancing stuff?
  fprintf(stderr, "[fit] M = %g	%g	%g\n"
                  "          %g	%g	%g\n"
                  "          %g	%g	%g\n",
                  xyz_to_cam[0], xyz_to_cam[1], xyz_to_cam[2],
                  xyz_to_cam[3], xyz_to_cam[4], xyz_to_cam[5],
                  xyz_to_cam[6], xyz_to_cam[7], xyz_to_cam[8]);
  // in particular, xyz white 1 1 1 maps to this camera rgb:
  double white[3] = {0.0}, one[3] = {1.0, 1.0, 1.0};
  mat3_mulv(xyz_to_cam, one, white);
  double wbcoeff[3] = {1.0/white[0], 1.0/white[1], 1.0/white[2]};
  fprintf(stderr, "[fit] white = %g %g %g\n", white[0], white[1], white[2]);
  fprintf(stderr, "[fit] wb coeff = %g %g %g\n", wbcoeff[0], wbcoeff[1], wbcoeff[2]);

  // init initial cfa params and lower/upper bounds:
  double cfa[30] = {0.0};
  double lb[30] = {0.0};
  double ub[30] = {0.0};
  // these bounds are important for regularisation. else we'll fit to box spectra to be
  // able to match the matrix better. +-50 seem to work really well in most cases.
  // fuji/nikon sometimes asks for extended range, but i think it would be better to increase
  // the number of coefficients/degree of the polynomial instead.
  for(int k=0;k<3*num_coeff;k++) lb[k] = -50;
  for(int k=0;k<3*num_coeff;k++) ub[k] =  50;
  // for(int k=0;k<3*num_coeff;k++) lb[k] = -(ub[k] = 500); // for kodak need more complex blue
  int e = num_coeff-3; // quadratic term
  cfa[e+0] = cfa[e+num_coeff] = cfa[e+2*num_coeff] = -3.0;

  // construct data point arrays for levmar, 3x for each tristimulus/camera rgb channel:
  double *data   = calloc(num, sizeof(double) * 3); // data point: spectral coeff for input
  double *target = calloc(num, sizeof(double) * 2); // target chroma

  fprintf(stderr, "[fit] starting optimiser..");
  double resid = 1.0;

  // run optimisation in mini-batches
  for (int batch=0;batch<batch_cnt;batch++)
  {
    for(int i=0;i<num;i++)
    {
      // pick a random srgb colour inside gamut
      double rgb[3] = {0.0}, xyz[3] = {0.0};
      sample_rgb(rgb, 0.17, 1);  // chosen to reach all of srgb at least potentially
      // sample_rgb(rgb, 0.3, 1);  // relaxed for kodak
      mat3_mulv(srgb_to_xyz, rgb, xyz);
      normalise1(xyz);

      double cf[3]; // look up the coeffs for the sampled colour spectrum
      fetch_coeff(xyz, spectra, header.wd, header.ht, cf);

      // apply xyz to camera rgb matrix:
      double cam_rgb[3] = {0.0};
      mat3_mulv(xyz_to_cam, xyz, cam_rgb);
      normalise1(cam_rgb);

      memcpy(target+2*i, cam_rgb, sizeof(double)*2);
      memcpy(data+3*i, cf, sizeof(double)*3);
    }

#ifdef USE_LEVMAR
    double info[LM_INFO_SZ] = {0};
    double opts[LM_OPTS_SZ] = {
      // init-mu    eps Jte   eps Dp    eps err   eps diff
      0.2, 1E-15, 1E-40, 1E-15, 1e-5};//LM_DIFF_DELTA};

    if(der)
    {
      dlevmar_bc_der(
          lm_callback, lm_jacobian, cfa, target, 3*num_coeff, 2*num,
          lb, ub, NULL, // dscl, // diagonal scaling constants (?)
          num_it, opts, info, NULL, NULL, data);
    }
    else
    {
      dlevmar_bc_dif(
          lm_callback, cfa, target, 3*num_coeff, 2*num,
          lb, ub, NULL, // dscl, // diagonal scaling constants (?)
          num_it, opts, info, NULL, NULL, data);
    }
    // fprintf(stderr, "    ||e||_2, ||J^T e||_inf,  ||Dp||_2, mu/max[J^T J]_ii\n");
    // fprintf(stderr, "info %g %g %g %g\n", info[1], info[2], info[3], info[4]);
    fprintf(stderr, "\r[fit] batch %d/%d it %04d/%d reason %g resid %g -> %g                ",
        batch+1, batch_cnt, (int)info[5], num_it, info[6], info[0], info[1]);
    fprintf(stdout, "%g %g\n", info[0], info[1]);
#else
    fprintf(stdout, "%g ", resid);
    // resid = minimize_nelder_mead(
        // cfa, num_it, &nm_objective, target, 2*num, data);
    resid = gauss_newton_cg(
        lm_callback,
        // lm_jacobian, // does not like our analytic jacobian
        lm_jacobian_dif,
        cfa, target, 3*num_coeff, 2*num,
        lb, ub, num_it, data);
    fprintf(stderr, "\r[fit] batch %d/%d resid %g               ",
        batch+1, batch_cnt, resid);
    fprintf(stdout, "%g\n", resid); // write convergence history
#endif
  } // end mini batches
  fprintf(stderr, "\n");

  // now we're done, prepare the data for some useful output:

  // load reference spectra from txt, if we can
  double cfa_spec[100][4] = {{0.0}};
  const int cfa_spec_cnt = load_reference_cfa_spectra(model, cfa_spec);

  // create the actual 2D chroma lut
  int wd, ht;
  float *buf = create_chroma_lut(&wd, &ht, spectra, &header, cfa, cfa_spec, cfa_spec_cnt, xyz_to_cam, cam_to_xyz);

#if 1 // then hole fill it
  buf_t inpaint_buf = {
    .dat = buf,
    .wd  = wd,
    .ht  = ht,
    .cpp = 4,
  };
  inpaint(&inpaint_buf);
#endif

  char basename[256] = {0}; // get sanitised basename
  snprintf(basename, sizeof(basename), "%s", model);
  int len = strnlen(basename, sizeof(basename));
  if(!strcasecmp(".txt", basename + len - 4))
    basename[len-4] = 0;
  for(int i=0;i<len;i++) if(basename[i] == ' ') basename[i] = '_';

  // write the chroma lut to half float .lut as well as .pfm for debugging:
  write_chroma_lut(basename, buf, wd, ht);

  // write a couple of sample points for debug vector plots
  write_sample_points(basename, cfa, spectra, &header, cfa_spec, cfa_spec_cnt, xyz_to_cam, cam_to_xyz, buf, wd, ht);

  // output the coefficients to console
  print_cfa_coeffs(cfa);

  // write the cfa curves to a file for plotting
  write_camera_curves(basename, cfa);

  // write a camera rgb == rec2020 identity lut for debugging
  // write_identity_lut(wd, ht);

  free(buf);

  exit(0);
}
