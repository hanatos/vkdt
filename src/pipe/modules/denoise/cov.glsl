#if 0
// XXX TODO: compute gaussian fit to gradient magnitudes in 4x4 grid!
vec3 response(sampler2D img, ivec2 p, out vec4 cov)
{
  vec2 isz = 1.0/textureSize(img, 0);
  float buf[5];
  for(int i=-2;i<=2;i++)
    if(level == 0)
      buf[i+2] = luminance_rec2020(texture(img, (p + 0.5 + ivec2(i, 0))*isz).rgb);
    else
      buf[i+2] = texture(img, (p + 0.5 + ivec2(i, 0))*isz).r;
  for(int j=-1;j<=2;j++)
  {
    for(int i=-2;i<=2;i++)
    {
    }
  }
}
#else
// compute gaussian covariance response
vec3 response(sampler2D img, ivec2 p, out vec4 cov, int level)
{
  vec2 isz = 1.0/textureSize(img, 0);
  // compute covariance
  mat2 Sw = mat2(0,0,0,0);
  mat2 Sb = mat2(0,0,0,0);
  float sw = 0.0, sb = 0.0;

  // XXX this is a test with arbitrary mean. i don't really want this
  // XXX because i'd rather have the gaussian go through the center pixel
  vec2 mw = vec2(0.0), mb = vec2(0.0);
  float smw = 0.0, smb = 0.0;
  float mean_b = 0.0;
  // float pc[5] = {1./6., 4./6., 6./6., 4./6., 1./6.}; // windowing function
  float pc[5] = {1., 1., 1., 1., 1.};
#if 1 // not clear which one is better here :( seems to depend on the case
  for(int j=-2;j<=2;j++) for(int i=-2;i<=2;i++)
  {
    float px;
    if(level == 0)
      px = luminance_rec2020(texture(img, (p + 0.5 + ivec2(i, j))*isz).rgb);
    else
      px = texture(img, (p + 0.5 + ivec2(i, j))*isz).r; // yuv
    float w = pc[j+2]*pc[i+2];
    vec2 p = vec2(i, j);
    mw += p*px*w;
    smw += px*w;
    mb += p/px*w;
    smb += 1.0/px*w;
  }
  mw /= smw;
  mb /= smb;
#else
  mw = mb = vec2(0.0); // XXX no mean (definitely has better smooth edges)
#endif
  for(int j=-2;j<=2;j++) for(int i=-2;i<=2;i++)
  {
    float px;
    if(level == 0)
      px = luminance_rec2020(texture(img, (p + 0.5 + ivec2(i, j))*isz).rgb);
    else
      px = texture(img, (p + 0.5 + ivec2(i, j))*isz).r; // yuv
    float w = pc[j+2]*pc[i+2];
    mean_b += px/25.0;
    float p2 = px * px * w;
    vec2 p = vec2(i, j) - mw;
    Sw[0][0] += p2 * p.x * p.x;
    Sw[0][1] += p2 * p.x * p.y;
    Sw[1][0] += p2 * p.y * p.x;
    Sw[1][1] += p2 * p.y * p.y;
    sw += p2;
    p = vec2(i, j) - mb;
    p2 = 1.0/(px*px) * w;
    Sb[0][0] += p2 * p.x * p.x;
    Sb[0][1] += p2 * p.x * p.y;
    Sb[1][0] += p2 * p.y * p.x;
    Sb[1][1] += p2 * p.y * p.y;
    sb += p2;
  }
  Sw /= sw;
  Sb /= sb;
  // mat2 S = Sb; // XXX no black features
  // mat2 S = Sw[0][0] < Sb[0][0] ? Sw : Sb;
  mat2 S = determinant(Sw) < determinant(Sb) ? Sw : Sb;
  // mat2 S = Sw[0][0] + abs(Sw[1][0]) + Sw[1][1] <
    // Sb[0][0] + abs(Sb[1][0]) + Sb[1][1] ? Sw : Sb;
  // S *= 1.0/24.0;
  // compute response
  vec2 eval, evec0, evec1;
#if 0
  vec2 alw, ecw0, ecw1, alb, ecb0, ecb1;
  evd2x2(alw, ecw0, ecw1, Sw);
  evd2x2(alb, ecb0, ecb1, Sb);
  // if(alw.x < alb.x)
  // if(alw.x/alw.y < alb.x/alb.y)
  if(alw.x+alw.y < alb.x+alb.y)
  {eval = alw; evec0 = ecw0; evec1 = ecw1; }
  else
  {eval = alb; evec0 = ecb0; evec1 = ecb1; }
#else
  evd2x2(eval, evec0, evec1, S);
#endif
#if 0 // outlier detection, ineffective
  if(eval.x < 0.2)// && eval.y < 0.2)
  {
    const float pascal[5] = {1, 4, 6, 4, 1};
    vec3 res = vec3(0);
    float wt = 0.0;
    for(int j=-2;j<=2;j++) for(int i=-2;i<=2;i++)
    {
      vec3 px = texture(img, (p + 0.5 + ivec2(i, j))*isz).rgb;
      if(i==0 && j==0) continue;
      float w = pascal[2+j]*pascal[2+i];
      res += w * px;
      wt += w;
    }
    return res / wt;
  }
#endif
  eval *= vec2(1.0, 0.05);
  eval = clamp(eval, 0.01, 25.0);
  cov = vec4(eval, evec0);

  vec3 res = vec3(0);
  float wt = 0.0;
  for(int j=-2;j<=2;j++) for(int i=-2;i<=2;i++)
  {
    vec3 px = texture(img, (p + 0.5 + ivec2(i, j))*isz).rgb;
    if(px.r > 3.0 * mean_b) continue; // detect hot pixels
    vec2 x = vec2(i, j);
    x = vec2(dot(x, evec0), dot(x, evec1));
    float w = max(1e-9, exp(-0.5*dot(x/eval, x)));
    res += w * px;
    wt += w;
  }
  return res / max(wt, 1e-8);
}
#endif
