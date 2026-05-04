vec3 noise_sigma(
    float noise_a, // gaussian part of the noise
    float noise_b, // poissonian part of the noise
    float black,
    float white,
    vec4  edges,
    float val)     // pixel value in [0, 1]
{
  return clamp(exp2(12.0*edges.rgb+edges.a)*sqrt(noise_a + max(0, (val-black)/(white-black))*noise_b),
      vec3(1e-3), vec3(1e3));
}
