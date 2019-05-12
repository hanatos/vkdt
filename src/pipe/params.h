#pragma once

// parameters to go along with every module
typedef struct dt_ui_param_t
{
  // TODO: make it so this maps 1:1 to binary input if any.
  // TODO: this means if this is a string argument, put it to the end
  // TODO: such that the zero terminated string is in the right spot after this struct
  dt_token_t name;
  dt_token_t type;
  int32_t cnt;
  union
  {
    float val[0];
    char str[0];
  };
  // directly after this go float* or char*
  // TODO: how to sort value and min/max or whatever gui stuff we need if it's cnt>1?
}
dt_ui_param_t;

// TODO: copy over block of floats.
// TODO: how to keep mental sanity in glsl code? provide macros for offsets by name?
