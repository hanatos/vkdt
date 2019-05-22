#pragma once

#include "token.h"
#include "params.h"
#include "connector.h"

// static global structs to keep around for all instances of pipelines.
// this queries the modules on startup, does the dlopen and expensive
// parsing once, and holds a list for modules to quickly access run time.

typedef struct dt_graph_t dt_grap_t; // fwd declare
typedef struct dt_module_t dt_module_t;
typedef void (*dt_module_create_nodes_t)  (dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_modify_roi_out_t)(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_modify_roi_in_t )(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_write_output_t)(dt_module_t *module, void *buf);
typedef void (*dt_module_load_input_t)  (dt_module_t *module, void *buf);
typedef int  (*dt_module_init_t)    (dt_module_t *module);
typedef void (*dt_module_cleanup_t )(dt_module_t *module);

// this is all the "class" info that is not bound to an instance and can be
// read once on startup
typedef struct dt_module_so_t
{
  dt_token_t name;

  void *dlhandle;

  // pass full image forward through pipe, init roi.full_{wd,ht}
  dt_module_modify_roi_out_t modify_roi_out;

  // pull required pixels from inputs, init roi.roi_* and ctx_*
  dt_module_modify_roi_in_t  modify_roi_in;

  // creates the nodes for this module after roi negotiations
  dt_module_create_nodes_t create_nodes;

  // for sink nodes, will be called once processing ended
  dt_module_write_output_t write_output;

  // for source nodes, will be called before processing starts
  dt_module_load_input_t load_input;

  dt_module_init_t init;
  dt_module_init_t cleanup;

  dt_connector_t connector[10]; // enough for everybody, right?
  int num_connectors;

  // pointer to variably-sized parameters
  dt_ui_param_t *param[10];
  int num_params;
}
dt_module_so_t;

typedef struct dt_pipe_global_t
{
  char module_dir[2048];
  dt_module_so_t *module;
  uint32_t num_modules;
}
dt_pipe_global_t;

extern dt_pipe_global_t dt_pipe;

// returns non-zero on failure:
int dt_pipe_global_init();

// global cleanup:
void dt_pipe_global_cleanup();
