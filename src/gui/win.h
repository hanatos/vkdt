// wrapper for windowing io stuff (i.e. glfw)
#pragma once

#ifdef __ANDROID__
#include <vulkan/vulkan.h>
#include "android_native_app_glue.h"
#include <android/keycodes.h>
#include <android/log.h>

typedef struct GLFWwindow GLFWwindow;

/* printable keys */
#define GLFW_KEY_SPACE              AKEYCODE_SPACE
#define GLFW_KEY_APOSTROPHE         AKEYCODE_APOSTROPHE
#define GLFW_KEY_COMMA              AKEYCODE_COMMA
#define GLFW_KEY_MINUS              AKEYCODE_MINUS
#define GLFW_KEY_PERIOD             AKEYCODE_PERIOD
#define GLFW_KEY_SLASH              AKEYCODE_SLASH
#define GLFW_KEY_0                  AKEYCODE_0
#define GLFW_KEY_1                  AKEYCODE_1
#define GLFW_KEY_2                  AKEYCODE_2
#define GLFW_KEY_3                  AKEYCODE_3
#define GLFW_KEY_4                  AKEYCODE_4
#define GLFW_KEY_5                  AKEYCODE_5
#define GLFW_KEY_6                  AKEYCODE_6
#define GLFW_KEY_7                  AKEYCODE_7
#define GLFW_KEY_8                  AKEYCODE_8
#define GLFW_KEY_9                  AKEYCODE_9
#define GLFW_KEY_SEMICOLON          AKEYCODE_SEMICOLON
#define GLFW_KEY_EQUAL              AKEYCODE_EQUALS
#define GLFW_KEY_A                  AKEYCODE_A
#define GLFW_KEY_B                  AKEYCODE_B
#define GLFW_KEY_C                  AKEYCODE_C
#define GLFW_KEY_D                  AKEYCODE_D
#define GLFW_KEY_E                  AKEYCODE_E
#define GLFW_KEY_F                  AKEYCODE_F
#define GLFW_KEY_G                  AKEYCODE_G
#define GLFW_KEY_H                  AKEYCODE_H
#define GLFW_KEY_I                  AKEYCODE_I
#define GLFW_KEY_J                  AKEYCODE_J
#define GLFW_KEY_K                  AKEYCODE_K
#define GLFW_KEY_L                  AKEYCODE_L
#define GLFW_KEY_M                  AKEYCODE_M
#define GLFW_KEY_N                  AKEYCODE_N
#define GLFW_KEY_O                  AKEYCODE_O
#define GLFW_KEY_P                  AKEYCODE_P
#define GLFW_KEY_Q                  AKEYCODE_Q
#define GLFW_KEY_R                  AKEYCODE_R
#define GLFW_KEY_S                  AKEYCODE_S
#define GLFW_KEY_T                  AKEYCODE_T
#define GLFW_KEY_U                  AKEYCODE_U
#define GLFW_KEY_V                  AKEYCODE_V
#define GLFW_KEY_W                  AKEYCODE_W
#define GLFW_KEY_X                  AKEYCODE_X
#define GLFW_KEY_Y                  AKEYCODE_Y
#define GLFW_KEY_Z                  AKEYCODE_Z
#define GLFW_KEY_LEFT_BRACKET       AKEYCODE_LEFT_BRACKET
#define GLFW_KEY_BACKSLASH          AKEYCODE_BACKSLASH
#define GLFW_KEY_RIGHT_BRACKET      AKEYCODE_RIGHT_BRACKET
#define GLFW_KEY_GRAVE_ACCENT       AKEYCODE_GRAVE
// uhm yeah whatever
// #define GLFW_KEY_WORLD_1            161 /* non-US #1 */
// #define GLFW_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define GLFW_KEY_ESCAPE             AKEYCODE_ESCAPE
#define GLFW_KEY_ENTER              AKEYCODE_ENTER
#define GLFW_KEY_TAB                AKEYCODE_TAB
#define GLFW_KEY_BACKSPACE          AKEYCODE_DEL
#define GLFW_KEY_INSERT             AKEYCODE_INSERT
#define GLFW_KEY_DELETE             AKEYCODE_FORWARD_DEL
#define GLFW_KEY_RIGHT              0 // ??
#define GLFW_KEY_LEFT               1 // ??
#define GLFW_KEY_DOWN               2 // ??
#define GLFW_KEY_UP                 3 // ??
#define GLFW_KEY_PAGE_UP            AKEYCODE_PAGE_UP
#define GLFW_KEY_PAGE_DOWN          AKEYCODE_PAGE_DOWN
#define GLFW_KEY_HOME               AKEYCODE_MOVE_HOME
#define GLFW_KEY_END                AKEYCODE_MOVE_END
#define GLFW_KEY_CAPS_LOCK          AKEYCODE_CAPS_LOCK
#define GLFW_KEY_SCROLL_LOCK        AKEYCODE_SCROLL_LOCK
#define GLFW_KEY_NUM_LOCK           AKEYCODE_NUM_LOCK
#define GLFW_KEY_PRINT_SCREEN       AKEYCODE_SYSRQ
#define GLFW_KEY_PAUSE              AKEYCODE_BREAK
#define GLFW_KEY_F1                 AKEYCODE_F1
#define GLFW_KEY_F2                 AKEYCODE_F2
#define GLFW_KEY_F3                 AKEYCODE_F3
#define GLFW_KEY_F4                 AKEYCODE_F4
#define GLFW_KEY_F5                 AKEYCODE_F5
#define GLFW_KEY_F6                 AKEYCODE_F6
#define GLFW_KEY_F7                 AKEYCODE_F7
#define GLFW_KEY_F8                 AKEYCODE_F8
#define GLFW_KEY_F9                 AKEYCODE_F9
#define GLFW_KEY_F10                AKEYCODE_F10
#define GLFW_KEY_F11                AKEYCODE_F11
#define GLFW_KEY_F12                AKEYCODE_F12
#define GLFW_KEY_KP_0               AKEYCODE_NUMPAD_0
#define GLFW_KEY_KP_1               AKEYCODE_NUMPAD_1
#define GLFW_KEY_KP_2               AKEYCODE_NUMPAD_2
#define GLFW_KEY_KP_3               AKEYCODE_NUMPAD_3
#define GLFW_KEY_KP_4               AKEYCODE_NUMPAD_4
#define GLFW_KEY_KP_5               AKEYCODE_NUMPAD_5
#define GLFW_KEY_KP_6               AKEYCODE_NUMPAD_6
#define GLFW_KEY_KP_7               AKEYCODE_NUMPAD_7
#define GLFW_KEY_KP_8               AKEYCODE_NUMPAD_8
#define GLFW_KEY_KP_9               AKEYCODE_NUMPAD_9
#define GLFW_KEY_KP_DECIMAL         AKEYCODE_NUMPAD_DOT
#define GLFW_KEY_KP_DIVIDE          AKEYCODE_NUMPAD_DIVIDE
#define GLFW_KEY_KP_MULTIPLY        AKEYCODE_NUMPAD_MULTIPLY
#define GLFW_KEY_KP_SUBTRACT        AKEYCODE_NUMPAD_SUBTRACT
#define GLFW_KEY_KP_ADD             AKEYCODE_NUMPAD_ADD
#define GLFW_KEY_KP_ENTER           AKEYCODE_NUMPAD_ENTER
#define GLFW_KEY_KP_EQUAL           AKEYCODE_NUMPAD_EQUALS
#define GLFW_KEY_LEFT_SHIFT         AKEYCODE_SHIFT_LEFT
#define GLFW_KEY_LEFT_CONTROL       AKEYCODE_CTRL_LEFT
#define GLFW_KEY_LEFT_ALT           AKEYCODE_ALT_LEFT
#define GLFW_KEY_LEFT_SUPER         AKEYCODE_META_LEFT
#define GLFW_KEY_RIGHT_SHIFT        AKEYCODE_SHIFT_RIGHT
#define GLFW_KEY_RIGHT_CONTROL      AKEYCODE_CTRL_RIGHT
#define GLFW_KEY_RIGHT_ALT          AKEYCODE_ALT_RIGHT
#define GLFW_KEY_RIGHT_SUPER        AKEYCODE_META_RIGHT
#define GLFW_KEY_MENU               AKEYCODE_MENU

