#include "von_mises_fisher.glsl"

struct mat_state_t
{ // cache stuff once so we don't need to re-evaluate it
  vec4 col_base;
  vec4 col_emit;
  vec3 normal; // shading normal
  vec3 du, dv; // shading normal tangent frame
  float roughness;
  float alpha;    // alpha transparency
  uint geo_flags; // geo_flags_t on cpu: lava, slime, nonorm, ..
};

vec3 emission_to_hdr(vec3 c)
{
  if(any(greaterThan(c, vec3(1e-3)))) // avoid near div0
    return c/dot(c, vec3(1)) * 10.0*(exp2(4.0*dot(c, vec3(1)))-1.0);
  return c;
}

vec3 quake_envmap(in vec3 w, uint sky_rt, uint sky_bk, uint sky_lf, uint sky_ft, uint sky_up, uint sky_dn)
{
  if(sky_lf == 0xffff)
  { // classic quake sky
    vec2 st = 0.5 + 0.5*vec2(-w.y,w.x) / abs(w.z);
    vec2 t = params.cltime * 60.0 * vec2(0.002, 0.001);
    vec4 bck = texelFetch(img_tex[nonuniformEXT(sky_rt)], ivec2(mod(st+0.1*t, vec2(1))*textureSize(img_tex[nonuniformEXT(sky_rt)], 0)), 0);
    vec4 fnt= texelFetch(img_tex[nonuniformEXT(sky_bk)], ivec2(mod(st+t, vec2(1))*textureSize(img_tex[nonuniformEXT(sky_bk)], 0)), 0);
    vec3 tex = mix(bck.rgb, fnt.rgb, fnt.a);
    return 100*tex*tex;
  }
  else
  { // cubemap: gfx/env/*{rt,bk,lf,ft,up,dn}
    // vec3 sundir = normalize(vec3(1, 1, 1)); // this where the moon is in ad_azad
    // vec3 sundir = normalize(vec3(1, -1, 1)); // this comes in more nicely through the windows for debugging
    vec3 sundir = normalize(vec3(1, -1, 1)); // ad_tears
    const float k0 = 4.0, k1 = 30.0, k2 = 4.0, k3 = 3000.0;
    vec3 emcol = vec3(0.0);
    emcol  = vec3(0.50, 0.50, 0.50) * /*(k0+1.0)/(2.0*M_PI)*/ pow(0.5*(1.0+dot(sundir, w)), k0);
    emcol += vec3(1.00, 0.70, 0.30) * /*(k1+1.0)/(2.0*M_PI)*/ pow(0.5*(1.0+dot(sundir, w)), k1);
    // emcol += 1000*vec3(1.00, 1.00, 1.00) * /*(k1+1.0)/(2.0*M_PI)*/ pow(0.5*(1.0+dot(sundir, w)), k3);
    emcol += 30.0*vec3(1.1, 1.0, 0.9)*vmf_pdf(w, sundir, k3);
    emcol += vec3(0.20, 0.08, 0.02) * /*(k2+1.0)/(2.0*M_PI)*/ pow(0.5*(1.0-w.z), k2);
    // emcol *= 2.0;
    int m = 0;
    if(abs(w.y) > abs(w.x) && abs(w.y) > abs(w.z)) m = 1;
    if(abs(w.z) > abs(w.x) && abs(w.z) > abs(w.y)) m = 2;
    uint side = 0;
    vec2 st;
    if     (m == 0 && w.x > 0) { side = sky_rt; st = 0.5 + 0.5*vec2(-w.y, -w.z) / abs(w.x);} // rt
    else if(m == 0 && w.x < 0) { side = sky_lf; st = 0.5 + 0.5*vec2( w.y, -w.z) / abs(w.x);} // lf
    else if(m == 1 && w.y > 0) { side = sky_bk; st = 0.5 + 0.5*vec2( w.x, -w.z) / abs(w.y);} // bk
    else if(m == 1 && w.y < 0) { side = sky_ft; st = 0.5 + 0.5*vec2(-w.x, -w.z) / abs(w.y);} // ft
    else if(m == 2 && w.z > 0) { side = sky_up; st = 0.5 + 0.5*vec2(-w.y,  w.x) / abs(w.z);} // up
    else if(m == 2 && w.z < 0) { side = sky_dn; st = 0.5 + 0.5*vec2(-w.y, -w.x) / abs(w.z);} // dn
    ivec2 tc = 
      clamp(ivec2(textureSize(img_tex[nonuniformEXT(side)], 0)*st),
          ivec2(0), textureSize(img_tex[nonuniformEXT(side)], 0)-1);
    vec3 tex = texelFetch(img_tex[nonuniformEXT(side)], tc, 0).rgb;
    emcol += tex*tex; // mul "un-gamma"d sky texture
    return emcol;
  }
}

