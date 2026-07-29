#include "spectrum.h"
#include "sigmoid.h"
#include "matrices.h"
#include "upsample.h"
#include "core/clip.h"
#include "core/core.h"
#include "core/fs.h"
#include "core/half.h"
#include "core/inpaint.h"
#include <math.h>

#define MKCLUT_MAX_ANCHORS 16

// Planckian spectrum, 360--830nm at 1nm.
static inline int
synth_illuminant_planckian(
    double         T,
    double       (*out)[4])
{
  const double h = 6.62606957e-34; // planck's constant [J s]
  const double c = 299792458.0;    // speed of light [m/s]
  const double k = 1.3807e-23;     // boltzmann's constant [J/K]
  int cnt = 0;
  for(double l = 360.0; l <= 830.0; l += 1.0, cnt++)
  {
    const double lambda_m = l * 1e-9;
    const double lambda5 = lambda_m*lambda_m*lambda_m*lambda_m*lambda_m;
    const double c1 = 2.0 * h * c * c / lambda5;
    const double c2 = h * c / (lambda_m * T * k);
    out[cnt][0] = l;
    out[cnt][1] = c1 / (exp(c2) - 1.0);
    out[cnt][2] = out[cnt][3] = 0.0;
  }
  return cnt;
}

// CIE daylight basis functions, 300--830nm at 5nm.
static const double cie_day_wl[107] = {
  300,305,310,315,320,325,330,335,340,345,350,355,360,
  365,370,375,380,385,390,395,400,405,410,415,420,425,
  430,435,440,445,450,455,460,465,470,475,480,485,490,
  495,500,505,510,515,520,525,530,535,540,545,550,555,
  560,565,570,575,580,585,590,595,600,605,610,615,620,
  625,630,635,640,645,650,655,660,665,670,675,680,685,
  690,695,700,705,710,715,720,725,730,735,740,745,750,
  755,760,765,770,775,780,785,790,795,800,805,810,815,
  820,825,830
};
static const double cie_day_s0[107] = {
  0.04,3.02,6,17.8,29.6,42.45,55.3,56.3,57.3,59.55,61.8,61.65,61.5,
  65.15,68.8,66.1,63.4,64.6,65.8,80.3,94.8,99.8,104.8,105.35,105.9,101.35,
  96.8,105.35,113.9,119.75,125.6,125.55,125.5,123.4,121.3,121.3,121.3,117.4,113.5,
  113.3,113.1,111.95,110.8,108.65,106.5,107.65,108.8,107.05,105.3,104.85,104.4,102.2,
  100,98,96,95.55,95.1,92.1,89.1,89.8,90.5,90.4,90.3,89.35,88.4,
  86.2,84,84.55,85.1,83.5,81.9,82.25,82.6,83.75,84.9,83.1,81.3,76.6,
  71.9,73.1,74.3,75.35,76.4,69.85,63.3,67.5,71.7,74.35,77,71.1,65.2,
  56.45,47.7,58.15,68.6,66.8,65,65.5,66,63.5,61,57.15,53.3,56.1,
  58.9,60.4,61.9
};
static const double cie_day_s1[107] = {
  0.02,2.26,4.5,13.45,22.4,32.2,42,41.3,40.6,41.1,41.6,39.8,38,
  40.2,42.4,40.45,38.5,36.75,35,39.2,43.4,44.85,46.3,45.1,43.9,40.5,
  37.1,36.9,36.7,36.3,35.9,34.25,32.6,30.25,27.9,26.1,24.3,22.2,20.1,
  18.15,16.2,14.7,13.2,10.9,8.6,7.35,6.1,5.15,4.2,3.05,1.9,0.95,
  0,-0.8,-1.6,-2.55,-3.5,-3.5,-3.5,-4.65,-5.8,-6.5,-7.2,-7.9,-8.6,
  -9.05,-9.5,-10.2,-10.9,-10.8,-10.7,-11.35,-12,-13,-14,-13.8,-13.6,-12.8,
  -12,-12.65,-13.3,-13.1,-12.9,-11.75,-10.6,-11.1,-11.6,-11.9,-12.2,-11.2,-10.2,
  -9,-7.8,-9.5,-11.2,-10.8,-10.4,-10.5,-10.6,-10.15,-9.7,-9,-8.3,-8.8,
  -9.3,-9.55,-9.8
};
static const double cie_day_s2[107] = {
  0,1,2,3,4,6.25,8.5,8.15,7.8,7.25,6.7,6,5.3,
  5.7,6.1,4.55,3,2.1,1.2,0.05,-1.1,-0.8,-0.5,-0.6,-0.7,-0.95,
  -1.2,-1.9,-2.6,-2.75,-2.9,-2.85,-2.8,-2.7,-2.6,-2.6,-2.6,-2.2,-1.8,
  -1.65,-1.5,-1.4,-1.3,-1.25,-1.2,-1.1,-1,-0.75,-0.5,-0.4,-0.3,-0.15,
  0,0.1,0.2,0.35,0.5,1.3,2.1,2.65,3.2,3.65,4.1,4.4,4.7,
  4.9,5.1,5.9,6.7,7,7.3,7.95,8.6,9.2,9.8,10,10.2,9.25,
  8.3,8.95,9.6,9.05,8.5,7.75,7,7.3,7.6,7.8,8,7.35,6.7,
  5.95,5.2,6.3,7.4,7.1,6.8,6.9,7,6.7,6.4,5.95,5.5,5.8,
  6.1,6.3,6.5
};

