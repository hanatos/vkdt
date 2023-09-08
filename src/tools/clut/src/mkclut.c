#include "spectrum.h"
#include "sigmoid.h"
#include "matrices.h"
#include "upsample.h"
#include "core/clip.h"
#include "core/core.h"
#include "core/fs.h"
#include "core/half.h"
#include "core/inpaint.h"

#include <limits.h>

#if 0
// XXX DEBUG write just the matrix from xyz to rec2020 for precision checks:
static inline float*
create_chroma_lut_DEBUG(
    int                   *wd_out,
    int                   *ht_out,
    const float           *spectra,          // sigmoid upsampling table
    const dt_lut_header_t *sh,               // lut header for spectra
    const double         (*cfa_spec)[4],     // tabulated cfa spectra
    const int              cfa_spec_cnt,
    const double         (*cie_spec)[4],     // tabulated cie observer
    const int              cie_spec_cnt)
{
  int wd  = sh->wd, ht = sh->ht; // output dimensions
  float *buf = calloc(sizeof(float)*3, wd*ht+1);

  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
  {
    double xz[2] = {(i+0.5)/wd, (j+0.5)/ht};
    quad2tri(xz+0, xz+1);
    // double xyz[3] = {xy[0], xy[1], 1.0-xy[0]-xy[1]}; // = "cam rgb"
    double xyz[3] = {xz[0], 1.0-xz[0]-xz[1], xz[1]}; // = "cam rgb"

    double rec2020[3];
    mat3_mulv(xyz_to_rec2020, xyz, rec2020);
    // const double ref_L = white_cam_rgb_L1 / white_rec2020_L1;
    const double rec2020_L1 = normalise1(rec2020); // XXX * ref_L ???
    const double cam_rgb_L1 = normalise1(xyz);

    buf[3*(j*wd + i)+0] = rec2020[0];
    buf[3*(j*wd + i)+1] = rec2020[2];
    buf[3*(j*wd + i)+2] = rec2020_L1 / cam_rgb_L1;
  }
  *wd_out = wd;
  *ht_out = ht;
  return buf;
}
#endif

// create 2.5D chroma lut
static inline float*
create_chroma_lut(
    int                   *wd_out,
    int                   *ht_out,
    const float           *spectra,          // sigmoid upsampling table
    const dt_lut_header_t *sh,               // lut header for spectra
    const double         (*cfa_spec)[4],     // tabulated cfa spectra
    const int              cfa_spec_cnt,
    const double         (*cie_spec)[4],     // tabulated cie observer
    const int              cie_spec_cnt)
{
  // to avoid interpolation artifacts we only want to place straight pixel
  // center values of our spectra.lut in the output:
  int swd = sh->wd, sht = sh->ht; // sampling dimensions
  int wd  = swd, ht = sht; // output dimensions
  float *buf = calloc(sizeof(float)*3, wd*ht+1);

  // do two passes over the data
  // get illum E white point (lowest saturation) in camera rgb and quad param:
  const double wcf[] = {0.0, 0.0, 100000.0}; // illuminant E
  double white_cam_rgb[3] = {
    spectrum_integrate(cfa_spec, 0, cfa_spec_cnt, wcf, 3),
    spectrum_integrate(cfa_spec, 1, cfa_spec_cnt, wcf, 3),
    spectrum_integrate(cfa_spec, 2, cfa_spec_cnt, wcf, 3)};
  const double white_cam_rgb_L1 = normalise1(white_cam_rgb);
  tri2quad(white_cam_rgb, white_cam_rgb+2);
  double xyz_spec[3] = {0.0};
  for(int k=0;k<3;k++)
    xyz_spec[k] = spectrum_integrate(cie_spec, k, cie_spec_cnt, wcf, 3);
  double rec2020[3];
  mat3_mulv(xyz_to_rec2020, xyz_spec, rec2020);
  const double white_rec2020_L1 = normalise1(rec2020);
  const double ref_L = white_cam_rgb_L1 / white_rec2020_L1;

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
      cam_rgb_spec[k] = spectrum_integrate(cfa_spec, k, cfa_spec_cnt, cf, 3);
    normalise1(cam_rgb_spec);
    double u0 = cam_rgb_spec[0], u1 = cam_rgb_spec[2];
    tri2quad(&u0, &u1);
    float fxy[] = {xy[0], xy[1]}, white[] = {1.0f/3.0f, 1.0f/3.0f};
    float sat = dt_spectrum_saturation(fxy, white);
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

  // 2nd pass:
// #pragma omp parallel for schedule(dynamic) collapse(2) default(shared)
  for(int j=0;j<sht;j++) for(int i=0;i<swd;i++)
  {
    double xy[2] = {(i+0.5)/swd, (j+0.5)/sht};
    quad2tri(xy+0, xy+1);

    double cf[3]; // look up the coeffs for the sampled colour spectrum
    fetch_coeff(xy, spectra, sh->wd, sh->ht, cf); // interpolate
    // fetch_coeffi(xy, spectra, sh->wd, sh->ht, cf); // nearest
    if(cf[0] == 0) continue; // discard out of spectral locus

    double cam_rgb_spec[3] = {0.0}; // camera rgb by processing spectrum * cfa spectrum
    for(int k=0;k<3;k++)
      cam_rgb_spec[k] = spectrum_integrate(cfa_spec, k, cfa_spec_cnt, cf, 3);
    const double cam_rgb_L1 = normalise1(cam_rgb_spec);
    double xyz_spec[3] = {0.0}; // camera rgb by processing spectrum * cfa spectrum
    for(int k=0;k<3;k++)
      xyz_spec[k] = spectrum_integrate(cie_spec, k, cie_spec_cnt, cf, 3);
    double rec2020[3];
    mat3_mulv(xyz_to_rec2020, xyz_spec, rec2020);
    const double rec2020_L1 = normalise1(rec2020) * ref_L;

    float fxy[] = {xy[0], xy[1]}, white[2] = {1.0f/3.0f, 1.0f/3.0f};
    float sat = dt_spectrum_saturation(fxy, white);

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

    buf[3*(jj*wd + ii)+0] = rec2020[0];
    buf[3*(jj*wd + ii)+1] = rec2020[2];
    buf[3*(jj*wd + ii)+2] = rec2020_L1 / cam_rgb_L1;
  }
  free(angular_ds);

  *wd_out = wd;
  *ht_out = ht;
  return buf;
}

