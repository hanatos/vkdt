#include "spectrum.h"
#include "sigmoid.h"
#include "q2t.h"
#include "matrices.h"
#include "core/clip.h"
#include "core/core.h"
#include "core/half.h"
#include "core/inpaint.h"
#include "core/lut.h"

#if 1 // TODO XXX put into spectrum lookup header?
static inline float*
load_spectra_lut(
    const char *filename,
    dt_lut_header_t *header)
{
  float *spectra = 0;
  FILE *f = fopen(filename, "rb");
  if(!f) goto error;
  if(fread(header, sizeof(dt_lut_header_t), 1, f) != 1) goto error;
  if(header->channels != 4) goto error;
  if(header->magic    != dt_lut_header_magic)   goto error;
  if(header->version  != dt_lut_header_version) goto error;
  if(header->datatype != dt_lut_header_f32)     goto error;
  spectra = calloc(4*sizeof(float), header->wd * header->ht);
  fread(spectra, header->wd*header->ht, 4*sizeof(float), f);
  fclose(f);
  if(0)
  {
error:
    if(f) fclose(f);
    fprintf(stderr, "[vkdt-mkclut] could not read %s!\n", filename);
  }
  return spectra;
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
#endif

// create 2.5D chroma lut
static inline float*
create_chroma_lut(
    int                   *wd_out,
    int                   *ht_out,
    const float           *spectra,          // sigmoid upsampling table
    const dt_lut_header_t *sh,               // lut header for spectra
    const double         (*cfa_spec)[4],     // tabulated cfa spectra
    const int              cfa_spec_cnt)
{
  // to avoid interpolation artifacts we only want to place straight pixel
  // center values of our spectra.lut in the output:
  int swd = sh->wd, sht = sh->ht; // sampling dimensions
  int wd  = swd, ht = sht; // output dimensions
  float *buf = calloc(sizeof(float)*2, wd*ht+1);

  // do two passes over the data
  // get illum E white point (lowest saturation) in camera rgb and quad param:
  const double wcf[] = {0.0, 0.0, 100000.0}; // illuminant E
  double white_cam_rgb[3] = {
    spectrum_integrate(cfa_spec, 0, cfa_spec_cnt, wcf, 3),
    spectrum_integrate(cfa_spec, 1, cfa_spec_cnt, wcf, 3),
    spectrum_integrate(cfa_spec, 2, cfa_spec_cnt, wcf, 3)};
  normalise1(white_cam_rgb);
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
    const double xyz[3] = {xy[0], xy[1], 1.0-xy[0]-xy[1]};

    double cf[3]; // look up the coeffs for the sampled colour spectrum
    fetch_coeff(xy, spectra, sh->wd, sh->ht, cf); // interpolate
    // fetch_coeffi(xy, spectra, sh->wd, sh->ht, cf); // nearest
    if(cf[0] == 0) continue; // discard out of spectral locus

    double cam_rgb_spec[3] = {0.0}; // camera rgb by processing spectrum * cfa spectrum
    for(int k=0;k<3;k++)
      cam_rgb_spec[k] = spectrum_integrate(cfa_spec, k, cfa_spec_cnt, cf, 3);
    normalise1(cam_rgb_spec);

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
    double rec2020[3];
    mat3_mulv(xyz_to_rec2020, xyz, rec2020);
    normalise1(rec2020);

    buf[2*(jj*wd + ii)+0] = rec2020[0];
    buf[2*(jj*wd + ii)+1] = rec2020[2];
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
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s.pfm", basename);
  FILE *f = fopen(filename, "wb");
  fprintf(f, "PF\n%d %d\n-1.0\n", wd, ht);
  for(int k=0;k<wd*ht;k++)
  {
    float col[3] = {buf0[2*k], buf0[2*k+1], 1.0-buf0[2*k]-buf0[2*k+1]};
    fwrite(col, sizeof(float), 3, f);
  }
  fclose(f);
  dt_lut_header_t hout = {
    .magic    = dt_lut_header_magic,
    .version  = dt_lut_header_version,
    .channels = 4,
    .datatype = dt_lut_header_f16,
    .wd       = wd,
    .ht       = ht,
  };
  snprintf(filename, sizeof(filename), "%s.lut", basename);
  f = fopen(filename, "wb");
  fwrite(&hout, sizeof(hout), 1, f);
  uint16_t *b16 = calloc(sizeof(uint16_t), wd*ht*4);
  for(int k=0;k<wd*ht;k++)
  {
    b16[4*k+0] = float_to_half(buf0[2*k+0]);
    b16[4*k+1] = float_to_half(buf0[2*k+1]);
    b16[4*k+2] = float_to_half(buf1[2*k+0]);
    b16[4*k+3] = float_to_half(buf1[2*k+1]);
  }
  fwrite(b16, sizeof(uint16_t), wd*ht*4, f);
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
  float *sp_buf = load_spectra_lut("spectra.lut", &sp_header);
  if(!sp_buf) exit(1);

  double cfa_spec[100][4];

  int clut_wd, clut_ht;
  float *clut0, *clut1;
  for(int ill=0;ill<2;ill++)
  {
    int cfa_spec_cnt = spectrum_load(model, cfa_spec);
    if(!cfa_spec_cnt) exit(2);
    double illum_spec[1000][4];
    int illum_cnt = spectrum_load(ill ? illum_file1 : illum_file0, illum_spec);
    if(!illum_cnt) exit(3);
    spectrum_wb(cfa_spec_cnt, illum_cnt, cfa_spec, illum_spec);

    float **clut = (ill ? &clut1 : &clut0);
    *clut = create_chroma_lut(
        &clut_wd, &clut_ht,
        sp_buf, &sp_header,
        cfa_spec,
        cfa_spec_cnt);

    dt_inpaint_buf_t inpaint_buf = {
      .dat = (ill ? clut1 : clut0),
      .wd  = clut_wd,
      .ht  = clut_ht,
      .cpp = 2,
    };
    dt_inpaint(&inpaint_buf);
  }

  write_chroma_lut(model, clut0, clut1, clut_wd, clut_ht);

  free(sp_buf);
  free(clut0);
  free(clut1);

  exit(0);
}
