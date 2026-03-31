#version 450 core
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_nonuniform_qualifier    : enable
#include "shared.glsl"

layout(location = 0) out vec4 tex_coord;

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

layout(push_constant, std140) uniform push_t
{
  float aspect;
  uint  wd;
} push;

void decode_vert(
    out float radius,
    out float opacity,
    out float hardness,
    out vec2 p,
    uint v)
{
  radius = 2.0 * unpackUnorm2x16(ssbo.v[2*v+1]).x;
  vec2 u = unpackUnorm4x8(ssbo.v[2*v+1]).zw;
  opacity  = u.x;
  hardness = u.y;
  p = unpackUnorm2x16(ssbo.v[2*v]) * 4.0 - 2.0;
}

void main()
{
  float r0, o0, h0, r1, o1, h1;
  vec2 p0, p1;
  // we're drawing 6 triangles = 18 vertices per line segment
  uint v = gl_VertexIndex / 18;
  uint i = gl_VertexIndex - 18*v;
  decode_vert(r0, o0, h0, p0, v);
  decode_vert(r1, o1, h1, p1, v+1);

  vec2 dir = p1 - p0; // also discard "outlier" style directions often encountered at the start of a stroke
  if(r0 == 0 || r1 == 0 || length(dir)*push.wd <= 2)
  {
    tex_coord = vec4(0);
    gl_Position = vec4(-666,-666, 0, 1.0); // emit degenerate triangles
    return; // zero radius indicates end of stroke
  }

  vec2 t0 = normalize(p1 - p0);
  vec2 t1 = t0;
  t0 *= params.radius * r0;
  t1 *= params.radius * r1;
  // account for viewport from -1 to 1, radius is relative to width (i.e. 0 to 1).
  t0 *= 2.0*vec2(1.0, push.aspect);
  t1 *= 2.0*vec2(1.0, push.aspect);
  vec2 e0 = vec2(t0.y, -t0.x);
  vec2 e1 = vec2(t1.y, -t1.x);
  // XXX DEBUG
  // t0 *= 4;
  // t1 *= 4;

  if(i == 0 || i == 3)
  {
    gl_Position = vec4(p0 - e0 - t0, 0, 1);
    tex_coord = vec4(0, 0, o0, h0);
  }
  else if(i == 1)
  {
    gl_Position = vec4(p0 + e0 - t0, 0, 1);
    tex_coord = vec4(1, 0, o0, h0);
  }
  else if(i == 2 || i == 4 || i == 7)
  {
    gl_Position = vec4(p0 + e0, 0, 1);
    tex_coord = vec4(1, 1, o0, h0);
  }
  else if(i == 5 || i == 6 || i == 9)
  {
    gl_Position = vec4(p0 - e0, 0, 1);
    tex_coord = vec4(0, 1, o0, h0);
  }
  else if(i == 11 || i == 12 || i  == 15)
  {
    gl_Position = vec4(p1 - e1, 0, 1);
    tex_coord = vec4(0, 1, o1, h1);
  }
  else if(i == 10 || i == 8 || i == 13)
  {
    gl_Position = vec4(p1 + e1, 0, 1);
    tex_coord = vec4(1, 1, o1, h1);
  }
  else if(i == 17)
  {
    gl_Position = vec4(p1 - e1 + t1, 0, 1);
    tex_coord = vec4(0, 0, o1, h1);
  }
  else if(i == 14 || i == 16)
  {
    gl_Position = vec4(p1 + e1 + t1, 0, 1);
    tex_coord = vec4(1, 0, o1, h1);
  }
}
