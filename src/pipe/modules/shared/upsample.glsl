#include "matrices.h"
#include "shared/skin.glsl"
// a version of Jakob2019 sigmoid spectra, but for emission.
void tri2quad(inout vec2 tc)
{
  tc.y = tc.y / (1.0-tc.x);
  tc.x = (1.0-tc.x)*(1.0-tc.x);
}

vec4 fetch_coeff(
    sampler2D img_coeff, // spectral upsampling table texture (see mkspectra)
    vec3      xyz)       // xyz coordinate, depending on texture means CIE XYZ or camera rgb (replacing cmf with cfa ssf)
{
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
  float y = inversesqrt(x * x + 1.0);
  float val = 0.5 * x * y +  0.5;
  return val * coeff.w;
}

vec4 sigmoid_eval(
    vec4 coeff,    // from fetch_coeff
    vec4 lambda)   // four bands in nanometers
{
  vec4 x = (coeff.x * lambda + coeff.y) * lambda + coeff.z;
  vec4 y = inversesqrt(x * x + vec4(1.0));
  return (0.5 * x * y + vec4(0.5)) * coeff.w;
}
