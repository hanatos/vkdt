#version 450

layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec4 mask;

void main()
{
  vec2 x = vec2(2.0*abs(tex_coord.x-.5), 1.0-tex_coord.y);
  float sigma2 = 2.*2.;
  float zero = exp(-sigma2);

	mask = vec4(
      clamp((exp(-dot(x,x)*sigma2)-zero)/(1.0-zero), 0.0, 1.0));
      // smoothstep(1.0, 0.0, length(x)));
}
