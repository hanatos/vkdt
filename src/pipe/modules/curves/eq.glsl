
float eq_unpack(int v)
{
  int l = v & 3;
  int h = v / 4;
  if(l == 0)     return params.vtx[h].x;
  if(l == 1)     return params.vtx[h].y;
  if(l == 2)     return params.vtx[h].z;
  /*if(l == 3)*/ return params.vtx[h].w;
}

float eq_x(int c, int i)
{
  return eq_unpack(6*c+i);
}

float eq_y(int c, int i)
{
  return eq_unpack(36+6*c+i);
}

