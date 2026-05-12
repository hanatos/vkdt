#version 460
#extension GL_EXT_shader_16bit_storage             : enable
#extension GL_EXT_shader_8bit_storage              : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
struct vtx_t
{
  f16vec2 p;
  f16vec2 uv;
  u8vec4  col;
};
layout(set = 1, binding = 0) buffer ssbo_t { vtx_t v[]; } ssbo;
layout(set = 1, binding = 1) uniform sampler2D img;
layout(location = 0) in  vec4 in_colour;
layout(location = 1) in  vec2 in_uv;
layout(location = 0) out vec4 out_colour;
layout(push_constant) uniform push_t
{
  float strength;
} pc;
float median(vec3 v)
{
  return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}
void main()
{
  vec4 tex = texture(img, in_uv);
  if(pc.strength > 0)
  { // render msdf font
    float dist = median(tex.rgb) - 1.0 + pc.strength;
    //
    // dist = dist / fwidth(dist);
    // float opacity = clamp(dist + 0.5, 0.0, 1.0);
    //
    // float dx = dFdx(dist);
    // float dy = dFdy(dist);
    // float g = length(vec2(dx,dy))*0.70710678118; // sqrt(2)/2
    // float opacity = smoothstep(-g, g, dist);
    //
    // minification has some overblur issues, clamp that early, erring on the aliasing side
    dist *= clamp(dot(vec2(1.0/textureSize(img, 0)), 1.0/fwidth(in_uv)), 1.0, 1000.0);
    // float opacity = clamp(dist + 0.5, 0.0, 1.0);
    float opacity = smoothstep(0.0, 1.0, dist + 0.5); // we're blending post gamma, this looks better
    if(opacity < 1.0/256.0) opacity = 0.0; // pull down to straight zero to avoid stupid rounding/fog artifacts
    tex = mix(vec4(in_colour.rgb, 0.0), in_colour, opacity);
  }
  else if(pc.strength < 0) tex = in_colour;
  else tex = in_colour * tex;

  out_colour = tex;
}
