// path sampling routines:
// * retrace an existing path verbatim to re-evaluate validity of measurement
// * retrace + mutate for small steps
// * trace from scratch for large steps


// XXX TODO do we really want this, ever, or do we just mutate it with an extra small
// gaussian covariance matrix? in a best-effort kinda sense the modified path will be
// something meaningful and the difference will vanish with startup bias.
vec4 pt_retrace(uint pi)
{
  uint vidx = pcp.path[pi].v;
  pc_vtx_t vtx = pcv.v[vidx];
  vec4 lambda = pcp.path[pi].lambda;

  vec3 x = camera.o.xyz;
  vec3 w = normalize(vtx.v - camera.o.xyz);
  // connect vertex to camera
  vec2 px;
  vec4 f = vec4(camera_eval(vec4(camera.o, -1), w, px));
  if(any(lessThan(px, vec2(0.0))) || any(greaterThanEqual(px, vec2(1.0))))
    return vec4(0.0); // outside frame buffer

  // vec4 f is spectral measurement contribution in solid angle, i.e. omit geo terms

  for(int i=0;i<5;i++)
  { // while we got more vertices
    // trace ray from curr to next vertex
    uvec3 mat;
    vec2 st;
    vec3 n, ng;
    float dist = 1e30;
    if(cast_ray(w, x.xyz, dist, n, ng, st, mat))
    { // if distance too far from expected vertex return 0
      dist = distance(w.xyz, vtx.v);
      if(dist > 1e-3) return vec4(0.0);
    }
    else
    { // if volume or envmap this is fine, else return 0
      if(((vtx.flags & s_volume) == 0) && (vtx.flags & s_envmap) == 0) return vec4(0.0);
    }
    // evaluate measurement in solid angle space
    f *= volume_transmittance(x, vtx.v);

    vec3 tmp;
    mat_state_t ms = mat_init(mat, st, n, vtx.flags, lambda, w, tmp);
    if(vtx.next == -1u)
    { // last vertex, consider emission
      if((vtx.flags & s_emit) != 0) // emitter, envmap or geo
        return f * ms.col_emit;
      return vec4(0.0); // last vertex did not find emissive things
    }
    else
    { // we're in the middle of a path, eval bsdf/ph
      x = vtx.v; // move on
      vidx = vtx.next;
      vtx = pcv.v[vidx];
      vec3 wo = normalize(vtx.v - x);
      f *= mat_eval(ms, vtx.flags, w, n, wo, lambda);
      w = wo;
    }
    // don't record in path cache, obviously it's already there.
  }

  // uuhm how to tell this is better or worse than last time?
  // maybe have to store measurement and pdf along with path?
  // this will end up in a list for resampling, along with the new paths
  // storing their MC contribution c = f/p. we need to put b = mean image
  // brightness (so far, excluding the new sample set..) to achieve detailed
  // balance for *static* scenarios.
  // dynamic: correct with f_new / f_old ? correct by pdf too? G terms cancel out of PDF, mostly? in our solid angle sampling case always?
  // note that dynamic env introduces new startup bias, want to make good paths discoverable here. how should they be treated? can't pretend independent sampling,
  // assume changes are small

  return f;
}

vec4 dithermask(vec4 xi, ivec2 rp)
{
  return fract(xi + texelFetch(img_blue, rp, 0));
}