#include "sdielectric.glsl"
mat_state_t
mat_init(
    uvec4 mat,         // texture ids base, emit, normals, roughness and flags + alpha, inside flag
    vec2 st,           // texture coordinates
    vec3 n,            // normal
    inout uint flags,  // scatter mode (reflect, transmit, emit, ..)
    vec4 lambda,       // four wavelengths, .x is hero wavelength
    vec3 w,            // incident direction pointing towards surface
    out vec3 albedo)   // output approximate albedo/base colour in rgb (as aov for denoising)
{
  albedo = vec3(0);
  if((flags & s_volume) != 0)
    return mat_state_t(vec4(0), vec4(0), vec3(0), vec3(0), vec3(0), 0.0, 1.0, 0);
  if((flags & s_envmap) != 0)
  {
    flags |= s_emit;
    vec3 env = env_lookup(w, img_env);
    albedo = env;
    vec4 L = colour_upsample(env, lambda);
    return mat_state_t(vec4(0), L, vec3(0), vec3(0), vec3(0), 0.0, 1.0, 0);
  }

  if(all(equal(n, vec3(0))))
  {
    vec3 env = quake_envmap(w, 
        mat.x & 0xffff, mat.x >> 16,
        mat.y & 0xffff, mat.y >> 16,
        mat.z & 0xffff, mat.z >> 16);
    flags |= s_emit;
    vec4 L = colour_upsample(env, lambda);
    return mat_state_t(vec4(0), L, vec3(0), vec3(0), vec3(0), 0.0, 1.0, 0);
  }

  uint tex_r = mat.z >> 16;
  // uint tex_n = mat.z & 0xffff; // already applied during ray cast
  uint tex_b = mat.x & 0xffff;
  uint tex_e = mat.y & 0xffff;
  uint geo_flags = mat.x >> 16;
  if(mat.w > 0) geo_flags |= s_geo_inside; // mark as inside (flipped normal)
  float alpha = unpackUnorm2x16(mat.y).y;

  if(geo_flags > 0 && geo_flags < 6)
  { // all esoteric surfaces warp
    st = vec2(
        st.x + 0.2*sin(st.y*2.0 + params.cltime),
        st.y + 0.2*sin(st.x*2.0 + params.cltime));
  }

  vec4 base = vec4(0.5);
  vec4 emit = vec4(0);
  float roughness = 0.26;

  if(tex_b > 0)
  {
    ivec2 tc = ivec2(textureSize(img_tex[nonuniformEXT(tex_b)], 0)*st);
    tc = clamp(tc, ivec2(0), textureSize(img_tex[nonuniformEXT(tex_b)], 0)-1);
    vec3 tex = texelFetch(img_tex[nonuniformEXT(tex_b)], tc, 0).rgb;
    albedo = tex*tex;
    base = colour_upsample(tex*tex, lambda);
  }
  if(tex_e > 0)
  {
    ivec2 tc = ivec2(textureSize(img_tex[nonuniformEXT(tex_e)], 0)*st);
    tc = clamp(tc, ivec2(0), textureSize(img_tex[nonuniformEXT(tex_e)], 0)-1);
    vec3 tex = texelFetch(img_tex[nonuniformEXT(tex_e)], tc, 0).rgb;
    emit = colour_upsample(emission_to_hdr(tex*tex), lambda);
  }
  if(tex_r > 0)
  {
    ivec2 tc = ivec2(textureSize(img_tex[nonuniformEXT(tex_r)], 0)*st);
    tc = clamp(tc, ivec2(0), textureSize(img_tex[nonuniformEXT(tex_r)], 0)-1);
    roughness = texelFetch(img_tex[nonuniformEXT(tex_r)], tc, 0).r;
  }

  if((geo_flags & 7) == s_geo_watere) // tears waterfall hack
    emit = 2.0*base;

  if(any(greaterThan(emit, vec4(0)))) flags |= s_emit;
  mat3 onb = make_frame(n);
  return mat_state_t(base, emit, n, onb[0], onb[1], roughness, alpha, geo_flags);
}

vec4 // return evaluation of f_r for the given wavelengths
mat_eval(
    mat_state_t mat,
    uint  flags,  // s_volume, ..
    vec3  w,      // observer direction, pointing towards surf
    vec3  n,      // shading normal
    vec3  wo,     // light direction, pointing away from surf
    vec4  lambda) // hero wavelengths
{
  if((flags & s_volume) > 0) return vec4(volume_phase_function(dot(w, wo)));
  if((mat.geo_flags & s_geo_dielectric) > 0) return dielectric_eval(mat, flags, w, n, wo, lambda);
  vec3 h = normalize(wo-w);
  return mat.col_base * bsdf_rough_eval(w, mat.du, mat.dv, n, wo, vec2(mat.roughness)) * fresnel(0, lambda, 1.0, dot(wo, h));
}

vec3 // return outgoing direction wo, sampled according to f_r * cos
mat_sample(
    mat_state_t mat,
    inout uint flags,  // s_volume, ..
    vec3       w,      // incident direction pointing towards intersection point
    vec3       n,      // shading normal
    vec4       lambda, // wavelengths
    vec3       xi,     // random numbers
    out vec4   c)      // value of monte carlo estimator f_r*cos/pdf for the four wavelengths
{
  if((flags & s_volume) > 0)
  {
    const float cosu = volume_sample_phase_function_cos(xi.xy);
    mat3 onb = make_frame(w);
    const float phi = 2.0*M_PI*xi.z;
    return onb * vec3(sqrt(1.0-cosu*cosu) * vec2(sin(phi), cos(phi)), cosu);
  }
  if((mat.geo_flags & s_geo_dielectric) > 0) return dielectric_sample(mat, flags, w, n, lambda, xi, c);
  float X = 1.0;
  vec3 wo = bsdf_rough_sample(w, mat.du, mat.dv, n, vec2(mat.roughness), xi.xy, X);
  vec3 h = normalize(wo-w);
  c = mat.col_base * X * fresnel(0, lambda, 1.0, dot(wo, h));
  if(dot(wo,n) <= 0.0) { flags = s_absorb; c = vec4(0.0); } // sampled under surface
  flags |= s_reflect;
  return wo;
}

float // return solid angle pdf
mat_pdf(
    mat_state_t mat,
    uint flags,   // s_volume, ..
    vec3 w,       // incident
    vec3 n,       // shading normal
    vec3 wo,      // outgoing
    float lambda) // the hero wavelength
{
  if((flags & s_volume) > 0) return volume_phase_function(dot(w, wo));
  if((mat.geo_flags & s_geo_dielectric) > 0) return dielectric_pdf(mat, flags, w, n, wo, lambda);
  return bsdf_rough_pdf(w, mat.du, mat.dv, n, wo, vec2(mat.roughness));
}
