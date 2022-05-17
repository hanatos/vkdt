// process a camera rgb colour to xyz (d50) according to
// the dng spec version 1.6, chapter 6, "Mapping Camera Color Space to CIE XYZ Space"
#pragma once

#include "matrices.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// holds whatever we need to do the transform.
// these values can be directly filled by exif data.
// since we don't actually use this to interpolate
// for white balancing, the contents are always only
// valid for one of the two or three calibrationilluminants
// in the dng.
// also we're assuming n=3, so no four colours in the cfa.
typedef struct dng_profile_t
{
  char  model[50]; // UniqueCameraModel
  double cm[3][3]; // ColorMatrix [123] nx3
  double cc[3][3]; // CameraCalibration [123] nxn
  double ab[3][3]; // diag(AnalogBalance) nxn
  double rm[3][3]; // ReductionMatrix [123] 3xn
  double fm[3][3]; // ForwardMatrix [123] 3xn
  double wb[3];    // XYZ white balance. A xy = (0.44758, 0.40745) D65 xy = (0.3127,0.329)
  double nt[3];    // AsShotNeutral

  int hsm_dim[3]; // ProfilehueSatmapDims
  float *hsm;     // ProfileHueSatMapData [123]
} dng_profile_t;

static inline void
rgb_to_hsv(const double *rgb, double *hsv)
{
  double M = fmax(rgb[0], fmax(rgb[1], rgb[2]));
  double m = fmin(rgb[0], fmin(rgb[1], rgb[2]));
  double V = M;
  double C = M - m;
  double S = V > 1e-8 ? C / V : 0;
  double H = 0.0;
  if(C == 0) H = 0;
  else if(V == rgb[0]) H = 60.0 * (0 + (rgb[1] - rgb[2])/C);
  else if(V == rgb[1]) H = 60.0 * (2 + (rgb[2] - rgb[0])/C);
  else if(V == rgb[2]) H = 60.0 * (4 + (rgb[0] - rgb[1])/C);
  hsv[0] = H;
  hsv[1] = S;
  hsv[2] = V;
}

static inline void
hsv_to_rgb(const double *hsv, double *rgb)
{
  double V = hsv[2];
  double C = hsv[2] * hsv[1];
  double H = hsv[0] / 60.0;
  double X = C * (1.0 - fabs(fmod(H, 2.0) - 1.0));
  if     (H < 1.0) { rgb[0] = C; rgb[1] = X; rgb[2] = 0; }
  else if(H < 2.0) { rgb[0] = X; rgb[1] = C; rgb[2] = 0; }
  else if(H < 3.0) { rgb[0] = 0; rgb[1] = C; rgb[2] = X; }
  else if(H < 4.0) { rgb[0] = 0; rgb[1] = X; rgb[2] = C; }
  else if(H < 5.0) { rgb[0] = X; rgb[1] = 0; rgb[2] = C; }
  else if(H < 6.0) { rgb[0] = C; rgb[1] = 0; rgb[2] = X; }
  double m = V - C;
  rgb[0] += m;
  rgb[1] += m;
  rgb[2] += m;
}

