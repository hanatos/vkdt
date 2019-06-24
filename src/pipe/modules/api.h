#pragma once
#include "pipe/node.h"
#include "pipe/graph.h"
#include "pipe/module.h"

// some module specific helpers
// TODO: is this the best place? what to add here?

// convenience function to detect inputs
static inline int
dt_connector_input(dt_connector_t *c)
{
  return c->type == dt_token("read") || c->type == dt_token("sink");
}
static inline int
dt_connector_output(dt_connector_t *c)
{
  return c->type == dt_token("write") || c->type == dt_token("source");
}
static inline int
dt_node_source(dt_node_t *n)
{
  return n->connector[0].type == dt_token("source");
}
static inline int
dt_node_sink(dt_node_t *n)
{
  return n->connector[0].type == dt_token("sink");
}

// TODO: also need _copy_ctx() version for the context buffer!
static inline void
dt_connector_copy(
    dt_graph_t  *graph,
    dt_module_t *module,
    int nid,   // node id
    int mc,    // connector id on module to copy
    int nc)    // connector id on node to copy to
{
  module->connected_nodeid[mc] = nid;
  graph->node[nid].connector[nc] = module->connector[mc];
  if(dt_connector_input(module->connector+mc) &&
      module->connector[mc].connected_mid >= 0)
    graph->node[nid].connector[nc].connected_mid = graph->module[
      module->connector[mc].connected_mid].connected_nodeid[mc];
}
