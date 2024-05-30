#pragma once
int nodes_enter();
int nodes_leave();
void nodes_process();
void nodes_mouse_button(GLFWwindow *window, int button, int action, int mods);
void nodes_mouse_scrolled(GLFWwindow *window, double xoff, double yoff);
void nodes_keyboard(GLFWwindow *window, int key, int scancode, int action, int mods);
