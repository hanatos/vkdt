#pragma once

static inline void
lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
  {
    if(key == GLFW_KEY_E)
    {
      if(dt_db_current_imgid(&vkdt.db) != -1u)
        dt_view_switch(s_view_darkroom);
    }
    // TODO: work on selection instead of current_image?
#define RATE(X)\
    else if(key == GLFW_KEY_ ## X )\
    {\
      if(dt_db_current_imgid(&vkdt.db) != -1u)\
        vkdt.db.image[dt_db_current_imgid(&vkdt.db)].rating = X;\
    }
    RATE(1)
    RATE(2)
    RATE(3)
    RATE(4)
    RATE(5)
    RATE(0)
#undef RATE
#define LABEL(X)\
    else if(key == GLFW_KEY_F ## X)\
    {\
      if(dt_db_current_imgid(&vkdt.db) != -1u)\
        vkdt.db.image[dt_db_current_imgid(&vkdt.db)].labels ^= 1<<(X-1);\
    }
    LABEL(1)
    LABEL(2)
    LABEL(3)
    LABEL(4)
    LABEL(5)
#undef LABEL
  }
}

static inline void
lighttable_mouse_button(GLFWwindow* window, int button, int action, int mods) {}

static inline void
lighttable_mouse_position(GLFWwindow* window, double x, double y) {}

static inline void
lighttable_mouse_scrolled(GLFWwindow* window, double xoff, double yoff) {}
