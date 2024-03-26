#version 460
layout(binding = 0, set = 1) uniform sampler2D img;
layout(location = 0) in  vec4 in_colour;
layout(location = 1) in  vec2 in_uv;
layout(location = 0) out vec4 out_colour;

void main()
{
  vec4 tex = texture(img, in_uv);
  // font sdf experiment. for images with a=1 it does nothing.
  const float smoothing = 0.5;
  tex.a = smoothstep(0.5 - smoothing, 0.5 + smoothing, tex.a);
  // XXX TODO: insert display colour management here (matrix multiply and gamma or srgb TRC)
  out_colour = pow(in_colour * tex , vec4(vec3(1.0/2.2), 1));
}
