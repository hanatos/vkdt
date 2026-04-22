float noise_sigma(
    float noise_a, // gaussian part of the noise
    float noise_b, // poissonian part of the noise
    float black,
    float white,
    float edges,
    float val)     // pixel value in [0, 1]
{
  return (1.0/edges)*sqrt(noise_a + max(0, (val-black)/(white-black))*noise_b);
}
