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

#include "core/threads.h"
#include "core/inpaint.h"
#include "core/lut.h"
#include "shared/q2t.h"
#include "shared/fit-spectra.h"
#include "shared/macadam.h"

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

void work_fit(uint32_t idx, void *data)
{
  const macadam_t *const d = data;
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
  macadam_t par = (macadam_t) {
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
    macadam_integrate_spec(lambda0, lambda1, xyz);
    const double b = xyz[0]+xyz[1]+xyz[2];
    double x = xyz[0]/b, y = xyz[1]/b;
    tri2quad(&x,&y);
    par.peak_val = b;
    par.peak[0] = x;
    par.peak[1] = y;
  }
  // find max radial extent
  for(int i=0;i<CIE_FINE_SAMPLES;i++)
    macadam_work(i, &par);
  // now rasterise all spectra, ignoring the lower value layer
  // since the peak (white) connects smoothly with the upper/brighter layer (likely less saturated for same chroma)
  par.iteration = 1;
  for(int i=0;i<CIE_FINE_SAMPLES;i++)
    macadam_work(i, &par);

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
  init_tables(ssf_filename, cie_d65);
  create_cfa_table(ssf_filename);
  threads_global_cleanup();
  exit(0);
}
