// wrapper for windowing io stuff (i.e. glfw)
#pragma once

#ifdef __ANDROID__

#include <android/log.h>
#include <vulkan_wrapper.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

typedef struct GLFWwindow GLFWwindow;

// TODO MOUSE_BUTTON_ and GLFW_MOD_


extern uint8_t *glfw_keystate;

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
// #define GLFW_KEY_RIGHT              ??
// #define GLFW_KEY_LEFT               ??
// #define GLFW_KEY_DOWN               ??
// #define GLFW_KEY_UP                 ??
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
#define GLFW_KEY_F13                AKEYCODE_F13
#define GLFW_KEY_F14                AKEYCODE_F14
#define GLFW_KEY_F15                AKEYCODE_F15
#define GLFW_KEY_F16                AKEYCODE_F16
#define GLFW_KEY_F17                AKEYCODE_F17
#define GLFW_KEY_F18                AKEYCODE_F18
#define GLFW_KEY_F19                AKEYCODE_F19
#define GLFW_KEY_F20                AKEYCODE_F20
#define GLFW_KEY_F21                AKEYCODE_F21
#define GLFW_KEY_F22                AKEYCODE_F22
#define GLFW_KEY_F23                AKEYCODE_F23
#define GLFW_KEY_F24                AKEYCODE_F24
#define GLFW_KEY_F25                AKEYCODE_F25
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

#define GLFW_KEY_LAST               AKEYCODE_F24


#define GLFW_RELEASE 0
#define GLFW_PRESS 1
#define GLFW_REPEAT 2
static inline int
glfwGetKey(void *, int key)
{
  if(key < 0 || key > GLFW_KEY_LAST) return 0;
  return glfw_keystate[key];
}

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

// TODO
void glfwGetCursorPos(void*, double *x, double *y)
{
  // XXX grab from cached values
}
#define glfwGetTime dt_time

#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif
