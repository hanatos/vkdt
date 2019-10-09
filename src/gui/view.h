#pragma once

#include "gui/gui.h"
#include "gui/darkroom.h"

static inline int
dt_view_switch(dt_gui_view_t view)
{
  int err = 0;
  switch(vkdt.view_mode)
  {
    case s_view_darkroom:
      err = darkroom_leave();
      break;
    default:;
  }
  if(err) return err;
  dt_gui_view_t old_view = vkdt.view_mode;
  vkdt.view_mode = view;
  switch(vkdt.view_mode)
  {
    case s_view_darkroom:
      err = darkroom_enter();
      break;
    default:;
  }
  // TODO: reshuffle this stuff so we don't have to re-enter the old view?
  if(err)
  {
    vkdt.view_mode = old_view;
    return err;
  }
  return 0;
}
