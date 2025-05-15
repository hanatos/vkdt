// assume we have bound:
// rt_accel
// buf_vtx

// scattermode flags
const uint s_reflect  =   1u;
const uint s_transmit =   2u;
const uint s_volume   =   4u;
const uint s_fiber    =   8u;
const uint s_emit     =  16u;
const uint s_sensor   =  32u;
const uint s_glossy   =  64u;
const uint s_specular = 128u;
const uint s_envmap   = 256u;
const uint s_absorb   =   0u;


void prepare_intersection(
    rayQueryEXT rq,
    vec3        w,    // ray direction
    inout vec3  x,    // intersection position
    out vec3    n,    // shading normal
    out vec3    ng,   // geo normal
    out vec2    st,   // texture coordinates, modulo'd into [0,1)
    out uvec3   mat)  // material ids
{ // access and unpack geometry data
  uint pi = 3*rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);
  uint it = rayQueryGetIntersectionInstanceIdEXT(rq, true);
  mat = uvec3(buf_vtx[it].v[pi+0].tex,
              buf_vtx[it].v[pi+1].tex,
              buf_vtx[it].v[pi+2].tex);
  vec3 v0 = vec3(buf_vtx[it].v[pi+0].x, buf_vtx[it].v[pi+0].y, buf_vtx[it].v[pi+0].z);
  vec3 v1 = vec3(buf_vtx[it].v[pi+1].x, buf_vtx[it].v[pi+1].y, buf_vtx[it].v[pi+1].z);
  vec3 v2 = vec3(buf_vtx[it].v[pi+2].x, buf_vtx[it].v[pi+2].y, buf_vtx[it].v[pi+2].z);
  vec3 n0 = geo_decode_normal(buf_vtx[it].v[pi+0].n);
  vec3 n1 = geo_decode_normal(buf_vtx[it].v[pi+1].n);
  vec3 n2 = geo_decode_normal(buf_vtx[it].v[pi+2].n);
  vec3 b;
  b.yz = rayQueryGetIntersectionBarycentricsEXT(rq, true);
  b.x = 1.0-b.z-b.y;
#if 0
  float t = rayQueryGetIntersectionTEXT(rq, true);
  x += t*w;
#else
  x = mat3(v0, v1, v2) * b;
#endif
  ng = normalize(cross(v1-v0, v2-v0)); // geo normal
  uint geo_flags = mat.x >> 16; // extract surface flags;
  if((geo_flags & 8)>0) n = ng;
  else n = normalize(mat3(n0, n1, n2) * b); // vertex normals

  if(geo_flags == 7)
  { // sky box, extract cube map textures from n and mark as sky:
    n = vec3(0);
    mat.x = buf_vtx[it].v[pi+0].n;
    mat.y = buf_vtx[it].v[pi+1].n;
    mat.z = buf_vtx[it].v[pi+2].n;
    return;
  }

  if(dot(w, n) > 0) n = -n;
  if(dot(w, ng) > 0) ng = -ng;
  vec2 st0 = unpackHalf2x16(buf_vtx[it].v[pi+0].st);
  vec2 st1 = unpackHalf2x16(buf_vtx[it].v[pi+1].st);
  vec2 st2 = unpackHalf2x16(buf_vtx[it].v[pi+2].st);
  st = fract(mat3x2(st0, st1, st2) * b);

  uint tex_n = mat.z & 0xffff;
  if(tex_n > 0)
  { // if normal map texture, load + apply it here
    ivec2 tc = ivec2(textureSize(img_tex[nonuniformEXT(tex_n)], 0)*st);
    tc = clamp(tc, ivec2(0), textureSize(img_tex[nonuniformEXT(tex_n)], 0)-1);
    vec3 du = v2 - v0, dv = v1 - v0;
    vec2 duv1 = st2 - st0, duv2 = st1 - st0;
    float det = duv1.x * duv2.y - duv2.x * duv1.y;
    if(abs(det) > 1e-8)
    {
      vec3 du2 =  normalize(( duv2.y * du - duv1.y * dv) / det);
      dv = -normalize((-duv2.x * du + duv1.x * dv) / det);
      du = du2;
    }
    n = normalize(mat3(du, dv, n) * ((texelFetch(img_tex[nonuniformEXT(tex_n)], tc, 0).xyz - vec3(0.5)) * vec3(2)));
    if(dot(w,n) > 0) n -= w*dot(w,n);
  }

  if((geo_flags & 8)==0 && (tex_n == 0))
  { // now fix shading normals below horizon and terminator problem:
    if(dot(w,n0) > 0) n0 -= w*dot(w,n0);
    if(dot(w,n1) > 0) n1 -= w*dot(w,n1);
    if(dot(w,n2) > 0) n2 -= w*dot(w,n2);
    n = normalize(mat3(n0, n1, n2) * b);
    vec3 tmpu = x - v0, tmpv = x - v1, tmpw = x - v2;
    float dotu = min(0.0, dot(tmpu, n0));
    float dotv = min(0.0, dot(tmpv, n1));
    float dotw = min(0.0, dot(tmpw, n2));
    tmpu -= dotu*n0;
    tmpv -= dotv*n1;
    tmpw -= dotw*n2;
    x += mat3(tmpu, tmpv, tmpw) * b;
  }
}

