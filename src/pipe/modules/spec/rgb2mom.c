// this is the coefficient cube optimiser from the paper:
//
// Wenzel Jakob and Johannes Hanika. A low-dimensional function space for
// efficient spectral upsampling. Computer Graphics Forum (Proceedings of
// Eurographics), 38(2), March 2019. 
//

// run like
// make && ./rgb2mom 256 lut.pfm XYZ && eu lut.pfm -w 1400 -h 1400

// 2D sigmoid as comparison
#define SIGMOID
// #define SIG_SWZ

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "details/lu.h"
#include "details/matrices.h"
#include "mom.h"
#include "clip.h"

#if 0
// okay let's also hack the cie functions to our taste (or the gpu approximations we'll do)
#define CIE_SAMPLES 10
#define CIE_FINE_SAMPLES 10
#define CIE_LAMBDA_MIN 400.0
#define CIE_LAMBDA_MAX 700.0
#else

#include "details/cie1931.h"
/// Discretization of quadrature scheme
#define CIE_FINE_SAMPLES ((CIE_SAMPLES - 1) * 3 + 1)
#endif
#define RGB2SPEC_EPSILON 1e-4
#define MOM_EPS 1e-3

/// Precomputed tables for fast spectral -> RGB conversion
double lambda_tbl[CIE_FINE_SAMPLES],
       phase_tbl[CIE_FINE_SAMPLES],
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

// gauss blur a spectrum explicitly:
static inline void gauss_blur(
    const double  sigma_nm,         // in nanometers
    const double *spectrum,
    double       *spectrum_blur,
    const int     cnt)
{
  const double sigma = sigma_nm * cnt / (double)CIE_FINE_SAMPLES; // in bin widths
  const int r = 3*sigma;
  double max = 0.0;
  for(int i=0;i<cnt;i++) spectrum_blur[i] = 0.0;
  for(int i=0;i<cnt;i++)
  {
    double w = 0.0;
    for(int j=-r;j<=r;j++)
    {
      if(i+j < 0 || i+j >= cnt) continue;
      double wg = exp(-j*j / (2.0*sigma*sigma));
      spectrum_blur[i] += spectrum[i+j] * wg;
      w += wg;
    }
    spectrum_blur[i] /= w;
    max = fmax(max, spectrum_blur[i]);
  } // end gauss blur the spectrum loop
  for(int i=0;i<cnt;i++) spectrum_blur[i] /= max;
}

double sqrd(double x) { return x * x; }

void quantise_coeffs(double coeffs[3], float out[3])
{
#ifdef SIGMOID
  // account for normalising lambda:
  double c0 = 360.0, c1 = 1.0 / (830.0 - 360.0);
  double A = coeffs[0], B = coeffs[1], C = coeffs[2];

  out[0] = (float)(A*(sqrd(c1)));
  out[1] = (float)(B*c1 - 2*A*c0*(sqrd(c1)));
  out[2] = (float)(C - B*c0*c1 + A*(sqrd(c0*c1)));
#if 0
  // convert to c0 y dom-lambda:
  A = out[0]; B = out[1]; C = out[2];
  out[0] = A;                        // square slope stays
  out[2] = B / (-2.0*A);             // dominant wavelength
  out[1] = C - A * out[2] * out[2];  // y

  // XXX visualise abs:
  // out[0] = fabsf(out[0]); // goes from 1.0/256.0 (spectral locus) .. 0 (purple ridge through white)
  // out[1] = fabsf(out[1]); // somewhat useful from 0..large purple ridge..spectral locus, but high-low-high for purple tones
  // out[1] = -out[1];
#endif
#if 0
  // convert to shift width slope:
  A = out[0]; B = out[1]; C = out[2];
  // TODO: if 4ac - b^2 > 0:
  int firstcase = 4*A*C - B*B > 0.0;
  if(firstcase)
  {
    out[0] = - sqrt(4*A*C - B*B) / 2.0;   // FIXME something with the signs i don't get
    out[1] = - sqrt(4*A*C - B*B) / (2.0*A);
    out[2] = - B / (2.0 * A);  // dominant wavelength
  } else {
    out[0] = - sqrt(B*B - 4*A*C) / 2.0;
    out[1] = - sqrt(B*B - 4*A*C) / (2.0*A);
    out[2] = - B / (2.0 * A);
  }
  {
  const double slope = out[0], width = out[1], dom_lambda = out[2];
  // TODO: if first case
  double c0, c1, c2;
  c0 = slope/width;
  c1 = -2.0*c0*dom_lambda;
  c2 = c0 * (dom_lambda*dom_lambda - width*width);
  if(4*c0*c2 > c1*c1)
    c2 = slope * width + c0 * dom_lambda*dom_lambda;
  // if(A != 0 || B != 0 || C != 0) fprintf(stderr, "input: %g %g %g\n", slope, width, dom_lambda);
  if(A != 0 || B != 0 || C != 0) fprintf(stderr, "roundtrip: %g %g %g -- %g %g %g \n", A, B, C, c0, c1, c2);
  }
  // slope = +/- sqrt(4 a c - b^2) / 2
  // width = +/- sqrt(4 a c - b^2) / (2a)
  // dlamb = - b / (2a)
  // DEBUG:
  // out[2] = (out[2] - CIE_LAMBDA_MIN) / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN); /* Scale lambda to 0..1 range */
#endif
#else
  out[0] = coeffs[0];
  out[1] = coeffs[1];
  out[2] = coeffs[2];
#endif
}

