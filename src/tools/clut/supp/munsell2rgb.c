#include "core/core.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// convert xy chromaticity coordinates to munsell colour definition and back.
//
// i think the dependency on V or Y is bogus. needs absolute calibration
// and can't be read from this data. especially not when generalising to hdr.
// so the best i think we can go is use V=5 (has most real entries and no
// negative xy), use munsell-all.dat (with extrapolation) and create a 2d lut
// here, projecting any brightness to this level first.
// the lut is a grid of munsell points with xy entries 40x21.

// the precomputed table
static const int hdim = 40;
static const int cdim = 21;
static float tex[4*40*21] = {0.0f};
// TODO: need to adapt to d65! (0.31271, 0.32902) (maybe we can cheat and it's still far enough inside the inner ellipse..)
static const float illuminant_C[2] = { 0.31006,	0.31616 };

typedef struct mcol_t
{
  char name[10];
  float V, C;
  float x, y, Y;
}
mcol_t;
static inline float
xy_to_monotone_hue_angle(
    float x,
    float y)
{
  // the colours in the chart go clockwise, the angle will go counter clockwise, hence the sign.
  // also we subtract the angle of hue index==0 so the output will start at zero and grow monotonically.
  // this is required for the binary search to work correctly.
  return fmod(2.0f*M_PI-2.52f-atan2f(y-illuminant_C[1], x-illuminant_C[0]), 2.0f*M_PI);
}

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

  int j=0;
  // for(;i<2634;i++)
  for(int i=0;i<5000;i++)
  {
    int ret = fscanf(f, "%s %g %g %g %g %g\n",
        c[j].name, &c[j].V, &c[j].C, &c[j].x, &c[j].y, &c[j].Y);
    if(ret < 6) break; // probably end of file

    // now discard stuff that isn't V==5
    if(c[j].V != 5) continue;

    // parse hue name into something useful
    char *letters = c[j].name;
    float q = strtof(c[j].name, &letters); // quantisation in between bases, this is 2.5 5 7.5 or 10
    float base = 0;
    if     (!strncmp(letters, "BG", 2)) base = 2;
    else if(!strncmp(letters, "GY", 2)) base = 4;
    else if(!strncmp(letters, "YR", 2)) base = 6;
    else if(!strncmp(letters, "RP", 2)) base = 8;
    else if(!strncmp(letters, "PB", 2)) base = 10;
    else if(!strncmp(letters, "B", 1))  base = 1;
    else if(!strncmp(letters, "G", 1))  base = 3;
    else if(!strncmp(letters, "Y", 1))  base = 5;
    else if(!strncmp(letters, "R", 1))  base = 7;
    else if(!strncmp(letters, "P", 1))  base = 9;
    if(base == 0)
      fprintf(stderr, "could not parse colour code! %s\n", c[j].name);

    // convert to a single hue index (we don't really care what it means or how
    // to translate back to munsell):
    base *= 4; // make room for q, now [4..40]
    q *= 4.0 / 10.0; // now {1,2,3,4}
    base -= q;
    const int hue_idx = base;

    // C is {2,4,6,..,30} and {32,..,40} in the -all variant.
    // so we can fill a 40x20 grid hue x chroma.
    const int chroma_idx = c[j].C / 2; // [1..20] (0 will be white)
    tex[4*(cdim * hue_idx + chroma_idx) + 0] = c[j].x;
    tex[4*(cdim * hue_idx + chroma_idx) + 1] = c[j].y;
    tex[4*(cdim * hue_idx + chroma_idx) + 2] = xy_to_monotone_hue_angle(c[j].x, c[j].y);
    tex[4*(cdim * hue_idx + chroma_idx) + 3] = (c[j].x - illuminant_C[0])*(c[j].x - illuminant_C[0])
                                             + (c[j].y - illuminant_C[1])*(c[j].y - illuminant_C[1]);
    // these are constant *Y* not constant *X+Y+Z*, which is annoying.
    // anyhow we're ignoring the dependency on brightness, so we might as well pretend Y is constant too.
    j++; // mark as valid
  }
  fclose(f);

  for(int hi=0;hi<hdim;hi++)
  { // fill in the illuminant C white point at chroma_idx=0 
    tex[4*(cdim * hi + 0) + 0] = illuminant_C[0];
    tex[4*(cdim * hi + 0) + 1] = illuminant_C[1];
    tex[4*(cdim * hi + 0) + 2] = tex[4*(cdim * hi + 1) + 2]; // copy angle from one up so the quad tree can work with it
    tex[4*(cdim * hi + 0) + 3] = 0;
    float delta = tex[4*(cdim * hi + 1) + 3];
    for(int ci=1;ci<cdim;ci++)
    { // fill in all uninitialised values by extrapolating radius and keeping angle.
      if(tex[4*(cdim * hi + ci) + 3] == 0)
        tex[4*(cdim * hi + ci) + 3] = tex[4*(cdim * hi + ci - 1) + 3] + delta;
      if(tex[4*(cdim * hi + ci) + 2] == 0)
        tex[4*(cdim * hi + ci) + 2] = tex[4*(cdim * hi + ci - 1) + 2];
      if(tex[4*(cdim * hi + ci) + 0] == 0)
      {
        float rr = sqrtf(tex[4*(cdim * hi + ci) + 3]/tex[4*(cdim * hi + ci - 1) + 3]);
        tex[4*(cdim * hi + ci) + 0] = (tex[4*(cdim * hi + ci - 1) + 0] - illuminant_C[0]) * rr + illuminant_C[0];
        tex[4*(cdim * hi + ci) + 1] = (tex[4*(cdim * hi + ci - 1) + 1] - illuminant_C[1]) * rr + illuminant_C[1];
      }
      delta = tex[4*(cdim * hi + ci) + 3] - tex[4*(cdim * hi + ci - 1) + 3];
    }
  }
  return j;
}

