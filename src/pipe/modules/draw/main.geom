#version 450
layout(lines) in;
layout(triangle_strip, max_vertices = 8) out;

layout(location = 0) in vec2 tex_coord_vs[2];
layout(location = 0) out vec2 tex_coord;

void main(void)
{
  vec2 p[2];
  p[0] = vec2(gl_in[0].gl_Position);
  p[1] = vec2(gl_in[1].gl_Position);

  const float thickness = 0.05;
  vec2 t = normalize(p[1] - p[0]);
  t *= thickness;
  vec2 e = vec2(t.y, -t.x);

  gl_Position = vec4(p[0] - e - t, 0, 1);
  tex_coord = vec2(0, 0);
  EmitVertex();
  gl_Position = vec4(p[0] + e - t, 0, 1);
  tex_coord = vec2(1, 0);
  EmitVertex();
  gl_Position = vec4(p[0] - e, 0, 1);
  tex_coord = vec2(0, 1);
  EmitVertex();
  gl_Position = vec4(p[0] + e, 0, 1);
  tex_coord = vec2(1, 1);
  EmitVertex();
  gl_Position = vec4(p[1] - e, 0, 1);
  tex_coord = vec2(0, 1);
  EmitVertex();
  gl_Position = vec4(p[1] + e, 0, 1);
  tex_coord = vec2(1, 1);
  EmitVertex();
  gl_Position = vec4(p[1] - e + t, 0, 1);
  tex_coord = vec2(0, 0);
  EmitVertex();
  gl_Position = vec4(p[1] + e + t, 0, 1);
  tex_coord = vec2(1, 0);
  EmitVertex();

  EndPrimitive();
}
