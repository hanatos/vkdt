#pragma once

// this struct defines gui widgets to appear for module parameters.
// the parameters themselves are stored in dt_ui_param_t structs (pipe/params.h).
// pipe/global.c parses both params and params.ui definitions and thus will fill
// these structs for the gui.
typedef struct dt_widget_descriptor_t
{
  dt_token_t type;  // such as "slider"
  float      min;   // min slider value
  float      max;   // max slider value
  int        grpid; // param id: group this widget belongs to, or -1
  int        mode;  // widget only shows if int group param is in this mode
  int        cntid; // param id: this integer param holds the multiplicity of this widget, or -1
  int        sep;   // widget wants a separator to be drawn after it
  void      *data;  // any extra data a widget might need to initialise (strings for combo boxes)
}
dt_widget_descriptor_t;
