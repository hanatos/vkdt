#pragma once

static inline void
lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
  {
    if(key == GLFW_KEY_E)
    {
      if(vkdt.db.current_image != -1u)
        dt_view_switch(s_view_darkroom);
    }
    else if(key == GLFW_KEY_1)
    { // TODO: or work on selection?
      if(vkdt.db.current_image != -1u)
        vkdt.db.image[vkdt.db.current_image].rating = 1;
    }
    else if(key == GLFW_KEY_2)
    { // TODO: or work on selection?
      if(vkdt.db.current_image != -1u)
        vkdt.db.image[vkdt.db.current_image].rating = 2;
    }
    else if(key == GLFW_KEY_3)
    { // TODO: or work on selection?
      if(vkdt.db.current_image != -1u)
        vkdt.db.image[vkdt.db.current_image].rating = 3;
    }
    else if(key == GLFW_KEY_4)
    { // TODO: or work on selection?
      if(vkdt.db.current_image != -1u)
        vkdt.db.image[vkdt.db.current_image].rating = 4;
    }
    else if(key == GLFW_KEY_5)
    { // TODO: or work on selection?
      if(vkdt.db.current_image != -1u)
        vkdt.db.image[vkdt.db.current_image].rating = 5;
    }
    else if(key == GLFW_KEY_0)
    { // TODO: or work on selection?
      if(vkdt.db.current_image != -1u)
        vkdt.db.image[vkdt.db.current_image].rating = 0;
    }
  }
}

static inline void
lighttable_mouse_button(GLFWwindow* window, int button, int action, int mods) {}

static inline void
lighttable_mouse_position(GLFWwindow* window, double x, double y) {}
