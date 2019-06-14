#pragma once

// parameters to go along with every module
typedef struct dt_ui_param_t
{
  // TODO: make it so this maps 1:1 to binary input if any.
  // TODO: this means if this is a string argument, put it to the end
  // TODO: such that the zero terminated string is in the right spot after this struct
  dt_token_t name;  // name of param
  dt_token_t type;  // type of param: "float" ..
  int32_t cnt;      // number of elements
  int32_t offset;   // offset into uniform buffer
  union
  {
    float val[3]; // let's assume we at least have one argument + min/max
    char str[12];
  };
  // directly after this go float* or char*
  // TODO: how to sort value and min/max or whatever gui stuff we need if it's cnt>1?
}
dt_ui_param_t;

// TODO: copy over block of floats.
// TODO: how to keep mental sanity in glsl code? provide macros for offsets by name?
