#pragma once

// some module specific helpers
// TODO: is this the best place? what to add here?

// TODO: also need _copy_ctx() version for the context buffer!
static inline void
dt_connector_copy(
    dt_graph_t  *graph,
    dt_module_t *module,
    int nid,   // node id
    int mc,    // connector id on module to copy
    int nc)    // connector id on node to copy to
{
  module->connected_nodeid[c] = nid;
  graph->node[nid].connector[c] = module->connector[c];
  if(dt_connector_input(module->connector+c) &&
      module->connector[c].connected_mid >= 0)
    graph->node[nid].connector[c].connected_mid = graph->module[
      module->connector[c].connected_mid].connected_nodeid[c];
}
