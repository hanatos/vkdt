
vec4 // return four fresnel evaluations for four wavelengths lambda
fresnel(
    int mat,     // select material, 0 is gold
    vec4 lambda, // four wavelengths to evaluate
    float n1,    // the ior of the medium we're currently in
    float cosr)  // the cosine of the incoming/mirror reflected ray
{
  // turingbot says this is a good fit in 380--720 (diverges a bit away around 800 nm)
  vec4 ior_Au = 1.16308-sin(1.14531*(0.457845+cos(0.00894816*(20.1947*cos(0.0334696*(24.8434+lambda))+lambda))));
  vec4 kappa_Au = 0.00738787*((2.33998-cos(0.0344368*(lambda-20.7609)))*(30.9339*cos(5.48433*log(lambda))-10.4363-1.58234*cos(lambda*(-7.3151e-05)*lambda))+lambda-124.02);

  vec4 ret;
  vec2 n2, ratio, cost;
  float Rs, Rp;
  n2 = vec2(ior_Au.x, -kappa_Au.x);
  ratio = complex_mul_mixed(n1, complex_rcp(n2));
  cost = complex_sqrt(vec2(1,0) - complex_mul_mixed(1.0 - cosr*cosr, complex_mul(ratio, ratio)));
  Rs = sqrt(complex_abs_sqr(complex_div(vec2(n1*cosr,0) - complex_mul(n2,cost), vec2(n1*cosr,0) + complex_mul(n2,cost))));
  Rp = sqrt(complex_abs_sqr(complex_div(complex_mul_mixed(n1,cost) - complex_mul_mixed(cosr,n2), complex_mul_mixed(n1,cost) + complex_mul_mixed(cosr,n2))));
  ret.x = min(1.0, (Rs*Rs + Rp*Rp)*.5);

  n2 = vec2(ior_Au.y, -kappa_Au.y);
  ratio = complex_mul_mixed(n1, complex_rcp(n2));
  cost = complex_sqrt(vec2(1,0) - complex_mul_mixed(1.0 - cosr*cosr, complex_mul(ratio, ratio)));
  Rs = sqrt(complex_abs_sqr(complex_div(vec2(n1*cosr,0) - complex_mul(n2,cost), vec2(n1*cosr,0) + complex_mul(n2,cost))));
  Rp = sqrt(complex_abs_sqr(complex_div(complex_mul_mixed(n1,cost) - complex_mul_mixed(cosr,n2), complex_mul_mixed(n1,cost) + complex_mul_mixed(cosr,n2))));
  ret.y = min(1.0, (Rs*Rs + Rp*Rp)*.5);

  n2 = vec2(ior_Au.z, -kappa_Au.z);
  ratio = complex_mul_mixed(n1, complex_rcp(n2));
  cost = complex_sqrt(vec2(1,0) - complex_mul_mixed(1.0 - cosr*cosr, complex_mul(ratio, ratio)));
  Rs = sqrt(complex_abs_sqr(complex_div(vec2(n1*cosr,0) - complex_mul(n2,cost), vec2(n1*cosr,0) + complex_mul(n2,cost))));
  Rp = sqrt(complex_abs_sqr(complex_div(complex_mul_mixed(n1,cost) - complex_mul_mixed(cosr,n2), complex_mul_mixed(n1,cost) + complex_mul_mixed(cosr,n2))));
  ret.z = min(1.0, (Rs*Rs + Rp*Rp)*.5);

  n2 = vec2(ior_Au.w, -kappa_Au.w);
  ratio = complex_mul_mixed(n1, complex_rcp(n2));
  cost = complex_sqrt(vec2(1,0) - complex_mul_mixed(1.0 - cosr*cosr, complex_mul(ratio, ratio)));
  Rs = sqrt(complex_abs_sqr(complex_div(vec2(n1*cosr,0) - complex_mul(n2,cost), vec2(n1*cosr,0) + complex_mul(n2,cost))));
  Rp = sqrt(complex_abs_sqr(complex_div(complex_mul_mixed(n1,cost) - complex_mul_mixed(cosr,n2), complex_mul_mixed(n1,cost) + complex_mul_mixed(cosr,n2))));
  ret.w = min(1.0, (Rs*Rs + Rp*Rp)*.5);

  return ret;
}
