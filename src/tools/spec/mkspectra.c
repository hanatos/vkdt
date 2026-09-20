// this uses the coefficient cube optimiser from the paper:
//
// Wenzel Jakob and Johannes Hanika. A low-dimensional function space for
// efficient spectral upsampling. Computer Graphics Forum (Proceedings of
// Eurographics), 38(2), March 2019. 

// creates a lut for spectral upsampling, (x,y) -> c0 c1 c2 b
// where (x,y) is the quad representation of xyz computed from arbitrary ssf (pass on command line).
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <float.h>
#include <limits.h>

/* Working precision for the optimizer. Single precision is sufficient for the
 * table's accuracy and lets the integration loops auto-vectorize to 4-wide SIMD
 * (define this to `double` to fall back to double precision). The reference
 * colorimetric data in cie1931.h deliberately stays double. */
#define Float float

#include "shared/lu.h"
#include "shared/matrices.h"
#include "core/inpaint.h"
#include "core/threads.h"
#include "core/lut.h"
#include "shared/q2t.h"
#include "core/core.h"
#include "clut/src/spectrum.h"

// discretisation of quadrature scheme
#define CIE_SAMPLES 95
#define CIE_LAMBDA_MIN 360.0
#define CIE_LAMBDA_MAX 830.0
#define CIE_FINE_SAMPLES ((CIE_SAMPLES - 1) * 3 + 1)

/// Sample count padded to a multiple of 16 (the widest SIMD lane count for
/// float) so the quadrature loop vectorizes with no scalar remainder. The extra
/// entries stay zero and therefore contribute nothing to the integral.
#define CIE_FINE_SAMPLES_PAD (((CIE_FINE_SAMPLES + 15) / 16) * 16)

#include "shared/cie1931.h"

// Macros to help compilers autovectorize code in this project
#if defined(__clang__)
#  define RGB2SPEC_FP_FAST
#  define RGB2SPEC_FP_REASSOC _Pragma("clang fp reassociate(on) contract(fast)")
#elif defined(__GNUC__)
#  define RGB2SPEC_FP_FAST __attribute__((optimize("-fassociative-math", \
       "-fno-signed-zeros", "-fno-trapping-math", "-fno-math-errno", "-ffp-contract=fast")))
#  define RGB2SPEC_FP_REASSOC
#else
#  define RGB2SPEC_FP_FAST
#  define RGB2SPEC_FP_REASSOC
#endif

#if defined(_MSC_VER)
#  define RGB2SPEC_FP_PUSH _Pragma("float_control(precise, off, push)")
#  define RGB2SPEC_FP_POP  _Pragma("float_control(pop)")
#else
#  define RGB2SPEC_FP_PUSH
#  define RGB2SPEC_FP_POP
#endif

/// Precomputed tables for fast spectral -> RGB conversion
// we have alignas in c23 but github ci doesn't support that on all platforms
_Alignas(128) Float lambda_tbl[CIE_FINE_SAMPLES_PAD];
_Alignas(128) Float rgb_tbl[3][CIE_FINE_SAMPLES_PAD];
Float // rgb_to_xyz[3][3],
      // xyz_to_rgb[3][3],
      xyz_whitepoint[3];

double sqrd(double x) { return x * x; }

#if 0
void spiral_init(const int c[2], int p[2], int *r)
{
  *r = 0;
  p[0] = c[0];
  p[1] = c[1];
}

void spiral_step(const int c[2], int p[2], int *r)
{
  if(*r == 0) { p[0]++; *r = 1; }
  else if (p[1] == c[1] - *r) { if (p[0] == c[0] + *r) (*r)++; p[0]++; }
  else if (p[0] == c[0] - *r) p[1]--;
  else if (p[1] == c[1] + *r) p[0]--;
  else if (p[0] == c[0] + *r) p[1]++;
}
#endif

void quantise_coeffs(double coeffs[3], float out[3])
{ // account for normalising lambda:
  double c0 = CIE_LAMBDA_MIN, c1 = 1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);
  double A = coeffs[0], B = coeffs[1], C = coeffs[2];

  const double A2 = (A*(sqrd(c1)));
  const double B2 = (B*c1 - 2*A*c0*(sqrd(c1)));
  const double C2 = (C - B*c0*c1 + A*(sqrd(c0*c1)));
  out[0] = (float)A2;
  out[1] = (float)B2;
  out[2] = (float)C2;
}

