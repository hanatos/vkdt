#pragma once
#include "gui/gui.h"
#include "gui/darkroom.h"
#include "gui/darkroom-util.h"
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
  if(vkdt.state.scale < 1.0f)
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

static inline void
dt_gui_dr_next()
{
  if(vkdt.graph_dev.frame_cnt != 1)
    vkdt.state.anim_playing ^= 1; // start/stop playing animation
  else
  { // advance to next image in lighttable collection
    uint32_t next = dt_db_current_colid(&vkdt.db) + 1;
    if(next < vkdt.db.collection_cnt)
    {
      int err;
      err = darkroom_leave(); // writes back thumbnails. maybe there'd be a cheaper way to invalidate.
      if(err) return;
      dt_db_selection_clear(&vkdt.db);
      dt_db_selection_add(&vkdt.db, next);
      err = darkroom_enter();
      if(err)
      { // roll back
        dt_db_selection_clear(&vkdt.db);
        dt_db_selection_add(&vkdt.db, next-1);
        darkroom_enter(); // hope they take us back
      }
    }
  }
}

static inline void
dt_gui_dr_prev()
{
  if(vkdt.graph_dev.frame_cnt != 1)
  {
    vkdt.graph_dev.frame = vkdt.state.anim_frame = 0; // reset to beginning
    vkdt.state.anim_no_keyframes = 0;  // (re-)enable keyframes
  }
  else
  { // backtrack to last image in lighttable collection
    int32_t next = dt_db_current_colid(&vkdt.db) - 1;
    if(next >= 0)
    {
      int err;
      err = darkroom_leave(); // writes back thumbnails. maybe there'd be a cheaper way to invalidate.
      if(err) return;
      dt_db_selection_clear(&vkdt.db);
      dt_db_selection_add(&vkdt.db, next);
      err = darkroom_enter();
      if(err)
      { // roll back
        dt_db_selection_clear(&vkdt.db);
        dt_db_selection_add(&vkdt.db, next+1);
        darkroom_enter(); // hope they take us back
      }
    }
  }
}

// assumes the crop start button has been pressed and we're in crop mode in the ui
static inline void
dt_gui_dr_crop_adjust(
    float edge,   // value to set vkdt.wtstate.selected edge to
    int   inc)    // if nonzero, increment edge instead of setting
{
  if(vkdt.wstate.selected >= 0 && vkdt.wstate.selected < 4)
  {
    if(inc) vkdt.wstate.state[vkdt.wstate.selected] += edge;
    else    vkdt.wstate.state[vkdt.wstate.selected]  = edge;
    if(vkdt.wstate.aspect > 0.0f)
    { // fix up aspect ratio
      float target_aspect = vkdt.wstate.aspect;
      if(vkdt.wstate.selected == 0)
      { // left, move right side along:
        vkdt.wstate.state[1] = vkdt.wstate.state[0] + target_aspect * (vkdt.wstate.state[3]-vkdt.wstate.state[2]);
      }
      else if(vkdt.wstate.selected == 2)
      { // top, move bottom side along:
        vkdt.wstate.state[3] = vkdt.wstate.state[2] + 1.0f/target_aspect * (vkdt.wstate.state[1]-vkdt.wstate.state[0]);
      }
      else if(vkdt.wstate.selected == 1)
      { // right, move top and bottom simultaneously to keep center:
        float c = (vkdt.wstate.state[3] + vkdt.wstate.state[2])*0.5f;
        float w =  vkdt.wstate.state[1] - vkdt.wstate.state[0];
        vkdt.wstate.state[3] = c + 0.5f / target_aspect * w;
        vkdt.wstate.state[2] = c - 0.5f / target_aspect * w;
      }
      else if(vkdt.wstate.selected == 3)
      { // bottom, move left and right simultaneously to keep center:
        float c = (vkdt.wstate.state[1] + vkdt.wstate.state[0])*0.5f;
        float w =  vkdt.wstate.state[3] - vkdt.wstate.state[2];
        vkdt.wstate.state[1] = c + 0.5f * target_aspect * w;
        vkdt.wstate.state[0] = c - 0.5f * target_aspect * w;
      }
    }
    // make sure min <= max
    if(vkdt.wstate.state[1] < vkdt.wstate.state[0]) { float tmp = vkdt.wstate.state[1]; vkdt.wstate.state[1] = vkdt.wstate.state[0]; vkdt.wstate.state[0] = tmp; }
    if(vkdt.wstate.state[3] < vkdt.wstate.state[2]) { float tmp = vkdt.wstate.state[3]; vkdt.wstate.state[3] = vkdt.wstate.state[2]; vkdt.wstate.state[2] = tmp; }
  }
}

