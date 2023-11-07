#pragma once
// stuff that goes with a discretised spectrum
#include "sigmoid.h"
#include <stdio.h>
#include <string.h>

static inline double
spectrum_integrate(
    const double (*cfa_spec)[4],
    const int      channel,
    const int      cnt,
    const double  *cf,
    const int      cf_cnt)
{
  double out = 0.0;
  for(int i=0;i<cnt;i++)
  {
    double lambda = cfa_spec[i][0];
    double s = sigmoid(poly(cf, lambda, cf_cnt));
    double t = cfa_spec[i][1+channel];
    out += s*t;
  }
  return out * (cfa_spec[cnt-1][0] - cfa_spec[0][0]) / (double) cnt;
}

static inline double
spectrum_interp(
    const double (*spec)[4],
    const int      cnt,
    const int      chn,
    const double   lambda)
{
  if(lambda < spec[0][0])     return 0.0;
  if(lambda > spec[cnt-1][0]) return 0.0;
  int i0 = 0, i1 = cnt-1;
  while(i1 > i0+1)
  {
    int   im = (i0+i1)/2;
    float lm = spec[im][0];
    if(lm > lambda) i1 = im;
    else            i0 = im;
  }
  double l0 = spec[i0][0];
  double l1 = spec[i1][0];
  double t = (lambda - l0)/(l1 - l0);
  return (1.0-t) * spec[i0][chn+1] + t * spec[i1][chn+1];
}

// loads up to 4 columns from each row in a space-separated file
// pass cfa_spec = 0 for a dry run counting lines
static inline int
spectrum_load(
    const char *model,
    double (*cfa_spec)[4])
{
  char filename[256];
  snprintf(filename, sizeof(filename), "%s.txt", model);
  int /*len = strlen(filename), */ cfa_spec_cnt = 0;
  // for(int i=0;i<len;i++) if(filename[i]==' ') filename[i] = '_';
  FILE *fr = fopen(filename, "rb");
  char line[2048];
  if(fr)
  {
    while(!feof(fr))
    {
      if(fscanf(fr, "%[^\n]", line) == EOF) break;
      fgetc(fr); // \n
      double buf[4] = {0.0};
      if (strstr(line, ", ")) {
        if(1 > sscanf(line, "%lg, %lg, %lg, %lg", buf, buf+1, buf+2, buf+3))
          continue;
      }
      else if (strstr(line, ",")) {
        if(1 > sscanf(line, "%lg,%lg,%lg,%lg", buf, buf+1, buf+2, buf+3))
          continue;
      }
      else {
        if(1 > sscanf(line, "%lg %lg %lg %lg", buf, buf+1, buf+2, buf+3))
          continue; // do nothing, we'll ignore comments and stuff gone wrong
      }
      if(cfa_spec) for(int i=0;i<4;i++) cfa_spec[cfa_spec_cnt][i] = buf[i];
      // fprintf(stderr, "read %d %g %g %g %g\n", cfa_spec_cnt,
      //     cfa_spec[cfa_spec_cnt][0],
      //     cfa_spec[cfa_spec_cnt][1],
      //     cfa_spec[cfa_spec_cnt][2],
      //     cfa_spec[cfa_spec_cnt][3]);
      cfa_spec_cnt++;
    }
    fclose(fr);
  }
  else fprintf(stderr, "[vkdt-mkclut] can't open response curves! `%s'\n", filename);
  return cfa_spec_cnt;
}

// changes the interval of the spec data to the one specified.
// interpolates new values 
static inline int
spectrum_chg_interval(
  double (*spec)[4],
  int spec_cnt,
  int interval
  )
{
  double new_int[1000][4];
  int i=0;
  for (double w = spec[0][0]; w<=spec[spec_cnt-1][0]; w+=interval) {
    new_int[i][0] = w;
    for (int j=0; j<spec_cnt; j++) {
      if (spec[j][0] == w) { //identical input and output wavelengths
        new_int[i][1] = spec[j][1];
        new_int[i][2] = spec[j][2];
        new_int[i][3] = spec[j][3];
        break;
      }
      else if (spec[j][0] < w && spec[j+1][0] > w) {  //interpolate between spec[j] and spec[j+1]
        double interp = (w - spec[j][0]) / (spec[j+1][0] - spec[j][0]);
        new_int[i][1] = spec[j][1] + (spec[j+1][1] - spec[j][1]) * interp;
        new_int[i][2] = spec[j][2] + (spec[j+1][2] - spec[j][2]) * interp;
        new_int[i][3] = spec[j][3] + (spec[j+1][3] - spec[j][3]) * interp;
        break;
      }
    }
    i++;
  }
  for (int k=0; k<i; k++) {
    spec[k][0] = new_int[k][0];
    spec[k][1] = new_int[k][1];
    spec[k][2] = new_int[k][2];
    spec[k][3] = new_int[k][3];
  }
  return i;
}

// white balance three cfa spectra by dividing out one illuminant and multiplying
// by another. this alters the cfa to include "in-scene" white balancing that is
// conceptually happening before the light travels through the sensor.
// the mul spectrum is assumed to be at least as fine grained in wavelength
// resolution as the cfa spectrum. only the same wavelengths as in cfa[.][0] will
// be considered.
static inline void
spectrum_wb(
    int cfa_cnt,
    int mul_cnt,
    double (*cfa)[4],
    double (*mul)[4])
{
  int j = 0;//, k = 0;
  for(int i=0;i<cfa_cnt;i++)
  {
    while(cfa[i][0] > mul[j][0] && j < mul_cnt-1) j++;
    if(cfa[i][0] == mul[j][0])
      for(int l=0;l<3;l++) cfa[i][1+l] *= mul[j][1];
  }
}
