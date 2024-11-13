// environment map importance sampling

vec3 env_lookup(vec3 w, sampler2D img_env)
{
  vec2 tc = vec2((atan(w.y, w.x)+M_PI)/(2.0*M_PI), acos(clamp(w.z, -1, 1))/M_PI);
  // TODO probably use texelFetch and make this rescaling here pixel accurate
  tc.y = 12.0/13.0 * tc.y;
  return texture(img_env, tc).rgb;
}

vec3 env_sample(vec2 xi, sampler2D img_env)
{
  ivec2 size = textureSize(img_env, 0);
  ivec2 x = ivec2(0);

  int off = 2796201; // XXX for 8k!
  int wd = 1;
  while(2*wd < size.x)
  {
    int idx = x.x + wd * x.y;
    int iy = idx / size.x;
    int ix = idx % size.x;
    ivec2 tc = ivec2(ix, size.x/2 + iy);
    x *= 2; wd *= 2;
    vec4 v = texelFetch(img_env, tc, 0);
    float thrx = (v.r + v.b) / (v.r+v.g+v.b+v.a), thry;
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
    off -= wd * wd / 2;
  }

  // now last step, resample on full envmap blocks directly:
  x *= 2; // 2*wd == size.x now
  float l00 = dot(vec3(1), texelFetch(img_env, x+ivec2(0,0), 0).rgb) * sin(((x.y+  0.5)/float(size.y) - 0.5) * M_PI);
  float l01 = dot(vec3(1), texelFetch(img_env, x+ivec2(0,1), 0).rgb) * sin(((x.y+1+0.5)/float(size.y) - 0.5) * M_PI);
  float l10 = dot(vec3(1), texelFetch(img_env, x+ivec2(1,0), 0).rgb) * sin(((x.y+  0.5)/float(size.y) - 0.5) * M_PI);
  float l11 = dot(vec3(1), texelFetch(img_env, x+ivec2(1,1), 0).rgb) * sin(((x.y+1+0.5)/float(size.y) - 0.5) * M_PI);

  float thry;
  float thrx = (l00 + l10) / (l00+l01+l10+l11);
  if(xi.x > thrx)
  {
    x.x++;
    thry = l11 / (l11 + l01);
  }
  else thry = l10 / (l10 + l00);
  if(xi.y > thry) x.y++;

  // TODO: keep pdf alive through all the stuff above

  vec2 angles = x / vec2(size);
  return vec3(sin(angles.x*2.0*M_PI), cos(angles.x*2.0*M_PI), cos(angles.y*M_PI-M_PI/2.0));
}
