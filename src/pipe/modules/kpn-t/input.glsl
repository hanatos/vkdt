// abstraction for feeding input to the MLP (used during inferencing and backpropagation)
vec2 // return channel [0,1]
load_input_tap(
    sampler2D img,
    ivec2 px,   // pixel coordinate of center pixel
    uint tp)    // tap number [0,14] or extra: 15
{
  const ivec2 tap[] = {           ivec2(0, -2),
    ivec2(-2, -1), ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1), ivec2(2, -1),
                   ivec2(-1,  0), ivec2(0,  0), ivec2(1,  0),
    ivec2(-2,  1), ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1), ivec2(2,  1),
                                  ivec2(0,  2), ivec2(666,666)
  };
  if(px.y >= textureSize(img, 0).y)
  { // out of bounds for the input image
    return vec2(0.0);
  }
  if(tp < 15)
  { // return luminance + fake hsl saturation
    px += tap[nonuniformEXT(tp)];
    vec3 rgb = texture(img, (px+0.5)/vec2(textureSize(img, 0))).rgb;
    return vec2(luminance_rec2020(rgb), max(rgb.r, max(rgb.g, rgb.b)) - min(rgb.r, min(rgb.g, rgb.b)));
  }
  // TODO: return noise stddev of center pixel in first channel here:
  return vec2(0.0, 1.0);
}