void init_coeffs(double coeffs[3])
{
#ifdef SIGMOID
#ifdef SIG_SWZ
  coeffs[0] = 0.1;
  coeffs[1] = 80.0;
  coeffs[2] = 550.0;
#else
  coeffs[0] = 0.0;
  coeffs[1] = 0.0;
  coeffs[2] = 0.0;
#endif
#else
  coeffs[0] = 0.5;
  coeffs[1] = 0.0;
  coeffs[2] = 0.0;
#endif
}

void clamp_coeffs(double coeffs[3])
{
#ifdef SIGMOID
#ifdef SIG_SWZ
  if(coeffs[2] < 200)  coeffs[2] = 200;
  if(coeffs[2] > 1000) coeffs[2] = 1000;
  if(coeffs[1] < 1e-5) coeffs[1] = 1e-5;
  if(coeffs[1] > 400.0) coeffs[1] = 400.0;
  if(coeffs[0] < -100.0) coeffs[0] = -100.0;
  if(coeffs[0] >  100.0) coeffs[0] =  100.0;
#else
  double max = fmax(fmax(fabs(coeffs[0]), fabs(coeffs[1])), fabs(coeffs[2]));
  if (max > 1000) {
    for (int j = 0; j < 3; ++j)
      coeffs[j] *= 1000 / max;
  }
#endif
#else
  const double ceps = MOM_EPS;
  coeffs[0] = fmin(fmax(coeffs[0],  ceps), 1.0-ceps);
  coeffs[1] = fmin(fmax(coeffs[1], -1.0/M_PI+ceps), 1.0/M_PI-ceps);
  coeffs[2] = fmin(fmax(coeffs[2], -1.0/M_PI+ceps), 1.0/M_PI-ceps);
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
  return spectrum_outside(x, y);
}

#if 0
// compute residual in cie lab. this may or may not be a good idea.
// it's more "perceptual" but may lead to negative L for extreme colours.
void cie_lab(double *p) {
    double X = 0.0, Y = 0.0, Z = 0.0,
      Xw = xyz_whitepoint[0],
      Yw = xyz_whitepoint[1],
      Zw = xyz_whitepoint[2];

    for (int j = 0; j < 3; ++j) {
        X += p[j] * rgb_to_xyz[0][j];
        Y += p[j] * rgb_to_xyz[1][j];
        Z += p[j] * rgb_to_xyz[2][j];
    }

    auto f = [](double t) -> double {
        double delta = 6.0 / 29.0;
        if (t > delta*delta*delta)
            return cbrt(t);
        else
            return t / (delta*delta * 3.0) + (4.0 / 29.0);
    };

    p[0] = 116.0 * f(Y / Yw) - 16.0;
    p[1] = 500.0 * (f(X / Xw) - f(Y / Yw));
    p[2] = 200.0 * (f(Y / Yw) - f(Z / Zw));
}
#endif


