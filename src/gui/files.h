#pragma once
#include "gui/render.h"

static inline int
files_enter()
{
  dt_gamepadhelp_set(dt_gamepadhelp_ps,              "toggle this help");
  // dt_gamepadhelp_set(dt_gamepadhelp_button_square,   "plus L1/R1: switch panel");
  // dt_gamepadhelp_set(dt_gamepadhelp_button_circle,   "back to lighttable without changing folders");
  // dt_gamepadhelp_set(dt_gamepadhelp_button_triangle, "enter lighttable for highlighted folder");
  // dt_gamepadhelp_set(dt_gamepadhelp_button_cross,    "navigate to highlighted folder");
  // dt_gamepadhelp_set(dt_gamepadhelp_analog_stick_L,  "scroll view");
  // dt_gamepadhelp_set(dt_gamepadhelp_arrow_up,        "select entry one up");
  // dt_gamepadhelp_set(dt_gamepadhelp_arrow_down,      "select entry one down");
  return 0;
}

static inline int
files_leave()
{
  dt_gamepadhelp_clear();
  return 0;
}

void files_mouse_button(GLFWwindow *window, int button, int action, int mods);
void files_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
