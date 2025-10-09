#pragma once
#define GLFW_INCLUDE_VULKAN
#include "gui/win.h"

void darkroom_mouse_button(GLFWwindow* window, int button, int action, int mods);
void darkroom_mouse_position(GLFWwindow* window, double x, double y);
void darkroom_mouse_scrolled(GLFWwindow* window, double xoff, double yoff);
int  darkroom_enter();
void darkroom_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
void darkroom_pentablet_data(double x, double y, double z, double pressure, double pitch, double yaw, double roll);
void darkroom_gamepad(GLFWwindow *window, GLFWgamepadstate *last, GLFWgamepadstate *curr);
void darkroom_process();
int  darkroom_leave();
