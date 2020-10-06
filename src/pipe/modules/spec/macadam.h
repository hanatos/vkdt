#pragma once

// simple header file to evaluate a MacAdam-style box spectrum from a precomputed map.

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CLAMP(x, m, M) (((x) < (M)) ? (((x) > (m)) ? (x) : (m)) : (M))

typedef struct MacAdam_t
{
  uint64_t w, h;
  float *buf;
}
MacAdam_t;

static inline int
MacAdam_init(MacAdam_t *m, const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;

  if(fscanf(f, "PF\n%" PRIu64 " %" PRIu64 "\n%*[^\n]", &m->w, &m->h) != 2) { fclose(f); return 2; }
  fgetc(f); // eat new line
  m->buf = (float*)malloc(3*sizeof(float)*m->w*m->h); // stupid c++ requires the cast here
  if(fread(m->buf, 3*sizeof(float), m->w*m->h, f) != m->w*m->h) { fclose(f); return 3; }
  fclose(f);
  return 0;
}

static inline void
MacAdam_cleanup(MacAdam_t *m)
{
  free(m->buf);
  memset(m, 0, sizeof(*m));
}

static inline float
MacAdam_eval_brightness(const MacAdam_t *m, const float *xy)
{
  const int i = CLAMP(xy[0] * m->w, 0, m->w-1);
  const int j = CLAMP(xy[1] * m->h, 0, m->h-1);
  // maximum brightness for this colour (= X+Y+Z):
  return m->buf[3*(m->w*j+i)+0];
}

static inline float
MacAdam_eval(const MacAdam_t *m, const float lambda, const float *xyz)
{
  const float b = xyz[0] + xyz[1] + xyz[2];
  const float xy[2] = {xyz[0]/b, xyz[1]/b};
  const int i = CLAMP(xy[0] * m->w+.5, 0, m->w-1);
  const int j = CLAMP(xy[1] * m->h+.5, 0, m->h-1);
  // maximum brightness for this colour (= X+Y+Z):
  const float mb = m->buf[3*(m->w*j+i)+0];
  const float l0 = m->buf[3*(m->w*j+i)+1];
  const float l1 = m->buf[3*(m->w*j+i)+2];
  //fprintf(stderr, "input %g %g %g -- %g\n", mb, l0, l1, b);
  float result = 0.0f;
  if(l0 < l1)
  { // peak
    if(lambda > l0 && lambda <= l1) result = b/mb;
  }
  else
  { // dip
    if(lambda <= l1 || lambda > l0) result = b/mb;
  }
  return result;//CLAMP(result, 0.0f, 1.0f);
}
