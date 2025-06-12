// stupid dielectric. monochromatic and specular.

float dielectric_fresnel(float n1, float n2, float cosr, float cost)
{
  float r1 = n1*cosr, r2 = n2*cosr;
  float t1 = n1*cost, t2 = n2*cost;
  float Rs = (r1-t2)/(r1+t2);
  float Rp = (t1-r2)/(t1+r2);
  return mix(clamp((Rs*Rs+Rp*Rp)*0.5, 0.0, 1.0), 1.0, cost<0.0);
}

vec4 // return evaluation of f_r for the given wavelengths
dielectric_eval(
    mat_state_t mat,
    uint  flags,  // s_volume, ..
    vec3  w,      // observer direction, pointing towards surf
    vec3  n,      // shading normal
    vec3  wo,     // light direction, pointing away from surf
    vec4  lambda) // hero wavelengths
{
  return vec4(0.0);
}

vec3 // return outgoing direction wo, sampled according to f_r * cos
dielectric_sample(
    mat_state_t mat,
    inout uint flags,  // s_volume, ..
    vec3       w,      // incident direction pointing towards intersection point
    vec3       n,      // shading normal
    vec4       lambda, // wavelengths
    vec3       xi,     // random numbers
    out vec4   c)      // value of monte carlo estimator f_r*cos/pdf for the four wavelengths
{
  const float eta_ratio = ((mat.geo_flags & s_geo_inside)>0)? 1.3 : 1.0/1.3;

  float cosr = -dot(w, n);
  float cost2 = 1.0 - eta_ratio*eta_ratio * (1.0-cosr*cosr);
  float cost = mix(sqrt(cost2), 0.0, cost2 < 0.0);
  float R = dielectric_fresnel(eta_ratio, 1.0, -dot(w, n), cost);

  vec3 wo;
  c = mat.col_base;
  if(xi.x < R)
  { // reflect
    wo = reflect(w, n);
    flags = s_reflect;
  }
  else
  { // transmit
    wo = refract(w, n, eta_ratio);
    flags = s_transmit;
  }
  return wo;
}

float // return solid angle pdf
dielectric_pdf(
    mat_state_t mat,
    uint flags,   // s_volume, ..
    vec3 w,       // incident
    vec3 n,       // shading normal
    vec3 wo,      // outgoing
    float lambda) // the hero wavelength
{
  return 0.0;
}
