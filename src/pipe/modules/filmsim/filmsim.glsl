// filmsim.glsl: pure colour and spectral math shared by every filmsim shader.

#extension GL_KHR_shader_subgroup_basic      : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable

const vec4 lambda_arr[11] = vec4[](
  vec4(380.0, 390.0, 400.0, 410.0),
  vec4(420.0, 430.0, 440.0, 450.0),
  vec4(460.0, 470.0, 480.0, 490.0),
  vec4(500.0, 510.0, 520.0, 530.0),
  vec4(540.0, 550.0, 560.0, 570.0),
  vec4(580.0, 590.0, 600.0, 610.0),
  vec4(620.0, 630.0, 640.0, 650.0),
  vec4(660.0, 670.0, 680.0, 690.0),
  vec4(700.0, 710.0, 720.0, 730.0),
  vec4(740.0, 750.0, 760.0, 770.0),
  vec4(780.0, 790.0, 800.0, 810.0)
);

#define SPECTRAL_DYE_LIGHT(RAW, DENS, DYE_R, DYE_G, DYE_B, FAC_R, FAC_G, FAC_B) \
  do { \
    RAW = vec3(0.0); \
    [[unroll]] \
    for(int i = 0; i < 10; i++) \
    { \
      vec4 ds = (DENS).x * DYE_R[i] + (DENS).y * DYE_G[i] + (DENS).z * DYE_B[i]; \
      vec4 light = exp2(-ds); \
      RAW.r += dot(light, FAC_R[i]); \
      RAW.g += dot(light, FAC_G[i]); \
      RAW.b += dot(light, FAC_B[i]); \
    } \
    { \
      float ds = (DENS).x * DYE_R[10].x + (DENS).y * DYE_G[10].x + (DENS).z * DYE_B[10].x; \
      float light = exp2(-ds); \
      RAW += vec3(light) * vec3(FAC_R[10].x, FAC_G[10].x, FAC_B[10].x); \
    } \
  } while(false)

#define SPECTRAL_COEFF_LIGHT(RAW, COEFF, FAC_R, FAC_G, FAC_B, N) \
  do { \
    RAW = vec3(0.0); \
    [[unroll]] \
    for(int i = 0; i < min(N, 10); i++) \
    { \
      vec4 lambda = lambda_arr[i]; \
      vec4 x = ((COEFF).x * lambda + (COEFF).y) * lambda + (COEFF).z; \
      vec4 y = inversesqrt(x * x + vec4(1.0)); \
      vec4 val = (0.5 * x * y + vec4(0.5)) * (COEFF).w; \
      RAW.r += dot(val, FAC_R[i]); \
      RAW.g += dot(val, FAC_G[i]); \
      RAW.b += dot(val, FAC_B[i]); \
    } \
    if ((N) > 10) \
    { \
      float lambda = lambda_arr[10].x; \
      float x = ((COEFF).x * lambda + (COEFF).y) * lambda + (COEFF).z; \
      float y = inversesqrt(x * x + 1.0); \
      float val = (0.5 * x * y + 0.5) * (COEFF).w; \
      RAW += vec3(val) * vec3(FAC_R[10].x, FAC_G[10].x, FAC_B[10].x); \
    } \
  } while(false)

// Select the reduction type and shared accumulator lanes.
#define SUBGROUP_REDUCE(TYPE, SWIZZLE, ZERO, VAL, RESULT) \
  do { \
    TYPE subsum = subgroupAdd(VAL); \
    if (subgroupElect()) shared_reduce_acc[gl_SubgroupID].SWIZZLE = subsum; \
    barrier(); \
    if (gl_SubgroupID == 0) \
    { \
      TYPE val = (gl_SubgroupInvocationID < gl_NumSubgroups) ? shared_reduce_acc[gl_SubgroupInvocationID].SWIZZLE : ZERO; \
      TYPE sum = subgroupAdd(val); \
      if (gl_SubgroupInvocationID == 0) RESULT = sum; \
    } \
    barrier(); \
  } while(false)

float envelope(float w)
{
  return 1000.0 * smoothstep(380.0, 400.0, w) * (1.0 - smoothstep(700.0, 730.0, w));
}

vec2 daylight_locus_xy(float T)
{
  T = clamp(T, 4000.0, 25000.0);
  float invT = 1.0 / T;
  float invT2 = invT * invT;
  float invT3 = invT2 * invT;
  float x = (T <= 7000.0) ?
    -4.6070e9*invT3 + 2.9678e6*invT2 + 99.11*invT + 0.244063 :
    -2.0064e9*invT3 + 1.9018e6*invT2 + 247.48*invT + 0.237040;
  float y = -3.0*x*x + 2.870*x - 0.275;
  return vec2(x, y);
}

vec2 daylight_weights(float T)
{
  vec2 xy = daylight_locus_xy(T);
  float d = 0.2562*xy.x - 0.7341*xy.y + 0.0241;
  float invd = 1.0 / (abs(d) < 1e-9 ? 1e-9 : d);
  return (vec2(-1.7703, -31.4424) * xy.x + vec2(5.9114, 30.0717) * xy.y + vec2(-1.3515, 0.0300)) * invd;
}

float norm_cdf(float z)
{
  return 1.0 / (1.0 + exp2(-z * (0.10294312 * z * z + 2.30220556)));
}

vec3 norm_cdf(vec3 z)
{
  return 1.0 / (1.0 + exp2(-z * (vec3(0.10294312) * z * z + vec3(2.30220556))));
}

vec3 gumbel_cdf(vec3 z)
{
  return exp2(-exp2(-(z * vec3(model_gumbel_scale) + vec3(model_gumbel_loc))));
}

vec3 dichroic_filters(float w)
{
  bool low = (w <= 550.0);
  vec3 edges = low ? vec3(607.0, 500.0, 516.0) : vec3(607.0, 610.0, 516.0);
  vec3 inv_w = vec3(0.176776695, 0.176776695, 0.11785113);
  vec3 cdf   = norm_cdf((vec3(w) - edges) * inv_w);
  return vec3(1.0 - cdf.x, low ? 1.0 - cdf.y : cdf.y, cdf.z);
}

uvec3 pcg3d(uvec3 v)
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
  v ^= v >> 16u;
  v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
  return v;
}

vec3 hash3(ivec2 p, uint stream)
{
  return vec3(pcg3d(uvec3(uvec2(p), stream))) * (1.0/4294967296.0);
}

const vec3 d65_xyz = vec3(0.950430, 1.0, 1.088801);
const float d65_cct = 6504.0; // rec2020's native white; scene_ill corrects captures back to this

mat3 chromatic_adapt(mat3 cat_m, mat3 cat_minv, vec3 src_xyz, vec3 dst_xyz)
{
  vec3 scale = (cat_m * dst_xyz) / max(cat_m * src_xyz, vec3(1e-6));
  return mat3(cat_minv[0]*scale.x, cat_minv[1]*scale.y, cat_minv[2]*scale.z) * cat_m;
}

