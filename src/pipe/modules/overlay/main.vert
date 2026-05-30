#version 460
#extension GL_EXT_shader_16bit_storage             : enable
#extension GL_EXT_shader_8bit_storage              : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_GOOGLE_include_directive             : enable
#include "shared.glsl"
struct vtx_t
{
  f16vec2 p;
  f16vec2 uv;
  u8vec4  col;
};
// BUFFER_REF_DECLARE_ARRAY(bref_vtx_t, vtx_t, readonly, 8)
layout(std430, buffer_reference, buffer_reference_align = 8) readonly buffer bref_vtx_t
{
  vtx_t v[];
};
layout(set = 0, binding = 2) uniform bref_t
{
  bref_vtx_t vtx;
} bref;
// layout(set = 1, binding = 0) buffer ssbo_t { vtx_t v[]; } ssbo;
layout(set = 1, binding = 1) uniform sampler2D img;

layout(location = 0) out vec4 frag_col;
layout(location = 1) out vec2 frag_uv;

void main()
{
  vtx_t v = bref.vtx.v[gl_VertexIndex];
  frag_uv = vec2(v.uv);
  frag_col = vec4(v.col/255.0);
  gl_Position = vec4(v.p, 0, 1);
}