static inline void
lookup(
    const int hdim,
    const int cdim,
    const float *tex,
    int hue_idx,
    int chroma_idx,
    float res[4])     // read x y theta rad2 from lut
{
  hue_idx    = (hue_idx % hdim + hdim) % hdim; // unsigned modulo
  chroma_idx = CLAMP(chroma_idx, 0, cdim-1);   // clamp to range
  res[0] = tex[4*(cdim * hue_idx + chroma_idx) + 0];
  res[1] = tex[4*(cdim * hue_idx + chroma_idx) + 1];
  res[2] = tex[4*(cdim * hue_idx + chroma_idx) + 2];
  res[3] = tex[4*(cdim * hue_idx + chroma_idx) + 3];
}

// compute which side of a line v0--v1 the point p lies
static inline float
side(
    const float *v0,
    const float *v1,
    const float *p)
{
  return (v1[0]-v0[0])*(p[1]-v0[1]) - (v1[1]-v0[1])*(p[0]-v0[0]);
}

// forward method, interpolate same two triangles as the other direction
static inline void
m2xy(
    const float mhc[2], // 2d munsell coordinate: hue and chroma
    float       xy[2])  // output
{
  float hm = mhc[0] * hdim;
  float cm = mhc[1] * cdim;
  int hidxm = (int)hm;
  int cidxm = (int)cm;
  float hu = hm - hidxm;
  float cu = cm - cidxm;
  float res0[4], res1[4], res2[4], res3[4];
  lookup(hdim, cdim, tex, hidxm, cidxm+1, res3); lookup(hdim, cdim, tex, hidxm+1, cidxm+1, res2);
  lookup(hdim, cdim, tex, hidxm, cidxm,   res0); lookup(hdim, cdim, tex, hidxm+1, cidxm,   res1);
  //  cu = 1
  //    3---2
  // t1 |  /|                    01  11
  //    |/  | t0
  //    0---1  hu = 1            00  10
  if(hu >= cu) // triangle 0
  {
    xy[0] = (1-hu) * res0[0] + (hu-cu) * res1[0] + (cu) * res2[0];
    xy[1] = (1-hu) * res0[1] + (hu-cu) * res1[1] + (cu) * res2[1];
  }
  else // t1
  {
    xy[0] = (hu) * res2[0] + (cu-hu) * res3[0] + (1-cu) * res0[0];
    xy[1] = (hu) * res2[1] + (cu-hu) * res3[1] + (1-cu) * res0[1];
  }
}

