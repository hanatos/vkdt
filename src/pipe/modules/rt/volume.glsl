// ======================================================================
// volume sampling
// ======================================================================

float volume_sample_dist(vec3 x, vec3 w, float xi)
{
  return -log(1.0-xi)/volume.mu_t;
}

float volume_transmittance(vec3 x0, vec3 x1)
{
  return exp(-volume.mu_t*length(x1-x0));
}

float _sample_draine_cos(in float g, in float a, in float xi)
{
  const float g2 = g * g;
  const float g3 = g * g2;
  const float g4 = g2 * g2;
  const float g6 = g2 * g4;
  const float pgp1_2 = (1 + g2) * (1 + g2);
  const float T1 = (-1 + g2) * (4 * g2 + a * pgp1_2);
  const float T1a = -a + a * g4;
  const float T1a3 = T1a * T1a * T1a;
  const float T2 = -1296 * (-1 + g2) * (a - a * g2) * (T1a) * (4 * g2 + a * pgp1_2);
  const float T3 = 3 * g2 * (1 + g * (-1 + 2 * xi)) + a * (2 + g2 + g3 * (1 + 2 * g2) * (-1 + 2 * xi));
  const float T4a = 432 * T1a3 + T2 + 432 * (a - a * g2) * T3 * T3;
  const float T4b = -144 * a * g2 + 288 * a * g4 - 144 * a * g6;
  const float T4b3 = T4b * T4b * T4b;
  const float T4 = T4a + sqrt(-4 * T4b3 + T4a * T4a);
  const float T4p3 = pow(T4, 1.0 / 3.0);
  const float T6 = (2 * T1a + (48 * pow(2, 1.0 / 3.0) *
    (-(a * g2) + 2 * a * g4 - a * g6)) / T4p3 + T4p3 / (3. * pow(2, 1.0 / 3.0))) / (a - a * g2);
  const float T5 = 6 * (1 + g2) + T6;
  return (1 + g2 - pow(-0.5 * sqrt(T5) + sqrt(6 * (1 + g2) - (8 * T3) / (a * (-1 + g2) * sqrt(T5)) - T6) / 2., 2)) / (2. * g);
}

float _eval_draine(in float g, in float a, in float u)
{ // draine phase function
  return ((1 - g*g)*(1 + a*u*u))/(4.*(1 + (a*(1 + 2*g*g))/3.) * M_PI * pow(1 + g*g - 2*g*u,1.5));
}

float _eval_hg(in float g, in float u)
{ // henyey greenstein phase function
  return (1.0 - g*g) / (4.0*M_PI*pow(1.0 + g*g - 2.0*g*u, 1.5));
}

float _sample_hg_cos(in float g, in float xi)
{
  const float sqr = (1.0-g*g)/(1.0+g*(2.0*xi-1.0));
  return 1.0/(2.0*g) * (1.0 + g*g - sqr*sqr);
}

// jendersie and d'eon 2023
float _eval_mie_fit(
    float d, // particle diameter
    float u) // cosine theta
{
  float g_hg = exp(-0.0990567/(d-1.67154));
  float g_d  = exp(-2.20679/(d+3.91029) - 0.428934);
  float a    = exp(3.62489 - 8.29288/(d+5.52825));
  float w_d  = exp(-0.599085/(d-0.641583) - 0.665888);
  return (1.0-w_d) * _eval_hg(g_hg, u) + w_d * _eval_draine(g_d, a, u);
}

float _sample_mie_fit_cos(
    vec2 xi, float d)
{ // return cosu
  float g_hg = exp(-0.0990567/(d-1.67154));
  float g_d  = exp(-2.20679/(d+3.91029) - 0.428934);
  float a    = exp(3.62489 - 8.29288/(d+5.52825));
  float w_d  = exp(-0.599085/(d-0.641583) - 0.665888);
  if(xi.x < w_d) return _sample_draine_cos(g_d,  a, xi.y);
  return                _sample_hg_cos    (g_hg, xi.y);
}

float volume_phase_function(float cosu)
{
  return _eval_mie_fit(volume.d, cosu);
}

float volume_sample_phase_function_cos(vec2 xi)
{
  return _sample_mie_fit_cos(xi, volume.d);
}