Float cie_lab_f(Float t)
{
  Float delta = (Float)(6.0 / 29.0);
  if (t > delta*delta*delta)
    return cbrtf(t);
  else
    return t / (delta*delta * (Float)(3)) + (Float)(4.0 / 29.0);
}

void cie_lab(Float *p) {
    Float X = 0, Y = 0, Z = 0,
      Xw = xyz_whitepoint[0],
      Yw = xyz_whitepoint[1],
      Zw = xyz_whitepoint[2];

    X = p[0];
    Y = p[1];
    Z = p[2];
#if 0
    for (int j = 0; j < 3; ++j) {
        X += p[j] * rgb_to_xyz[0][j];
        Y += p[j] * rgb_to_xyz[1][j];
        Z += p[j] * rgb_to_xyz[2][j];
    }
#endif

    p[0] = (Float)(116) * cie_lab_f(Y / Yw) - (Float)(16);
    p[1] = (Float)(500) * (cie_lab_f(X / Xw) - cie_lab_f(Y / Yw));
    p[2] = (Float)(200) * (cie_lab_f(Y / Yw) - cie_lab_f(Z / Zw));
}

void init_tables(const char *ssf_filename)
{
  memset(rgb_tbl, 0, sizeof(rgb_tbl));
  memset(xyz_whitepoint, 0, sizeof(xyz_whitepoint));
  memset(lambda_tbl, 0, sizeof(lambda_tbl));

  const double *illuminant = cie_e;
  if(ssf_filename) illuminant = cie_d65;

  double cfa_spec[1000][4];
  int cfa_spec_cnt = 0;
  double cfa_norm = 1.0;
  if(ssf_filename)
  {
    cfa_spec_cnt = spectrum_load(ssf_filename, cfa_spec);
    if(!cfa_spec_cnt)
    {
      fprintf(stderr, "could not load %s\n", ssf_filename);
      exit(1);
    }
#if 0 // normalise to max 1 (such as cie cmf)
    double mv = 0.0;
    for(int i=0;i<cfa_spec_cnt;i++) mv = MAX(mv, cfa_spec[2][i]);
    cfa_norm = 1.0 / mv;
#endif
  }

  const double h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (double)CIE_FINE_SAMPLES;
  for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
  {
    double lambda, weight;
    lambda = CIE_LAMBDA_MIN + (i+0.5) * h;
    weight = h;
    const double I = cie_interp(illuminant, lambda);
    double xyz[3];
    if(ssf_filename)
    {
      xyz[0] = cfa_norm * spectrum_interp(cfa_spec, cfa_spec_cnt, 0, lambda);
      xyz[1] = cfa_norm * spectrum_interp(cfa_spec, cfa_spec_cnt, 1, lambda);
      xyz[2] = cfa_norm * spectrum_interp(cfa_spec, cfa_spec_cnt, 2, lambda);
    }
    else
    {
      xyz[0] = cie_interp(cie_x, lambda);
      xyz[1] = cie_interp(cie_y, lambda);
      xyz[2] = cie_interp(cie_z, lambda);
    }

    lambda_tbl[i] = lambda;
    for (int k = 0; k < 3; ++k)
        rgb_tbl[k][i] = xyz[k] * I * weight;
      // for (int j = 0; j < 3; ++j)
        // XXX not needed
        // rgb_tbl[k][i] += xyz_to_rgb[k][j] * xyz[j] * I * weight;
    // XXX works without changing shader code, but makes colour space smaller
    // rgb_tbl[k][i] += rec2020_to_xyz[k][j] * xyz[j] * I * weight;

    for (int k = 0; k < 3; ++k)
      xyz_whitepoint[k] += xyz[k] * I * weight;
  }
}

/* Relax FP / enable vectorization for the integration loops below (see macros above). */
RGB2SPEC_FP_PUSH

