layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std140, set = 0, binding = 1) uniform params_t
{
  vec4  mul;
  mat3  cam_to_rec2020;
  uvec4 N;
  mat3  rbf_P;
  vec4  rbf_c[24];
  vec4  rbf_p[24];
  float temp;
  uint  colour_mode;
  float saturation;
  uint  pick_mode;
  uint  gamut_mode;
  uint  primaries;
  uint  trc;
  float clip_highlights;
  vec4  auto_wb;
} params;

layout(push_constant, std140) uniform push_t { int have_pick; } push;

layout(set = 1, binding = 0) uniform sampler2D img_clut;
layout(set = 1, binding = 1) uniform writeonly image2D img_temp;
layout(set = 1, binding = 2
#ifdef HAVE_NO_ATOMICS
) uniform usampler2D img_pick;
#else
) uniform sampler2D img_pick;
#endif

#include "clut.glsl"

void tri2quad(inout vec2 tc)
{
  tc.y = tc.y / (1.0-tc.x);
  tc.x = (1.0-tc.x)*(1.0-tc.x);
}

vec3 picked_rgb()
{
#ifdef HAVE_NO_ATOMICS
  return vec3(
      uintBitsToFloat(texelFetch(img_pick, ivec2(0, 0), 0).r),
      uintBitsToFloat(texelFetch(img_pick, ivec2(0, 1), 0).r),
      uintBitsToFloat(texelFetch(img_pick, ivec2(0, 2), 0).r));
#else
  return vec3(
      texelFetch(img_pick, ivec2(0, 0), 0).r,
      texelFetch(img_pick, ivec2(0, 1), 0).r,
      texelFetch(img_pick, ivec2(0, 2), 0).r);
#endif
}

void main()
{
  if(params.temp >= 0.0) { imageStore(img_temp, ivec2(0), vec4(-1.0)); return; }

  ivec2 clut_size = textureSize(img_clut, 0);
  int nbands = clut_size.x / clut_size.y;
  int n = clut_nanchor(nbands);

  vec3 neutral = vec3(1.0/max(params.auto_wb.r, 1e-6), 1.0, 1.0/max(params.auto_wb.b, 1e-6));
  if(push.have_pick != 0 && (params.pick_mode & 4) != 0) neutral = picked_rgb();
  float nb = max(neutral.r+neutral.g+neutral.b, 1e-6);
  vec2 ntc = neutral.rb/nb;
  tri2quad(ntc);

  const vec2 target = vec2(1.0/3.0);
  vec2 rb_prev = clut_chroma(ntc, 0, nbands);
  float best_bp = 0.0, best_res = 1e30;
  for(int k=0;k<n-1;k++)
  {
    vec2 rb_next = clut_chroma(ntc, k+1, nbands);
    vec2 dv = rb_next - rb_prev;
    float denom = dot(dv, dv);
    float m = denom > 1e-12 ? dot(target-rb_prev, dv)/denom : 0.0;
    float mc = clamp(m, 0.0, 1.0);
    float res = length(target-(rb_prev+mc*dv));
    if(res < best_res) { best_res = res; best_bp = float(k)+mc; }
    rb_prev = rb_next;
  }
  imageStore(img_temp, ivec2(0), vec4(best_bp/float(max(n-1, 1))));
}
