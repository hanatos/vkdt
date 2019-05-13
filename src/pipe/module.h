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
  // TODO: preload once on startup!
  void *dlhandle;
  dt_module_create_nodes_t create_nodes;

  dt_connector_t connector[10]; // enough for everybody, right?
  int num_connectors;

  // pointer to variably-sized parameters
  // TODO: pool or malloc?
  dt_ui_param_t **params;
}
dt_module_so_t;

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
  int num_params;
  // TODO: pointer or index for realloc?
  dt_ui_param_t **params;  // points into pool stored with graph
}
dt_module_t;

typedef struct dt_module_so_list_t
{
  dt_module_so_t *module;
  uint32_t num_modules;
}
dt_module_so_list_t;


// the graph is stored as list of modules and list of nodes.
// these have connectors with detailed buffer information which
// also hold the id to the other connected module or node. thus,
// there is no need for an explicit list of connections.
typedef struct dt_module_graph_t
{
  dt_module_t *module;
  uint32_t num_modules, max_modules;

  dt_node_t *node;
  uint32_t num_nodes, max_nodes;

  // memory pool for node params. this is a simple allocator that increments
  // the end pointer until it is flushed completely.
  uint8_t *params_pool;
  uint32_t params_end, params_max;
}
dt_module_graph_t;

// info about a region of interest.
// stores full buffer dimensions, context and roi.
typedef struct dt_roi_t
{
  uint32_t full_wd, full_ht; // full input size
  uint32_t ctx_wd, ctx_ht;   // size of downscaled context buffer
  uint32_t roi_wd, roi_ht;   // dimensions of region of interest
  uint32_t roi_ox, roi_oy;   // offset in full image
  float roi_scale;           // scale: roi_wd * roi_scale is on input scale
}
dt_roi_t;

// connectors are used for modules as well as for nodes.
// modules:
// TODO: these have to be setup very quickly using tokens from a file
// TODO: some annotations here which will be filled by the tiling/roi passes:
// if roi, then this comes with a context buffer, too.
// these are only meta connections, the finest layer DAG will connect nodes
// with memory allocations for vulkan etc.
// nodes:
// the ctx info is always ignored.
typedef struct dt_connector_t
{
  // TODO: if these config things change between context and roi we'll need to redesign!
  dt_token_t name;   // connector name
  dt_token_t type;   // read write source sink
  dt_token_t chan;   // rgb yuv..
  dt_token_t format; // f32 ui16

  // TODO: do we really need these?
  int mid;           // pointing back to module or node
  int cid;
  // TODO: outputs (write buffers) can be connected to multiple inputs
  // TODO: inputs (read buffers) can only be connected to exactly one output
  // TODO: maybe only keep track of where inputs come from? this is also
  //       how we'll access it in the DAG during DFS from sinks
  int connected_mid;  // pointing to connected module or node (or -1)
  int connected_cid;

  // memory allocations for region of interest
  // as well as the context buffers, if any:
  uint64_t roi_offset, roi_size;
  uint64_t ctx_offset, ctx_size;

  // information about buffer dimensions transported here:
  dt_roi_t roi;
}
dt_connector_t;



dt_module_connect();

dt_module_t *dt_module_add(dt_token_t name, dt_token_t inst);

dt_module_t *dt_module_get(dt_token_t name, dt_token_t inst);

// serialise (i.e. write module name and instance along with parameters)
// this can be ascii or binary format.
dt_module_write();

// read module from ascii or binary
dt_module_read();
