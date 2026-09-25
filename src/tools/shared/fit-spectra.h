#pragma once
// this uses the coefficient cube optimiser from the paper:
//
// Wenzel Jakob and Johannes Hanika. A low-dimensional function space for
// efficient spectral upsampling. Computer Graphics Forum (Proceedings of
// Eurographics), 38(2), March 2019. 
//
// with newer updates from the mitsuba repo (levenberg marquardt, analytic jacobian, f32)

/* Working precision for the optimizer. Single precision is sufficient for the
 * table's accuracy and lets the integration loops auto-vectorize to 4-wide SIMD
 * (define this to `double` to fall back to double precision). The reference
 * colorimetric data in cie1931.h deliberately stays double. */
#define Float float

#include "shared/lu.h"
#include "shared/matrices.h"
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

void init_tables(const char *ssf_filename, const double *illuminant)
{
  memset(rgb_tbl, 0, sizeof(rgb_tbl));
  memset(xyz_whitepoint, 0, sizeof(xyz_whitepoint));
  memset(lambda_tbl, 0, sizeof(lambda_tbl));

  if(!illuminant) illuminant = cie_d65;

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

