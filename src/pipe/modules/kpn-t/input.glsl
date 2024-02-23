// abstraction for feeding input to the MLP (used during inferencing and backpropagation)
float // return channel
load_input_tap(
    sampler2D img,
    ivec2     px,     // pixel coordinate of center pixel
    uint      chan)   // 2 channels per tap number [0,14] or extra: 15
{
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
#ifdef INFERENCE
  return mul * sqrt(params.noise_a + scale*max(lum, 0)*params.noise_b)/scale;
#else
  // during training, grab noise params from ssbo routed in from cnngenin
  return mul * sqrt(ssbo_nab.noise_a + scale*max(lum, 0)*ssbo_nab.noise_b)/scale;
#endif
#endif
}
