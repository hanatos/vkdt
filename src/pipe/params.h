#pragma once

#include "gui/widget_descriptor.h"

// parameters to go along with every module
typedef struct dt_ui_param_t
{
  // TODO: make it so this maps 1:1 to binary input if any.
  // TODO: this means if this is a string argument, put it to the end
  // TODO: such that the zero terminated string is in the right spot after this struct
  dt_token_t name;  // name of param
  dt_token_t type;  // type of param: "float" "int" "string"
  int32_t cnt;      // number of elements
  int32_t offset;   // offset into uniform buffer
  dt_widget_descriptor_t widget; // for interoperability with gui
  union
  {
    float   val [1]; // let's assume we at least have one argument
    int32_t vali[1];
    char    str [4];
  };
  // directly after this go float*, int32_t* or char*
}
dt_ui_param_t;

static inline size_t
dt_ui_param_type_size(const dt_token_t type)
{
  if(type == dt_token("float"))  return sizeof(float);
  if(type == dt_token("int"))    return sizeof(int32_t);
  if(type == dt_token("string")) return sizeof(char);
  return 0;
}

static inline size_t
dt_ui_param_size(const dt_token_t type, const int cnt)
{
  return cnt * dt_ui_param_type_size(type);
}
