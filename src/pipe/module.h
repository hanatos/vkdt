#pragma once

// a module is a user facing macroscopic block of compute
// that potentially consists of several nodes. a wavelet transform
// would for instance iterate on a couple of analysis nodes and then
// resynthesize the data with different nodes.
// a module has one set of user parameters which map to the gui
// and one set that maps to the pipeline/uniform buffers of the node kernels.

// modules and their connections need to be setup quickly from config files
// whereas nodes will be constructed by the module which also manages their
// connections based on the size and roi of the buffers to process.

// none of these functions are thread safe, a graph should only ever be owned
// by one thread.

#include "global.h"
#include "connector.h"


// this is an instance of a module.
typedef struct dt_module_t
{
  dt_module_so_t *so; // class of module
  dt_token_t inst;    // instance name

  // TODO: has a list of publicly visible connectors
  // TODO: actually two list so we can store the context pipeline independently?
  dt_connector_t connector[10]; // enough for everybody, right?
  int num_connectors;

  // TODO: parameters:
  // human facing parameters for gui + serialisation
  // compute facing parameters for uniform upload
  // simple code path for when both are equivalent
  // TODO: pointer or index for realloc?
  float *params[10];  // points into pool stored with graph
  int num_params;
}
dt_module_t;


dt_module_t *dt_module_add(dt_token_t name, dt_token_t inst);

dt_module_t *dt_module_get(dt_token_t name, dt_token_t inst);

// serialise (i.e. write module name and instance along with parameters)
// this can be ascii or binary format.
dt_module_write();

// read module from ascii or binary
dt_module_read();