RGB2SPEC_FP_FAST
void eval_residual(const Float *coeffs, const Float *rgb, Float *residual)
{
    RGB2SPEC_FP_REASSOC
    Float out[3] = { 0, 0, 0 };
    const Float lo  = (Float) CIE_LAMBDA_MIN,
                inv = (Float) (1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN));
    Float c0 = coeffs[0], c1 = coeffs[1], c2 = coeffs[2];

    for (int i = 0; i < CIE_FINE_SAMPLES_PAD; ++i) {
        Float lambda = (lambda_tbl[i] - lo) * inv;          /* scale to 0..1 */
        Float x = (c0 * lambda + c1) * lambda + c2;         /* polynomial */
        Float s = (Float)0.5 * x / sqrtf((Float)1 + x * x) + (Float)0.5;

        for (int j = 0; j < 3; ++j)
            out[j] += rgb_tbl[j][i] * s;
    }
    cie_lab(out);
    for (int j = 0; j < 3; ++j) residual[j] = rgb[j];
    cie_lab(residual);
    for (int j = 0; j < 3; ++j)
        residual[j] -= out[j];
}

/// Evaluate the CIELab color of the RGB input 'in', returning the value in 'lab'
/// and its analytic Jacobian d(lab)/d(in) in 'jac'.
void cie_lab_jac(const Float in[3], Float lab[3], Float jac[3][3]) {
    Float wn[3] = { xyz_whitepoint[0], xyz_whitepoint[1], xyz_whitepoint[2] };

    Float rgb_to_xyz[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    Float xyz[3] = { 0, 0, 0 };
    for (int a = 0; a < 3; ++a)
        for (int j = 0; j < 3; ++j)
            xyz[a] += in[j] * rgb_to_xyz[a][j];

    Float fv[3], gd[3];      /* f(t) and f'(t)/w_n, with t = xyz / whitepoint */
    for (int k = 0; k < 3; ++k) {
        Float t = xyz[k] / wn[k], delta = (Float)(6.0 / 29.0);
        if (t > delta*delta*delta) {
            Float cr = cbrtf(t);
            fv[k] = cr;
            gd[k] = (Float)(1.0 / 3.0) / (cr*cr * wn[k]);
        } else {
            fv[k] = t / (delta*delta * (Float)3) + (Float)(4.0 / 29.0);
            gd[k] = (Float)1.0 / (delta*delta * (Float)3 * wn[k]);
        }
    }
    lab[0] = (Float)(116) * fv[1] - (Float)(16);
    lab[1] = (Float)(500) * (fv[0] - fv[1]);
    lab[2] = (Float)(200) * (fv[1] - fv[2]);

    /* d Lab / d XYZ */
    Float g[3][3] = {
        {            0,   (Float)(116) * gd[1],                0 },
        { (Float)(500)*gd[0],  (Float)(-500) * gd[1],            0 },
        {            0,   (Float)(200) * gd[1],  (Float)(-200)*gd[2] }
    };

    /* d Lab / d RGB = (d Lab / d XYZ) * rgb_to_xyz */
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b) {
            Float s = 0;
            for (int k = 0; k < 3; ++k)
                s += g[a][k] * rgb_to_xyz[k][b];
            jac[a][b] = s;
        }
}

