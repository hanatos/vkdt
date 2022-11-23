// shared functions for 3d rendering

#define M_PI   3.14159265358979323846

// 32-bit normal encoding from Journal of Computer Graphics Techniques Vol. 3, No. 2, 2014
// A Survey of Efficient Representations for Independent Unit Vectors,
// almost like oct30
vec2 sign_no0(vec2 v)
{
  return mix(vec2(1.0), vec2(-1.0), lessThan(v, vec2(0.0)));
}
vec3 geo_decode_normal(const uint enc)
{
  vec2 projected = unpackSnorm2x16(enc); // -1..1
  vec3 vec = vec3(projected, 1.0-abs(projected.x)-abs(projected.y));
  if(vec.z < 0.0) vec.xy = vec2(1.0-abs(vec.yx)) * sign_no0(vec.xy);
  return normalize(vec);
}
uint geo_encode_normal(vec3 n)
{
  const float invL1Norm = 1.0 / (abs(n.x) + abs(n.y) + abs(n.z));
  vec2 enc; // find floating point values of octahedral map in [-1,1]:
  if(n.z < 0.0) enc = (1.0-abs(n.yx * invL1Norm)) * sign_no0(n.xy);
  else          enc = n.xy * invL1Norm;
  return packSnorm2x16(enc);
}

// sample hemisphere, cos lobe, p = cos(theta)/pi
vec3 sample_cos(vec2 x)
{
  float su = sqrt(x.x);
  return vec3(su*cos(2.0*3.1415*x.y), su*sin(2.0*3.1415*x.y), sqrt(1.0 - x.x));
}

vec3 bsdf_diffuse_sample(vec3 wi, vec3 du, vec3 dv, vec3 n, vec2 xi)
{
  return mat3(du, dv, n) * sample_cos(xi);
}

float bsdf_diffuse_pdf(vec3 wi, vec3 n, vec3 wo)
{
  return 1.0/M_PI;
}

float bsdf_diffuse_eval()
{
  return 1.0/M_PI;
}

vec3 bsdf_rough_sample(vec3 wi, vec3 du, vec3 dv, vec3 n, vec2 xi)
{
  // TODO:
  return vec3(0);
}
//==================================================
// microfacet model by jonathan:
float erfc(float x) {
  return 2.0 * exp(-x * x) / (2.319 * x + sqrt(4.0 + 1.52 * x * x));
}
float erf(float x) {
  float a  = 0.140012;
  float x2 = x*x;
  float ax2 = a*x2;
  return sign(x) * sqrt( 1.0 - exp(-x2*(4.0/M_PI + ax2)/(1.0 + ax2)) );
}
float Lambda(float cosTheta, float sigmaSq) {
  float v = cosTheta / sqrt((1.0 - cosTheta * cosTheta) * (2.0 * sigmaSq));
  return max(0.0, (exp(-v * v) - v * sqrt(M_PI) * erfc(v)) / (2.0 * v * sqrt(M_PI)));
  //return (exp(-v * v)) / (2.0 * v * sqrt(M_PI)); // approximate, faster formula
}
// L, V, N, Tx, Ty in world space
float bsdf_rough_eval(
    vec3 V, vec3 Tx, vec3 Ty, vec3 N, vec3 L, vec2 sigmaSq)
{
  V = -V; // all pointing away from surface intersection point
  vec3 H = normalize(L + V);
  float zetax = dot(H, Tx) / dot(H, N);
  float zetay = dot(H, Ty) / dot(H, N);

  float zL = dot(L, N); // cos of source zenith angle
  float zV = dot(V, N); // cos of receiver zenith angle
  float zH = dot(H, N); // cos of facet normal zenith angle
  if(zL < 0 || zV < 0 || zH < 0) return 0.0;
  float zH2 = zH * zH;

  float p = exp(-0.5 * (zetax * zetax / sigmaSq.x + zetay * zetay / sigmaSq.y))
    / (2.0 * M_PI * sqrt(sigmaSq.x * sigmaSq.y));

  float tanV = atan(dot(V, Ty), dot(V, Tx));
  float cosV2 = 1.0 / (1.0 + tanV * tanV);
  float sigmaV2 = sigmaSq.x * cosV2 + sigmaSq.y * (1.0 - cosV2);

  float tanL = atan(dot(L, Ty), dot(L, Tx));
  float cosL2 = 1.0 / (1.0 + tanL * tanL);
  float sigmaL2 = sigmaSq.x * cosL2 + sigmaSq.y * (1.0 - cosL2);

  float fresnel = 0.02 + 0.98 * pow(1.0 - dot(V, H), 5.0);

  zL = max(zL, 0.01);
  zV = max(zV, 0.01);

  return mix(1.0/M_PI, p / ((1.0 + Lambda(zL, sigmaL2) + Lambda(zV, sigmaV2)) * zV * zH2 * zH2 * 4.0), fresnel);
}
//==================================================


// multiplexing:
vec3 bsdf_sample(uint m, vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 param, vec2 xi)
{
  return bsdf_diffuse_sample(wi, du, dv, n, xi);
}
float bsdf_pdf(uint m, vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 wo, vec3 param)
{
  return bsdf_diffuse_pdf(wi, n, wo);
}
float bsdf_eval(uint m, vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 wo, vec3 param)
{ // evaluate *without* albedo, that has to multiplied in the end
  if(m == 1) return bsdf_rough_eval(wi, du, dv, n, wo, param.xy);
  return bsdf_diffuse_eval();
}
