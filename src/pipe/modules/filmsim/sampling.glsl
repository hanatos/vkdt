// sampling.glsl: image-space helpers used by passes that resample film data.

vec3 fetch_upsampled(sampler2D img, ivec2 ipos, vec2 tc)
{
  return (textureSize(img, 0) == imageSize(img_out)) ? texelFetch(img, ipos, 0).rgb : sample_catmull_rom(img, tc).rgb;
}

vec3 fetch_coupler_subtracted_exposure(ivec2 ipos, sampler2D img_coupler)
{
  ivec2 out_sz = imageSize(img_out);
  vec2 tc = (ipos + 0.5) / vec2(out_sz);
  vec3 coupler = texture(img_coupler, tc).rgb;
  if (params.couplers > 0) coupler = coupler_saturate_post_blur(coupler);
  return fetch_upsampled(img_in, ipos, tc) - coupler;
}