/// Evaluate the residual together with its Jacobian in a single integration pass.
RGB2SPEC_FP_FAST
void eval_residual_jac(const Float *coeffs, const Float *rgb,
                       Float *residual, Float jac[3][3])
{
    RGB2SPEC_FP_REASSOC
    Float out[3] = { 0, 0, 0 };
    Float dout[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }; /* d out[j] / d coeffs[a] */
    const Float lo  = (Float) CIE_LAMBDA_MIN,
                inv = (Float) (1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN));
    Float c0 = coeffs[0], c1 = coeffs[1], c2 = coeffs[2];

    for (int i = 0; i < CIE_FINE_SAMPLES_PAD; ++i) {
        Float lambda = (lambda_tbl[i] - lo) * inv;
        Float x = (c0 * lambda + c1) * lambda + c2;
        Float dx0 = lambda * lambda, dx1 = lambda;            /* dP/dcoeffs */

        /* Sigmoid and its derivative (both share q = 1 / sqrt(1 + x^2)) */
        Float q  = (Float)1.0 / sqrtf((Float)1.0 + x * x);
        Float s  = (Float)0.5 * x * q + (Float)0.5;
        Float sp = (Float)0.5 * q * q * q;

        for (int j = 0; j < 3; ++j) {
            Float w = rgb_tbl[j][i];
            out[j] += w * s;
            Float wsp = w * sp;
            dout[j][0] += wsp * dx0;
            dout[j][1] += wsp * dx1;
            dout[j][2] += wsp;
        }
    }
    /* Residual in CIELab. The reproduced color needs both its Lab value and the
       Lab Jacobian, so compute them together in one tristimulus pass. */
    Float out_lab[3];
    Float lab_jac[3][3] = {0};
    cie_lab_jac(out, out_lab, lab_jac);
    for (int j = 0; j < 3; ++j) residual[j] = rgb[j];
    cie_lab(residual);
    for (int j = 0; j < 3; ++j)
        residual[j] -= out_lab[j];

    /* Chain rule: d residual / d coeffs = -(d Lab / d out) * (d out / d coeffs) */
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b) {
            Float s = 0;
            for (int k = 0; k < 3; ++k)
                s += lab_jac[a][k] * dout[k][b];
            jac[a][b] = -s;
        }
}
RGB2SPEC_FP_POP

static inline void
spec_integrate_macadam(
    Float lambda0,  // rising edge
    Float lambda1,  // falling edge
    Float *rgb)     // output stored here
{
  Float out[3] = { 0, 0, 0 };
  for (int i = 0; i < CIE_FINE_SAMPLES_PAD; ++i)
  {
    Float lambda = lambda_tbl[i];
    Float s = 0.0;
    if(lambda0 < lambda1)
    { // peak
      if(lambda > lambda0 && lambda <= lambda1) s = 1.0;
    }
    else
    { // dip
      if(lambda <= lambda1 || lambda > lambda0) s = 1.0;
    }
    for (int j = 0; j < 3; ++j)
      out[j] += rgb_tbl[j][i] * s;
  }
  for(int k=0;k<3;k++) rgb[k] = out[k];
}

typedef struct parallel_shared_t
{
  float *out;
  int res; // resolution of out is res x res
  int str; // stride of out
  int iteration;
  int num_it;
  // assume value will fall off mostly smoothly from the peak value:
  float peak_val;
  float peak[2]; // coordinate with max x+y+z during macadam pass
  float radial[180][2]; // v d, in 2 degree steps, d only needed to create the table
}
parallel_shared_t;

void work_macadam(uint32_t item, void *data)
{ // enumerate all possible box spectra in the sense of [MacAdam 1935],
  const int iw0 = item;
  parallel_shared_t *d = data;
  for(int iw1=0;iw1<CIE_FINE_SAMPLES;iw1++)
  {
    if(iw0 == iw1) continue; // will be zero anyways
    const Float lambda0 = lambda_tbl[iw0];
    const Float lambda1 = lambda_tbl[iw1];
    Float xyz[3] = {0};
    spec_integrate_macadam(lambda0, lambda1, xyz);

    Float b = xyz[0]+xyz[1]+xyz[2];
    if(b < 1e-8) continue; // paranoid safeguard. who would design ssf like that.
    double x = xyz[0]/b;
    double y = xyz[1]/b;
    tri2quad(&x,&y);
    // rasterize into map
    const int i = x*d->res+0.5f, j = y*d->res+0.5f;
    if(i>=0 && i<d->res && j>=0 && j<d->res)
    { // compute radial bin
      x -= d->peak[0];
      y -= d->peak[1];
      const float angle = M_PI+atan2f(y, x);
      const int cnt = sizeof(d->radial) / sizeof(d->radial[0]);
      const int bin = (int)CLAMP(cnt*angle/(2.0*M_PI) + 0.5, 0, cnt-1);
      if(d->iteration == 1 && d->radial[bin][0] <= b)
      { // reject if radial bin value > b, i.e. our sample is in the lower brightness layer
        d->out[d->str*(j*d->res+i) + 3] = b*0.5;
      }
      else if(d->iteration == 0 && d->radial[bin][1] < x*x+y*y)
      {
        d->radial[bin][0] = b;
        d->radial[bin][1] = x*x+y*y;
      }
    }
  }
}

