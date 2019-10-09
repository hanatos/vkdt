#pragma once

#include "gui/gui.h"

#include <SDL.h>

int dt_view_switch(dt_gui_view_t view);
void dt_view_handle_event(SDL_Event *event);
void dt_view_process();
