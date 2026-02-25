#include "matrices.h"
vec3 rec2020_to_xyY(vec3 rec2020)
{
  const mat3 rec2020_to_xyz = matrix_rec2020_to_xyz;
  const vec3 xyz = rec2020_to_xyz * rec2020;
  return vec3(xyz.xy / dot(vec3(1),xyz), xyz.y);
}

vec3 XYZ_to_rec2020(vec3 xyz)
{
  const mat3 xyz_to_rec2020 = matrix_xyz_to_rec2020;
  return xyz_to_rec2020 * xyz;
}

vec3 xyY_to_rec2020(vec3 xyY)
{
  const vec3 xyz = vec3(xyY.xy, 1.0-xyY.x-xyY.y) * xyY.z / xyY.y;
  return XYZ_to_rec2020(xyz);
}

vec3 rec2020_to_oklab(vec3 rgb)
{
  mat3 M = mat3( // M1 * rgb_to_xyz
      0.61668844, 0.2651402 , 0.10015065,
      0.36015907, 0.63585648, 0.20400432,
      0.02304329, 0.09903023, 0.69632468);
  vec3 lms = M * rgb;
  lms = pow(max(vec3(0.0), lms), vec3(1.0/3.0));
  mat3 M2 = mat3(
      0.21045426,  1.9779985 ,  0.02590404,
      0.79361779, -2.42859221,  0.78277177,
     -0.00407205,  0.45059371, -0.80867577);
  return M2 * lms;
}

vec3 oklab_to_rec2020(vec3 oklab)
{
  mat3 M2inv = mat3(
      1.        ,  1.00000001,  1.00000005,
      0.39633779, -0.10556134, -0.08948418,
      0.21580376, -0.06385417, -1.29148554);
  vec3 lms = M2inv * oklab;
  lms = lms*lms*lms;
  mat3 M = mat3( // = xyz_to_rec2020 * M1inv
      2.14014041, -0.88483245, -0.04857906,
     -1.24635595,  2.16317272, -0.45449091,
      0.10643173, -0.27836159,  1.50235629);
  return M * lms;
}

vec3 rec2020_to_oklsh(vec3 rgb)
{
  vec3 oklab = rec2020_to_oklab(rgb);
  if(oklab.x <= 0.0) return vec3(0);
  oklab.yz /= 0.4*max(0.01, oklab.x); // divide L for "hunt effect" (saturation, not chroma)
  return vec3(oklab.x, length(oklab.yz), fract(1.0 + atan(oklab.z, oklab.y)/(2.0*M_PI)));
}

vec3 oklsh_to_rec2020(vec3 lsh)
{
  vec3 lab = vec3(lsh.x, lsh.y * cos(2.0*M_PI*lsh.z), lsh.y * sin(2.0*M_PI*lsh.z));
  if(lab.x <= 0.0) return vec3(0);
  lab.yz *= 0.4*max(0.01, lab.x);
  return oklab_to_rec2020(lab);
}
// since hsv is a severely broken concept, we mean oklab LCh (or hCL really)
vec3 rgb2hsv(vec3 c)
{
  vec3 oklab = rec2020_to_oklab(c);
  return vec3(fract(1.0 + atan(oklab.z, oklab.y)/(2.0*M_PI)), length(oklab.yz), oklab.x);
}

vec3 hsv2rgb(vec3 hCL)
{
  vec3 oklab = vec3(hCL.z, hCL.y * cos(2.0*M_PI*hCL.x), hCL.y * sin(2.0*M_PI*hCL.x));
  if(oklab.x <= 0.0) return vec3(0);
  return oklab_to_rec2020(oklab);
}

// converts rec2020 to YCbCr via M3 in ITU-R BT.2087-0
vec3 rec2020_to_YCbCr(vec3 c)
{
  mat3 M3 = matrix_rec2020_to_ycbcr;
  return M3 * c;
}

vec3 YCbCr_to_rec2020(vec3 c)
{
  mat3 M3I = matrix_ycbcr_to_rec2020;
  return M3I *  c;
}

vec3 rgb2ych(vec3 c)
{
  vec3 yuv = rec2020_to_YCbCr(c);
  return vec3(yuv.x, 8.0*length(yuv.yz), fract(1.0 + atan(yuv.z, yuv.y)/(2.0*M_PI)));
}

vec3 ych2rgb(vec3 c)
{
  vec3 YCbCr = vec3(c.x, c.y/8.0 * cos(2.0*M_PI*c.z), c.y/8.0 * sin(2.0*M_PI*c.z));
  if(YCbCr.x <= 0.0) return vec3(0);
  return YCbCr_to_rec2020(YCbCr);
}
