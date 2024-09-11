#pragma once

#include "gui/api.h"
#include "gui/render.h"

// implementation in render_lighttable.c, deals with hotkeys
void lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods);
void lighttable_mouse_scrolled(GLFWwindow* window, double xoff, double yoff);
void lighttable_mouse_button(GLFWwindow* window, int button, int action, int mods);
void lighttable_gamepad(GLFWwindow *window, GLFWgamepadstate *last, GLFWgamepadstate *curr);

static inline void
lighttable_mouse_position(GLFWwindow* window, double x, double y) {}

int lighttable_enter();
int lighttable_leave();
