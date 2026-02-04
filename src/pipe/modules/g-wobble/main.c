#include "modules/api.h"

// TODO: need to figure out how to *not* execute geo shaders on the way once done for static bvh
// commit_params is run *last*, after everything is setup, right before the
// command buffer is recorded and submitted to the queue.
// the deal is that every animated geo module needs to update roi on module and nodes in this callback.
// other than this max count we can only set triangles to NaN to keep them out of the BVH.
void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO cache our node for more efficient access
  for(int n=0;n<graph->num_nodes;n++)
  {
    if(graph->node[n].name == dt_token("g-wobble") &&
       graph->node[n].kernel == dt_token("main") &&
       graph->node[n].module->inst == module->inst)
    {
      const dt_connector_t *c = graph->node[n].connector;
      const int tri_cnt = !dt_cid_unset(c->connected) ?
        graph->node[c->connected.i].connector[c->connected.c].roi.full_wd : 0;
      graph->node[n].connector[0].roi.full_wd = tri_cnt;
      graph->node[n].connector[1].roi.full_wd = tri_cnt;
      graph->node[n].wd = (tri_cnt + 31)/32*DT_LOCAL_SIZE_X;
      break;
    }
  }
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int pc[] = { module->connector[0].roi.full_wd };
  int id_geo = dt_node_add(graph, module, "g-wobble", "main", 
      (module->connector[0].roi.wd+31)/32*DT_LOCAL_SIZE_X, 1, 1, sizeof(pc), pc, 2,
      "input",  "read",  "ssbo", "tri", dt_no_roi,
      "output", "write", "ssbo", "tri", &module->connector[0].roi);
  dt_connector_copy(graph, module, 0, id_geo, 0);
  dt_connector_copy(graph, module, 1, id_geo, 1);
}
