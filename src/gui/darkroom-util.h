#pragma once

#include <float.h>

static inline void
dt_view_to_image(
    const float v[2],
    float       img[2])
{
  dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
  assert(out);
  float wd  = (float)out->connector[0].roi.wd;
  float ht  = (float)out->connector[0].roi.ht;
  float fwd = (float)out->connector[0].roi.full_wd/out->connector[0].roi.scale;
  float fht = (float)out->connector[0].roi.full_ht/out->connector[0].roi.scale;
  float imwd = vkdt.state.center_wd, imht = vkdt.state.center_ht;
  float scale = MIN(imwd/wd, imht/ht);
  if(vkdt.state.scale > 0.0f) scale = vkdt.state.scale;
  float cvx = vkdt.state.center_wd *.5f;
  float cvy = vkdt.state.center_ht *.5f;
  if(vkdt.state.look_at_x == FLT_MAX) vkdt.state.look_at_x = wd/2.0f;
  if(vkdt.state.look_at_y == FLT_MAX) vkdt.state.look_at_y = ht/2.0f;
  float ox = cvx - scale * vkdt.state.look_at_x;
  float oy = cvy - scale * vkdt.state.look_at_y;
  float x = ox + vkdt.state.center_x, y = oy + vkdt.state.center_y;
  img[0] = (v[0] - x) / (scale * fwd);
  img[1] = (v[1] - y) / (scale * fht);
}

// convert normalised image coordinates to pixel coord on screen
static inline void
dt_image_to_view(
    const float img[2], // image pixel coordinate in [0,1]^2
    float       v[2])   // window pixel coordinate
{
  dt_node_t *out = dt_graph_get_display(&vkdt.graph_dev, dt_token("main"));
  assert(out);
  float wd  = (float)out->connector[0].roi.wd;
  float ht  = (float)out->connector[0].roi.ht;
  float fwd = (float)out->connector[0].roi.full_wd/out->connector[0].roi.scale;
  float fht = (float)out->connector[0].roi.full_ht/out->connector[0].roi.scale;
  float imwd = vkdt.state.center_wd, imht = vkdt.state.center_ht;
  float scale = MIN(imwd/wd, imht/ht);
  if(vkdt.state.scale > 0.0f) scale = vkdt.state.scale;
  float cvx = vkdt.state.center_wd *.5f;
  float cvy = vkdt.state.center_ht *.5f;
  if(vkdt.state.look_at_x == FLT_MAX) vkdt.state.look_at_x = wd/2.0f;
  if(vkdt.state.look_at_y == FLT_MAX) vkdt.state.look_at_y = ht/2.0f;
  float ox = cvx - scale * vkdt.state.look_at_x;
  float oy = cvy - scale * vkdt.state.look_at_y;
  float x = ox + vkdt.state.center_x, y = oy + vkdt.state.center_y;
  v[0] = x + scale * img[0] * fwd;
  v[1] = y + scale * img[1] * fht;
}
