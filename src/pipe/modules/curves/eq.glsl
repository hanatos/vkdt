float eq_unpack4(vec4 f, int l)
{
  if(l == 0)     return f.x;
  if(l == 1)     return f.y;
  if(l == 2)     return f.z;
  /*if(l == 3)*/ return f.w;
}

float eq_unpack(int c, int v)
{
  int l = v & 3;
  int h = v / 4;
  if(c == 1) return eq_unpack4(params.vtx1[h], l);
  if(c == 2) return eq_unpack4(params.vtx2[h], l);
  if(c == 3) return eq_unpack4(params.vtx3[h], l);
  if(c == 4) return eq_unpack4(params.vtx4[h], l);
  if(c == 5) return eq_unpack4(params.vtx5[h], l);
  if(c == 6) return eq_unpack4(params.vtx6[h], l);
  if(c == 7) return eq_unpack4(params.vtx7[h], l);
  /*c == 8*/ return eq_unpack4(params.vtx8[h], l);
}

float eq_x(int c, int i)
{
  return eq_unpack(c, i);
}

float eq_y(int c, int i)
{
  return eq_unpack(c, 6+i);
}
