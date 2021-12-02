#pragma once
#include "gui/gui.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// api functions for gui interactions, c portion.
// the c++ header api.hh includes this one too, but this here
// can be called from the non-imgui parts of the code.

static inline void
dt_gui_dr_zoom()
{ // zoom 1:1
  // where does the mouse look in the current image?
  double x, y;
  glfwGetCursorPos(qvk.window, &x, &y);
  if(vkdt.wstate.m_x > 0 && vkdt.state.scale > 0.0f)
  { // maybe not needed? update as if mouse cursor moved (for gamepad nav)
    int dx = x - vkdt.wstate.m_x;
    int dy = y - vkdt.wstate.m_y;
    vkdt.state.look_at_x = vkdt.wstate.old_look_x - dx / vkdt.state.scale;
    vkdt.state.look_at_y = vkdt.wstate.old_look_y - dy / vkdt.state.scale;
    vkdt.state.look_at_x = CLAMP(vkdt.state.look_at_x, 0.0f, vkdt.wstate.wd);
    vkdt.state.look_at_y = CLAMP(vkdt.state.look_at_y, 0.0f, vkdt.wstate.ht);
  }
  float imwd = vkdt.state.center_wd, imht = vkdt.state.center_ht;
  float scale = vkdt.state.scale <= 0.0f ? MIN(imwd/vkdt.wstate.wd, imht/vkdt.wstate.ht) : vkdt.state.scale;
  float im_x = (x - (vkdt.state.center_x + imwd)/2.0f);
  float im_y = (y - (vkdt.state.center_y + imht)/2.0f);
  if(vkdt.state.scale <= 0.0f)
  {
    vkdt.state.scale = 1.0f;
    const float dscale = 1.0f/scale - 1.0f/vkdt.state.scale;
    vkdt.state.look_at_x += im_x  * dscale;
    vkdt.state.look_at_y += im_y  * dscale;
  }
  else if(vkdt.state.scale >= 8.0f || vkdt.state.scale < 1.0f)
  {
    vkdt.state.scale = -1.0f;
    vkdt.state.look_at_x = vkdt.wstate.wd/2.0f;
    vkdt.state.look_at_y = vkdt.wstate.ht/2.0f;
  }
  else if(vkdt.state.scale >= 1.0f)
  {
    vkdt.state.scale *= 2.0f;
    const float dscale = 1.0f/scale - 1.0f/vkdt.state.scale;
    vkdt.state.look_at_x += im_x  * dscale;
    vkdt.state.look_at_y += im_y  * dscale;
  }
}