#define GLFW_KEY_LAST               316


#define GLFW_RELEASE 0
#define GLFW_PRESS 1
#define GLFW_REPEAT 2

#define GLFW_GAMEPAD_BUTTON_A               0
#define GLFW_GAMEPAD_BUTTON_B               1
#define GLFW_GAMEPAD_BUTTON_X               2
#define GLFW_GAMEPAD_BUTTON_Y               3
#define GLFW_GAMEPAD_BUTTON_LEFT_BUMPER     4
#define GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER    5
#define GLFW_GAMEPAD_BUTTON_BACK            6
#define GLFW_GAMEPAD_BUTTON_START           7
#define GLFW_GAMEPAD_BUTTON_GUIDE           8
#define GLFW_GAMEPAD_BUTTON_LEFT_THUMB      9
#define GLFW_GAMEPAD_BUTTON_RIGHT_THUMB     10
#define GLFW_GAMEPAD_BUTTON_DPAD_UP         11
#define GLFW_GAMEPAD_BUTTON_DPAD_RIGHT      12
#define GLFW_GAMEPAD_BUTTON_DPAD_DOWN       13
#define GLFW_GAMEPAD_BUTTON_DPAD_LEFT       14
#define GLFW_GAMEPAD_BUTTON_LAST            GLFW_GAMEPAD_BUTTON_DPAD_LEFT