// write look up table based on hue and chroma:
static inline void
write_chroma_lut(
    const char  *basename,
    const float *buf0,
    const float *buf1,
    const int    wd,
    const int    ht)
{
  dt_lut_header_t hout = {
    .magic    = dt_lut_header_magic,
    .version  = dt_lut_header_version,
    .channels = 2,
    .datatype = dt_lut_header_f16,
    .wd       = 3*wd,
    .ht       = ht,
  };
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s.lut", basename);
  FILE *f = fopen(filename, "wb");
  fwrite(&hout, sizeof(hout), 1, f);
  uint16_t *b16 = calloc(sizeof(uint16_t), wd*(uint64_t)ht*6);
  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
  {
    b16[2*(3*wd*j+i)+0]      = float_to_half(buf0[3*(wd*j+i)+0]);
    b16[2*(3*wd*j+i)+1]      = float_to_half(buf0[3*(wd*j+i)+1]);
    b16[2*(3*wd*j+i+wd)+0]   = float_to_half(buf0[3*(wd*j+i)+2]);
    b16[2*(3*wd*j+i+wd)+1]   = float_to_half(buf1[3*(wd*j+i)+2]);
    b16[2*(3*wd*j+i+2*wd)+0] = float_to_half(buf1[3*(wd*j+i)+0]);
    b16[2*(3*wd*j+i+2*wd)+1] = float_to_half(buf1[3*(wd*j+i)+1]);
  }
  fwrite(b16, sizeof(uint16_t), wd*(uint64_t)ht*6, f);
  // append metadata, the source spectrum:
  fprintf(f, "##### created by vkdt mkclut, from following input\n");
  snprintf(filename, sizeof(filename), "%s.txt", basename);
  FILE *f2 = fopen(filename, "rb");
  if(f2)
  {
    char buf[BUFSIZ];
    while(!feof(f2))
    {
      fscanf(f2, "%[^\n]", buf);
      fgetc(f2);
      fprintf(f, "%s\n", buf);
      buf[0] = 0;
    }
    fclose(f2);
  }
  fclose(f);
  free(b16);
}