// inversion method
// convert xy to polar around illuminant (C)
// find matching munsell box and interpolate the corners
static inline void
xy2m(
    const float xy[2],  // 2d cie chromaticity coordinate
    float       mhc[2]) // output: munsell hue and chroma
{
  int hidxm = 0, hidxM = hdim;
  int cidxm = 0, cidxM = cdim-1;
  const float theta = xy_to_monotone_hue_angle(xy[0], xy[1]);
  const float rad2  = (xy[1]-illuminant_C[1])*(xy[1]-illuminant_C[1]) + (xy[0]-illuminant_C[0])*(xy[0]-illuminant_C[0]);
  float res[4];
  while(1)
  { // quad tree refinement step using midpoint
    int hidx = (hidxm + hidxM)/2;
    int cidx = (cidxm + cidxM)/2;
    lookup(hdim, cdim, tex, hidx, cidx, res);
    if(res[2] <= theta) hidxm = hidx;
    else                hidxM = hidx;
    if(res[3] <= rad2)  cidxm = cidx;
    else                cidxM = cidx;
    if(hidxM <= hidxm + 1 && cidxM <= cidxm + 1) // reached last level/resolution limit
      break;
  }

  // now we reached the leaf level and may have found our quad.
  // unfortunately the quad tree is just very approximate, so maybe
  // we'll be outside the quad. in this case, we'll walk the grid
  // a bit until we find a containing quad.
  while(1)
  {
    assert(cidxm >= 0);
    float res0[4], res1[4], res2[4], res3[4];
    // this polygon winds ccw:
    lookup(hdim, cdim, tex, hidxm, cidxm+1, res3); lookup(hdim, cdim, tex, hidxm+1, cidxm+1, res2);
    lookup(hdim, cdim, tex, hidxm, cidxm,   res0); lookup(hdim, cdim, tex, hidxm+1, cidxm,   res1);
    // these are all positive if xy is inside the polygon
    float s0 = side(res0, res1, xy), s1 = side(res1, res2, xy);
    float s2 = side(res2, res3, xy), s3 = side(res3, res0, xy);
    // step to next cells
    if     (s0 < 0 && cidxm > 0) cidxm--;
    else if(s0 < 0 && cidxm == 0) hidxm = ((hidxm + hdim/2) % hdim + hdim) % hdim; // other side of white
    else if(s2 < 0 && cidxm < cdim-2) cidxm++;
    if     (s1 < 0) hidxm++;
    else if(s3 < 0) hidxm--;
    if(s0 >= 0 && s1 >= 0 && s3 >= 0 && (s2 >= 0 || cidxm >= cdim-2))
    { // yay we're inside our quad!
      float t0 = side(res0, res1, res2); // size of triangle 0
      float t1 = side(res2, res3, res0); // size of triangle 1
      float u0, u1, u2, u3;
      if(cidxm > 0 && s0 + s1 <= t0)
      { // triangle 0
        u2 = s0 / t0;
        u0 = s1 / t0;
        u1 = 1.0f-u0-u2;
        u3 = 0.0f;
      }
      else
      { // triangle 1
        u2 = s3 / t1;
        u0 = s2 / t1;
        u3 = 1.0-u0-u2;
        u1 = 0.0f;
      }
      // now interpolate grid corner coordinates via these barycentrics:
      float hi = u0 * hidxm + u1 * (hidxm+1.0f) + u2 * (hidxm+1.0f) + u3 *  hidxm;
      float ci = u0 * cidxm + u1 *  cidxm       + u2 * (cidxm+1.0f) + u3 * (cidxm+1.0f);
      mhc[0] = hi / hdim;
      mhc[1] = ci / cdim;
      return;
    }
  }
}

int main(int argc, char *argv[])
{
  mcol_t *col;
  // int cnt = parse_data("munsell.dat", &col);
  int cnt = parse_data("munsell-all.dat", &col);
  if(!cnt)
  {
    fprintf(stderr, "could not parse munsell.dat!\n");
    exit(2);
  }
#if 1
  fprintf(stdout, "const float16_t munsell_xy[2*21*40] = {\n");
  for(int hi=0;hi<hdim;hi++) for(int ci=0;ci<cdim;ci++)
  {
    float cat[3][3] = { // this is bruce lindbloom's C->D65 bradford matrix
      { 0.9904476, -0.0071683, -0.0116156},
      {-0.0123712,  1.0155950, -0.0029282},
      {-0.0035635,  0.0067697,  0.9181569}};
    float xyz_c[3] = { tex[4*(cdim * hi + ci) + 0], tex[4*(cdim * hi + ci) + 1]};
    xyz_c[2] = 1.0f - xyz_c[0] - xyz_c[1];
    float xyz_d65[3] = {0.0f};
    for(int j=0;j<3;j++)
      for(int i=0;i<3;i++)
        xyz_d65[j] += cat[j][i] * xyz_c[i];

    const float b = xyz_d65[0] + xyz_d65[1] + xyz_d65[2];
    xyz_d65[0] /= b;
    xyz_d65[1] /= b;

    fprintf(stdout, "float16_t(%g), float16_t(%g), ", xyz_d65[0], xyz_d65[1]);
  }
  fprintf(stdout, "};\n");
#endif
#if 0
  float munsell[2];
  for(int j=0;j<100;j++)
  for(int i=0;i<100;i++)
  {
    float xy[2] = {i/100.0, j/100.0};
    // float xy[2] = {illuminant_C[0] + 0.05 - 0.1 * i/100.0, illuminant_C[1] + 0.05 - 0.1 * j/100.0};
    xy2m(xy, munsell);
    m2xy(munsell, xy);
    fprintf(stderr, "%g %g %g %g %g %g\n", munsell[0], munsell[1], xy[0], xy[1], i/100.0, j/100.0);
  }
#endif
  exit(0);
}
