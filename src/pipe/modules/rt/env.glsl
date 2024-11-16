// environment map importance sampling

vec3 env_lookup(vec3 w, sampler2D img_env)
{
  vec2 tc = vec2((atan(w.y, w.x)+M_PI)/(2.0*M_PI), acos(clamp(w.z, -1, 1))/M_PI);
  ivec2 size = ivec2(textureSize(img_env, 0).x, 0);
  size.y = size.x/2;
#if 0 // XXX DEBUG visualise mip maps
  int m = 10;
  size = ivec2(2,1);
  int off = 1;
  while(--m > 0)
  {
    off += size.x * size.y;
    size *= 2;
  }
  ivec2 tci = ivec2(tc * size);
  int idx = off + tci.y * size.x + tci.x;
  int wd = textureSize(img_env, 0).x;
  tci = ivec2(idx % wd, wd/2 + idx / wd);

  return texelFetch(img_env, tci, 0).rgb;
#else
  ivec2 tci = clamp(ivec2(tc * size + 0.5), ivec2(0), size-1);
  return texelFetch(img_env, tci, 0).rgb;
#endif
}

vec3 env_sample(vec2 xi, sampler2D img_env, inout float X)
{
  ivec2 size = textureSize(img_env, 0);
  size.y = size.x / 2;
  ivec2 x = ivec2(0);

  int off = 0;
  int wd = 1;
  float sum = -1.0;
  while(2*wd < size.x)
  {
    int idx = off + x.x + wd * x.y;
    int iy = idx / size.x;
    int ix = idx % size.x;
    ivec2 tc = ivec2(ix, size.y + iy);
    x *= 2;
    vec4 v = texelFetch(img_env, tc, 0);
    float thrx = (v.r + v.b) / (v.r+v.g+v.b+v.a), thry;
    if(sum < 0.0) sum = v.r+v.g;//+v.b+v.a;
    if(xi.x > thrx)
    {
      x.x++;
      xi.x = (xi.x - thrx) / (1.0-thrx);
      thry = v.g / (v.g + v.a);
    }
    else
    {
      xi.x /= thrx;
      thry = v.r / (v.r + v.b);
    }
    if(xi.y > thry)
    {
      x.y++;
      xi.y = (xi.y-thry) / (1.0-thry);
    }
    else xi.y /= thry;
    off += wd * max(1, wd / 2);
    wd *= 2;
  }
  // 2*wd == size.x now, x < wd.
  // max mip accessed so far is wd/2

  // now last step, resample on full envmap blocks directly:
  x *= 2;
  float l;
  float l00 = dot(vec3(1), texelFetch(img_env, x+ivec2(0,0), 0).rgb) * sin((x.y+  0.5)/float(size.y) * M_PI);
  float l01 = dot(vec3(1), texelFetch(img_env, x+ivec2(0,1), 0).rgb) * sin((x.y+1+0.5)/float(size.y) * M_PI);
  float l10 = dot(vec3(1), texelFetch(img_env, x+ivec2(1,0), 0).rgb) * sin((x.y+  0.5)/float(size.y) * M_PI);
  float l11 = dot(vec3(1), texelFetch(img_env, x+ivec2(1,1), 0).rgb) * sin((x.y+1+0.5)/float(size.y) * M_PI);

  float thry;
  float thrx = (l00 + l10) / (l00+l01+l10+l11);
  if(xi.x > thrx)
  {
    x.x++;
    thry = l11 / (l11 + l01);
  }
  else thry = l10 / (l10 + l00);
  if(xi.y > thry) x.y++;
  if(xi.x <= thrx && xi.y <= thry) l = l00;
  if(xi.x >  thrx && xi.y <= thry) l = l01;
  if(xi.x <= thrx && xi.y >  thry) l = l10;
  if(xi.x >  thrx && xi.y >  thry) l = l11;

  float pdf = l / sum;
  vec2 angles = (x + xi) / vec2(size);

  // TODO: ratio of quantised vs unquantised sin(theta_pixel_center)/sin(theta)
  pdf *= 1.0 / (2.0*M_PI*M_PI); // convert to solid angle
  X /= pdf;

  return vec3(cos((angles.x-0.5)*2.0*M_PI), sin((angles.x-0.5)*2.0*M_PI), cos(angles.y*M_PI));
}

float env_pdf(vec3 w, sampler2D img_env)
{
  vec2 tc = vec2((atan(w.y, w.x)+M_PI)/(2.0*M_PI), acos(clamp(w.z, -1, 1))/M_PI);
  ivec2 size = ivec2(textureSize(img_env, 0).x, 0);
  size.y = size.x/2;
  ivec2 tci = clamp(ivec2(tc * size + 0.5), ivec2(0), size-1);
  float l = dot(vec3(1), texelFetch(img_env, tci, 0).rgb) * sin((tci.y+0.5)/float(size.y) * M_PI);
  vec4 v = texelFetch(img_env, ivec2(0, size.y), 0);
  float sum = v.r+v.g;//+v.b+v.a;
  float pdf = l / sum;
  return pdf / (2.0*M_PI*M_PI); // convert to solid angle
}
