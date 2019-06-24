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
  // connect the node layer on the module:
  module->connector[mc].connected_ni = nid;
  module->connector[mc].connected_nc = nc;
  // connect the node:
  graph->node[nid].connector[nc] = module->connector[mc];
  // input connectors have a unique source. connect their node layer:
  if(dt_connector_input(module->connector+mc) &&
      module->connector[mc].connected_mi >= 0)
  {
    // connect our node to the nodeid stored on the module
    graph->node[nid].connector[nc].connected_mi = graph->module[
      module->connector[mc].connected_mi].connector[
        module->connector[mc].connected_mc].connected_ni;
    graph->node[nid].connector[nc].connected_mc = graph->module[
      module->connector[mc].connected_mi].connector[
        module->connector[mc].connected_mc].connected_nc;
  }
}
