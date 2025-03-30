vec4 abcd(int c, int i)
{
  if(c == 0) return params.abcdr[i];
  if(c == 1) return params.abcdg[i];
  if(c == 2) return params.abcdb[i];
}

float curve_x(int c, int i)
{
  if(c == 0)
  {
    if(i == 0) return params.xr0.x;
    if(i == 1) return params.xr0.y;
    if(i == 2) return params.xr0.z;
    if(i == 3) return params.xr0.w;
    if(i == 4) return params.xr1.x;
    if(i == 5) return params.xr1.y;
    if(i == 6) return params.xr1.z;
    if(i == 7) return params.xr1.w;
  }
  else if(c == 1)
  {
    if(i == 0) return params.xg0.x;
    if(i == 1) return params.xg0.y;
    if(i == 2) return params.xg0.z;
    if(i == 3) return params.xg0.w;
    if(i == 4) return params.xg1.x;
    if(i == 5) return params.xg1.y;
    if(i == 6) return params.xg1.z;
    if(i == 7) return params.xg1.w;
  }
  else if(c == 2)
  {
    if(i == 0) return params.xb0.x;
    if(i == 1) return params.xb0.y;
    if(i == 2) return params.xb0.z;
    if(i == 3) return params.xb0.w;
    if(i == 4) return params.xb1.x;
    if(i == 5) return params.xb1.y;
    if(i == 6) return params.xb1.z;
    if(i == 7) return params.xb1.w;
  }
}

float curve_y(int c, int i)
{
  if(c == 0)
  {
    if(i == 0) return params.yr0.x;
    if(i == 1) return params.yr0.y;
    if(i == 2) return params.yr0.z;
    if(i == 3) return params.yr0.w;
    if(i == 4) return params.yr1.x;
    if(i == 5) return params.yr1.y;
    if(i == 6) return params.yr1.z;
    if(i == 7) return params.yr1.w;
  }
  else if(c == 1)
  {
    if(i == 0) return params.yg0.x;
    if(i == 1) return params.yg0.y;
    if(i == 2) return params.yg0.z;
    if(i == 3) return params.yg0.w;
    if(i == 4) return params.yg1.x;
    if(i == 5) return params.yg1.y;
    if(i == 6) return params.yg1.z;
    if(i == 7) return params.yg1.w;
  }
  else if(c == 2)
  {
    if(i == 0) return params.yb0.x;
    if(i == 1) return params.yb0.y;
    if(i == 2) return params.yb0.z;
    if(i == 3) return params.yb0.w;
    if(i == 4) return params.yb1.x;
    if(i == 5) return params.yb1.y;
    if(i == 6) return params.yb1.z;
    if(i == 7) return params.yb1.w;
  }
}

float curve_eval(int c, float x, out float ddx)
{ // cubic spline
  float x0 = curve_x(c, 0);
  float y0 = curve_y(c, 0);
  float x1, y1;
  const int cnt = c == 0 ? params.cntr : c == 1 ? params.cntg : params.cntb;
  ddx = c==0?params.ddr0:c==1?params.ddg0:params.ddb0;
  if(x <= x0) return y0 + (x-x0)*ddx;
  for(int i=0;i<cnt-1;i++)
  {
    x1 = curve_x(c, i+1);
    y1 = curve_y(c, i+1);
    if(x <= x1)
    {
      x -= x0;
      vec4 d = vec4(0, 1, 2*x, 3*x*x);
      vec4 p = vec4(1, x, x*x, x*x*x);
      vec4 cf = abcd(c, i);
      ddx = dot(cf, d);
      return dot(cf, p);
    }
    x0 = x1;
    y0 = y1;
  }
  ddx = c==0?params.ddrn:c==1?params.ddgn:params.ddbn;
  return y1 + (x-x1)*ddx;
}

#if 0 // linear
float curve_eval(float x, out float ddx)
{
  float x0 = curve_x(0);
  float y0 = curve_y(0);
  float x1, y1;
  if(x <= x0) return y0;
  for(int i=1;i<params.cnt;i++)
  {
    x1 = curve_x(i);
    y1 = curve_y(i);
    if(x <= x1)
    {
      ddx = (y1-y0)/(x1-x0);
      return mix(y0, y1, (x-x0)/(x1-x0));
    }
    x0 = x1;
    y0 = y1;
  }
  return y1;
}
#endif