// Journal of Computer Graphics Techniques, Simple Analytic Approximations to
// the CIE XYZ Color Matching Functions Vol. 2, No. 2, 2013 http://jcgt.org
//Inputs:  Wavelength in nanometers
double xFit_1931( double wave )
{
  double t1 = (wave-442.0)*((wave<442.0)?0.0624:0.0374);
  double t2 = (wave-599.8)*((wave<599.8)?0.0264:0.0323);
  double t3 = (wave-501.1)*((wave<501.1)?0.0490:0.0382);
  return 0.362*exp(-0.5*t1*t1) + 1.056*exp(-0.5*t2*t2)- 0.065*exp(-0.5*t3*t3);
}
double yFit_1931( double wave )
{
  double t1 = (wave-568.8)*((wave<568.8)?0.0213:0.0247);
  double t2 = (wave-530.9)*((wave<530.9)?0.0613:0.0322);
  return 0.821*exp(-0.5*t1*t1) + 0.286*exp(-0.5*t2*t2);
}
double zFit_1931( double wave )
{
  double t1 = (wave-437.0)*((wave<437.0)?0.0845:0.0278);
  double t2 = (wave-459.0)*((wave<459.0)?0.0385:0.0725);
  return 1.217*exp(-0.5*t1*t1) + 0.681*exp(-0.5*t2*t2);
}

/**
 * This function precomputes tables used to convert arbitrary spectra
 * to RGB (either sRGB or ProPhoto RGB)
 *
 * A composite quadrature rule integrates the CIE curves, reflectance, and
 * illuminant spectrum over each 5nm segment in the 360..830nm range using
 * Simpson's 3/8 rule (4th-order accurate), which evaluates the integrand at
 * four positions per segment. While the CIE curves and illuminant spectrum are
 * linear over the segment, the reflectance could have arbitrary behavior,
 * hence the extra precations.
 */
void init_tables(Gamut gamut) {
    memset(rgb_tbl, 0, sizeof(rgb_tbl));
    memset(xyz_whitepoint, 0, sizeof(xyz_whitepoint));

    double h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (CIE_FINE_SAMPLES - 1);

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

    double norm = 0.0, n2[3] = {0.0};
    for (int i = 0; i < CIE_FINE_SAMPLES; ++i) {

#if 1
        double lambda = CIE_LAMBDA_MIN + i * h;
        double xyz[3] = { cie_interp(cie_x, lambda),
                          cie_interp(cie_y, lambda),
                          cie_interp(cie_z, lambda) },
               I = cie_interp(illuminant, lambda);
#else
        // double lambda = CIE_LAMBDA_MIN + (i+0.5) * h;
        double lambda = CIE_LAMBDA_MIN + i * h;
        double xyz[3] = {
            xFit_1931(lambda),
            yFit_1931(lambda),
            zFit_1931(lambda), },
               I = cie_interp(illuminant, lambda);
             // I = blackbody_radiation(lambda, 6504.0);
#endif
        norm += I;

#if 1
        double weight = 3.0 / 8.0 * h;
        if (i == 0 || i == CIE_FINE_SAMPLES - 1)
            ;
        else if ((i - 1) % 3 == 2)
            weight *= 2.f;
        else
            weight *= 3.f;
#else
        double weight = h;
#endif

        lambda_tbl[i] = lambda;
        phase_tbl[i] = mom_warp_lambda(lambda);
        for (int k = 0; k < 3; ++k)
            for (int j = 0; j < 3; ++j)
                rgb_tbl[k][i] += xyz_to_rgb[k][j] * xyz[j] * I * weight;

        for (int k = 0; k < 3; ++k)
            xyz_whitepoint[k] += xyz[k] * I * weight;
        for (int k = 0; k < 3; ++k)
            n2[k] += xyz[k] * weight;
    }
}

