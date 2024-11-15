#pragma once
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_KEYSTATE_BASED_INPUT
// FIXME: on x11 when using wacom tablets as input, grabbing the mouse messes up pointer positions.
// FIXME: apparently it's an issue on windows too
// FIXME: probably need to make this a runtime switch and enable on wayland/macos? only
#define NK_GLFW_GL4_MOUSE_GRABBING
// #define NK_BUTTON_TRIGGER_ON_RELEASE
#include "nuklear.h"
#include "nuklear_glfw_vulkan.h"