bool cast_ray(
    vec3        w,    // ray direction
    inout vec3  x,    // intersection position
    inout float dist, // max ray tracing distance
    out vec3    n,    // shading normal
    out vec3    ng,   // geo normal
    out vec2    st,   // texture coordinates
    out uvec3   mat)  // material ids
{
  rayQueryEXT rq;
  rayQueryInitializeEXT(rq, rt_accel,
      gl_RayFlagsNoneEXT,
      // gl_RayFlagsTerminateOnFirstHitEXT,// | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
      0xFF, x, 1e-3, w, dist);
  while(rayQueryProceedEXT(rq)) {
    if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
    {
      // TODO: skip this step if the whole instance is sure to not have transparency?
      nonuniformEXT int it = rayQueryGetIntersectionInstanceIdEXT(rq, false); // which of our ssbo
      nonuniformEXT int pi = 3*rayQueryGetIntersectionPrimitiveIndexEXT(rq, false); // primitive inside instance
      mat = uvec3(buf_vtx[it].v[pi+0].tex,
                  buf_vtx[it].v[pi+1].tex,
                  buf_vtx[it].v[pi+2].tex);
      uint tex_b = mat.x & 0xffff;
      if(true)// XXX switch off transparency tex_b == 0)
      {
        rayQueryConfirmIntersectionEXT(rq);
      }
      else
      {
        uint flags = mat.x >> 16; // extract surface flags;
        vec2 st0 = unpackHalf2x16(buf_vtx[it].v[pi+0].st);
        vec2 st1 = unpackHalf2x16(buf_vtx[it].v[pi+1].st);
        vec2 st2 = unpackHalf2x16(buf_vtx[it].v[pi+2].st);
        vec3 b;
        b.yz = rayQueryGetIntersectionBarycentricsEXT(rq, false);
        b.x = 1.0-b.z-b.y;
        st = mat3x2(st0, st1, st2) * b;
        if(flags > 0) st = vec2(
            st.x + 0.2*sin(st.y*2.0 + params.cltime),
            st.y + 0.2*sin(st.x*2.0 + params.cltime));
        ivec2 tc = ivec2(textureSize(img_tex[nonuniformEXT(tex_b)], 0)*fract(st));
        tc = clamp(tc, ivec2(0), textureSize(img_tex[nonuniformEXT(tex_b)], 0)-1);
        vec4 col_base = texelFetch(img_tex[nonuniformEXT(tex_b)], tc, 0);
        if(col_base.a > 0.666)
          rayQueryConfirmIntersectionEXT(rq);
      }
    }
  }
  if(rayQueryGetIntersectionTypeEXT(rq, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
  {
    prepare_intersection(rq, w, x, n, ng, st, mat);
    dist = rayQueryGetIntersectionTEXT(rq, true);
    return true;
  }
  dist = 1e10;
  return false;
}