static inline void
dt_gui_dr_pers_adjust(
    float *n,   // corner coordinates in image space
    int    inc) // if set increment instead of set
{
  if(vkdt.wstate.selected < 0 || vkdt.wstate.selected >= 4) return;
  // copy to quad state at corner c
  if(inc)
  {
    vkdt.wstate.state[2*vkdt.wstate.selected+0] += n[0];
    vkdt.wstate.state[2*vkdt.wstate.selected+1] += n[1];
  }
  else
  {
    vkdt.wstate.state[2*vkdt.wstate.selected+0] = n[0];
    vkdt.wstate.state[2*vkdt.wstate.selected+1] = n[1];
  }
}

static inline void
dt_gui_dr_toggle_fullscreen_view()
{
  vkdt.wstate.fullscreen_view ^= 1;
  if(vkdt.wstate.fullscreen_view)
  {
    vkdt.state.center_x = 0;
    vkdt.state.center_y = 0;
    vkdt.state.center_wd = qvk.win_width;
    vkdt.state.center_ht = qvk.win_height;
    darkroom_reset_zoom();
  }
  else
  {
    vkdt.state.center_x = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_y = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_wd = qvk.win_width * (1.0f-2.0f*vkdt.style.border_frac) - vkdt.state.panel_wd;
    vkdt.state.center_ht = qvk.win_height - 2*vkdt.style.border_frac * qvk.win_width;
  }
}

static inline void
dt_gui_dr_toggle_history()
{
  vkdt.wstate.history_view ^= 1;
  if(vkdt.wstate.history_view)
  {
    vkdt.state.center_x = vkdt.style.border_frac * qvk.win_width + vkdt.state.panel_wd;
    vkdt.state.center_y = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_wd = qvk.win_width * (1.0f-2.0f*vkdt.style.border_frac) - 2*vkdt.state.panel_wd;
    vkdt.state.center_ht = qvk.win_height - 2*vkdt.style.border_frac * qvk.win_width;
  }
  else
  {
    vkdt.state.center_x = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_y = vkdt.style.border_frac * qvk.win_width;
    vkdt.state.center_wd = qvk.win_width * (1.0f-2.0f*vkdt.style.border_frac) - vkdt.state.panel_wd;
    vkdt.state.center_ht = qvk.win_height - 2*vkdt.style.border_frac * qvk.win_width;
  }
}

static inline void
dt_gui_dr_enter_fullscreen_view()
{
  if(!vkdt.wstate.fullscreen_view)
    dt_gui_dr_toggle_fullscreen_view();
}

static inline void
dt_gui_dr_leave_fullscreen_view()
{
  if(vkdt.wstate.fullscreen_view)
    dt_gui_dr_toggle_fullscreen_view();
}

static inline void
dt_gui_lt_duplicate()
{
  if(!vkdt.db.selection_cnt) return; // no images selected
  dt_db_duplicate_selected_images(&vkdt.db); // just create .cfg files, don't update db
  char dir[PATH_MAX]; // reload directory:
  snprintf(dir, sizeof(dir), "%s", vkdt.db.dirname);
  dt_gui_switch_collection(dir);
}
