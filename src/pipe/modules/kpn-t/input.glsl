// abstraction for feeding input to the MLP (used during inferencing and backpropagation)
float // return channel
load_input_tap(
    sampler2D img,
    uint      pxid,   // pixel coordinate of center pixel
    uint      chan,   // channel
    vec2      noise)  // gaussian and poissonian noise parameters
{
#if 1 // XXX this only for demosaicing blocks!
  const uint stride = textureSize(img, 0).x/2;
  ivec2 px = 2*ivec2(pxid % stride, pxid / stride);
#if WIDTH==64
  uint j = chan / 8;
  uint i = chan - 8*j;
  px += ivec2(i-3, j-3);
  if(px.x < 0) px.x = -2*((px.x+1)/2) + (px.x & 1);
  if(px.x >= textureSize(img, 0).x) px.x = 2*(textureSize(img, 0).x - px.x/2 - 1) + (px.x & 1);
  if(px.y < 0) px.y = -2*((px.y+1)/2) + (px.y & 1);
  if(px.y >= textureSize(img, 0).y) px.y = 2*(textureSize(img, 0).y - px.y/2 - 1) + (px.y & 1);
  return texelFetch(img, px, 0).r;
#else
#error demosaicing needs 64 wide networks!
#endif
#else
  const uint stride = textureSize(img, 0).x;
  ivec2 px = ivec2(pxid % stride, pxid / stride);
#if WIDTH==64
  uint i = chan / 8;
  uint j = chan - 8*i;
  px += ivec2(i-4, j-4); // not symmetric but who cares
  return luminance_rec2020(texture(img, (px+0.5)/vec2(textureSize(img, 0))).rgb);
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
#endif
}