// trace independent new sample, simple pt, starting from gbuf
vec4 // returns rgb L + second moment of luminance
pt_trace(
    ivec2 ipos,      // pixel coordinate
    uint  seed,      // random seed
    ivec2 fbdim,     // frame buffer dimensions
    out vec3 albedo) // return base texture on first hit as aov for denoiser
{
  // blue noise dither mask
  const ivec2 rp = ivec2(ipos % ivec2(textureSize(img_blue, 0)));

  camera = camera_t(0.5, vec2(1.0, fbdim.y/float(fbdim.x)), params.cam_x.xyz, mat3(1));
  camera_setup(params.cam_x.xyz, params.cam_w.xyz, params.cam_u.xyz);

  uint vidx = -1u;    // vertex index in path cache that we stored last iteration
  vec4 acc = vec4(0); // value of our estimator, accumulate this
  vec4 hrp = vec4(1); // hero wavelength p, only deviates from 1 if pdf is different per wavelength
  vec4 rand = vec4(mrand(seed), mrand(seed), mrand(seed), mrand(seed));
  vec4 contrib = vec4(1.0);
  // dithermask shows *no* difference for stratified hwl:
  uint seed2 = 19937*global.frame;
  vec4 lambda = colour_sample_lambda(dithermask(vec4(mrand(seed2)), rp), contrib, hrp);

  vec4 x;
  vec3 w, n, ng; // ray position, direction, hit normal
  vec2 st;       // texture coordinates
  uvec3 mat;
  float dist;
  contrib *= camera_sample_xw((ipos+filter_bh_sample(rand.yz))/fbdim, x, w);
  // contrib *= camera_sample_xw(vec2(ipos)/fbdim, x, w);
  int vcnt = 1; // vertex count, one on camera
  int act = mix(1, 0, all(lessThanEqual(contrib, vec4(0.0))));

  for(int i=0;i<2;i++)
  { // for a max number of bounces
    mat_state_t ms;
    uint flags = 0;
    dist = 1e20;
    const float tmax = 1e12;//volume_sample_dist(x.xyz, w, mrand(seed));
    if(act > 0)
    {
      if(i == 0)
      { // read geo intersection info
        vec2 dn = texelFetch(img_dn, ipos, 0).rg;
        dist = dn.x;
        n  = geo_decode_normal(floatBitsToUint(dn.y));
        ng = geo_decode_normal(buf_mat.v[ipos.x+fbdim.x*ipos.y].ng);
        mat = uvec3(
            buf_mat.v[ipos.x+fbdim.x*ipos.y].m0,
            buf_mat.v[ipos.x+fbdim.x*ipos.y].m1,
            buf_mat.v[ipos.x+fbdim.x*ipos.y].m2);
        uint stui = buf_mat.v[ipos.x+fbdim.x*ipos.y].st;
        if(stui == -1u) n = vec3(0);
        st = unpackUnorm2x16(stui);
        if(dist < tmax) flags = 0;
        else          { flags = s_volume; dist = tmax; }
        if(dist >=1e10) flags = s_envmap;
        x.xyz = x.xyz + dist * w + 0.001*ng;
      }
      else
      { // trace ray
        dist = tmax;
        if(cast_ray(w, x.xyz, dist, n, ng, st, mat)) flags = 0;
        else if(dist < tmax) flags = s_volume;
        if(dist >= 1e10)     flags = s_envmap;
      }

      vec3 tmp;
      ms = mat_init(mat, st, n, flags, lambda, w, tmp);
      if(i == 0) albedo = tmp;
      if(i == 0) albedo = vec3(-dot(n, w));
      // XXX
      // return vec4(0.001*st.x, 0, 0, 0);
      // return vec4(colour_to_rgb(ms.col_base, lambda, hrp), 1);
      // float d = max(0,-dot(n, w));
      // return vec4(d);

      if((flags & s_emit) != 0)
        acc += contrib * ms.col_emit;
    }

    act = mix(act, 0, all(lessThanEqual(contrib, vec4(0.0))));
    if(subgroupAdd(act) <= 0) break; // keep subgroup in sync

    if((act > 0) && ((flags & (s_emit|s_envmap)) == 0))
    {
      rand = vec4(mrand(seed), mrand(seed), mrand(seed), mrand(seed));
      w = mat_sample(ms, flags, w, n, lambda, rand.xyz, contrib);
      if(dot(ng, w) <= 0) act = 0;
    }

    if(subgroupAdd(act) <= 0) break; // keep subgroup in sync

    { // path vertex cache stuff
      // allocate new vertex for path. the first vertex (camera) is not explicitly stored
      uint new_addr = pc_allocate_vertices(act);
      if(act != 0)
      { // wow this is all like SIMD on CPU again:
        if(vidx != -1u)
        { // add next vertex
          pcv.v[vidx].next = new_addr;
        }
        else if(i == 0)
        { // add first vertex to path
          uint pidx = ipos.x + fbdim.x * ipos.y;
          pcp.path[pidx].v = new_addr;
        }
        vidx = new_addr;
        if(vidx != -1u) // allocation failed?
          pcv.v[vidx] = pc_vtx_t(x.xyz, flags, -1u);
        vcnt++; // now have one more vertex
      }
    }
    act = mix(act, 0, (flags & (s_emit|s_envmap)) > 0); // endpoint found
    act = mix(act, 0, vidx == -1); // vtx alloc failed
    if(subgroupAdd(act) <= 0) break; // keep subgroup in sync
  }
  vec4 acc_rgb;
  acc_rgb.rgb = colour_to_rgb(acc, lambda, hrp);
  uint pidx = ipos.x + fbdim.x * ipos.y;
  // *not* rgb luminance, since that could be negative
  // XXX this is the second moment of something odd that we can't reconstruct from rgb mean!
  // acc_rgb.w = vcnt > 1 ? clamp(dot(acc, acc), 0.0, 1e8) : 0.0;
  acc_rgb.w = vcnt > 2 ? clamp(length(acc), 0.0, 1e8) : 0.0;
  // XXX TODO path also needs to know about ipos/pixel coordinate? and wavelengths?
  // set directly visible lights to zero for path resampling purposes
  // XXX TODO set to actual measurement
  pcp.path[pidx].f = acc_rgb.w;// XXX uh what? now it's contrib = f/p
  // acc_rgb.w *= acc_rgb.w;
  return acc_rgb;
}