#ifdef SIGMOID
#if 0
static inline float rgb2spec_fma(float a, float b, float c) {
    #if defined(__FMA__)
        // Only use fmaf() if implemented in hardware
        return fmaf(a, b, c);
    #else
     return a*b + c;
    #endif
}
#endif
double sigmoid(double x) {
    return 0.5 * x / sqrt(1.0 + x * x) + 0.5;
}
void eval_residual(const double *coeff, const double *rgb, double *residual)
{
    double out[3] = { 0.0, 0.0, 0.0 };

    for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
    {
      // the optimiser doesn't like nanometers.
      // we'll do the normalised lambda thing and later convert when we write out.
#ifndef SIG_SWZ
      double lambda = (lambda_tbl[i] - CIE_LAMBDA_MIN) / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN); /* Scale lambda to 0..1 range */
      double cf[3] = {coeff[0], coeff[1], coeff[2]};
#else
      double lambda = lambda_tbl[i];
      // float x = rgb2spec_fma(rgb2spec_fma(coeff[0], lambda, coeff[1]), lambda, coeff[2]),
      //       y = 1.0f / sqrtf(rgb2spec_fma(x, x, 1.0f));
      // float s = rgb2spec_fma(.5f * x, y, .5f);
      double slope      = coeff[0];
      double width      = coeff[1];
      double dom_lambda = coeff[2];
#if 0 // from alisa's jupyter notebook:
      double s = slope;      // coeff[0]
      double w = width;      // coeff[1]
      double z = dom_lambda; // coeff[2]

      const double t = (fabs(s) * w + sqrt(s*s*w*w + 1.0/9.0) ) / (2.0*fabs(s)*w);
      const double sqrt_t = sqrt(t);
      const double c0 = s * sqrt_t*sqrt_t*sqrt_t / w;
      const double c1 = -2.0 * c0 * z;
      const double c2 = c0 * z*z + s*w*sqrt_t*(5.0*t - 6.0);
#endif
#if 1 // my simpler version (but forgot what the values mean)
      const double c0 = slope/width;
      const double c1 = -2.0*c0*dom_lambda;
      const double c2 = slope * width + c0 * dom_lambda*dom_lambda;
      // this has the advantage that mathematica can invert it:
      // slope = +/- sqrt(4 a c - b^2) / 2
      // width = +/- sqrt(4 a c - b^2) / (2a)
      // dlamb = - b / (2a)
#endif
      double cf[3] = {c0, c1, c2};
#endif

      { // scope
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
    }
    // cie_lab(out);
    memcpy(residual, rgb, sizeof(double) * 3);
    // cie_lab(residual);

    for (int j = 0; j < 3; ++j)
        residual[j] -= out[j];
}
#else
void eval_residual(const double *coeffs, const double *rgb, double *residual) {
    double out[3] = { 0.0, 0.0, 0.0 };

    float cf[3] = {(float)coeffs[0], (float)coeffs[1], (float)coeffs[2]};
    float_complex exp_mom[3], eval_poly[3];
    prepareReflectanceSpectrumReal3(exp_mom, eval_poly, cf);
    if(!(eval_poly[0].x == eval_poly[0].x) ||
       !(eval_poly[1].x == eval_poly[1].x) ||
       !(eval_poly[2].x == eval_poly[2].x) ||
       !(eval_poly[0].y == eval_poly[0].y) ||
       !(eval_poly[1].y == eval_poly[1].y) ||
       !(eval_poly[2].y == eval_poly[2].y))
    {
      fprintf(stderr, "c %g %g %g\n", cf[0], cf[1], cf[2]);
      residual[0] = residual[1] = residual[2] = 666.0;
      return;
    }
    assert(eval_poly[0].x == eval_poly[0].x);
    assert(eval_poly[1].x == eval_poly[1].x);
    assert(eval_poly[2].x == eval_poly[2].x);
    assert(eval_poly[0].y == eval_poly[0].y);
    assert(eval_poly[1].y == eval_poly[1].y);
    assert(eval_poly[2].y == eval_poly[2].y);
    for (int i = 0; i < CIE_FINE_SAMPLES; ++i)
    {
        double phase = phase_tbl[i];
        double s = evaluateReflectanceSpectrum3(phase, exp_mom, eval_poly);
        if(!(s==s))
        {
          fprintf(stderr, "lambda %g %d\n", lambda_tbl[i], i);
          fprintf(stderr, "c %g %g %g\n", cf[0], cf[1], cf[2]);
          fprintf(stderr, "ep %g %g %g %g %g %g\n",
              eval_poly[0].x, eval_poly[0].y,
              eval_poly[1].x, eval_poly[1].y,
              eval_poly[2].x, eval_poly[2].y);
          float_complex circlePoint=(float_complex){cosf(phase),sinf(phase)};
          float_complex herglotzTransform=evaluateFastHerglotzTransform3(circlePoint,exp_mom, eval_poly);
          fprintf(stderr, "h %g %g\n", herglotzTransform.x, herglotzTransform.y);
          float_complex conjCirclePoint=conjugate(circlePoint);
          float polynomial2=eval_poly[0].x;
          float_complex polynomial1=add(eval_poly[1],multiply_mixed(polynomial2,conjCirclePoint));
          float_complex polynomial0=add(eval_poly[2],multiply(conjCirclePoint,polynomial1));
          float_complex dotProduct=add(multiply(polynomial1,exp_mom[1]),multiply_mixed(polynomial2,exp_mom[2]));
          fprintf(stderr, "poly %g %g %g %g %g\n",
              polynomial0.x, polynomial0.y, polynomial1.x,
              polynomial1.y, polynomial2);
          fprintf(stderr, "circle %g %g\n", circlePoint.x, circlePoint.y);
          residual[0] = residual[1] = residual[2] = 666.0;
          return;
          // continue;

        }
        
        assert(s==s);

        /* Integrate against precomputed curves */
        for (int j = 0; j < 3; ++j)
            out[j] += rgb_tbl[j][i] * s;
    }
    // cie_lab(out);
    memcpy(residual, rgb, sizeof(double) * 3);
    // cie_lab(residual);

    for (int j = 0; j < 3; ++j)
        residual[j] -= out[j];
}
#endif

