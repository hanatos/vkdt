// this is the coefficient cube optimiser from the paper:
//
// Wenzel Jakob and Johannes Hanika. A low-dimensional function space for
// efficient spectral upsampling. Computer Graphics Forum (Proceedings of
// Eurographics), 38(2), March 2019. 
//
// the difference is that we want to produce a single 3D cube as output lut.
// for easy texture handling, we store it as a tiled 2D texture,
// say 512 = 64x64 x 8x8 or 4096 = 256x256 x 16x16.

// #if defined(_MSC_VER)
// #  define NOMINMAX
// #endif

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <algorithm>

#include "details/lu.h"
#include "details/matrices.h"

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

/// Precomputed tables for fast spectral -> RGB conversion
double lambda_tbl[CIE_FINE_SAMPLES],
       rgb_tbl[3][CIE_FINE_SAMPLES],
       rgb_to_xyz[3][3],
       xyz_to_rgb[3][3],
       xyz_whitepoint[3];

/// Currently supported gamuts
enum Gamut {
    SRGB,
    ProPhotoRGB,
    ACES2065_1,
    ACES_AP1,
    REC2020,
    ERGB,
    XYZ,
};

double sigmoid(double x) {
    return 0.5 * x / std::sqrt(1.0 + x * x) + 0.5;
}

double smoothstep(double x) {
    return x * x * (3.0 - 2.0 * x);
}

double sqr(double x) { return x * x; }

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

double blackbody_radiation(
    double lambda,      // in [nm]
    double temperature) // in [K]
{
  const double h = 6.62606957e-34; // Planck's constant [J s]
  const double c = 299792458.0;    // speed of light [m/s]
  const double k = 1.3807e-23;     // Boltzmann's constant [J/K]
  const double lambda_m = lambda*1e-9; // lambda [m]
  const double lambda2 = lambda_m*lambda_m;
  const double lambda5 = lambda2*lambda_m*lambda2;
  const double c1 = 2. * h * c * c / lambda5;
  const double c2 = h * c / (lambda_m * temperature * k);
  // convert to spectral radiance in [W/m^2 / sr / nm]
  return 2.21566e-16 * c1 / (exp(c2)-1.0); // such that it integrates y to 1.0
}


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

    const double *illuminant = nullptr;

    switch (gamut) {
        case SRGB:
            // illuminant = cie_d65;
            memcpy(xyz_to_rgb, xyz_to_srgb, sizeof(double) * 9);
            memcpy(rgb_to_xyz, srgb_to_xyz, sizeof(double) * 9);
            break;

        case ERGB:
            // illuminant = cie_e;
            memcpy(xyz_to_rgb, xyz_to_ergb, sizeof(double) * 9);
            memcpy(rgb_to_xyz, ergb_to_xyz, sizeof(double) * 9);
            break;

        case XYZ:
            // illuminant = cie_e;
            memcpy(xyz_to_rgb, xyz_to_xyz, sizeof(double) * 9);
            memcpy(rgb_to_xyz, xyz_to_xyz, sizeof(double) * 9);
            break;

        case ProPhotoRGB:
            // illuminant = cie_d50;
            memcpy(xyz_to_rgb, xyz_to_prophoto_rgb, sizeof(double) * 9);
            memcpy(rgb_to_xyz, prophoto_rgb_to_xyz, sizeof(double) * 9);
            break;

        case ACES2065_1:
            // illuminant = cie_d60;
            memcpy(xyz_to_rgb, xyz_to_aces2065_1, sizeof(double) * 9);
            memcpy(rgb_to_xyz, aces2065_1_to_xyz, sizeof(double) * 9);
            break;

        case ACES_AP1:
            // illuminant = cie_d60;
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
        double lambda = CIE_LAMBDA_MIN + (i+0.5) * h;
        double xyz[3] = {
            xFit_1931(lambda),
            yFit_1931(lambda),
            zFit_1931(lambda), },
             I = blackbody_radiation(lambda, 6504.0);
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
        for (int k = 0; k < 3; ++k)
            for (int j = 0; j < 3; ++j)
                rgb_tbl[k][i] += xyz_to_rgb[k][j] * xyz[j] * I * weight;

        for (int k = 0; k < 3; ++k)
            xyz_whitepoint[k] += xyz[k] * I * weight;
        for (int k = 0; k < 3; ++k)
            n2[k] += xyz[k] * weight;
    }
    // fprintf(stderr, "norm = %g\n", 1.0/norm);
    fprintf(stderr, "wp = %g %g %g\n", 1.0/xyz_whitepoint[0], 1.0/xyz_whitepoint[1], 1.0/xyz_whitepoint[2]);
    fprintf(stderr, "cie norms = %g %g %g\n", n2[0], n2[1], n2[2]);
    double n3[3] = {0.0};
    const int num_l = 10;
    for(int i=0;i<num_l;i++)
    {
      const double norm[3] = {0.354678, 0.355529, 0.34371};
      const double lambda = 400.0 + 300.0 * (i+0.5)/num_l;
      double xyz[3] = {
        xFit_1931(lambda),
        yFit_1931(lambda),
        zFit_1931(lambda), };
      double I = blackbody_radiation(lambda, 6504.0);
      for(int k=0;k<3;k++)
        n3[k] += I * xyz[k] / norm[k] / num_l / 0.0093498;
    }
    fprintf(stderr, "cie norms = %g %g %g\n", n3[0], n3[1], n3[2]);
}

