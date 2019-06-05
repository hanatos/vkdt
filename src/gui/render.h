#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// initialise imgui. has to go after sdl and vulkan surface have been inited.
// will steal whatever is needed from global qvk struct.
int dt_gui_init_imgui();

// tear down imgui and free resources
void dt_gui_cleanup_imgui();

// poll imgui events from given event
void dt_gui_poll_event(SDL_Event *event);

// render imgui frame
void dt_gui_render_frame();

#ifdef __cplusplus
}
#endif
