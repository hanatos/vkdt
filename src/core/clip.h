#pragma once
#include <stdint.h>
#include <math.h>

static const float dt_spectrum_clip[] =
{ // these are straight x y chromaticities from the CIE 2deg 5nm 1932 cmf (360..830)
  // for reasons of increased accuracy when clipping against this polygon in 2d, we remove a few very similar points:
0.17556022, 0.0052938368,      // 360nm
// 0.17516121, 0.005256346,    // 365nm
// 0.1748206, 0.0052206009,    // 370nm
// 0.1745097, 0.0051816395,    // 375nm
// 0.17411223, 0.0049637253,   // 380nm
// 0.17400792, 0.0049805483,   // 385nm
// 0.17380077, 0.0049154116,   // 390nm
// 0.1735599, 0.0049232021,    // 395nm
// 0.17333688, 0.0047967434,   // 400nm
// 0.17302096, 0.0047750501,   // 405nm
// 0.17257656, 0.0047993022,   // 410nm
// 0.17208663, 0.0048325243,   // 415nm
// 0.17140743, 0.0051021711,   // 420nm
0.17030099, 0.0057885051,      // 425nm
0.16887753, 0.0069002444,
0.1668953, 0.008555606,
0.16441177, 0.010857558,
0.16110459, 0.013793359,
0.15664095, 0.017704805,
0.1509854, 0.022740193,
0.1439604, 0.029702971,
0.13550267, 0.039879121,
0.12411848, 0.057802513,
0.10959432, 0.0868425,
0.091293514, 0.13270205,
0.068705924, 0.20072323,
0.045390733, 0.29497597,
0.023459941, 0.41270345,
0.0081680277, 0.53842306,
0.0038585211, 0.65482318,
0.013870245, 0.75018644,
0.038851801, 0.81201601,
0.074302427, 0.83380306,
0.11416072, 0.82620692,
0.15472205, 0.8058635,
0.1928761, 0.78162926,
0.22961967, 0.75432903,
0.26577508, 0.72432393,
0.30160379, 0.69230777,
0.33736333, 0.65884829,
0.37310153, 0.62445086,
0.40873623, 0.58960682,
0.44406244, 0.5547139,
0.47877479, 0.52020234,
0.5124864, 0.4865908,
0.54478651, 0.45443413,
0.57515132, 0.42423227,
0.60293275, 0.39649659,
0.62703663, 0.37249115,
0.64823312, 0.35139492,
0.66576356, 0.33401066,
0.68007886, 0.31974724,
0.691504, 0.30834225,
0.70060605, 0.2993007,
0.70791781, 0.29202709,
0.71403158, 0.28592888,
0.71903288, 0.28093493,
0.72303158, 0.27694833,
0.72599232, 0.27400771,
0.72827172, 0.27172828,
0.72996902, 0.27003098,
0.73108935, 0.26891059,
0.73199332, 0.26800671,
0.73271894, 0.26728109,
0.73341697, 0.26658306,
// 0.73404729, 0.26595268,
// 0.7343902, 0.26560983,
// 0.73459166, 0.26540834,
// 0.73469001, 0.26530999,
// 0.73469001, 0.26530999,
// 0.73468995, 0.26531002,
// 0.73469001, 0.26530999,
// 0.73469001, 0.26530999,
// 0.73469001, 0.26530999,
// 0.73468989, 0.26531002,
// 0.73469001, 0.26531002,
// 0.73468995, 0.26530999,
// 0.73469001, 0.26530999,
// 0.73469001, 0.26530996,
// 0.73469001, 0.26530999,
// 0.73468995, 0.26531005,
// 0.73468989, 0.26531008,
// 0.73469001, 0.26531002,
// 0.73469001, 0.26531002,
// 0.73468995, 0.26531002,
// 0.73469001, 0.26531002,
// 0.73468995, 0.26531005,
// 0.73468995, 0.26531002,
// 0.73468995, 0.26530999,
// 0.73468995, 0.26530999,
// 0.73469001, 0.26530999,
// 0.73469001, 0.26530999,
// 0.73469001, 0.26531002,
// 0.73468995, 0.26531002,
// 0.73468995, 0.26531002,
0.17556022, 0.0052938368,
};

static inline int
dt_spectrum_outside(float x, float y)
{
  if(x+y > 1.0f) return 1;
  const int num_bins = sizeof(dt_spectrum_clip)/2/sizeof(dt_spectrum_clip[0]);
  const float *prev = dt_spectrum_clip;
  for(int l=1;l<num_bins;l++)
  {
    const float *curr = dt_spectrum_clip + 2*l;
    const float edge1[2] = {curr[0]-prev[0], curr[1]-prev[1]};
    const float edge2[2] = {x-prev[0], y-prev[1]};
    const float det = edge1[0]*edge2[1] - edge1[1]*edge2[0];
    if(det >= 0.0f) return 1;
    prev = curr;
  }
  return 0;
}

// clip the given point v to the polygon p by projecting towards white w
static inline void
dt_spectrum_clip_poly(
    const float *p,        // pointer to polygon points
    uint32_t     p_cnt,    // number of corners
    const float *w,        // constant white
    float       *v)        // point potentially outside, will clip this to p
{
  for(int i=0;i<p_cnt-1;i++)
  {
    float vp0 [] = {v[0]-p[2*i+0], v[1]-p[2*i+1]};
    float p1p0[] = {p[2*i+2+0]-p[2*i+0], p[2*i+2+1]-p[2*i+1]};
    float n[] = {p1p0[1], -p1p0[0]};
    float dotv = n[0]*vp0[0] + n[1]*vp0[1];
    if(dotv >= 0) continue; // inside

    // intersect the straight line with the polygon:
    float dotvw = n[0]*(v[0]-w[0]) + n[1]*(v[1]-w[1]);
    float dotpw = n[0]*(p[2*i]-w[0]) + n[1]*(p[2*i+1]-w[1]);
    float t = dotpw/dotvw;
    if(t > 0.0f && t < 1.0f)
    { // only clip if in front of us and shorter distance than current
      v[0] = w[0] + t * (v[0] - w[0]);
      v[1] = w[1] + t * (v[1] - w[1]);
    }
  }
}

// compute a fake saturation value, 0.0 means white and 1.0 means spectral
static inline float
dt_spectrum_saturation(
    const float *xy,   // the cie xy chromaticity value pair
    const float *w)    // white e.g. D65: {.3127266, .32902313}
{
  // extrude xy coords to far out
  float out[2] = {xy[0] - w[0], xy[1] - w[1]};
  float scale = 1.0f/sqrtf(out[0]*out[0] + out[1]*out[1]);
  out[0] = out[0] * scale + w[0];
  out[1] = out[1] * scale + w[1];
  const int cnt = sizeof(dt_spectrum_clip)/2/sizeof(dt_spectrum_clip[0]);
  dt_spectrum_clip_poly(dt_spectrum_clip, cnt, w, out);
  // now "out" is on the spectral locus
  // compute saturation by comparing distance to white:
  out[0] -= w[0];
  out[1] -= w[1];
  float dist = sqrtf(out[0]*out[0] + out[1]*out[1]);
  return 1.0f/(dist * scale);
}
