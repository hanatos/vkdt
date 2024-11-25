// shared functions for 3d rendering

#define M_PI   3.14159265358979323846

struct rtgeo_vtx_t
{
  float x, y, z;
  uint tex;
  uint n;
  uint st;
};

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

// sample sphere, p = 1/4pi
vec3 sample_sphere(vec2 x)
{
  float z = 2.0*(x.x-0.5);
  float z2 = sqrt(1.0-z*z);
  return vec3(z2 * cos(2.0*M_PI*x.y), z2 * sin(2.0*M_PI*x.y), z);
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

// smith ggx shadowing/masking:
float bsdf_rough_G1(vec3 wi, vec3 wo, vec3 n, float a2)
{
  float dotNL = dot(-wi, n);
  float dotNV = dot( wo, n);
  float denomC = sqrt(a2 + (1.0 - a2) * dotNV * dotNV) + dotNV;

  return 2.0 * dotNV / denomC;
}

float bsdf_rough_G2(vec3 wi, vec3 wo, vec3 n, float a2)
{
  float dotNL = dot(-wi, n);
  float dotNV = dot( wo, n);
  float denomA = dotNV * sqrt(a2 + (1.0 - a2) * dotNL * dotNL);
  float denomB = dotNL * sqrt(a2 + (1.0 - a2) * dotNV * dotNV);
  return 2.0 * dotNL * dotNV / (denomA + denomB);
}

vec3 bsdf_rough_sample(vec3 wi, vec3 du, vec3 dv, vec3 n, vec2 alpha, vec2 xi, out float X)
{
#if 0 // Eric Heitz' vndf sampling for ggx:
  // vec3 sampleGGXVNDF(vec3 V_, float alpha_x, float alpha_y, float U1, float U2)
  vec3 V_ = -vec3(dot(wi, du), dot(wi, dv), dot(wi, n));
  // stretch view
  vec3 V = normalize(vec3(alpha.x * V_.x, alpha.y * V_.y, V_.z));
  // orthonormal basis around V
  vec3 T1 = (V.z < 0.9999) ? normalize(cross(V, vec3(0,0,1))) : vec3(1,0,0);
  vec3 T2 = cross(T1, V);
  // sample point with polar coordinates (r, phi)
  float a = 1.0 / (1.0 + V.z);
  float r = sqrt(xi.x);
  float phi = (xi.y<a) ? xi.y/a * M_PI : M_PI + (xi.y-a)/(1.0-a) * M_PI;
  float P1 = r*cos(phi);
  float P2 = r*sin(phi)*((xi.y<a) ? 1.0 : V.z);
  // compute micronormal
  vec3 h = P1*T1 + P2*T2 + sqrt(max(0.0, 1.0 - P1*P1 - P2*P2))*V;
  // unstretch
  h = normalize(vec3(alpha.x*h.x, alpha.y*h.y, max(0.0, h.z)));
  vec3 L = reflect(-V_, h);
  if(L.z <= 0.0)
  {
    X = 0.0;
    return vec3(0.0);
  }
  vec3 wo = du * L.x + dv * L.y + n * L.z;
  // compute value of estimator, we did not sample outgoing masking
  float a2 = alpha.x * alpha.x + alpha.y * alpha.y;
  X = bsdf_rough_G2(wi, wo, n, a2) / bsdf_rough_G1(wi, wo, n, a2);
  return wo;
#else // Bounded VNDF Sampling for the Smith-GGX BRDF, Tokuyoshi and Eto 2024
  vec3 i = -vec3(dot(wi,du),dot(wi,dv),dot(wi,n));
  vec3 i_std = normalize(vec3(i.xy * alpha, i.z));
  // Sample a spherical cap
  float phi = 2.0 * M_PI * xi.x;
  float a = clamp(min(alpha.x, alpha.y), 0.0, 1.0); // Eq. 7
  float s = 1.0 + length(i.xy); // Omit sgn for a <=1
  float a2 = a * a; float s2 = s * s;
  float k = (1.0 - a2) * s2 / (s2 + a2 * i.z * i.z); // Eq . 6
  float lower_bound = i.z > 0.0 ? -k * i_std.z : - i_std.z;
  float z = fma(lower_bound, xi.y, 1.0 - xi.y);
  float sin_theta = sqrt(clamp(1.0 - z * z, 0.0, 1.0));
  vec3 o_std = vec3(sin_theta * cos(phi), sin_theta * sin(phi), z);
  // Compute the microfacet normal m
  vec3 m_std = i_std + o_std;
  vec3 m = normalize(vec3(m_std.xy * alpha, m_std.z));
  // Return the reflection vector o
  vec3 o = 2.0 * dot(i, m) * m - i;
  float t = sqrt(dot(i_std.xy*alpha,i_std.xy*alpha) + i.z*i.z);
  vec3 wo = du * o.x + dv * o.y + n * o.z;
  // return f_r * cos / pdf, i.e. don't cancel the extra o.z
  X = bsdf_rough_G2(wi, wo, n, a2) / (4.0 * i.z) * (2.0 * (t + k * i.z));
  return wo;
#endif
}
// GGX normal distribution function
float bsdf_rough_D(float roughness, const vec3 n, const vec3 h)
{
  float cos2 = dot(n, h)*dot(n, h);  // cos2 theta
  float sin2 = max(0.0, 1.0-cos2);
  float a = sin2/cos2 + roughness*roughness;
  float k = roughness / (cos2 * a);
  return k * k / M_PI;
}

float bsdf_rough_eval(
    vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 wo, vec2 a)
{
  float roughness = a.x;
  vec3 h = normalize(wo-wi);
  float D  = bsdf_rough_D(roughness, n, h);
  float G2 = bsdf_rough_G2(wi, wo, n, roughness*roughness);
  return D * G2 / (4.0 * dot(-wi,n) * dot(wo,n));
}

float bsdf_rough_pdf(
    vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 wo, vec2 a)
{
#if 0
  vec3 h = normalize(wo-wi);
  float D  = bsdf_rough_D(a.x, n, h);
  float G1 = bsdf_rough_G1(wi, wo, n, a.x*a.x);
  return abs(G1 * dot(wi, h) * D / dot(wi, n));
#else // Bounded VNDF Sampling for the Smith-GGX BRDF, Tokuyoshi and Eto 2024
  float roughness = a.x;
  // i and o point away from surface and are in local tangent space!
  vec3 h = normalize(wo-wi);
  float D = bsdf_rough_D(roughness, n, h);
  vec2 ai = a * vec2(-dot(du, wi), -dot(dv, wi));
  float len2 = dot(ai, ai);
  float iz = dot(n,wi);
  float t = sqrt(len2 + iz*iz);
  if(iz < 0.0)
  { // incident from upper hemisphere
    float a = clamp(min(a.x , a.y), 0, 1); // Eq . 7
    float s = 1.0 + length(vec2(-dot(du, wi), -dot(dv, wi)));
    float a2 = a * a; float s2 = s * s;
    float k = (1.0 - a2) * s2 / (s2 + a2 * iz * iz); // Eq . 6
    return D / (2.0 * (t - k * iz)); // Eq . 10 * || dm / do ||
  }
  // return 0.0;
  // incident from under the surface. wtf?
  // Numerically stable form of the previous PDF for i.z < 0
  return D * (t + iz) / (2.0 * len2 ); // = Eq . 8 * || dm / do ||
#endif
}

#if 0
//====================================================
// microfacet model by jonathan, anisotropic beckmann:
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
// L, V, N, Tx, Ty in world space, pointing in rt direction: V->x->L
float bsdf_rough_eval(
    vec3 V, vec3 Tx, vec3 Ty, vec3 N, vec3 L, vec2 sigmaSq)
{
  V = -V; // make V pointing away from surface intersection point
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
#endif


// multiplexing:
vec3 bsdf_sample(uint m, vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 param, vec2 xi)
{
  if(m == 2) return sample_sphere(xi);
  return bsdf_diffuse_sample(wi, du, dv, n, xi);
}
float bsdf_pdf(uint m, vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 wo, vec3 param)
{
  if(m == 2) return 1.0/(4.0*M_PI);
  return bsdf_diffuse_pdf(wi, n, wo);
}
float bsdf_eval(uint m, vec3 wi, vec3 du, vec3 dv, vec3 n, vec3 wo, vec3 param)
{ // evaluate *without* albedo, that has to multiplied in the end
  if(m == 2) return 1.0/(4.0*M_PI);
  if(m == 1) return bsdf_rough_eval(wi, du, dv, n, wo, param.xy);
  return bsdf_diffuse_eval();
}


// numerically robust von Mises Fisher lobe
float vmf_eval(float kappa, float dotmu)
{
  if(kappa < 1e-4) return 1.0/(4.0*M_PI);
  return kappa/(2.0*M_PI*(1.0 - exp(-2.0*kappa))) * exp(kappa*(dotmu-1.0));
}

vec3 vmf_sample(float kappa, vec2 r)
{ // see wenzel's doc on numerically stable expression for vmm:
  float w = 1.0 + log(r.x + (1.0-r.x)*exp(-2.0*kappa))/kappa;
  vec2 v = vec2(sin(2.0*M_PI*r.y), cos(2.0*M_PI*r.y));
  return vec3(sqrt(1.0-w*w)*v, w);
}

float vmf_get_kappa(float x)
{ // compute concentration parameter for given maximum density x
  if(x > 0.795) return 2.0*M_PI*x;
  return max(1e-5, (168.479*x*x + 16.4585*x - 2.39942)/
      (-1.12718*x*x+29.1433*x+1.0));
}

