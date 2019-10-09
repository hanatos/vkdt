#pragma once

static inline void
lighttable_handle_event(SDL_Event *event)
{
  if (event->type == SDL_KEYDOWN)
  {
    // TODO: configurable key bindings
    if(event->key.keysym.sym == SDLK_PERIOD)
    {
      // TODO get selected image from hover?
      // dt_view_switch(s_view_darkroom);
    }
  }
}
