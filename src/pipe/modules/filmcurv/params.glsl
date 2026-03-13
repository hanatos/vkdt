layout(std140, set = 0, binding = 1) uniform params_t
{
  float brightness;
  float contrast;
  float bias;
  int   colourmode;
  float chroma;
  float rolloff;
  float red;
  float yellow;
  float blue;
  float shadows;
} params;

float
weibull_cdf(
    float x,  // input value (0, infty)
    float il, // weibull 1.0/lambda, scale parameter (0, infty)
    float k)  // weibull k, shape parameter          (0, infty)
{
  return 1.0 - exp(-pow(max(x, 1e-7)*il, k));
}

float // derivative of cdf:
weibull_pdf(float x, float il, float k)
{
  x = max(x, 1e-7);
  return k*il * pow(x*il, k-1.0) * exp(-pow(x*il, k));
}
vec3 // vector version
weibull_cdf(vec3 x, float il, float k)
{
  return 1.0 - exp(-pow(max(x, 1e-7)*il, vec3(k)));
}
