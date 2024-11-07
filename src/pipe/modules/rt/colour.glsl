// Simple Analytic Approximations to the CIE XYZ Color Matching Functions
// Chris Wyman, Peter-Pike Sloan, Peter Shirley
// thanks guys :)
float xFit_1931( float wave )
{
  float t1 = (wave-442.0)*((wave<442.0)?0.0624:0.0374);
  float t2 = (wave-599.8)*((wave<599.8)?0.0264:0.0323);
  float t3 = (wave-501.1)*((wave<501.1)?0.0490:0.0382);
  return 0.362*exp(-0.5*t1*t1) + 1.056*exp(-0.5*t2*t2)
    - 0.065*exp(-0.5*t3*t3);
}
float yFit_1931( float wave )
{
  float t1 = (wave-568.8)*((wave<568.8)?0.0213:0.0247);
  float t2 = (wave-530.9)*((wave<530.9)?0.0613:0.0322);
  return 0.821*exp(-0.5*t1*t1) + 0.286*exp(-0.5*t2*t2);
}
float zFit_1931( float wave )
{
  float t1 = (wave-437.0)*((wave<437.0)?0.0845:0.0278);
  float t2 = (wave-459.0)*((wave<459.0)?0.0385:0.0725);
  return 1.217*exp(-0.5*t1*t1) + 0.681*exp(-0.5*t2*t2);
}

vec3 colour_to_rgb(
    vec4 X,       // value of the estimator f(lambda_i) / p(lambda_0)
    vec4 lambda,  // wavelengths, lambda.x is the hero wavelength
    vec4 herop)   // factors in the pdf of the current sample if sampled by lambda_i that actually depend on lambda.
{                 // that is, for a path x it is p(x, lambda_i) divided by the common parts shared by all lambda_i
  float sumhp = herop.x+herop.y+herop.z+herop.w;
  X *= herop / sumhp; // balance heuristic
  const mat3 xyz_to_rec2020 = mat3(
      1.7166511880, -0.6666843518, 0.0176398574,
     -0.3556707838, 1.6164812366, -0.0427706133,
     -0.2533662814, 0.0157685458, 0.9421031212);

  vec3 xyz = vec3(0.0);
  xyz += vec3(xFit_1931(lambda.x), yFit_1931(lambda.x), zFit_1931(lambda.x)) * X.x;
  xyz += vec3(xFit_1931(lambda.y), yFit_1931(lambda.y), zFit_1931(lambda.y)) * X.y;
  xyz += vec3(xFit_1931(lambda.z), yFit_1931(lambda.z), zFit_1931(lambda.z)) * X.z;
  xyz += vec3(xFit_1931(lambda.w), yFit_1931(lambda.w), zFit_1931(lambda.w)) * X.w;
  return xyz_to_rec2020 * xyz;
}

vec4 colour_sample_lambda(vec4 xi, inout vec4 X, inout vec4 hrp)
{
  const float a = sin((380.0-550.0)/400.0*M_PI);
  const float b = sin((720.0-550.0)/400.0*M_PI)-a;
#if 0 // some cosine centered at 550 nm:
  // pdf really b*M_PI/400.0 but the range is odd and we leave it out (we can cancel common terms)
  vec4 x = asin(xi*b + a)/M_PI * 400.0 + 550.0; 
  X /= cos((x-550.0)/400.0 * M_PI);
  return x;
#elif 1 // same cosine but sampling only the hero wavelength + stratified
  vec4 x;
  x.x = asin(xi.x*b + a)/M_PI * 400.0 + 550.0; 
  x.yzw = mod(x.x - 380.0 + vec3(85, 170, 255), 720.0-380.0) + 380.0;
  hrp = cos((x-550.0)/400.0 * M_PI);
  X /= hrp;
  return x;
#else
  // uniform:
  // hrp *= vec4(1.0);
  return vec4(380.0) + vec4(340.0) * xi;
#endif
}

vec4 colour_blackbody(
    vec4  l,  // wavelengths in [nm]
    float T)  // temperature in [K]
{
  const float h2 = 6.62606957e+11;// Planck's constant [J s] (adjusted for 10-9^5 because lambda is not in m but in nm)
  const float h = 6.62606957e-34; // Planck's constant [J s]
  const float c = 299792458.0;    // speed of light [m/s]
  const float k = 1.3807e-23;     // Boltzmann's constant [J/K]
  const vec4 lambda_m = l*1e-9;   // lambda [m]
  const vec4 lambda2 = l*l;
  const vec4 lambda5 = lambda2*l*lambda2;
  const vec4 c1 = 2. * h2 * c * c / lambda5;
  const vec4 c2 = h * c / (lambda_m * T * k);
  // convert to spectral radiance in [W/m^2 / sr / nm]
  return 1e-14 * c1 / (exp(c2)-1.0); // chosen to about match return vec4(1) for 6500K in brightness
  // return 2.21566e-16 * c1 / (exp(c2)-1.0); // such that it integrates y to 1.0 (for other cie cmf)
}

// return a spectral gaussian lobe around u with std dev s
vec4 colour_lobe(vec4 l, float u, float s)
{
  return exp(-0.5*(l-u)*(l-u)/(s*s));
}
