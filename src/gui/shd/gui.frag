#version 460
layout(binding = 0, set = 1) uniform sampler2D img;
layout(location = 0) in  vec4 in_colour;
layout(location = 1) in  vec2 in_uv;
layout(location = 0) out vec4 out_colour;
layout(push_constant) uniform push_t
{
  float gamma0x, gamma0y, gamma0z;
  float gamma1x, gamma1y, gamma1z;
  float d0a, d0b, d0c, d0d, d0e, d0f, d0g, d0h, d0i; // rec2020 to dspy0
  float d1a, d1b, d1c, d1d, d1e, d1f, d1g, d1h, d1i; // rec2020 to dspy1
  float maxval;
  int winposx;
  int borderx;
  float strength; // font strength
} pc;

float rand(inout uint x)
{ // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x / 4294967296.0;
}

float median(float r, float g, float b)
{
  return max(min(r, g), min(max(r, g), b));
}

void main()
{
  vec4 tex = texture(img, in_uv);
  if(pc.strength > 0)
  { // render msdf font
    float dist = median(tex.r, tex.g, tex.b) - 1.0 + pc.strength;
    //
    // dist = dist / fwidth(dist);
    // float opacity = clamp(dist + 0.5, 0.0, 1.0);
    //
    // float dx = dFdx(dist);
    // float dy = dFdy(dist);
    // float g = length(vec2(dx,dy))*0.70710678118; // sqrt(2)/2
    // float opacity = smoothstep(-g, g, dist);
    //
    // minification has some overblur issues, clamp that early, erring on the aliasing side
    dist *= clamp(dot(vec2(1.0/textureSize(img, 0)), 1.0/fwidth(in_uv)), 1.0, 1000.0);
    // float opacity = clamp(dist + 0.5, 0.0, 1.0);
    float opacity = smoothstep(0.0, 1.0, dist + 0.5); // we're blending post gamma, this looks better
    if(opacity < 1.0/256.0) opacity = 0.0; // pull down to straight zero to avoid stupid rounding/fog artifacts
    tex = mix(vec4(in_colour.rgb, 0.0), in_colour, opacity);
  }
  else if(pc.strength < 0) tex = in_colour;
  else tex = in_colour * tex;

  // convert linear rec2020 to profile
  mat3 rec2020_to_dspy;
  vec3 gamma;
  if(gl_FragCoord.x + pc.winposx < pc.borderx)
  {
    rec2020_to_dspy = mat3(
        pc.d0a, pc.d0d, pc.d0g,
        pc.d0b, pc.d0e, pc.d0h,
        pc.d0c, pc.d0f, pc.d0i);
    gamma = vec3(pc.gamma0x, pc.gamma0y, pc.gamma0z);
  }
  else
  {
    rec2020_to_dspy = mat3(
        pc.d1a, pc.d1d, pc.d1g,
        pc.d1b, pc.d1e, pc.d1h,
        pc.d1c, pc.d1f, pc.d1i);
    gamma = vec3(pc.gamma1x, pc.gamma1y, pc.gamma1z);
  }
  tex.rgb = rec2020_to_dspy * tex.rgb;

  if(gamma.x > 0)
    tex.rgb = pow(tex.rgb, vec3(gamma));
  else if(gamma.x == 0) // sRGB TRC
    tex.rgb = mix(tex.rgb * 12.92, pow(tex.rgb, vec3(1.0/2.4))*(1+0.055)-0.055, lessThanEqual(tex.rgb, vec3(0.0031308)));
  else if(gamma.x == -1.0)
  { // HLG for hdr output
    // const float a = 0.17883277, b = 1.0-4.0*a, c = 0.5 - a*log(4.0*a);
    // tex.rgb = mix(sqrt(3.0*tex.rgb), a*log(12.0*tex.rgb-b) + c, greaterThan(tex.rgb, vec3(1.0/12.0)));
    // PQ oetf
    const float c3 = 2392.0/128.0;
    const float c2 = 2413.0/128.0;
    const float c1 = c3-c2+1.0;
    const float m1 = 1305.0/8192.0;
    const float m2 = 2523.0/32.0;
    // zimg says:
    // More stable arrangement that avoids some cancellation error.
    const float inv_avg_nits = 1.0/70.0;
    tex.rgb = max(vec3(0.0), inv_avg_nits*tex.rgb);
    tex.rgb = pow(tex.rgb, vec3(m1));
    vec3 num = (c1 - 1.0) + (c2 - c3) * tex.rgb;
    vec3 den = 1.0 + c3 * tex.rgb;
    tex.rgb = pow(1.0 + num / den, vec3(m2));
  }

  // dithering:
  uint state = 1337*666*uint(1 + dot(uvec2(gl_FragCoord.xy), uvec2(1, 13)));
  vec3 rnd;
  rnd.x = rand(state); rnd.y = rand(state); rnd.z = rand(state);
  vec3 quant = floor(tex.rgb * pc.maxval + rnd);
  out_colour = vec4(quant / pc.maxval, tex.a);
}
