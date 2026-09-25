// creates spectra.lut (c0*1e5 y l s)/(x y) and abney.lut (x y)/(s l)
#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "core/clip.h"
#include "core/lut.h"
#include "core/inpaint.h"
#include "core/threads.h"
#include "shared/q2t.h"
#include "core/half.h"
#include "core/core.h"
#include "shared/fit-spectra.h"
#include "shared/macadam.h"

void cvt_c0yl_c012(const double *c0yl, double *coeffs)
{
  coeffs[0] = c0yl[0];
  coeffs[1] = c0yl[2] * -2.0 * c0yl[0];
  coeffs[2] = c0yl[1] + c0yl[0] * c0yl[2] * c0yl[2];
}

void cvt_c012_c0yl(const Float *coeffs, Float *c0yl)
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

int check_gamut(Float xyz[3])
{
  // double xyz[3] = {0.0};
  // for(int j=0;j<3;j++)
  //   for(int i=0;i<3;i++)
  //     xyz[i] += rgb_to_xyz[i][j] * rgb[j];
  double x = xyz[0] / (xyz[0] + xyz[1] + xyz[2]);
  double y = xyz[1] / (xyz[0] + xyz[1] + xyz[2]);
  return dt_spectrum_outside(x, y);
}

#if 0
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
#endif

typedef struct parallel_shared_t
{
  float *out;
  float *lsbuf;
  int res;
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
  Float coeffs[4] = {0, 1, 0, d->out[5*item + 3]};
  Float rgb[3] = {x*coeffs[3], y*coeffs[3], (1-x-y)*coeffs[3]};
  if(check_gamut(rgb)) return;

  Float resid = LM(rgb, coeffs, 15);

  Float c0yl[3];
  cvt_c012_c0yl(coeffs, c0yl);

  (void)resid;
  d->out[5*item + 0] = coeffs[0];
  d->out[5*item + 1] = coeffs[1];
  d->out[5*item + 2] = coeffs[2];

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
  d->out[5*item + 3] = (lami+0.5f) / (float)lsres;
  d->out[5*item + 4] = (sati+0.5f) / (float)lsres;
}


int main(int argc, char **argv)
{
  init_tables(0, cie_d65);

  const int res = 512; // resolution of 2d lut
  int lsres = res; // /4;
  float *lsbuf = calloc(sizeof(float), 5*lsres*lsres);

  size_t bufsize = 5*res*res;
  float *out = calloc(sizeof(float), bufsize);

  {
    macadam_t par = (macadam_t) {
      .out   = out,
      .str   = 5,
      .res   = res,
      .iteration = 2, // ignore metamers, cie observer doesn't have problems here
    };
    fprintf(stdout, "computing macadam brightness..\n");
    for(int i=0;i<CIE_FINE_SAMPLES;i++)
      macadam_work(i, &par);

    dt_inpaint_buf_t inpaint_buf = {
      .dat = (float *)out,
      .wd  = res,
      .ht  = res,
      .cpp = 5,
    };
    dt_inpaint(&inpaint_buf);
  }

  printf("optimising ");
  parallel_shared_t par = (parallel_shared_t) {
    .out   = out,
    .lsbuf = lsbuf,
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

  { // scope write abney map on (lambda, saturation)
    dt_inpaint_buf_t inpaint_buf = {
      .dat = (float *)lsbuf,
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
    for(int j=0;j<lsres;j++)
    {
      for(int i=0;i<lsres-2;i++)
      {
        int ki = j*lsres + i, ko = j*lsres + i;
        b16[2*ko+0] = float_to_half(lsbuf[5*ki+0]);
        b16[2*ko+1] = float_to_half(lsbuf[5*ki+1]);
      }
      b16[2*(j*lsres+lsres-2)+0] = float_to_half(bound_rec709  [j]);
      b16[2*(j*lsres+lsres-2)+1] = float_to_half(bound_spectral[j]);
      b16[2*(j*lsres+lsres-1)+0] = float_to_half(bound_rec2020 [j]);
      b16[2*(j*lsres+lsres-1)+1] = float_to_half(bound_spectral[j]);
    }
    dt_lut_header_t head = (dt_lut_header_t) {
      .magic    = dt_lut_header_magic,
      .version  = dt_lut_header_version,
      .channels = 2,
      .datatype = dt_lut_header_f16,
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
  }

  {
    dt_inpaint_buf_t inpaint_buf = {
      .dat = (float *)out,
      .wd  = res,
      .ht  = res,
      .cpp = 5,
    };
    dt_inpaint(&inpaint_buf);
  }
  { // write spectra map: (x,y) |--> sigmoid coeffs + saturation
    dt_lut_header_t head = (dt_lut_header_t) {
      .magic    = dt_lut_header_magic,
      .version  = dt_lut_header_version,
      .channels = 4,
      .datatype = dt_lut_header_f32,
      .wd       = res,
      .ht       = res,
    };
    FILE *f = fopen("spectra.lut", "wb");
    if(f) fwrite(&head, sizeof(head), 1, f);
    for(int k=0;k<res*res;k++)
    {
      double coeffs[3] = {out[5*k+0], out[5*k+1], out[5*k+2]};
      float q[] = {0, 0, 0, out[5*k+4]}; // c0yl works in half, but doesn't interpolate upon lookup :(
      quantise_coeffs(coeffs, q);
      if(f)   fwrite(q, sizeof(float), 4, f);
    }
    if(f) fclose(f);
  }

  free(out);
  free(lsbuf);
  // threads_global_cleanup();
  printf("\n");
}
