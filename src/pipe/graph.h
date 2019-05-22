#pragma once
#include "node.h"
#include "module.h"
#include "alloc.h"

// the graph is stored as list of modules and list of nodes.
// these have connectors with detailed buffer information which
// also hold the id to the other connected module or node. thus,
// there is no need for an explicit list of connections.
typedef struct dt_graph_t
{
  dt_module_t *module;
  uint32_t num_modules, max_modules;

  dt_node_t *node;
  uint32_t num_nodes, max_nodes;

  // memory pool for node params. this is a simple allocator that increments
  // the end pointer until it is flushed completely.
  uint8_t *params_pool;
  uint32_t params_end, params_max;

  // TODO: also store full history somewhere

  // allocator for buffers
  dt_vkalloc_t alloc;
}
dt_graph_t;

void dt_graph_init(dt_graph_t *g);
void dt_graph_cleanup(dt_graph_t *g);

// read modules from ascii or binary
int dt_graph_read_config_ascii(
    dt_graph_t *graph,
    const char *filename);

// write only modules connected to sink modules,
// and only set parameters of modules once.
int dt_graph_write_compressed(
    dt_graph_t *graph,
    const char *filename);

// write everything, including full history?
int dt_graph_write(
    dt_graph_t *graph,
    const char *filename);

void dt_graph_setup_pipeline(
    dt_graph_t *graph);
