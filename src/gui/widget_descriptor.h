#pragma once

typedef struct dt_widget_descriptor_t
{
  dt_token_t type;  // such as "slider"
  float      min;   // min slider value
  float      max;
  char      *str;   // list of entries for combobox and checkboxflags
}
dt_widget_descriptor_t;
