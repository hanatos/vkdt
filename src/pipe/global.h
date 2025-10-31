#pragma once

#include "token.h"
#include "params.h"
#include "connector.h"
#include "graph-fwd.h"
#include <limits.h>

#define DT_MAX_PARAMS_PER_MODULE 50

// static global structs to keep around for all instances of pipelines.
// this queries the modules on startup, does the dlopen and expensive
// parsing once, and holds a list for modules to quickly access run time.

typedef struct dt_read_source_params_t
{ // future proof arguments
  int c;              // connector index (on the node)
  int a;              // array index
  dt_node_t *node;    // the callback lives on the module and needs to identify the real source
}
dt_read_source_params_t;

typedef struct dt_write_sink_params_t
{ // future proof arguments
  int c;              // connector index (on the node)
  int a;              // array index
  dt_node_t *node;    // the callback lives on the module and needs to identify the real source
}
dt_write_sink_params_t;

typedef struct dt_module_input_event_t
{ // glfw events for modules without linking glfw
  int type;       // activate 0 mouse button 1 mouse position 2 mouse scrolled 3 keyboard 4
  double x, y;    // mouse cursor pos
  double dx, dy;  // scrolled offsets
  int mbutton;    // mouse button int
  int action;     // press 1 release 0 repeat 2
  int mods;       // shift 1 ctrl 2 alt 4 super 8 caps 0x10 num lock 0x20
  int key;        // ascii character 'a' etc works. for the rest, see /usr/include/GLFW/glfw3.h
  int scancode;
  int grabbed;    // to distinguish grabbed and mouse-over-widget events
}
dt_module_input_event_t;

typedef void (*dt_module_create_nodes_t)  (dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_modify_roi_out_t)(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_modify_roi_in_t )(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_write_sink_t) (dt_module_t *module, void *buf, dt_write_sink_params_t *p);
typedef void (*dt_module_read_source_t)(dt_module_t *module, void *buf, dt_read_source_params_t *p);
typedef int  (*dt_module_init_t)    (dt_module_t *module);
typedef void (*dt_module_cleanup_t )(dt_module_t *module);
typedef int  (*dt_module_bs_init_t) ();
typedef void (*dt_module_animate_t)(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_commit_params_t)(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_ui_callback_t)(dt_module_t *module, dt_token_t param);
typedef int  (*dt_module_audio_t)(dt_module_t *module, uint64_t sample_beg, uint32_t sample_cnt, uint16_t **samples);
typedef dt_graph_run_t (*dt_module_input_t)(dt_module_t *module, dt_module_input_event_t *e);
typedef dt_graph_run_t (*dt_module_check_params_t)(dt_module_t *module, uint32_t parid, uint32_t num, void *oldval);

// this is all the "class" info that is not bound to an instance and can be
// read once on startup
typedef struct dt_module_so_t
{
  dt_token_t name;

  // for dlopen state
  void *dlhandle;

  // pass full image forward through pipe, init roi.full_{wd,ht}
  // this is also responsible of initing the img_params struct correctly
  // given the parameters of the input connectors. the default is to copy
  // over the struct of the first connector named "input" (this is done
  // regardless whether a module implements the callback or not, and before
  // the callback is executed if it exists).
  dt_module_modify_roi_out_t modify_roi_out;

  // pull required pixels from inputs, init roi.roi_* and ctx_*
  dt_module_modify_roi_in_t  modify_roi_in;

  // creates the nodes for this module after roi negotiations
  dt_module_create_nodes_t create_nodes;

  // animate (optional)
  dt_module_animate_t animate;

  // commit new parameters from module's gui params to binary float blob
  dt_module_commit_params_t commit_params;

  // for the specific 'callback' ui widget that will trigger custom code execution
  dt_module_ui_callback_t ui_callback;

  // check whether a parameter update would cause a graph update or just a parameter update
  dt_module_check_params_t check_params;

  // returns a pointer to audio sample data (optional)
  dt_module_audio_t audio;

  // enables input grabbing for modules
  dt_module_input_t input;

  // for source nodes, will be called before processing starts
  dt_module_read_source_t read_source;
  // for sink nodes, will be called once processing ended
  dt_module_write_sink_t  write_sink;

  dt_module_init_t init;
  dt_module_init_t cleanup;

  dt_module_bs_init_t bs_init; // windows module initialisation

  dt_connector_t connector[DT_MAX_CONNECTORS];
  int num_connectors;

  // pointer to variably-sized parameters
  dt_ui_param_t *param[DT_MAX_PARAMS_PER_MODULE];
  int num_params;

  // is this module simple, i.e. has a clear input and output connector chain?
  int has_inout_chain;
}
dt_module_so_t;

#ifdef __ANDROID__
struct android_app;
#endif
typedef struct dt_pipe_global_t
{
  // this is the directory where the vkdt binary resides,
  // i.e. if you build from git it would be the bin/ directory
  // in the repository. it is used to find all sorts of resources
  // relative to it, i.e. data/ and modules/.
  char basedir[PATH_MAX];
  char homedir[PATH_MAX]; // this is normally ${HOME}/.config/vkdt
  dt_module_so_t *module;
  uint32_t num_modules;
#ifdef __ANDROID__
  struct android_app *app; // access to gui stuff (android always has this)
#endif
}
dt_pipe_global_t;

#ifndef VKDT_DSO_BUILD
VKDT_API extern dt_pipe_global_t dt_pipe;
#endif

// ctx is an application context, the struct android_app* on android, 0 otherwise.
// returns non-zero on failure.
int dt_pipe_global_init(void *ctx);

// global cleanup:
void dt_pipe_global_cleanup();

// return total byte size of parameter storage
static inline size_t
dt_module_total_param_size(int soid)
{
  if(soid < 0 || soid >= (int)dt_pipe.num_modules) return 0;
  if(dt_pipe.module[soid].num_params <= 0) return 0;
  const dt_ui_param_t *p = dt_pipe.module[soid].param[dt_pipe.module[soid].num_params-1];
  return p->offset + dt_ui_param_size(p->type, p->cnt);
}