int main(int argc, char *argv[])
{
  const char *model = 0;
  const char *illum_file0 = "data/cie_d65";
  const char *illum_file1 = "data/cie_a";
  for(int k=1;k<argc;k++)
  {
    if     (!strcmp(argv[k], "--illum")  && argc > k+1) illum_file0 = argv[++k];
    else if(!strcmp(argv[k], "--illum0") && argc > k+1) illum_file0 = argv[++k];
    else if(!strcmp(argv[k], "--illum1") && argc > k+1) illum_file1 = argv[++k];
    else model = argv[k];
  }
  if(!model)
  {
    fprintf(stderr, "mkclut: create colour lookup table as input device transform\n");
    fprintf(stderr, "usage: mkclut <cam model>       load ssf from 'cam model.txt',\n"
                    "                                write 'cam model.lut'\n"
                    "              --illum0 <xx>     illuminant 0, default d65 (daylight)\n"
                    "              --illum1 <yy>     illuminant 1, default a (incandescent)\n");
    exit(1);
  }

  dt_lut_header_t sp_header;
  char filename[PATH_MAX+30], basedir[PATH_MAX];
  fs_basedir(basedir, sizeof(basedir));
  snprintf(filename, sizeof(filename), "%s/data/spectra.lut", basedir);
  float *sp_buf = load_spectra_lut(filename, &sp_header);
  if(!sp_buf)
  {
    fprintf(stderr, "[mkclut] can't load 'data/spectra.lut' upsampling table!\n");
    exit(1);
  }

  double cfa_spec[1000][4];
  double cie_spec[1000][4];
  double d65_spec[1000][4];
  snprintf(filename, sizeof(filename), "%s/data/cie_d65", basedir);
  int d65_cnt = spectrum_load(filename, d65_spec);

  int clut_wd, clut_ht;
  float *clut0, *clut1;
  for(int ill=0;ill<2;ill++)
  {
    snprintf(filename, sizeof(filename), "%s/data/cie_observer", basedir);
    int cfa_spec_cnt = spectrum_load(model,    cfa_spec);
    int cie_spec_cnt = spectrum_load(filename, cie_spec);
    if(!cfa_spec_cnt || !cie_spec_cnt)
    {
      fprintf(stderr, "[mkclut] could not open %s.txt or data/cie_observer.txt!\n", model);
      exit(2);
    }
    cfa_spec_cnt = spectrum_chg_interval( cfa_spec, cfa_spec_cnt, 5); //interpolates spectrum to 5nm intervals
    double illum_spec[1000][4];
    snprintf(filename, sizeof(filename), "%s/%s", basedir, ill ? illum_file1 : illum_file0);
    int illum_cnt = spectrum_load(filename, illum_spec);
    if(!illum_cnt) 
    {
      fprintf(stderr, "[mkclut] could not open illumination spectrum %s!\n", ill ? illum_file1 : illum_file0);
      exit(3);
    }
    spectrum_wb(cfa_spec_cnt, illum_cnt, cfa_spec, illum_spec);
    // white balancing is complicated. looking at constant 100% white albedo
    // under D65 (or A) illuminant, we want that to turn out as (1,1,1) in rec2020
    // later, so the chromaticity coordinate in cie XYZ should be the D65 white.
    // consider a unit test case where cfa=cie. cfa * D65 * 100% = xyD65 and this
    // should be mapped to the same in the reference (i.e. the lut should not move
    // anything). this only works if we also apply D65 when computing the reference:
    spectrum_wb(cie_spec_cnt, d65_cnt, cie_spec, d65_spec);

    float **clut = (ill ? &clut1 : &clut0);
    *clut = create_chroma_lut(
        &clut_wd, &clut_ht,
        sp_buf, &sp_header,
        cfa_spec,
        cfa_spec_cnt,
        cie_spec,
        cie_spec_cnt);

#if 0
    if(ill == 0)
    { // write debugging output for plots with the source data only,
      // to visualise sampling density and the limits of the spectral locus
      snprintf(filename, sizeof(filename), "%s.ppm", model);
      FILE *f = fopen(filename, "wb");
      fprintf(f, "P6\n%d %d\n255\n", clut_wd, clut_ht);
      for(int j=0;j<clut_ht;j++)
      for(int i=0;i<clut_wd;i++)
      {
        int k = (clut_ht-1-j)*clut_wd + i; // fucking flip so convert -> png shows correctly
        float col[3] = {clut0[3*k], clut0[3*k+1], 1.0-clut0[3*k]-clut0[3*k+1]};
        uint8_t c8[3] = {
          // CLAMP(256*(i+0.5)/clut_wd, 0, 255),
          // CLAMP(256*(j+0.5)/clut_ht, 0, 255),
          // CLAMP(256*0, 0, 255)};
          CLAMP(256*col[0], 0, 255),
          CLAMP(256*col[1], 0, 255),
          CLAMP(256*col[2], 0, 255)};
        if(c8[2] == 255) c8[0] = c8[1] = c8[2];
        fwrite(c8, sizeof(uint8_t), 3, f);
      }
      fclose(f);
    }
#endif

    dt_inpaint_buf_t inpaint_buf = {
      .dat = (ill ? clut1 : clut0),
      .wd  = clut_wd,
      .ht  = clut_ht,
      .cpp = 3,
    };
    dt_inpaint(&inpaint_buf);
  }

  write_chroma_lut(model, clut0, clut1, clut_wd, clut_ht);

  free(sp_buf);
  free(clut0);
  free(clut1);

  exit(0);
}
