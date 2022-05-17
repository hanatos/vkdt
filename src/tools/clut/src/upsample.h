#pragma once
#include "core/lut.h"
#include "q2t.h"

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