static inline double cie_day_interp(const double *tab, double l)
{
  if(l <= cie_day_wl[0])   return tab[0];
  if(l >= cie_day_wl[106]) return tab[106];
  int i = (int)((l - cie_day_wl[0]) / 5.0);
  double t = (l - cie_day_wl[i]) / 5.0;
  return tab[i]*(1.0-t) + tab[i+1]*t;
}

// CIE daylight spectrum, 360--830nm at 1nm.
static inline int
synth_illuminant_daylight(
    double         T,
    double       (*out)[4])
{
  double x = (T <= 7000.0)
    ? 0.244063 + 0.09911e3/T + 2.9678e6/(T*T) - 4.6070e9/(T*T*T)
    : 0.237040 + 0.24748e3/T + 1.9018e6/(T*T) - 2.0064e9/(T*T*T);
  double y = -3.000*x*x + 2.870*x - 0.275;
  double m = 0.0241 + 0.2562*x - 0.7341*y;
  double m1 = (-1.3515 - 1.7703*x + 5.9114*y) / m;
  double m2 = (0.0300 - 31.4424*x + 30.0717*y) / m;
  int cnt = 0;
  for(double l = 360.0; l <= 830.0; l += 1.0, cnt++)
  {
    out[cnt][0] = l;
    out[cnt][1] = cie_day_interp(cie_day_s0, l)
                + m1*cie_day_interp(cie_day_s1, l)
                + m2*cie_day_interp(cie_day_s2, l);
    out[cnt][2] = out[cnt][3] = 0.0;
  }
  return cnt;
}

