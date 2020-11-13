#version 450 core
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_nonuniform_qualifier    : enable
#include "shared.glsl"

layout(location = 0) out vec2 tex_coord_vs;

layout(std140, set = 0, binding = 1) uniform params_t
{
  float opacity;
  float radius;
} params;

layout(std430, set = 1, binding = 0) buffer ssbo_t
{
  vec2 v[]; // variable-length list of 2d vertices
} ssbo;

void main()
{
  // grab vertex position from ssbo:
  vec2 p = ssbo.v[gl_VertexIndex]*2.0 - 1.0;
  // we probably want to pack it into one uint per vertex and use
  // vec2 p = unpackUnorm2x16(v[]); // to save a fair amount of memory (to be used for pressure sensitivity..)
  // if doing that, pay attention that our magic end marker (-666.0) is out of range, need a different one (zero radius?)
  gl_Position = vec4(p, 0, 1.0);
}
