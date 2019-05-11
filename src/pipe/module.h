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


typedef int (*dt_module_create_nodes_t)(void *data);

// this is all the "class" info that is not bound to an instance and can be
// read once on startup
typedef struct dt_module_so_t
{
  dt_token_t name;
  // TODO: pull this out to dt_module_so_t and preload once on startup!
  void *dlhandle;
  dt_module_create_nodes_t create_nodes;
}
dt_module_so_t;

typedef struct dt_module_t
{
  // TODO: has a list of publicly visible connectors
  dt_module_connector_t connector[10]; // enough for everybody, right?
  int num_connectors;

  // TODO: parameters:
  // human facing parameters for gui + serialisation
  // compute facing parameters for uniform upload
  // simple code path for when both are equivalent

}
dt_module_t;

typedef struct dt_module_so_list_t
{
  dt_module_so_t *module;
  uint32_t num_modules;
}
dt_module_so_list_t;

typedef struct dt_module_graph_t
{
  dt_module_t *module;
  uint32_t num_modules;
  uint32_t max_modules;
}
dt_module_graph_t;

typedef struct dt_module_connector_t
{
  dt_token_t      name;   // connector name
  dt_con_type_t   type;   // read write source sink
  dt_buf_type_t   chan;   // rgb yuv..
  dt_buf_format_t format; // f32 ui16
  int modid;              // pointing back to module

  // TODO: these have to be setup very quickly using tokens from a file
  // TODO: some annotations here which will be filled by the tiling/roi passes:
  // if roi, then this comes with a context buffer, too

  // these are only meta connections, the finest layer DAG will connect nodes
  // with memory allocations for vulkan etc.
}
dt_module_connector_t;



dt_connect();

dt_module_t *dt_module_add(dt_token_t name, dt_token_t inst);

dt_module_t *dt_module_get(dt_token_t name, dt_token_t inst);

// serialise (i.e. write module name and instance along with parameters)
// this can be ascii or binary format.
dt_module_write();

// read module from ascii or binary
dt_module_read();
