#pragma once

static inline void
dt_radial_menu(
    struct nk_context *ctx,
    float  cx,
    float  cy,
    float  radius)
{
  struct nk_command_buffer *cmd = &vkdt.global_cmd;
  int N = 40;
  float phi =  -M_PI, delta_phi = (2.0f*M_PI)/N,
        r0 = 0.3*radius, r1 = radius;
  const struct nk_vec2 pos = ctx->input.mouse.pos;
  float dx = pos.x-cx, dy = pos.y-cy;
  const float mphi = (dx*dx+dy*dy>r0*r0) ? atan2f(pos.y-cy, pos.x-cx) : -666;
  for(int k=0;k<N;k++)
  {
    float x[] = {
      cx + cosf(phi+0.013) * r1,           cy + sinf(phi+0.013) * r1,
      cx + cosf(phi+delta_phi-0.013) * r1, cy + sinf(phi+delta_phi-0.013) * r1,
      cx + cosf(phi+delta_phi-0.013) * r0, cy + sinf(phi+delta_phi-0.013) * r0,
      cx + cosf(phi+0.013) * r0,           cy + sinf(phi+0.013) * r0};
    struct nk_color c = vkdt.style.colour[NK_COLOR_BUTTON];
    if(mphi > phi && mphi < phi+delta_phi)
      c = vkdt.style.colour[NK_COLOR_DT_ACCENT_HOVER];
    nk_fill_polygon(cmd, x, 4, k==N ? nk_rgba(100,100,100,200) : c);
    phi += delta_phi;
  }
}
