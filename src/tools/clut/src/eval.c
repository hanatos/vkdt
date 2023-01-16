#include "core/half.h"
#include "core/lut.h"
#include "core/core.h"
#include "cc24.h"
#include "dngproc.h"
#include "matrices.h"
#include "cie1931.h"
#include "spectrum.h"
#include "cfa.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
// TODO: create this input to evaluate: (Nikon D7000 because we have cc24 shots for it)
// [*] dcp from patches
// [*] dcp from adobe
// [ ] spec lut from ssf
// [ ] spec lut from patches
// [ ] spec lut from dcp from patches
// [ ] spec lut from dcp from adobe

static inline int
find_whitest(
    const float *ref,
    const float *res,
    const int    cnt)
{
  float min_sat = FLT_MAX;
  int min_i = -1;
  for(int i=0;i<cnt;i++)
  {
    const float s = MAX(ref[3*i+0], MAX(ref[3*i+1], ref[3*i+2]));
    const float b = ref[3*i+0] + ref[3*i+1] + ref[3*i+2];
    float sat = s/b;
    if(sat < min_sat)
    {
      min_sat = sat;
      min_i   = i;
    }
  }
  fprintf(stderr, "whitest patch is %d - %g %g %g\n", min_i, ref[3*min_i+0], ref[3*min_i+1], ref[3*min_i+2]);
  return min_i;
}

static inline uint16_t*
load_clut(
    const char      *filename,
    dt_lut_header_t *header)
{
  uint16_t *clut = 0;
  FILE *f = fopen(filename, "rb");
  if(!f) goto error;
  if(fread(header, sizeof(dt_lut_header_t), 1, f) != 1) goto error;
  if(header->channels != 2) goto error;
  if(header->magic    != dt_lut_header_magic)   goto error;
  if(header->version  != dt_lut_header_version) goto error;
  if(header->datatype != dt_lut_header_f16)     goto error;
  clut = calloc(2*sizeof(uint16_t), header->wd * header->ht);
  fread(clut, header->wd*header->ht, 2*sizeof(uint16_t), f);
  fclose(f);
  if(0)
  {
error:
    if(f) fclose(f);
    fprintf(stderr, "[vkdt-eval] could not read %s!\n", filename);
  }
  return clut;
}

// bilinear lookup
static inline void
texture(
    const double   *tc,     // texture coordinates
    const uint16_t *tex,    // texture data
    const int       wd,     // width of texture
    const int       ht,     // height of texture
    double         *out)    // bilinear lookup will end up here
{
  out[0] = out[1] = 0.0;
  double xf = tc[0]*wd, yf = tc[1]*ht;
  int x0 = (int)CLAMP(xf,   0, wd-1), y0 = (int)CLAMP(yf,   0, ht-1);
  int x1 = (int)CLAMP(x0+1, 0, wd-1), y1 = (int)CLAMP(y0+1, 0, ht-1);
  int dx = x1 - x0, dy = y1 - y0;
  double u = xf - x0, v = yf - y0;
  const uint16_t *c = tex + 2*(y0*wd + x0);
  for(int k=0;k<2;k++) out[k] += (1.0-u)*(1.0-v)*half_to_float(c[k]);
  for(int k=0;k<2;k++) out[k] += (    u)*(1.0-v)*half_to_float(c[k + 2*dx]);
  for(int k=0;k<2;k++) out[k] += (1.0-u)*(    v)*half_to_float(c[k + 2*wd*dy]);
  for(int k=0;k<2;k++) out[k] += (    u)*(    v)*half_to_float(c[k + 2*(wd*dy+dx)]);
}

void tri2quad(double *tc)
{
  tc[1] = tc[1] / (1.0-tc[0]);
  tc[0] = (1.0-tc[0])*(1.0-tc[0]);
}

void
process_clut(
    const dt_lut_header_t *clut_header,
    const uint16_t        *clut,
    const float           *rgb,  // camera rgb
    float                 *out,  // rec2020 rgb
    float                  T)    // colour temperature between 2856(A) and 6504(D65)
{
  // convert colour temperature T to
  // temperature normalised between 0 and 1 to blend between two illuminants in profile
  // T between 2856 and 6504
  // TODO: also try the dng spec inverse linear interpolation
  // double temp = (1.0/T - 1.0/2856.0)/(1.0/6504.0-1.0/2856.0);
  double temp = 1.0f - CLAMP(tanf(asinhf(46.3407f+T))+(-0.0287128f*cosf(0.000798585f*(714.855f-T)))+0.942275f, 0.0f, 1.0f);
  double b = rgb[0]+rgb[1]+rgb[2];
  double tc[] = {rgb[0]/b, rgb[2]/b};
  tri2quad(tc);
  tc[0] /= 3.0;
  double rbrb[4], L2[2];
  texture(tc, clut, clut_header->wd, clut_header->ht, rbrb);
  tc[0] += 1.0/3.0;
  texture(tc, clut, clut_header->wd, clut_header->ht, L2);
  tc[0] += 1.0/3.0;
  texture(tc, clut, clut_header->wd, clut_header->ht, rbrb+2);
  double L = mix(L2[0], L2[1], temp);
  double rb[2] = {
    mix(rbrb[0], rbrb[2], temp),
    mix(rbrb[1], rbrb[3], temp)};
  out[0] =  rb[0]            * L * b;
  out[1] = (1.0-rb[0]-rb[1]) * L * b;
  out[2] =  rb[1]            * L * b;
}

