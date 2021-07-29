float noise_sigma(
    float noise_a, // gaussian part of the noise
    float noise_b, // poissonian part of the noise
    float val)     // pixel value, in the raw 16-bit range of 0..65535
{
  return 5.0 * sqrt(noise_a + val*noise_b);
}
