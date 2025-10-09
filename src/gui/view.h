#pragma once

#include "gui/gui.h"
#include "gui/win.h"

int dt_view_switch(dt_gui_view_t view);

void dt_view_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
void dt_view_mouse_button(GLFWwindow *window, int button, int action, int mods);
void dt_view_mouse_position(GLFWwindow *window, double x, double y);
void dt_view_mouse_scrolled(GLFWwindow *window, double xoff, double yoff);
void dt_view_pentablet_data(double x, double y, double z, double pressure, double pitch, double yaw, double roll);
void dt_view_pentablet_proximity(int enter);
void dt_view_gamepad(GLFWwindow *window, GLFWgamepadstate *last, GLFWgamepadstate *curr);
void dt_view_process();
void dt_view_get_cursor_pos(GLFWwindow *window, double *x, double *y);