void
eval_clut(
    const char  *filename,
    const double T,
    const int    num,
    const float *cam_rgb,
    float       *xyz)
{
  dt_lut_header_t header;
  uint16_t *clut = load_clut(filename, &header);
  if(!clut) return;

  for(int k=0;k<num;k++)
  {
    float rec2020[3];
    process_clut(&header, clut, cam_rgb+3*k, rec2020, T);
    xyz[3*k+0] = xyz[3*k+1] = xyz[3*k+2] = 0.0f;
    for(int j=0;j<3;j++)
      for(int i=0;i<3;i++)
        xyz[3*k+j] += rec2020_to_xyz[j][i] * rec2020[i];
    fprintf(stderr, "out xyz %g %g %g\n", xyz[3*k+0], xyz[3*k+1], xyz[3*k+2]);
  }
  free(clut);
}

void
eval_dcp(
    const char  *filename,
    const double Ta,  // first illuminant, usually StdA, 2856.0
    const double Tb,  // second illuminant, usually D65, 6504.0
    const double T,
    const int    num,
    const float *cam_rgb,
    float       *xyz)
{
  dng_profile_t pa, pb, p;
  dng_profile_fill(&pa, filename, 1); // illuminant 1:A  2:D65
  dng_profile_fill(&pb, filename, 2);
  dng_profile_interpolate(&pa, Ta, &pb, Tb, &p, T);
  // p.hsm = 0; // XXX
  for(int i=0;i<num;i++)
  {
    double co[3], ci[3] = { cam_rgb[3*i+0], cam_rgb[3*i+1], cam_rgb[3*i+2]};
    dng_process(&p, ci, co);
    xyz[3*i+0] = co[0]; xyz[3*i+1] = co[1]; xyz[3*i+2] = co[2];
    fprintf(stderr, "out xyz %g %g %g\n", co[0], co[1], co[2]);
  }
  dng_cleanup(&pa);
  dng_cleanup(&pb);
  dng_cleanup(&p);
}

static inline int
test_dataset_cc24(
    const char   *ssf_filename,
    const double *ill,          // for instance cie_d65 or cie_a
    float       **cam_rgb,      // illuminant * cc24 * camera ssf
    float       **xyz)          // d65 * cc24 * cie cmf (since the built-in wb is meant for rec2020 which is d65)
{
  *xyz     = malloc(sizeof(float)*3*24);
  *cam_rgb = malloc(sizeof(float)*3*24);
  float (*res)[3] = (float (*)[3])*cam_rgb;
  float (*ref)[3] = (float (*)[3])*xyz;

  double *dat = malloc(sizeof(double)*1000*4);
  double (*cfa_spec)[4] = (double (*)[4])dat;
  int cfa_spec_cnt = spectrum_load(ssf_filename, cfa_spec);
  if(!cfa_spec_cnt)
  {
    fprintf(stderr, "[eval-profile] could not open %s.txt!\n", ssf_filename);
    exit(2);
  }
  const double *ref_ill = cie_d65; // dcp goes d50, our lut goes d65/rec2020
  for(int s=0;s<24;s++) res[s][0] = res[s][1] = res[s][2] = 0.0;
  for(int s=0;s<24;s++) ref[s][0] = ref[s][1] = ref[s][2] = 0.0;
  for(int i=0;i<cc24_nwavelengths;i++) // XXX this is just 10nm spacing, TODO use finer quadrature rule!
  {
    for(int s=0;s<24;s++)
    {
       ref[s][0] += cc24_spectra[s][i]
         * cie_interp(ref_ill, cc24_wavelengths[i])
         * cie_interp(cie_x,   cc24_wavelengths[i]);
       ref[s][1] += cc24_spectra[s][i]
         * cie_interp(ref_ill, cc24_wavelengths[i])
         * cie_interp(cie_y,   cc24_wavelengths[i]);
       ref[s][2] += cc24_spectra[s][i]
         * cie_interp(ref_ill, cc24_wavelengths[i])
         * cie_interp(cie_z,   cc24_wavelengths[i]);
       res[s][0] += cc24_spectra[s][i]
         * cie_interp(ill, cc24_wavelengths[i])
         * spectrum_interp(cfa_spec, cfa_spec_cnt, 0, cc24_wavelengths[i]);
       res[s][1] += cc24_spectra[s][i]
         * cie_interp(ill, cc24_wavelengths[i])
         * spectrum_interp(cfa_spec, cfa_spec_cnt, 1, cc24_wavelengths[i]);
       res[s][2] += cc24_spectra[s][i]
         * cie_interp(ill, cc24_wavelengths[i])
         * spectrum_interp(cfa_spec, cfa_spec_cnt, 2, cc24_wavelengths[i]);
    }
  }
  free(dat);
  for(int s=0;s<24;s++) for(int k=0;k<3;k++)
    res[s][k] *= (cc24_wavelengths[cc24_nwavelengths-1] - cc24_wavelengths[0]) /
      (double)cc24_nwavelengths;
  for(int s=0;s<24;s++) for(int k=0;k<3;k++)
    ref[s][k] *= (cc24_wavelengths[cc24_nwavelengths-1] - cc24_wavelengths[0]) /
      (double)cc24_nwavelengths;
  for(int s=0;s<24;s++) fprintf(stderr, "ref xyz %g %g %g\n", ref[s][0], ref[s][1], ref[s][2]);
  for(int s=0;s<24;s++) fprintf(stderr, "cam rgb %g %g %g\n", res[s][0], res[s][1], res[s][2]);
  return 24;
}

