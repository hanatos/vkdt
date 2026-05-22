// ssbo frame buffer access functions, assumes bref.fb is the 4-strided ssbo:f32 to be written/read

void fb_set(ivec2 px, ivec2 dim, vec4 L)
{
  uint idx = 4*(px.y * dim.x + px.x);
  bref.fb.v[idx+0] = L.r;
  bref.fb.v[idx+1] = L.g;
  bref.fb.v[idx+2] = L.b;
  bref.fb.v[idx+3] = L.a;
}

vec4 fb_get(ivec2 px, ivec2 dim)
{
  uint idx = 4*(px.y * dim.x + px.x);
  return vec4(bref.fb.v[idx+0], bref.fb.v[idx+1], bref.fb.v[idx+2], bref.fb.v[idx+3]);
}

void fb_add(ivec2 px, ivec2 dim, vec4 L)
{
  uint idx = 4*(px.y * dim.x + px.x);
  atomicAdd(bref.fb.v[idx+0], L.r);
  atomicAdd(bref.fb.v[idx+1], L.g);
  atomicAdd(bref.fb.v[idx+2], L.b);
  atomicAdd(bref.fb.v[idx+3], L.a);
}
