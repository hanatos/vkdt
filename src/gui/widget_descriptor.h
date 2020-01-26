#pragma once

typedef struct dt_widget_descriptor_t
{
  dt_token_t type;  // such as "slider"
  float      min;   // min slider value
  float      max;
}
dt_widget_descriptor_t;
