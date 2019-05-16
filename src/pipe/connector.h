#pragma once

#include "token.h"
#include "alloc.h"

#include <stdint.h>

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
  // TODO: if chan and format change between context and roi we'll need to redesign!
  dt_token_t name;   // connector name
  dt_token_t type;   // read write source sink
  dt_token_t chan;   // rgb yuv..
  dt_token_t format; // f32 ui16

  // outputs (write buffers) can be connected to multiple inputs
  // inputs (read buffers) can only be connected to exactly one output
  // we only keep track of where inputs come from. this is also
  // how we'll access it in the DAG during DFS from sinks.
  int connected_mid;  // pointing to connected module or node (or -1). is a reference count for write buffers.
  int connected_cid;

  // memory allocations for region of interest
  // as well as the context buffers, if any:
  uint64_t roi_offset, roi_size;
  uint64_t ctx_offset, ctx_size;

  // information about buffer dimensions transported here:
  dt_roi_t roi;

  // buffer associated with this in case it connects nodes:
  uint64_t offset, size;
  // mem object for allocator:
  // while this may seem duplicate with offset/size, it may be freed already
  // and the offset and size are still valid for successive runs through the
  // pipeline once it has been setup.
  dt_vkmem_t *mem;
}
dt_connector_t;

// "templatised" connection functions for both modules and nodes
typedef struct dt_graph_t dt_graph_t; // fwd declare
// connect source|write (m0,c0) -> sink|read (m1,c1)
int dt_module_connect(dt_graph_t *graph, int m0, int c0, int m1, int c1);
int dt_node_connect(dt_graph_t *graph, int m0, int c0, int m1, int c1);
