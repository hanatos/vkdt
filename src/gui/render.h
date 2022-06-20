#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

// initialise imgui. has to go after glfw and vulkan surface have been inited.
// will steal whatever is needed from global qvk struct.
int dt_gui_init_imgui();

// tear down imgui and free resources
void dt_gui_cleanup_imgui();

// poll imgui events from given event
// return non-zero if event should not be passed on
int dt_gui_poll_event_imgui(GLFWwindow *w, int key, int scancode, int action, int mods);

void dt_gui_imgui_keyboard(GLFWwindow *w, int key, int scancade, int action, int mods);
void dt_gui_imgui_mouse_button(GLFWwindow *w, int button, int action, int mods);
void dt_gui_imgui_mouse_position(GLFWwindow *w, double x, double y);
void dt_gui_imgui_character(GLFWwindow *w, int c);
void dt_gui_imgui_scrolled(GLFWwindow *w, double xoff, double yoff);
void dt_gui_imgui_window_position(GLFWwindow *w, int x, int y);

int dt_gui_imgui_want_mouse();
int dt_gui_imgui_want_keyboard();
int dt_gui_imgui_want_text();


void dt_gui_record_command_buffer_imgui(VkCommandBuffer cmd_buf);
// time-throttled access to nav input (keyboard/gamepad navigation)
float dt_gui_imgui_nav_input(int which);
float dt_gui_imgui_nav_button(int which);

#ifdef __cplusplus
struct ImFont;
ImFont *dt_gui_imgui_get_font(int which);
}
#endif
