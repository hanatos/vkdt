#pragma once
#include "gui/render.h"

int files_enter();
int files_leave();
void files_mouse_button(GLFWwindow *window, int button, int action, int mods);
void files_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
void files_mouse_scrolled(GLFWwindow *window, double xoff, double yoff);
