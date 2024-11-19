#include "modules/api.h"

void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int *p_ani = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("animate")));
  if(p_ani[0] == 0) return; // static bvh doesn't need to set the build flag
  // TODO cache our node for more efficient access
  for(int n=0;n<graph->num_nodes;n++)
  {
    if(graph->node[n].name == dt_token("bvh") &&
       graph->node[n].kernel == dt_token("main") &&
       graph->node[n].module->inst == module->inst)
    {
      graph->node[n].rt[graph->double_buffer].tri_cnt = module->connector[0].roi.wd;
      graph->node[n].flags |= s_module_request_build_bvh;
    }
  }
}