void eval_residual(const double *coeffs, const double *rgb, double *residual) {
    double out[3] = { 0.0, 0.0, 0.0 };

    for (int i = 0; i < CIE_FINE_SAMPLES; ++i) {
        /* Scale lambda to 0..1 range */
        double lambda = (lambda_tbl[i] - CIE_LAMBDA_MIN) /
                        (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);

        /* Polynomial */
        double x = 0.0;
        for (int i = 0; i < 3; ++i)
            x = x * lambda + coeffs[i];

        /* Sigmoid */
        double s = sigmoid(x);

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

void eval_jacobian(const double *coeffs, const double *rgb, double **jac) {
    double r0[3], r1[3], tmp[3];

    for (int i = 0; i < 3; ++i) {
        memcpy(tmp, coeffs, sizeof(double) * 3);
        tmp[i] -= RGB2SPEC_EPSILON;
        eval_residual(tmp, rgb, r0);

        memcpy(tmp, coeffs, sizeof(double) * 3);
        tmp[i] += RGB2SPEC_EPSILON;
        eval_residual(tmp, rgb, r1);

        for (int j = 0; j < 3; ++j)
            jac[j][i] = (r1[j] - r0[j]) * 1.0 / (2 * RGB2SPEC_EPSILON);
    }
}

double gauss_newton(const double rgb[3], double coeffs[3], int it = 15) {
    double r = 0;
    for (int i = 0; i < it; ++i) {
        double J0[3], J1[3], J2[3], *J[3] = { J0, J1, J2 };

        double residual[3];

        eval_residual(coeffs, rgb, residual);
        eval_jacobian(coeffs, rgb, J);

        int P[4];
        int rv = LUPDecompose(J, 3, 1e-15, P);
        if (rv != 1) {
            std::cout << "RGB " << rgb[0] << " " << rgb[1] << " " << rgb[2] << std::endl;
            std::cout << "-> " << coeffs[0] << " " << coeffs[1] << " " << coeffs[2] << std::endl;
            throw std::runtime_error("LU decomposition failed!");
        }

        double x[3];
        LUPSolve(J, P, residual, 3, x);

        r = 0.0;
        for (int j = 0; j < 3; ++j) {
            coeffs[j] -= x[j];
            r += residual[j] * residual[j];
        }
        double max = std::max(std::max(coeffs[0], coeffs[1]), coeffs[2]);

        if (max > 200) {
            for (int j = 0; j < 3; ++j)
                coeffs[j] *= 200 / max;
        }

        if (r < 1e-6)
            break;
    }
    return std::sqrt(r);
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

    // TODO: parse settings that work out (64 or 256)
    const int res = atoi(argv[1]); // resolution of cube
    const uint32_t tiles = sqrt(res);   // number of tiles
    const uint32_t wd = res * tiles;    // resolution of 2D tiled image
    if (res != 64 && res != 256) {
        printf("invalid resolution! currenly only supporting 64 and 256\n");
        exit(-1);
    }

    printf("Optimizing ");

    size_t bufsize = 3*res*res*res;
    float *out = new float[bufsize];

#if defined(_OPENMP)
#pragma omp parallel for default(none) schedule(dynamic) shared(stdout,out)
#endif
  for (int j = 0; j < res; ++j)
  {
    const double y = (j+0.5) / double(res);
    printf(".");
    fflush(stdout);
    for (int i = 0; i < res; ++i) {
      const double x = (i+0.5) / double(res);
      double coeffs[3], rgb[3];
      memset(coeffs, 0, sizeof(double)*3);

      const int start = res / 4;

      for (int k = start; k < res; ++k)
      {
        double z = (double) (k+0.5) / double(res);

        rgb[0] = x;
        rgb[1] = y;
        rgb[2] = z;

        double resid = gauss_newton(rgb, coeffs);
        (void) resid;

        double c0 = 360.0, c1 = 1.0 / (830.0 - 360.0);
        double A = coeffs[0], B = coeffs[1], C = coeffs[2];

        int idx = (k*res + j)*res+i;

        out[3*idx + 0] = float(A*(sqr(c1)));
        out[3*idx + 1] = float(B*c1 - 2*A*c0*(sqr(c1)));
        out[3*idx + 2] = float(C - B*c0*c1 + A*(sqr(c0*c1)));
        // out[3*idx + 2] = resid; // DEBUG: visualise residual
      }
      memset(coeffs, 0, sizeof(double)*3);
      for (int k = start-1; k >= 0; --k)
      {
        double z = (double) (k+0.5) / double(res);

        rgb[0] = x;
        rgb[1] = y;
        rgb[2] = z;

        double resid = gauss_newton(rgb, coeffs);
        (void) resid;

        double c0 = 360.0, c1 = 1.0 / (830.0 - 360.0);
        double A = coeffs[0], B = coeffs[1], C = coeffs[2];

        int idx = (k*res + j)*res+i;

        out[3*idx + 0] = float(A*(sqr(c1)));
        out[3*idx + 1] = float(B*c1 - 2*A*c0*(sqr(c1)));
        out[3*idx + 2] = float(C - B*c0*c1 + A*(sqr(c0*c1)));
        // out[3*idx + 2] = resid; // DEBUG: visualise residual
      }
    }
  }

  FILE *f = fopen(argv[2], "wb");
  if (f == nullptr)
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
  printf("\n");
}