// the order is value, hue, saturation, hsv (major to minor)
static inline void
lookup_hsm(
    dng_profile_t *p,
    const double  *hsv,
    float         *out)
{
  const int w = p->hsm_dim[1], h = p->hsm_dim[0], d = p->hsm_dim[2];
  float x =  hsv[1] * w;
  float y = (hsv[0] / 360.0f) * h;
  float z =  hsv[2] * d;
  int x0 = (int)x, y0 = (int)y, z0 = (int)z;
  x0 = x0 >= w ? w-1 : x0;
  y0 = (y0 % h + h) % h; // unsigned mod
  z0 = z0 >= d ? d-1 : z0;
  int x1 = x0+1 >= w ? w-1 : x0+1;
  int y1 = (y0 + 1) % h;
  int z1 = z0+1 >= d ? d-1 : z0+1;
  int fx = x - x0, fy = y - y0, fz = z - z0;
  out[0] = out[1] = out[2] = 0.0f;
  for(int k=0;k<3;k++)
  {
    out[k] += (1.0-fx)*(1.0-fy)*(1.0-fz) * p->hsm[3 * (x0 + w * (y0 + d * z0)) + k];
    out[k] += (    fx)*(1.0-fy)*(1.0-fz) * p->hsm[3 * (x1 + w * (y0 + d * z0)) + k];
    out[k] += (1.0-fx)*(    fy)*(1.0-fz) * p->hsm[3 * (x0 + w * (y1 + d * z0)) + k];
    out[k] += (    fx)*(    fy)*(1.0-fz) * p->hsm[3 * (x1 + w * (y1 + d * z0)) + k];
    out[k] += (1.0-fx)*(1.0-fy)*(    fz) * p->hsm[3 * (x0 + w * (y0 + d * z1)) + k];
    out[k] += (    fx)*(1.0-fy)*(    fz) * p->hsm[3 * (x1 + w * (y0 + d * z1)) + k];
    out[k] += (1.0-fx)*(    fy)*(    fz) * p->hsm[3 * (x0 + w * (y1 + d * z1)) + k];
    out[k] += (    fx)*(    fy)*(    fz) * p->hsm[3 * (x1 + w * (y1 + d * z1)) + k];
  }
}

