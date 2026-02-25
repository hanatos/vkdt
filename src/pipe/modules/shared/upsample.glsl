#include "matrices.h"
// a version of Jakob2019 sigmoid spectra, but for emission.
void tri2quad(inout vec2 tc)
{
  tc.y = tc.y / (1.0-tc.x);
  tc.x = (1.0-tc.x)*(1.0-tc.x);
}

vec4 fetch_coeff(vec3 rgb)
{
  const mat3 rec2020_to_xyz = matrix_rec2020_to_xyz;
  vec3 xyz = rec2020_to_xyz * rgb;
  float b = dot(vec3(1),xyz);
  vec2 tc = xyz.xy/b;
  tri2quad(tc);
  ivec2 tci = clamp(ivec2(tc * textureSize(img_coeff, 0) + 0.5), ivec2(0), ivec2(textureSize(img_coeff, 0)-1));
  vec4 coeff = texelFetch(img_coeff, tci, 0);
  coeff.w = b / coeff.w;
  return coeff;
}

float sigmoid_eval(
    vec4 coeff,   // from fetch_coeff
    float lambda) // in nanometers
{
  float x = (coeff.x * lambda + coeff.y) * lambda + coeff.z;
  float y = 1. / sqrt(x * x + 1.);
  float val = 0.5 * x * y +  0.5;
  return val * coeff.w;
}
