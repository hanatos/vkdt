#version 450
layout(lines) in;
layout(triangle_strip, max_vertices = 8) out;

layout(location = 0) in vs_t
{
  float opacity;
  float radius;
  float hardness;
} vs[];

layout(location = 0) out vec4 tex_coord;

layout(std140, set = 0, binding = 1) uniform params_t
{
  float opacity;
  float radius;
  float hardness;
} params;

layout(push_constant, std140) uniform push_t
{
  float aspect;
} push;

void main(void)
{
  vec2 p[2];
  p[0] = vec2(gl_in[0].gl_Position);
  p[1] = vec2(gl_in[1].gl_Position);

  if(vs[0].radius == 0 || vs[1].radius == 0) return; // zero radius indicates end of stroke

  vec2 vs0 = vec2(vs[0].opacity, vs[0].hardness);
  vec2 vs1 = vec2(vs[1].opacity, vs[1].hardness);
  vec2 t0 = normalize(p[1] - p[0]);
  t0 *= params.radius * vs[0].radius;
  vec2 e0 = vec2(t0.y, -t0.x);
  vec2 t1 = normalize(p[1] - p[0]);
  t1 *= params.radius * vs[1].radius;
  vec2 e1 = vec2(t1.y, -t1.x);
  t0 *= vec2(push.aspect, 1.0);
  t1 *= vec2(push.aspect, 1.0);
  e0 *= vec2(push.aspect, 1.0);
  e1 *= vec2(push.aspect, 1.0);

  gl_Position = vec4(p[0] - e0 - t0, 0, 1);
  tex_coord = vec4(0, 0, vs0);
  EmitVertex();
  gl_Position = vec4(p[0] + e0 - t0, 0, 1);
  tex_coord = vec4(1, 0, vs0);
  EmitVertex();
  gl_Position = vec4(p[0] - e0, 0, 1);
  tex_coord = vec4(0, 1, vs0);
  EmitVertex();
  gl_Position = vec4(p[0] + e0, 0, 1);
  tex_coord = vec4(1, 1, vs0);
  EmitVertex();
  gl_Position = vec4(p[1] - e1, 0, 1);
  tex_coord = vec4(0, 1, vs1);
  EmitVertex();
  gl_Position = vec4(p[1] + e1, 0, 1);
  tex_coord = vec4(1, 1, vs1);
  EmitVertex();
  gl_Position = vec4(p[1] - e1 + t1, 0, 1);
  tex_coord = vec4(0, 0, vs1);
  EmitVertex();
  gl_Position = vec4(p[1] + e1 + t1, 0, 1);
  tex_coord = vec4(1, 0, vs1);
  EmitVertex();

  EndPrimitive();
}
