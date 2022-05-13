#include "core/core.h"
#include "core/lut.h"
#include "q2t.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

typedef struct mcol_t
{
  char name[10];
  float V, C;
  float x, y, Y;
}
mcol_t;

// parses munsell data file like
//    h   V  C    x      y      Y
//  10RP  1  2 0.3629 0.2710  1.210
// ...
int // return number of colours parsed
parse_data(
    const char *filename,
    mcol_t    **colours)
{
  mcol_t *c = malloc(sizeof(*c)*5000); // 2734 colours in munsell.dat, 4k odd in munsell-all
  *colours = c;
  FILE *f = fopen(filename, "rb");
  if(!f) fprintf(stderr, "could not open file\n");
  fscanf(f, "%*[^\n]");
  fgetc(f); // read first line
  fscanf(f, "%*[^\n]");
  fgetc(f); // read second line
  int i=0;
  // for(;i<2634;i++)
  for(;i<5000;i++)
  {
    int ret = fscanf(f, "%s %g %g %g %g %g\n",
        c[i].name, &c[i].V, &c[i].C, &c[i].x, &c[i].y, &c[i].Y);
    if(ret < 6) break;
  }
  fclose(f);
  return i;
}

#if 1 // XXX TODO put in shared header
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
    fprintf(stderr, "[vkdt-mkidt] could not read %s!\n", filename);
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
#if 1 // conversions from createlut
double sqrd(double x) { return x * x; }

void cvt_c0yl_c012(const double *c0yl, double *coeffs)
{
  coeffs[0] = c0yl[0];
  coeffs[1] = c0yl[2] * -2.0 * c0yl[0];
  coeffs[2] = c0yl[1] + c0yl[0] * c0yl[2] * c0yl[2];
}

void cvt_c012_c0yl(const double *coeffs, double *c0yl)
{
#if 0 // not needed here, we get the already un-normalised values from dat
  // account for normalising lambda:
  double c0 = CIE_LAMBDA_MIN, c1 = 1.0 / (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);
  double A = coeffs[0], B = coeffs[1], C = coeffs[2];

  double A2 = (double)(A*(sqrd(c1)));
  double B2 = (double)(B*c1 - 2.0*A*c0*(sqrd(c1)));
  double C2 = (double)(C - B*c0*c1 + A*(sqrd(c0*c1)));
#endif
  double A2 = coeffs[0];
  double B2 = coeffs[1];
  double C2 = coeffs[2];

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
#endif

int main(int argc, char *argv[])
{
  dt_lut_header_t sp_header;
  float *sp_buf = load_spectra_lut(
      "spectra-c.lut", &sp_header);
  if(!sp_buf) exit(1);
  mcol_t *col;
  int cnt = parse_data("munsell.dat", &col);
  // int cnt = parse_data("munsell-all.dat", &col);
  if(!cnt)
  {
    fprintf(stderr, "could not parse munsell.dat!\n");
    exit(2);
  }
  // TODO: munsell data is illuminant C, need to adapt to our spectra!
  // also see http://www.brucelindbloom.com/index.html?UPLab.html
  // and https://github.com/colour-science/colour/blob/0df2ae3f1880d402f8d1f0600c38b34adc5f2a03/colour/notation/munsell.py#L908
  // and An Open-Source Inversion Algorithm for the Munsell Renotation, Paul Centore, 2012
  for(int i=0;i<cnt;i++)
  {
    // get xyY and convert to spectral coeffs
    double xy[2] = {col[i].x, col[i].y};
    double coeff[4], c0yl[3];
    fetch_coeff(xy, sp_buf, sp_header.wd, sp_header.ht, coeff);
    cvt_c012_c0yl(coeff, c0yl); // convert to mean wavelength
    // output
    fprintf(stdout, "%s %g %g %g %g %g\n",
        col[i].name, col[i].V, col[i].C,
        c0yl[0], c0yl[1], c0yl[2]);
  }
  exit(0);
}