void eval_jacobian(const double *coeffs, const double *rgb, double **jac) {
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
  int it = 15;
    double r = 0;
    for (int i = 0; i < it; ++i) {
        double J0[3], J1[3], J2[3], *J[3] = { J0, J1, J2 };

        double residual[3];

        clamp_coeffs(coeffs);
        eval_residual(coeffs, rgb, residual);
        eval_jacobian(coeffs, rgb, J);

#if 0
        // fix boundary issues when the coefficients do not change any more (some colours may be outside the representable range)
        const double eps = 1e-6;
        for(int j=0;j<3;j++)
        {
          if(fabs(J0[j]) < eps) J0[j] = ((drand48() > 0.5) ? 1.0 : -1.0)*eps*(0.5 + drand48());
          if(fabs(J1[j]) < eps) J1[j] = ((drand48() > 0.5) ? 1.0 : -1.0)*eps*(0.5 + drand48());
          if(fabs(J2[j]) < eps) J2[j] = ((drand48() > 0.5) ? 1.0 : -1.0)*eps*(0.5 + drand48());
        }
#endif

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

static Gamut parse_gamut(const char *str)
{
  if(!strcasecmp(str, "sRGB"))
    return SRGB;
  if(!strcasecmp(str, "eRGB"))
    return ERGB;
  if(!strcasecmp(str, "XYZ"))
    return XYZ;
  if(!strcasecmp(str, "ProPhotoRGB"))
    return ProPhotoRGB;
  if(!strcasecmp(str, "ACES2065_1"))
    return ACES2065_1;
  if(!strcasecmp(str, "ACES_AP1"))
    return ACES_AP1;
  if(!strcasecmp(str, "REC2020"))
    return REC2020;
  return SRGB;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Syntax: rgb2spec_opt <resolution> <output> [<gamut>]\n"
               "where <gamut> is one of sRGB,eRGB,XYZ,ProPhotoRGB,ACES2065_1,ACES_AP1,REC2020\n");
        exit(-1);
    }
    Gamut gamut = SRGB;
    if(argc > 3) gamut = parse_gamut(argv[3]);
    init_tables(gamut);

    // TODO: only output 2D table for emission
    // TODO: scaling rgb to max <<1.0 blends over to burg entropy as used by unbounded mese

    const int res = atoi(argv[1]); // resolution of cube
#if 0
    // parse settings that work out (64 or 256)
    // this constraint is for 3D
    if (res != 64 && res != 256) {
        printf("invalid resolution! currenly only supporting 64 and 256\n");
        exit(-1);
    }
#endif

    printf("Optimizing ");

#if 1 // 2D
    // read grey map from macadam:
    int max_w, max_h;
    float *max_b = 0;
    {
      // convert macad.pfm -fx 'r' -colorspace Gray -blur 15x15 brightness.pfm
      FILE *f = fopen("brightness.pfm", "rb");
      if(!f)
      {
        fprintf(stderr, "could not read macadam.pfm!!\n");
        exit(2);
      }
      fscanf(f, "Pf\n%d %d\n%*[^\n]", &max_w, &max_h);
      max_b = calloc(sizeof(float), max_w*max_h);
      fgetc(f); // \n
      fread(max_b, sizeof(float), max_w*max_h, f);
      fclose(f);
    }

    size_t bufsize = 5*res*res;
    float *ping = (float*)calloc(sizeof(float), bufsize);
    float *pong = (float*)calloc(sizeof(float), bufsize);
    float *out = ping;
#if defined(_OPENMP)
#pragma omp parallel for schedule(dynamic) shared(stdout,out,max_b,max_w,max_h)
#endif
  for (int j = 0; j < res; ++j)
  {
    const double y = (res - 1 - (j+0.5)) / (double)res;
    printf(".");
    fflush(stdout);
    for (int i = 0; i < res; ++i)
    {
      const double x = (i+0.5) / (double)res;
      double rgb[3];
      // range of fourier moments is [0,1]x[-1/pi,+1/pi]^2
      double coeffs[3];
      init_coeffs(coeffs);
      // normalise to max(rgb)=1
      rgb[0] = x;
      rgb[1] = y;
      rgb[2] = 1.0-x-y;
      if(check_gamut(rgb)) continue;
      // TODO: don't ask the optimizer impossible things. let's ask for brightness as in macadam/2 or so
#if 0
      // and then use 0.9 times what's in that.
      double m = 1.0/fmax(fmax(1.0, rgb[0]), fmax(rgb[1], rgb[2]));
      double resid;
      double lo = 0.0, hi = m;
      // binary search for max brightness
      for(int k=0;k<20;k++)
      {
          // coeffs[0] = 0.5; coeffs[1] = 0.0; coeffs[2] = 0.0;
        double rgbm[3] = {rgb[0] * m, rgb[1] * m, rgb[2] * m};
        resid = gauss_newton(rgbm, coeffs);
        // if residual too large, try lower brightness
        if(resid > pow(2.0,-10.0))
        {
          hi = m;
          coeffs[0] = (hi+lo)/2.0; coeffs[1] = 0.0; coeffs[2] = 0.0;
        }
        // else if(m > 0.9) break;
        else if(k > 18) break;
        else lo = m*0.9;
        m = (lo + hi)/2.0;
      }
#endif
      int ii = (int)fmin(max_w - 1, fmax(0, i * (max_w / (double)res)));
      int jj = max_h - 1 - (int)fmin(max_h - 1, fmax(0, j * (max_h / (double)res)));
      double m = fmax(0.001, 0.5*max_b[ii + max_w * jj]);
      double rgbm[3] = {rgb[0] * m, rgb[1] * m, rgb[2] * m};
      double resid = gauss_newton(rgbm, coeffs);

#if 1
      // TODO: now that we have a good spectrum:
      // explicitly instantiate it
      // explicitly gauss blur it
      // convert back to xy
      // store pointer to this other pixel
      const int cnt = CIE_FINE_SAMPLES;
      double spectrum[cnt];
      double spectrum_blur[cnt];
      for (int l = 0; l < cnt; l++)
      {
        double lambda = l/(cnt-1.0);
        // double lambda = (lambda_tbl[l] - CIE_LAMBDA_MIN) / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN); /* Scale lambda to 0..1 range */
        double x = 0.0;
        for (int i = 0; i < 3; ++i)
          x = x * lambda + coeffs[i];
        spectrum[l] = sigmoid(x);
      }
      const double sigma = 9.0; // FIXME: < 9 results in banding, > 12 results in second attractor
      gauss_blur(sigma, spectrum, spectrum_blur, cnt);
      double col[3] = {0.0};
#if 1 // cnt = CIE_FINE_SAMPLES
      for (int l = 0; l < cnt; l++)
        for (int j = 0; j < 3; ++j)
          col[j] += rgb_tbl[j][l] * spectrum_blur[l];
#else // otherwise
      for (int l = 0; l < cnt; l++)
      {
        double lambda = CIE_LAMBDA_MIN + l/(cnt-1.0) * (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);
        double xyz[3] = { cie_interp(cie_x, lambda),
                          cie_interp(cie_y, lambda),
                          cie_interp(cie_z, lambda) };
        col[0] += xyz[0] * spectrum_blur[l];
        col[1] += xyz[1] * spectrum_blur[l];
        col[2] += xyz[2] * spectrum_blur[l];
      }
#endif
      // col is in XYZ, we want the chromaticity coordinates:
      double b = col[0]+col[1]+col[2];
      // velocity vector:
      double velx = col[0] / b - x;
      double vely = col[1] / b - y;
      double speed = sqrt(velx*velx + vely*vely);
      velx /= speed;
      vely /= speed;
      velx = 0.5 + 0.5*velx;
      vely = 0.5 + 0.5*vely;
#endif

      int idx = j*res + i;
      if(coeffs[0] < m)
      {
        out[5*idx + 0] = coeffs[0];
        out[5*idx + 1] = coeffs[1];
        out[5*idx + 2] = coeffs[2];
      }
      else
      {
        out[5*idx + 0] = 0.0;
        out[5*idx + 1] = 0.0;
        out[5*idx + 2] = 0.0;
      }
      if(out[5*idx + 0] == 0.0)
        out[5*idx + 1] = out[5*idx + 2] = 0.0;
      out[5*idx + 0] = speed;
      out[5*idx + 3] = velx;//m;
      out[5*idx + 4] = vely;//resid;
    }
  }
// #if 1 // smoothing passes. seems only moments require it
#ifndef SIGMOID
  for(int it=0;it<10;it++) // make sure this is < even
  {
    float *in = 0;
    if(it & 1)
    {
      out = ping;
      in  = pong;
    }
    else
    {
      out = pong;
      in  = ping;
    }
    memcpy(out, in, sizeof(float)*bufsize);
#if defined(_OPENMP)
#pragma omp parallel for schedule(dynamic) shared(stdout,out,in)
#endif
  for (int j = 0; j < res; ++j)
  {
    const double y = (res - 1 - (j+0.5)) / (double)res;
    printf(".");
    fflush(stdout);
    for (int i = 0; i < res; ++i)
    {
      const double x = (i+0.5) / (double)res;
      double rgb[3];
      // range of fourier moments is [0,1]x[-1/pi,+1/pi]^2
      double coeffs[3];
      init_coeffs(coeffs);
      int idx = j*res + i;
      // normalise to max(rgb)=1
      rgb[0] = x;
      rgb[1] = y;
      rgb[2] = 1.0-x-y;
      if(check_gamut(rgb)) continue;
      double m = 0.0; // try average nb brightness
      for(int jj=fmax(0,j-1);jj<=fmin(res-1,j+1);jj++)
      for(int ii=fmax(0,i-1);ii<=fmin(res-1,i+1);ii++)
      {
        int idx2 = jj*res + ii;
        m += in[5*idx2 + 3]/9.0;
      }
      for(int jj=fmax(0,j-1);jj<=fmin(res-1,j+1);jj++)
      for(int ii=fmax(0,i-1);ii<=fmin(res-1,i+1);ii++)
      {
        int idx2 = jj*res + ii;
        {
          coeffs[0] = in[5*idx2 + 0];
          coeffs[1] = in[5*idx2 + 1];
          coeffs[2] = in[5*idx2 + 2];
          // m         = in[5*idx2 + 3]*0.95;
        }

        double rgbm[3] = {rgb[0] * m, rgb[1] * m, rgb[2] * m};
        double resid = gauss_newton(rgbm, coeffs);
        if(resid < out[5*idx+4] && coeffs[0] < m)
        {
          out[5*idx + 0] = coeffs[0];
          out[5*idx + 1] = coeffs[1];
          out[5*idx + 2] = coeffs[2];
          out[5*idx + 3] = m;
          out[5*idx + 4] = resid;
        }
      }
    }
  }
  }
#endif
  FILE *f = fopen(argv[2], "wb");
  if(f)
  {
    fprintf(f, "PF\n%d %d\n-1.0\n", res, res);
    for(int k=0;k<res*res;k++)
    {
      double coeffs[3] = {ping[5*k+0], ping[5*k+1], ping[5*k+2]};
      float q[3];
      quantise_coeffs(coeffs, q);
#if 0 // data
      fwrite(q, sizeof(float), 3, f);
#else // debug
      fwrite(ping+5*k+0, sizeof(float), 1, f);
      fwrite(ping+5*k+3, sizeof(float), 2, f);
#endif
    }
    fclose(f);
  }
  free(ping);
  free(pong);
#else // 3D
    const uint32_t tiles = sqrt(res);   // number of tiles
    const uint32_t wd = res * tiles;    // resolution of 2D tiled image
    size_t bufsize = 3*res*res*res;
    float *out = new float[bufsize];

#if defined(_OPENMP)
#pragma omp parallel for schedule(dynamic) shared(stdout,out)
#endif
  for (int j = 0; j < res; ++j)
  {
    const double z = (j+0.5) / double(res);
    printf(".");
    fflush(stdout);
    for (int i = 0; i < res; ++i)
    {
      const double x = (i+0.5) / double(res);
      double rgb[3];
      // range of fourier moments is [0,1]x[-1/pi,+1/pi]^2
      double coeffs[3] = {0.5, 0.0, 0.0};

      const int start = res / 3;

      for (int k = start; k < res; ++k)
      {
        double y = (double) (k+0.5) / double(res);

        rgb[0] = x;
        rgb[1] = y;
        rgb[2] = z;

        double resid = gauss_newton(rgb, coeffs);
        (void) resid;

        int idx = (j*res + k)*res+i;

        out[3*idx + 0] = coeffs[0];
        out[3*idx + 1] = coeffs[1];
        out[3*idx + 2] = coeffs[2];
        // out[3*idx + 2] = resid; // DEBUG: visualise residual
      }
      coeffs[0] = 0.5;
      coeffs[1] = 0.0;
      coeffs[2] = 0.0;
      for (int k = start-1; k >= 0; --k)
      {
        double y = (double) (k+0.5) / double(res);

        rgb[0] = x;
        rgb[1] = y;
        rgb[2] = z;

        double resid = gauss_newton(rgb, coeffs);
        (void) resid;

        int idx = (j*res + k)*res+i;

        out[3*idx + 0] = coeffs[0];
        out[3*idx + 1] = coeffs[1];
        out[3*idx + 2] = coeffs[2];
        // out[3*idx + 2] = resid; // DEBUG: visualise residual
      }
    }
  }

  FILE *f = fopen(argv[2], "wb");
  if (f == 0)
    throw std::runtime_error("Could not create file!");

  fprintf(f, "PF\n%d %d\n-1.0\n", wd, wd);
  // now swizzle into tiled 2D layout
  for(int j=0;j<wd;j++) for(int i=0;i<wd;i++)
  {
    int tx = i / res, ty = j / res;
    int ti = i - tx * res, tj = j - ty * res;
    int idx = ((tiles*ty+tx)*res + tj)*res+ti;
    fwrite(out + 3*idx, sizeof(float), 3, f);
  }
  delete[] out;
  fclose(f);
#endif
  printf("\n");
}
