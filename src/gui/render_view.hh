#pragma once
// declare the view-specific render functions
#include "imgui.h"
#include <math.h>

inline ImVec4 gamma(ImVec4 in)
{
  // TODO: these values will be interpreted as rec2020 coordinates. do we want that?
  // theme colours are given as float sRGB values in imgui, while we will
  // draw them in linear:
  return ImVec4(pow(in.x, 2.2), pow(in.y, 2.2), pow(in.z, 2.2), in.w);
}

void render_files();
void render_lighttable();
void render_darkroom();
void render_darkroom_cleanup();
