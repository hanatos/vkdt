#pragma once
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
// #define NK_BUTTON_TRIGGER_ON_RELEASE
// unfortunately the builtin-nk function triggers infinite loops at times:
#define NK_DTOA(S, D) sprintf(S, "%g", D)
#include "nuklear.h"
#include "nuklear_glfw_vulkan.h"
