#pragma once

#include "darkroom-util.h"
#include "gui/api.h"

static inline void
lighttable_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
  {
    if(key == GLFW_KEY_E)
    {
      if(dt_db_current_imgid(&vkdt.db) != -1u)
      {
        dt_view_switch(s_view_darkroom);
      }
    }
#define RATE(X)\
    else if(key == GLFW_KEY_ ## X )\
    {\
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);\
      for(int i=0;i<vkdt.db.selection_cnt;i++)\
        vkdt.db.image[sel[i]].rating = X;\
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
      const uint32_t *sel = dt_db_selection_get(&vkdt.db);\
      for(int i=0;i<vkdt.db.selection_cnt;i++)\
        vkdt.db.image[sel[i]].labels ^= 1<<(X-1);\
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

static inline int
lighttable_enter()
{
  if(vkdt.wstate.history_view)    dt_gui_dr_toggle_history();
  if(vkdt.wstate.fullscreen_view) dt_gui_dr_toggle_fullscreen_view();
  return 0;
}