static inline int
test_dataset_sat(
    const char   *ssf_filename,
    const double *ill,          // for instance cie_d65 or cie_a
    float       **cam_rgb,      // illuminant * cc24 * camera ssf
    float       **xyz)          // d65 * cc24 * cie cmf (since the built-in wb is meant for rec2020 which is d65)
{ // more saturated colours/LED
  double p[20] = {
     -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, // <= peak pos normalised as 0--1 between 420..680
    1.0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  *xyz     = malloc(sizeof(float)*3*24);
  *cam_rgb = malloc(sizeof(float)*3*24);
  float (*res)[3] = (float (*)[3])*cam_rgb;
  float (*ref)[3] = (float (*)[3])*xyz;

  double *dat = malloc(sizeof(double)*1000*4);
  double (*cfa_spec)[4] = (double (*)[4])dat;
  int cfa_spec_cnt = spectrum_load(ssf_filename, cfa_spec);
  if(!cfa_spec_cnt)
  {
    fprintf(stderr, "[eval-profile] could not open %s.txt!\n", ssf_filename);
    exit(2);
  }
  const double *ref_ill = cie_d65; // dcp goes d50, our lut goes d65/rec2020
  for(int s=0;s<24;s++) res[s][0] = res[s][1] = res[s][2] = 0.0;
  for(int s=0;s<24;s++) ref[s][0] = ref[s][1] = ref[s][2] = 0.0;
  for(int i=0;i<cc24_nwavelengths;i++)
  {
    for(int s=0;s<24;s++)
    {
      p[0] = (s-0.5)/23.0;
      double refspec = cfa_gauss_all(20, p, cc24_wavelengths[i]);
      if(!s) refspec = 1.0; // some reference white to adjust exposure/wb
      ref[s][0] += refspec
        * cie_interp(ref_ill, cc24_wavelengths[i])
        * cie_interp(cie_x,   cc24_wavelengths[i]);
      ref[s][1] += refspec
        * cie_interp(ref_ill, cc24_wavelengths[i])
        * cie_interp(cie_y,   cc24_wavelengths[i]);
      ref[s][2] += refspec
        * cie_interp(ref_ill, cc24_wavelengths[i])
        * cie_interp(cie_z,   cc24_wavelengths[i]);
      res[s][0] += refspec
        * cie_interp(ill, cc24_wavelengths[i])
        * spectrum_interp(cfa_spec, cfa_spec_cnt, 0, cc24_wavelengths[i]);
      res[s][1] += refspec
        * cie_interp(ill, cc24_wavelengths[i])
        * spectrum_interp(cfa_spec, cfa_spec_cnt, 1, cc24_wavelengths[i]);
      res[s][2] += refspec
        * cie_interp(ill, cc24_wavelengths[i])
        * spectrum_interp(cfa_spec, cfa_spec_cnt, 2, cc24_wavelengths[i]);
    }
  }
  free(dat);
  for(int s=0;s<24;s++) for(int k=0;k<3;k++)
    res[s][k] *= (cc24_wavelengths[cc24_nwavelengths-1] - cc24_wavelengths[0]) /
      (double)cc24_nwavelengths;
  for(int s=0;s<24;s++) for(int k=0;k<3;k++)
    ref[s][k] *= (cc24_wavelengths[cc24_nwavelengths-1] - cc24_wavelengths[0]) /
      (double)cc24_nwavelengths;
  for(int s=0;s<24;s++) fprintf(stderr, "ref xyz %g %g %g\n", ref[s][0], ref[s][1], ref[s][2]);
  for(int s=0;s<24;s++) fprintf(stderr, "cam rgb %g %g %g\n", res[s][0], res[s][1], res[s][2]);
  return 24;
}

static inline void
xyz_to_lab(
    const float *xyz0,
    float       *Lab)
{
  float xyz[] = {xyz0[0], xyz0[1], xyz0[2]};
#if 0 // normalise before use to see if we can at least get the hue/chroma part right
  float b = xyz[0]+xyz[1]+xyz[2];
  xyz[0] /= b;
  xyz[1] /= b;
  xyz[2] /= b;
#endif
  const float epsilon = 216.0f / 24389.0f;
  const float kappa = 24389.0f / 27.0f;
#define labf(x) ((x > epsilon) ? cbrtf(x) : (kappa * x + 16.0f) / 116.0f)
  float f[3] = { 0.0f };
  for(int i = 0; i < 3; i++) f[i] = labf(xyz[i]);
  Lab[0] = 116.0f * f[1] - 16.0f;
  Lab[1] = 500.0f * (f[0] - f[1]);
  Lab[2] = 200.0f * (f[1] - f[2]);
#undef labf
}

static inline float
cie_de76(
    float *xyz0,
    float *xyz1)
{
  float Lab0[3], Lab1[3];
  xyz_to_lab(xyz0, Lab0);
  xyz_to_lab(xyz1, Lab1);
  return sqrtf(
      (Lab0[0] - Lab1[0])*(Lab0[0] - Lab1[0])+
      (Lab0[1] - Lab1[1])*(Lab0[1] - Lab1[1])+
      (Lab0[2] - Lab1[2])*(Lab0[2] - Lab1[2]));
}

// TODO: arguments:
// - dcp or lut profile
// - if dcp, what are the two illuminants in the file (Calibration Illuminant 1        : Other)??
// - evaluate at which illuminant/temperature
int main(int argc, char *argv[])
{
  char *model = 0;
  if(argc < 2)
  {
    fprintf(stderr, "[eval-profile] evaluate the accuracy of a colour lookup table profile\n");
    fprintf(stderr, "usage: vkdt-eval-profile <model>\n");
    exit(1);
  }
  model = argv[1];
  const char *ssf_filename = model;
  char profile_name[256];
  char lut_name[256];
  snprintf(profile_name, sizeof(profile_name), "%s.dcp", model);
  snprintf(lut_name, sizeof(lut_name), "%s.lut", model);
  const double *illuminant = cie_d65;
  double Ta = 2856.0, Tb = 6504.0;
  double T  = 6504.0;
  // double illbb[CIE2_SAMPLES];
  // if(temp0 > 0) init_blackbody(illbb, temp0); // XXX TODO: init from blackbody
  // XXX parse!

  // create ground truth datasets:
  float *cam_rgb, *xyz, *xyz_p;
  int num = test_dataset_cc24(ssf_filename, illuminant, &cam_rgb, &xyz);
  // int num = test_dataset_sat(ssf_filename, illuminant, &cam_rgb, &xyz);


  // evaluate test set on profile
  xyz_p = malloc(sizeof(float)*3*num);
  // TODO: command line switch or only evaluate if the lut/dcp files are found:
#if 0
  // if(profile_name)
    eval_dcp (profile_name, Ta, Tb, T, num, cam_rgb, xyz_p);
#else
  // if(lut_name)
    eval_clut(lut_name, T, num, cam_rgb, xyz_p);
#endif

#if 1 // correct brightness and wb at whitest patch we find
  int whitest = find_whitest(xyz, xyz_p, num);
  float wb[] = {
    xyz[3*whitest+0]/xyz_p[3*whitest+0],
    xyz[3*whitest+1]/xyz_p[3*whitest+1],
    xyz[3*whitest+2]/xyz_p[3*whitest+2]};
  // all else going well this is just to match exposure:
  fprintf(stderr, "corrective wb %g %g %g\n", wb[0], wb[1], wb[2]);
  for(int i=0;i<num;i++)
    for(int k=0;k<3;k++)
      xyz_p[3*i+k] *= wb[k];
#endif

  // output report
  for(int i=0;i<num;i++)
  {
    float err = cie_de76(xyz+3*i, xyz_p+3*i);
    fprintf(stderr, "patch %d %c%02d %g\n", i, 'A'+(i/6), 1+(i%6), err);
    // TODO: nicer html/sort values/output max
  }

  exit(0);
}