// fill profile from DNG tags
static inline void
dng_profile_fill(
    dng_profile_t *p,          // profile data to be filled
    const char    *filename,   // DNG filename
    const int      illuminant) // 1 : A, 2 : D65
{
  memset(p, 0, sizeof(*p));
  FILE *f;
  char command[2048];
  snprintf(command, sizeof(command), "exiftool -ColorMatrix%d -b -m %s", illuminant, filename);
  f = popen(command, "r");
  double *ptr = &p->cm[0][0];
  fscanf(f, "%lg %lg %lg %lg %lg %lg %lg %lg %lg",
      ptr+0, ptr+1, ptr+2, ptr+3, ptr+4, ptr+5, ptr+6, ptr+7, ptr+8);
  fprintf(stderr, "ColorMatrix%d: %lg %lg %lg %lg %lg %lg %lg %lg %lg\n", illuminant,
      p->cm[0][0], p->cm[0][1], p->cm[0][2],
      p->cm[1][0], p->cm[1][1], p->cm[1][2],
      p->cm[2][0], p->cm[2][1], p->cm[2][2]);
  pclose(f);

  p->cc[0][0] = p->cc[1][1] = p->cc[2][2] = 1.0;
  snprintf(command, sizeof(command), "exiftool -CameraCalibration%d -b -m %s", illuminant, filename);
  f = popen(command, "r");
  ptr = &p->cc[0][0];
  fscanf(f, "%lg %lg %lg %lg %lg %lg %lg %lg %lg",
      ptr+0, ptr+1, ptr+2, ptr+3, ptr+4, ptr+5, ptr+6, ptr+7, ptr+8);
  fprintf(stderr, "CameraCalibration%d: %lg %lg %lg %lg %lg %lg %lg %lg %lg\n", illuminant,
      p->cc[0][0], p->cc[0][1], p->cc[0][2],
      p->cc[1][0], p->cc[1][1], p->cc[1][2],
      p->cc[2][0], p->cc[2][1], p->cc[2][2]);
  pclose(f);

  p->nt[0] = p->nt[1] = p->nt[2] = 1.0;
  snprintf(command, sizeof(command), "exiftool -AsShotNeutral -b -m %s", filename);
  f = popen(command, "r");
  ptr = &p->nt[0];
  fscanf(f, "%lg %lg %lg", ptr+0, ptr+1, ptr+2);
  fprintf(stderr, "AsShotNeutral: %lg %lg %lg\n", p->nt[0], p->nt[1], p->nt[2]);
  pclose(f);

  p->ab[0][0] = p->ab[1][1] = p->ab[2][2] = 1.0;
  snprintf(command, sizeof(command), "exiftool -AnalogBalance -b -m %s", filename);
  f = popen(command, "r");
  ptr = &p->ab[0][0];
  fscanf(f, "%lg %lg %lg", ptr+0, ptr+4, ptr+8);
  fprintf(stderr, "AnalogBalance: %lg %lg %lg %lg %lg %lg %lg %lg %lg\n",
      p->ab[0][0], p->ab[0][1], p->ab[0][2],
      p->ab[1][0], p->ab[1][1], p->ab[1][2],
      p->ab[2][0], p->ab[2][1], p->ab[2][2]);
  pclose(f);

  p->rm[0][0] = p->rm[1][1] = p->rm[2][2] = 1.0;
  snprintf(command, sizeof(command), "exiftool -ReductionMatrix%d -b -m %s", illuminant, filename);
  f = popen(command, "r");
  ptr = &p->rm[0][0];
  fscanf(f, "%lg %lg %lg %lg %lg %lg %lg %lg %lg",
      ptr+0, ptr+1, ptr+2, ptr+3, ptr+4, ptr+5, ptr+6, ptr+7, ptr+8);
  fprintf(stderr, "ReductionMatrix%d: %lg %lg %lg %lg %lg %lg %lg %lg %lg\n", illuminant,
      p->rm[0][0], p->rm[0][1], p->rm[0][2],
      p->rm[1][0], p->rm[1][1], p->rm[1][2],
      p->rm[2][0], p->rm[2][1], p->rm[2][2]);
  pclose(f);

  snprintf(command, sizeof(command), "exiftool -ForwardMatrix%d -b -m %s", illuminant, filename);
  f = popen(command, "r");
  ptr = &p->fm[0][0];
  fscanf(f, "%lg %lg %lg %lg %lg %lg %lg %lg %lg",
      ptr+0, ptr+1, ptr+2, ptr+3, ptr+4, ptr+5, ptr+6, ptr+7, ptr+8);
  fprintf(stderr, "ForwardMatrix%d: %lg %lg %lg %lg %lg %lg %lg %lg %lg\n", illuminant,
      p->fm[0][0], p->fm[0][1], p->fm[0][2],
      p->fm[1][0], p->fm[1][1], p->fm[1][2],
      p->fm[2][0], p->fm[2][1], p->fm[2][2]);
  pclose(f);

  int have_hsm = 1;
  snprintf(command, sizeof(command), "exiftool -ProfileHueSatMapDims -b -m %s", filename);
  f = popen(command, "r");
  if(fscanf(f, "%d %d %d", p->hsm_dim+0, p->hsm_dim+1, p->hsm_dim+2) != 3)
    have_hsm = 0;
  else fprintf(stderr, "found HueSatMap %d %d %d\n", p->hsm_dim[0], p->hsm_dim[1], p->hsm_dim[2]);
  pclose(f);

  if(have_hsm)
  {
    p->hsm = malloc(sizeof(float)*3*p->hsm_dim[0]*p->hsm_dim[1]*p->hsm_dim[2]);
    snprintf(command, sizeof(command), "exiftool -ProfileHueSatMapData%d -b -m %s", illuminant, filename);
    f = popen(command, "r");
    int i = 0;
    while(!feof(f))
    {
      fscanf(f, "%g", p->hsm+i);
      i++;
    }
    pclose(f);
  }

  // init wb as xyz of one of the two calibration illuminantsw
  // could double check the string in -CalibrationIlluminant[12]
  if(illuminant == 1)
  { // std illuminant A in XYZ (illum E)
    p->wb[0] = 0.44758;
    p->wb[1] = 0.40745;
    p->wb[2] = 1.0-p->wb[0]-p->wb[1];
  }
  if(illuminant == 2)
  { // D65 in XYZ (illum E)
    p->wb[0] = 0.3127;
    p->wb[1] = 0.329;
    p->wb[2] = 1.0-p->wb[0]-p->wb[1];
  }

  snprintf(command, sizeof(command), "exiftool -UniqueCameraModel -b -m %s", filename);
  f = popen(command, "r");
  fscanf(f, "%[^\n]", p->model);
  pclose(f);
}

