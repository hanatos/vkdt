#version 450

layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec4 mask;

layout(std140, set = 0, binding = 1) uniform params_t
{
  float opacity;
  float radius;
} params;

void main()
{
  vec2 x = vec2(2.0*abs(tex_coord.x-.5), 1.0-tex_coord.y);
  mask = vec4(params.opacity * smoothstep(1.0, 0.4, length(x)));
}
