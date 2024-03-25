#version 460
layout(binding = 0, set = 1) uniform sampler2D currentTexture;
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main()
{
  vec4 texColor = texture(currentTexture, fragUv);
  outColor = pow(fragColor * texColor, vec4(1.0/2.2));
}