#define GLFW_GAMEPAD_BUTTON_CROSS       GLFW_GAMEPAD_BUTTON_A
#define GLFW_GAMEPAD_BUTTON_CIRCLE      GLFW_GAMEPAD_BUTTON_B
#define GLFW_GAMEPAD_BUTTON_SQUARE      GLFW_GAMEPAD_BUTTON_X
#define GLFW_GAMEPAD_BUTTON_TRIANGLE    GLFW_GAMEPAD_BUTTON_Y

#define GLFW_GAMEPAD_AXIS_LEFT_X        0
#define GLFW_GAMEPAD_AXIS_LEFT_Y        1
#define GLFW_GAMEPAD_AXIS_RIGHT_X       2
#define GLFW_GAMEPAD_AXIS_RIGHT_Y       3
#define GLFW_GAMEPAD_AXIS_LEFT_TRIGGER  4
#define GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER 5
#define GLFW_GAMEPAD_AXIS_LAST          GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER
typedef struct GLFWgamepadstate
{
  uint8_t buttons[15];
  float   axes[6];
}
GLFWgamepadstate;

#define GLFW_MOD_SHIFT           0x0001
#define GLFW_MOD_CONTROL         0x0002
#define GLFW_MOD_ALT             0x0004
#define GLFW_MOD_SUPER           0x0008
#define GLFW_MOUSE_BUTTON_1         0
#define GLFW_MOUSE_BUTTON_2         1
#define GLFW_MOUSE_BUTTON_3         2
#define GLFW_MOUSE_BUTTON_4         3
#define GLFW_MOUSE_BUTTON_5         4
#define GLFW_MOUSE_BUTTON_6         5
#define GLFW_MOUSE_BUTTON_7         6
#define GLFW_MOUSE_BUTTON_8         7
#define GLFW_MOUSE_BUTTON_LAST      GLFW_MOUSE_BUTTON_8
#define GLFW_MOUSE_BUTTON_LEFT      GLFW_MOUSE_BUTTON_1
#define GLFW_MOUSE_BUTTON_RIGHT     GLFW_MOUSE_BUTTON_2
#define GLFW_MOUSE_BUTTON_MIDDLE    GLFW_MOUSE_BUTTON_3

#define GLFW_CURSOR                 0x00033001
#define GLFW_CURSOR_NORMAL          0x00034001
#define GLFW_CURSOR_HIDDEN          0x00034002
#define GLFW_CURSOR_DISABLED        0x00034003
#define GLFW_CURSOR_CAPTURED        0x00034004

#define GLFW_TRUE 1
#define GLFW_FALSE 0

static inline void
glfwGetCursorPos(GLFWwindow *w, double *x, double *y)
{
  // XXX grab from cached values
}
#define glfwGetTime dt_time
static inline void glfwDestroyWindow(GLFWwindow *w) {}
static inline int glfwWindowShouldClose(GLFWwindow *w)
{
  struct android_app *app = (struct android_app*)w;
  if(app) return app->destroyRequested;
  return 0;
}
static inline void glfwSetWindowShouldClose(GLFWwindow *w, int value)
{
  struct android_app *app = (struct android_app*)w;
  if(app && value) ANativeActivity_finish(app->activity);
} 
static inline const char *glfwGetClipboardString(GLFWwindow *w)
{
  return 0;
}
static inline void glfwSetClipboardString(GLFWwindow *w, const char *s) {}
static inline void
glfwGetFramebufferSize(GLFWwindow *w, int *wd, int *ht)
{
  // TODO
}
static inline void glfwSetInputMode(GLFWwindow *w, int cursor, int mode) {}
static inline int glfwGetMouseButton(GLFWwindow *w, int button)
{
  return 0; // TODO grab from cached array
}
void glfwPostEmptyEvent();

extern uint8_t g_keystate[GLFW_KEY_LAST+1];
static inline int
glfwGetKey(GLFWwindow *w, int key)
{
  if(key < 0 || key > GLFW_KEY_LAST) return 0;
  return g_keystate[key];
}
static inline void glfwTerminate() {}
static inline int glfwGetGamepadState(int joystick_id, GLFWgamepadstate *s)
{
  // TODO copy into s
  return 0; // XXX return > 0 if success!
}
#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif
