#version 450

layout(location = 0) in vec4 tex_coord;
layout(location = 0) out vec4 mask;

layout(std140, set = 0, binding = 1) uniform params_t
{
  float opacity;
  float radius;
  float hardness;
} params;

void main()
{
  vec2 x = vec2(2.0*abs(tex_coord.x-.5), 1.0-tex_coord.y);
  float opacity  = params.opacity  * tex_coord.z;
  float hardness = params.hardness * tex_coord.w;
  mask = vec4(opacity * smoothstep(1.0, hardness-1e-6, length(x)));
}
