#include "modules/api.h"

#if 0
// TODO or maybe have a "dynamic" flag on the bvh instead?
// TODO: then in fact need to figure out how to *not* execute geo shaders on the way once done..
void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO cache our node for more efficient access
  for(int n=0;n<graph->num_nodes;n++)
  {
    if(graph->node[n].name == dt_token("g-wobble") &&
       graph->node[n].kernel == dt_token("main") &&
       graph->node[n].module.inst == module->inst)
    {
      // TODO: how is this propagated to bvh?
      graph->node[n].rt[graph->double_buffer].tri_cnt = module->connector[0].roi.wd;
      graph->node[n].flags |= s_module_request_build_bvh;
    }
  }
}
#endif

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int pc[] = { module->connector[0].roi.wd };
  int id_geo = dt_node_add(graph, module, "g-wobble", "main", 
      (module->connector[0].roi.wd+31)/32*DT_LOCAL_SIZE_X, 1, 1, sizeof(pc), pc, 2,
      "input",  "read",  "ssbo", "tri", dt_no_roi,
      "output", "write", "ssbo", "tri", &module->connector[0].roi);
  dt_connector_copy(graph, module, 0, id_geo, 0);
  dt_connector_copy(graph, module, 1, id_geo, 1);
}
