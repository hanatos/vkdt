#pragma once
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
// #define NK_BUTTON_TRIGGER_ON_RELEASE
// unfortunately the builtin-nk function triggers infinite loops at times:
#define NK_DTOA(S, D) sprintf(S, "%g", D)
#include "nuklear.h"
#include "nuklear_glfw_vulkan.h"

// define groups of widgets that can be switched by pressing "tab"
#define nk_focus_group_property(TYPE, CTX, NAME, m, V, M, I1, I2)\
  do {\
    if(nk_focus_group_tab_keypress == 2) { nk_property_focus(CTX); nk_focus_group_tab_keypress = 0; }\
    nk_property_ ## TYPE(CTX, NAME, m, V, M, I1, I2);\
    int adv = nk_property_## TYPE ##_unfocus(CTX, NAME, m, V, M, I1, nk_focus_group_tab_keypress);\
    if(adv) { nk_focus_group_tab_keypress = 2; adv = 0; }\
  } while(0)
#define nk_focus_group_head() \
  static double nk_focus_group_time_tab; \
  static int nk_focus_group_tab_keypress = 0;\
  {\
  double nk_focus_group_time_now = glfwGetTime(); \
  if(glfwGetKey(vkdt.win.window, GLFW_KEY_TAB) == GLFW_PRESS && (nk_focus_group_time_now - nk_focus_group_time_tab > 0.2)) \
  { \
    nk_focus_group_time_tab = nk_focus_group_time_now; \
    nk_focus_group_tab_keypress = 1; \
  }}
