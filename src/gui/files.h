#pragma once

static inline void
files_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
    if(key == GLFW_KEY_ESCAPE || key == GLFW_KEY_CAPS_LOCK)
      dt_view_switch(s_view_lighttable);
}
