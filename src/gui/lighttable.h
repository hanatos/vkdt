#pragma once

#include "gui/api.h"
#include "gui/render.h"

// implementation in render_lighttable.c, deals with hotkeys
void lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods);
void lighttable_mouse_scrolled(GLFWwindow* window, double xoff, double yoff);
void lighttable_mouse_button(GLFWwindow* window, int button, int action, int mods);

static inline void
lighttable_mouse_position(GLFWwindow* window, double x, double y) {}

static inline int
lighttable_enter()
{
  if(vkdt.wstate.history_view)    dt_gui_dr_toggle_history();
  if(vkdt.wstate.fullscreen_view) dt_gui_dr_toggle_fullscreen_view();
  dt_gamepadhelp_set(dt_gamepadhelp_ps,              "display this help");
  dt_gamepadhelp_set(dt_gamepadhelp_button_square,   "plus L1/R1: switch panel");
  dt_gamepadhelp_set(dt_gamepadhelp_button_circle,   "back to files");
  dt_gamepadhelp_set(dt_gamepadhelp_button_triangle, "hold + L1/R1: stars, +L2/R2: colour labels");
  dt_gamepadhelp_set(dt_gamepadhelp_button_cross,    "select highlighted image (twice to enter darkroom)");
  dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_L,  "scroll view");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_up,        "highlight entry one up");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_down,      "highlight entry one down");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_left,      "highlight entry one left");
  dt_gamepadhelp_set(dt_gamepadhelp_arrow_right,     "highlight entry one right");
  vkdt.wstate.copied_imgid = -1u; // reset to invalid
  return 0;
}

int lighttable_leave();
