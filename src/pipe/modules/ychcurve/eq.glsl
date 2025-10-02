
float eq_unpack(int v)
{
  int l = v & 3;
  int h = v / 4;
  if(l == 0)     return params.vtx[h].x;
  if(l == 1)     return params.vtx[h].y;
  if(l == 2)     return params.vtx[h].z;
  /*if(l == 3)*/ return params.vtx[h].w;
}

float eq_eval(int i, float x, out float ddx)
{
  int j = 0; // smallest index with x[j] <= x
  float x0 = eq_unpack(6*i+0);
  float x1 = eq_unpack(6*i+1);
  if(x >= x1) { x0 = x1; x1 = eq_unpack(6*i+2); j = 1;}
  if(x >= x1) { x0 = x1; x1 = eq_unpack(6*i+3); j = 2;}
  if(x >= x1) { x0 = x1; x1 = eq_unpack(6*i+4); j = 3;}
  if(x >= x1) { x0 = x1; x1 = eq_unpack(6*i+5); j = 4;}
  if(x >= x1) { x0 = x1; x1 = eq_unpack(6*i+0); j = 5;}
  float y0 = eq_unpack(36+6*i+j);
  float y1 = eq_unpack(36+6*i+((j+1)%6)); // modulo, useful for hue (luminance and chroma not so much)
  ddx = 1; // XXX TODO
  return mix(y0, y1, smoothstep(x0, x1, x));
}

float eq_x(int c, int i)
{
  return eq_unpack(6*c+i);
}

float eq_y(int c, int i)
{
  return eq_unpack(36+6*c+i);
}

