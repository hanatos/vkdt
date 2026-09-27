layout(std140, set = 0, binding = 1) uniform params_t
{
  vec4  click[16];  // count, then x y label per click
  vec4  box;        // x0 y0 x1 y1, empty if x1 <= x0
  uvec4 draw[2048]; // vertex count, then brush vertices as written by the draw widget
  float radius;     // brush radius relative to the image width
} params;

float clk(int i) { return params.click[i/4][i%4]; }
uint  drw(int i) { return params.draw[i/4][i%4]; }

bool has_box() { return params.box.z > params.box.x && params.box.w > params.box.y; }

int num_verts() { return clamp(int(drw(0)), 0, 4095); }

// brush vertex in input image coordinates with radius, opacity, hardness. false for end markers
bool vert(int v, out vec2 p, out vec3 s)
{
  const uint a = drw(1+2*v), b = drw(2+2*v);
  p = unpackUnorm2x16(a) * 2.0 - 0.5;
  s = vec3(2.0 * unpackUnorm2x16(b).x, unpackUnorm4x8(b).zw);
  return s.x > 0.0;
}

bool has_draw()
{
  for(int v=0;v<num_verts();v++) if((drw(2+2*v) & 0xffffu) != 0u) return true;
  return false;
}

// region the network sees: the box plus a margin, or the full frame
vec4 crop()
{
  if(!has_box()) return vec4(0.0, 0.0, 1.0, 1.0);
  const vec2 wd = min(vec2(1.0), 1.8*(params.box.zw - params.box.xy));
  const vec2 c0 = clamp(0.5*(params.box.xy + params.box.zw - wd), vec2(0.0), 1.0 - wd);
  return vec4(c0, c0 + wd);
}
