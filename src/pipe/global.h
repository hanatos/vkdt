#pragma once

#include "token.h"
#include "params.h"
#include "connector.h"
#include "graph-fwd.h"

// static global structs to keep around for all instances of pipelines.
// this queries the modules on startup, does the dlopen and expensive
// parsing once, and holds a list for modules to quickly access run time.

typedef struct dt_read_source_params_t
{ // future proof arguments
  int c;  // connector
  int a;  // array index
}
dt_read_source_params_t;

typedef void (*dt_module_create_nodes_t)  (dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_modify_roi_out_t)(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_modify_roi_in_t )(dt_graph_t *graph, dt_module_t *module);
typedef void (*dt_module_write_sink_t) (dt_module_t *module, void *buf);
typedef void (*dt_module_read_source_t)(dt_module_t *module, void *buf, dt_read_source_params_t *p);
typedef void (*dt_module_read_geo_t)(dt_module_t *module, void *vtx, void *idx);
typedef int  (*dt_module_init_t)    (dt_module_t *module);
typedef void (*dt_module_cleanup_t )(dt_module_t *module);
typedef void (*dt_module_commit_params_t)(dt_graph_t *graph, dt_module_t *module);
typedef int  (*dt_module_audio_t)(dt_module_t *module, const int frame, uint16_t **samples);
typedef dt_graph_run_t (*dt_module_check_params_t)(dt_module_t *module, uint32_t parid, void *oldval);

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

  // commit new parameters from module's gui params to binary float blob
  dt_module_commit_params_t commit_params;

  // check whether a parameter update would cause a graph update or just a parameter update
  dt_module_check_params_t check_params;

  // returns a pointer to audio sample data (optional)
  dt_module_audio_t audio;

  // read geo for constructing ray tracing acceleration structures (optional)
  dt_module_read_geo_t read_geo;

  // for source nodes, will be called before processing starts
  dt_module_read_source_t read_source;
  // for sink nodes, will be called once processing ended
  dt_module_write_sink_t  write_sink;

  dt_module_init_t init;
  dt_module_init_t cleanup;

  dt_connector_t connector[DT_MAX_CONNECTORS];
  int num_connectors;

  // pointer to variably-sized parameters
  dt_ui_param_t *param[30];
  int num_params;
}
dt_module_so_t;

typedef struct dt_pipe_global_t
{
  // this is the directory where the vkdt binary resides,
  // i.e. if you build from git it would be the bin/ directory
  // in the repository. it is used to find all sorts of resources
  // relative to it, i.e. data/ and modules/.
  char basedir[512];
  dt_module_so_t *module;
  uint32_t num_modules;

  int modules_reloaded;
}
dt_pipe_global_t;

extern dt_pipe_global_t dt_pipe;

// returns non-zero on failure:
int dt_pipe_global_init();

// global cleanup:
void dt_pipe_global_cleanup();
