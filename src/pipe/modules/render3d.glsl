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

// evaluate *without* albedo, that has to multiplied in the end
float bsdf_diffuse_eval(vec3 wi, vec3 n, vec3 wo)
{
  return 1.0/M_PI;
}

vec3 bsdf_sample(vec3 wi, vec3 du, vec3 dv, vec3 n, vec2 xi)
{
  return bsdf_diffuse_sample(wi, du, dv, n, xi);
}
float bsdf_pdf(vec3 wi, vec3 n, vec3 wo)
{
  return bsdf_diffuse_pdf(wi, n, wo);
}
float bsdf_eval(vec3 wi, vec3 n, vec3 wo)
{
  return bsdf_diffuse_eval(wi, n, wo);
}