// Planckian below 4000K, daylight otherwise.
static inline int
synth_illuminant(
    double         T,
    double       (*out)[4])
{
  return T < 4000.0 ? synth_illuminant_planckian(T, out) : synth_illuminant_daylight(T, out);
}

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
    const int              cie_spec_cnt,
    const int              ss)              // source oversampling factor
{
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
  sample_wd = swd*ss, sample_ht = sht*ss;
  for(int j=0;j<sample_ht;j++) for(int i=0;i<sample_wd;i++)
  {
    double xy[2] = {(i+0.5)/sample_wd, (j+0.5)/sample_ht};
    quad2tri(xy+0, xy+1);
    double cf[3]; // look up the coeffs for the sampled colour spectrum
    fetch_coeff(xy, spectra, sh->wd, sh->ht, cf); // bilinear
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

// Write chroma bands and packed luminance pairs; n==2 uses the legacy layout.
static inline void
write_chroma_lut_n(
    const char   *basename,
    float       **clut, // n buffers, each in create_chroma_lut's buf layout
    const int     n,
    const int     wd,
    const int     ht)
{
  const int nbands = (n == 2) ? 3 : (n + (n+1)/2);
  dt_lut_header_t hout = {
    .magic    = dt_lut_header_magic,
    .version  = dt_lut_header_version,
    .channels = 2,
    .datatype = dt_lut_header_f16,
    .wd       = nbands*wd,
    .ht       = ht,
  };
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s.lut", basename);
  FILE *f = fopen(filename, "wb");
  fwrite(&hout, sizeof(hout), 1, f);
  uint16_t *b16 = calloc(sizeof(uint16_t), wd*(uint64_t)ht*nbands*2);
  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
  {
    const uint64_t px = (uint64_t)wd*j+i;
    if(n == 2)
    { // Legacy layout.
      b16[2*(3*wd*j+i)+0]      = float_to_half(clut[0][3*px+0]);
      b16[2*(3*wd*j+i)+1]      = float_to_half(clut[0][3*px+1]);
      b16[2*(3*wd*j+i+wd)+0]   = float_to_half(clut[0][3*px+2]);
      b16[2*(3*wd*j+i+wd)+1]   = float_to_half(clut[1][3*px+2]);
      b16[2*(3*wd*j+i+2*wd)+0] = float_to_half(clut[1][3*px+0]);
      b16[2*(3*wd*j+i+2*wd)+1] = float_to_half(clut[1][3*px+1]);
      continue;
    }
    const uint64_t row = (uint64_t)nbands*wd*j;
    for(int a=0;a<n;a++)
    {
      b16[2*(row + a*wd + i)+0] = float_to_half(clut[a][3*px+0]);
      b16[2*(row + a*wd + i)+1] = float_to_half(clut[a][3*px+1]);
    }
    for(int p=0;p<(n+1)/2;p++)
    {
      const int a0 = 2*p, a1 = (2*p+1 < n) ? 2*p+1 : 2*p;
      const uint64_t bidx = row + (uint64_t)(n+p)*wd + i;
      b16[2*bidx+0] = float_to_half(clut[a0][3*px+2]);
      b16[2*bidx+1] = float_to_half(clut[a1][3*px+2]);
    }
  }
  fwrite(b16, sizeof(uint16_t), wd*(uint64_t)ht*nbands*2, f);
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

// Fixed grid shared with colour/main.c.
#define MKCLUT_ANCHOR_T_LO 2000.0
#define MKCLUT_ANCHOR_T_HI 15000.0

int main(int argc, char *argv[])
{
  const char *model = 0;
  const char *illum_file0 = "data/cie_d65";
  const char *illum_file1 = "data/cie_a";
  int n_nanchor = 0;
  int have_nanchor = 0;
  int ss = 8;
  for(int k=1;k<argc;k++)
  {
    if     (!strcmp(argv[k], "--illum0") && argc > k+1) illum_file0 = argv[++k];
    else if(!strcmp(argv[k], "--illum1") && argc > k+1) illum_file1 = argv[++k];
    else if(!strcmp(argv[k], "--nanchor") && argc > k+1) { n_nanchor = atol(argv[++k]); have_nanchor = 1; }
    else if(!strcmp(argv[k], "--ss")      && argc > k+1) ss = MAX(1, atol(argv[++k]));
    else model = argv[k];
  }
  if(!model)
  {
    fprintf(stderr, "mkclut: create colour lookup table as input device transform\n");
    fprintf(stderr, "usage: mkclut <cam model>       load ssf from 'cam model.txt',\n"
                    "                                write 'cam model.lut'\n"
                    "              --illum0 <xx>     illuminant 0, default d65 (daylight)\n"
                    "              --illum1 <yy>     illuminant 1, default a (incandescent)\n"
                    "              --nanchor <n>     generate n anchors mired-spaced from\n"
                    "                                2000-15000k. this is what\n"
                    "                                the colour module's temp slider assumes for\n"
                    "                                n>=3 anchor cluts.\n"
                    "              --ss <n>          source oversampling factor, default 8.\n"
                    "                                pure quality/runtime knob, see\n"
                    "                                create_chroma_lut's comment.\n");
    exit(1);
  }
  if(have_nanchor)
  {
    n_nanchor = CLAMP(n_nanchor, 3, MKCLUT_MAX_ANCHORS);
  }
  const int n_anchor = have_nanchor ? n_nanchor : 2;

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
  float *clut[MKCLUT_MAX_ANCHORS];
  for(int ill=0;ill<n_anchor;ill++)
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
    int illum_cnt;
    if(have_nanchor)
    {
      const double m_lo = 1e6/MKCLUT_ANCHOR_T_HI, m_hi = 1e6/MKCLUT_ANCHOR_T_LO;
      const double m = m_lo + (m_hi-m_lo)*ill/(double)(n_anchor-1);
      illum_cnt = synth_illuminant(1e6/m, illum_spec);
    }
    else
    {
      snprintf(filename, sizeof(filename), "%s/%s", basedir, ill ? illum_file1 : illum_file0);
      illum_cnt = spectrum_load(filename, illum_spec);
    }
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

    clut[ill] = create_chroma_lut(
        &clut_wd, &clut_ht,
        sp_buf, &sp_header,
        cfa_spec,
        cfa_spec_cnt,
        cie_spec,
        cie_spec_cnt,
        ss);

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
        float col[3] = {clut[0][3*k], clut[0][3*k+1], 1.0-clut[0][3*k]-clut[0][3*k+1]};
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
      .dat = clut[ill],
      .wd  = clut_wd,
      .ht  = clut_ht,
      .cpp = 3,
    };
    dt_inpaint(&inpaint_buf);
  }

  write_chroma_lut_n(model, clut, n_anchor, clut_wd, clut_ht);

  free(sp_buf);
  for(int ill=0;ill<n_anchor;ill++) free(clut[ill]);

  exit(0);
}