static inline void
dng_process(
    dng_profile_t *p,
    const double  *cam_rgb,
    double        *xyz)      // D50
{
  double ab_cc[3][3], ab_cc_i[3][3], refn[3];
  mat3_mul(p->ab, p->cc, ab_cc); // mostly diag(1 1 1) * diag(moderate wb)
  mat3_inv(ab_cc, ab_cc_i);      // mostly invert a diagonal, often identity

  // XYZtoCamera = AB * CC * CM
  double xyz_to_cam[3][3], camn[3];
  mat3_mul(ab_cc, p->cm, xyz_to_cam);
  // CameraNeutral = XYZtoCamera * XYZ
  // where XYZ (p->wb) is the white balance neutral
  mat3_mulv(xyz_to_cam, p->wb, camn); // this white balances the output, i.e. A and D65 lit looks similar after transform

  // D  = diag nxn with
  mat3_mulv(ab_cc_i, camn, refn);
  // refn: ReferenceNeutral = Inverse (AB * CC) * CameraNeutral
  // D = Invert (AsDiagonalMatrix (ReferenceNeutral))
  double D[3][3] = {{1.0/refn[0], 0, 0},
                    {0, 1.0/refn[1], 0},
                    {0, 0, 1.0/refn[2]}};

  double cam_to_xyzd50[3][3], d_ab_cc_i[3][3];
  // CameraToXYZ_D50 = FM * D * Inverse (AB * CC)
  mat3_mul(D, ab_cc_i, d_ab_cc_i);
  mat3_mul(p->fm, d_ab_cc_i, cam_to_xyzd50);
#if 0
  // TODO in case the fm is missing/garbage (often times the case):
  // use the colour matrix instead.
  // we assume n=3 everywhere
  double cam_to_xyz[3][3];
  mat3_inv(xyz_to_cam, cam_to_xyz);
  // mat3_inv(p->cm, cam_to_xyz); // XXX DEBUG
  // CameraToXYZ_D50 = CA * CameraToXYZ
  // where CA is the linear bradford algorithm. what a bunch of bollocks.
  // TODO: probably have to choose this one according to A vs. D65. now it doesn't work.
  // this is just the illuminant E (xyz) to illuminant D50 matrix:
  double CA[3][3] = {{ 0.9977545, -0.0041632, -0.0293713},
                     {-0.0097677,  1.0183168, -0.0085490},
                     {-0.0074169,  0.0134416,  0.8191853}};
  mat3_mul(CA, cam_to_xyz, cam_to_xyzd50);
#endif

  // matrix mul xyz = cam_to_xyzd50 * cam_rgb
  mat3_mulv(cam_to_xyzd50, cam_rgb, xyz);

  if(!p->hsm) return; // no hsv lut in this dng profile
  // FIXME i still think i'm doing this very wrong. i can fit a profile with 10x less error
  // if i ignore this thing. it seems it's trying to do the right thing (adjust blues for fuji camera
  // slightly) but then something's off. scaling factor on the input from the optimiser?
  // probably read dng spec again and see if it assumes xyz to be normalised somehow.
  // only found the reference that camera rgb should be black subtracted and scaled/clipped to 0, 1.
  // since during optimisation we don't know how the intermediate CFA model will be normalised,
  // we could compute a canonical exposure value and try to fit the working set into [0,1] each time
  // we update CFA. or we assume the mapping here doesn't depend on V and normalise before entering here?
  return; // XXX
  // hsv map dance:
  double rgb[3], hsv[3];
  // convert to prophotorgb
  mat3_mulv(xyz_to_prophoto_rgb, xyz, rgb);
  // convert to hsv
  rgb_to_hsv(rgb, hsv);
  // lookup hsv map
  float hsvmap[3];
  lookup_hsm(p, hsv, hsvmap);
  // add hue shift and wrap
  hsv[0] = fmod(hsv[0] + hsvmap[0], 360.0f);
  // multiply sat factor and clamp
  hsv[1] *= hsvmap[1];
  if(hsv[1] > 1.0f) hsv[1] = 1.0f;
  // multiply value scale and clamp
  hsv[2] *= hsvmap[2];
  // this is in the spec but breaks highlights.
  // i'm not sure how this should be clipped/scaled before, so i just don't.
  // this is potentially a big problem because if the profile depends on V,
  // a wrong scale will shift it in weird directions. i have yet to see
  // one of these profiles though.
  // if(hsv[2] > 1.0f) hsv[2] = 1.0f;
  // convert to rgb
  hsv_to_rgb(hsv, rgb);
  // convert to xyz
  mat3_mulv(prophoto_rgb_to_xyz, rgb, xyz);
}
