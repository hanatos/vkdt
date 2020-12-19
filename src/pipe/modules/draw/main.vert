#version 450 core
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_nonuniform_qualifier    : enable
#include "shared.glsl"

layout(location = 0) out vs_t
{
  float opacity;
  float radius;
  float hardness;
} vs;

layout(std140, set = 0, binding = 1) uniform params_t
{
  float opacity;
  float radius;
  float hardness;
} params;

layout(std430, set = 1, binding = 0) buffer ssbo_t
{
  uint v[]; // variable-length list of bytes encoding position, radius, opacity, and hardness
} ssbo;

void main()
{
  // or use vec2 p = unpackHalf2x16 to support out of image viewing frustum?
  vs.radius = 2.0 * unpackUnorm2x16(ssbo.v[2*gl_VertexIndex+1]).x;
  vec2 u = unpackUnorm4x8(ssbo.v[2*gl_VertexIndex+1]).zw;
  vs.opacity  = u.x;
  vs.hardness = u.y;
  // grab vertex position from ssbo:
  vec2 p = unpackUnorm2x16(ssbo.v[2*gl_VertexIndex]) * 2.0 - 1.0;
  gl_Position = vec4(p, 0, 1.0);
}
