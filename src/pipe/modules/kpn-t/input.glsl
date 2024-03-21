#include "shared.glsl"
#include "zernike.glsl"
// abstraction for feeding input to the MLP (used during inferencing and backpropagation)
float // return channel
load_input_tap(
    sampler2D img,
    uint      pxid,   // pixel coordinate of center pixel
    uint      chan,   // channel
    vec2      noise)  // gaussian and poissonian noise parameters
{
  const uint stride = textureSize(img, 0).x;
  ivec2 px = ivec2(pxid % stride, pxid / stride);
#if WIDTH==64
#if 0 // zernike version:
  float z = 0;
  if(chan < 15)
  {
    const int R=4;
    for(int r1=-R;r1<=R;r1++) for(int r0=-R;r0<=R;r0++)
    {
      float rho = sqrt(r0*r0+r1*r1)/(R+0.5);
      float phi = atan(r0, r1);
      z += luminance_rec2020(abs(texture(img, (px+ivec2(r0,r1)+0.5)/vec2(textureSize(img, 0))).rgb))* zernike(chan, rho, phi);
    }
    return z;
  }
  chan-=15;
  uint i = chan / 7;
  uint j = chan - 7*i;
  px += ivec2(i-3, j-3);
  return luminance_rec2020(abs(texture(img, (px+0.5)/vec2(textureSize(img, 0))).rgb));
#else // plain surroundings
  uint i = chan / 8;
  uint j = chan - 8*i;
  px += ivec2(i-4, j-4);
  return luminance_rec2020(abs(texture(img, (px+0.5)/vec2(textureSize(img, 0))).rgb));
#endif
#else
  const ivec2 tap[] = {           ivec2(0, -2),
    ivec2(-2, -1), ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1), ivec2(2, -1),
                   ivec2(-1,  0), ivec2(0,  0), ivec2(1,  0),
    ivec2(-2,  1), ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1), ivec2(2,  1),
                                  ivec2(0,  2), ivec2(666,666)
  };
  if(px.y >= textureSize(img, 0).y || chan > 31)
  { // out of bounds for the input image or no such tap in this input (for 64-wide mlp)
    return 0.0;
  }
  uint tp = chan / 2;
  if(tp < 15)
  { // return luminance + fake hsl saturation
    px += tap[nonuniformEXT(tp)];
    vec3 rgb = texture(img, (px+0.5)/vec2(textureSize(img, 0))).rgb;
    if((chan & 1) != 0)
      return max(rgb.r, max(rgb.g, rgb.b)) - min(rgb.r, min(rgb.g, rgb.b));
    else
      return luminance_rec2020(rgb);
    // return vec2(luminance_rec2020(rgb), max(rgb.r, max(rgb.g, rgb.b)) - min(rgb.r, min(rgb.g, rgb.b)));
  }
  // return noise stddev of center pixel in first channel here.
  // both cases: attenuate by push constant indicating mip level:
  if((chan & 1) != 0) return 1.0;
  const float mul = pow(0.5, push.level); // our mipmaps are 2x2 averages, so the stddev is halved every time (Var = 4 * (1/4)^2*Var(X))
  const float scale = 65535.0;
  float lum = luminance_rec2020(texture(img, (px+0.5)/vec2(textureSize(img, 0))).rgb);
  return mul * sqrt(noise.x + scale*max(lum, 0)*noise.y)/scale;
#endif
}
