#pragma once
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// initialise imgui. has to go after sdl and vulkan surface have been inited.
// will steal whatever is needed from global qvk struct.
int dt_gui_init_imgui();

// tear down imgui and free resources
void dt_gui_cleanup_imgui();

// poll imgui events from given event
// return non-zero if event should not be passed on
int dt_gui_poll_event_imgui(SDL_Event *event);

// render imgui frame
void dt_gui_render_frame_imgui();

void dt_gui_record_command_buffer_imgui(VkCommandBuffer cmd_buf);

#ifdef __cplusplus
}
#endif