/**
 * Find the polynomial coefficients whose sigmoidal spectrum best reproduces the
 * target color 'rgb'. The objective is the squared CIELab distance, minimized
 * with the Levenberg-Marquardt algorithm.
 *
 * Inside the gamut the target color is exactly reachable, the Jacobian is well
 * conditioned, and LM converges quadratically to a zero residual. Near or beyond
 * the gamut boundary no exact solution exists and the Jacobian becomes singular
 * as the spectrum saturates; there the adaptive 'lambda' damping regularizes the
 * step so the method settles at the closest achievable color instead of diverging.
 */
Float LM(const Float rgb[3], Float coeffs[3], int it)
{
    Float residual[3], jac[3][3];
    eval_residual_jac(coeffs, rgb, residual, jac);
    Float cost = sqrd(residual[0]) + sqrd(residual[1]) + sqrd(residual[2]);

    Float lambda = (Float)1e-3;

    for (int i = 0; i < it && cost > (Float)1e-12; ++i) {
        /* Assemble the normal equations: A = J^T J,  g = J^T residual */
        Float A[3][3], g[3] = { 0 };
        for (int a = 0; a < 3; ++a) {
            for (int b = 0; b < 3; ++b) {
                Float s = 0;
                for (int k = 0; k < 3; ++k)
                    s += jac[k][a] * jac[k][b];
                A[a][b] = s;
            }
            for (int k = 0; k < 3; ++k)
                g[a] += jac[k][a] * residual[k];
        }

        /* Try damped steps, increasing 'lambda', until one decreases the cost */
        int accepted = 0;
        for (int t = 0; t < 10 && !accepted; ++t) {
            Float M0[3], M1[3], M2[3], *M[3] = { M0, M1, M2 };
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b)
                    M[a][b] = A[a][b] + (a == b ? lambda : (Float)0); /* A + lambda*I */

            int P[3+1];
            if (LUPDecompose(M, 3, (Float)1e-30, P) != 1) {
                lambda *= (Float)10; /* singular even when damped -> damp harder */
                continue;
            }

            /* Solve (A + lambda*I) step = g;  the LM update is coeffs -= step.
               Trials only need the cost, so use the cheaper residual-only eval. */
            Float step[3];
            LUPSolve(M, P, g, 3, step);

            Float trial[3];
            for(int k=0;k<3;k++) trial[k] = coeffs[k] - step[k];
            Float trial_res[3];
            eval_residual(trial, rgb, trial_res);
            Float trial_cost = sqrd(trial_res[0]) + sqrd(trial_res[1]) +
                               sqrd(trial_res[2]);

            if (trial_cost < cost) {
                memcpy(coeffs, trial, sizeof(Float) * 3);
                cost = trial_cost;
                lambda = fmaxf(lambda * (Float)0.5, (Float)1e-12); /* step worked: trust the model more */
                accepted = 1;
            } else {
                lambda *= (Float)10; /* step failed: trust the model less */
                if (lambda > (Float)1e12)
                    break;
            }
        }

        if (!accepted)
            break; /* converged, or no damped step can improve further */

        /* Refresh residual and analytic Jacobian at the accepted point */
        eval_residual_jac(coeffs, rgb, residual, jac);
    }

    return sqrtf(cost);
}

void work_fit(uint32_t idx, void *data)
{
  const parallel_shared_t *const d = data;
  const int j = idx / d->res;
  const int i = idx - j * d->res;
  if(i == 0)
  {
    printf(".");
    fflush(stdout);
  }
  double x = (i) / (Float)d->res;
  double y = (j) / (Float)d->res;
  quad2tri(&x, &y);
  Float coeffs[4] = {0, 1, 0, d->out[d->str*idx + 3]};
  Float rgbm[3] = { x * coeffs[3], y * coeffs[3], (1-x-y) * coeffs[3]};
  Float resid = LM(rgbm, coeffs, d->num_it); // wenzel's levenberg marquardt
  (void)resid;
  d->out[d->str*idx + 0] = coeffs[0];
  d->out[d->str*idx + 1] = coeffs[1];
  d->out[d->str*idx + 2] = coeffs[2];
}

static void
create_cfa_table(const char *ssf_filename)
{
  int res = 512, str = 4;
  float *out = calloc(sizeof(float), str*res*res);
  parallel_shared_t par = (parallel_shared_t) {
    .out   = out,
    .str   = str,
    .res   = res,
    .iteration = 0,
    .num_it = 15,
  };
  fprintf(stdout, "computing macadam brightness..\n");
  { // compute peak value (broadest spectrum)
    const Float lambda0 = lambda_tbl[0];
    const Float lambda1 = lambda_tbl[CIE_FINE_SAMPLES-1];
    Float xyz[3] = {0};
    spec_integrate_macadam(lambda0, lambda1, xyz);
    const double b = xyz[0]+xyz[1]+xyz[2];
    double x = xyz[0]/b, y = xyz[1]/b;
    tri2quad(&x,&y);
    par.peak_val = b;
    par.peak[0] = x;
    par.peak[1] = y;
  }
  // find max radial extent
  for(int i=0;i<CIE_FINE_SAMPLES;i++)
    work_macadam(i, &par);
  // now rasterise all spectra, ignoring the lower value layer
  // since the peak (white) connects smoothly with the upper/brighter layer (likely less saturated for same chroma)
  par.iteration = 1;
  for(int i=0;i<CIE_FINE_SAMPLES;i++)
    work_macadam(i, &par);

#if 0 // inspect radial values
  fprintf(stdout, "peak: %g %g %g\n", par.peak[0], par.peak[1], par.peak_val);
  int cnt = sizeof(par.radial) / sizeof(par.radial[0]);
  for(int i=0;i<cnt;i++)
      fprintf(stdout, "%d %g %g\n", i, par.radial[i][0], par.radial[i][1]);
#endif

  fprintf(stdout, "inpainting..\n");
  dt_inpaint_buf_t inpaint_buf = {
    .dat = (float *)out,
    .wd  = res,
    .ht  = res,
    .cpp = str,
  };
  dt_inpaint(&inpaint_buf);

  fprintf(stdout, "fitting..");
  const int nt = threads_num();
  int taskid = -1;
  for(int i=0;i<nt;i++)
    taskid = threads_task("mkspectra", res*res, taskid, &par, work_fit, 0);
  threads_wait(taskid);
  fprintf(stdout, "\n");

  { // write spectra map: (x,y) |--> sigmoid coeffs + scale
    dt_lut_header_t head = (dt_lut_header_t) {
      .magic    = dt_lut_header_magic,
      .version  = dt_lut_header_version,
      .channels = 4,
      .datatype = dt_lut_header_f32,
      .wd       = res,
      .ht       = res,
    };
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s-em.lut", ssf_filename ? ssf_filename : "spectra");
    FILE *f = fopen(filename, "wb");
    fprintf(stdout, "writing lut to %s\n", filename);
    if(f) fwrite(&head, sizeof(head), 1, f);
    for(int k=0;k<res*res;k++)
    {
      double coeffs[4] = {out[str*k+0], out[str*k+1], out[str*k+2], out[str*k+3]};
      float q[4] = {0,0,0, coeffs[3]};
      quantise_coeffs(coeffs, q);
      if(f) fwrite(q, sizeof(float), 4, f);
    }
    if(f) fprintf(f,
        "spectral upsampling table for Jakob 2019 style sigmoid spectra.\n"
        "created from ssf: %s\n"
        "lookup using triangle-to-quad xy chromaticities.\n"
        "this is meant to be used for emissions, i.e. querying at the equal energy white\n"
        "coordinates (1/3,1/3) will yield the equal energy spectrum.\n"
        "the fourth channel is the brightness b=X+Y+Z of the corresponding colour coordinate.\n"
        "this is not 1.0/constant because the bounded spectra are subject to MacAdams limit.",
        ssf_filename ? ssf_filename : "CIE 1931 2deg CMF");
    if(f) fclose(f);
  }
  free(out);
}

int main(int argc, char **argv)
{
  const char *ssf_filename = 0;
  if(argc > 1) ssf_filename = argv[1];

  threads_global_init();
  init_tables(ssf_filename);
  create_cfa_table(ssf_filename);
  threads_global_cleanup();
  exit(0);
}
