#pragma once

static inline void
lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS && key == GLFW_KEY_E)
  {
    if(vkdt.db.current_image >= 0)
      dt_view_switch(s_view_darkroom);
  }
}

static inline void
lighttable_mouse_button(GLFWwindow* window, int button, int action, int mods) {}

static inline void
lighttable_mouse_position(GLFWwindow* window, double x, double y) {}
