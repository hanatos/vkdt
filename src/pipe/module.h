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

typedef struct vkp_module_t
{
  // TODO: has a list of publicly visible connectors
}
vkp_module_t;

typedef struct vkp_connector_t
{
  // TODO: these have to be setup very quickly using tokens from a file
  // TODO: some annotations here which will be filled by the tiling/roi passes:
  // if roi, then this comes with a context buffer, too

  // these are only meta connections, the finest layer DAG will connect nodes
  // with memory allocations for vulkan etc.
}
vkp_connector_t;



vkp_connect();
