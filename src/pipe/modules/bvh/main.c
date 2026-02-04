#include "modules/api.h"

void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int *p_ani = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("animate")));
  // TODO cache our node for more efficient access
  for(int n=0;n<graph->num_nodes;n++)
  {
    if(graph->node[n].name == dt_token("bvh") &&
       graph->node[n].kernel == dt_token("main") &&
       graph->node[n].module->inst == module->inst)
    {
      // walk back *one* step and grab full_wd! (we allocate wd, full_wd is actual tri count)
      // all modules are responsible in their commit_params () callback to
      // update their full_wd on the node output.
      const dt_connector_t *c = graph->node[n].connector;
      const int tri_cnt = !dt_cid_unset(c->connected) ?
        graph->node[c->connected.i].connector[c->connected.c].roi.full_wd : 0;
      graph->node[n].rt.tri_cnt = tri_cnt;
      if(p_ani[0]) // static bvh doesn't need to set the build flag, it's reset in core
        graph->node[n].flags |= s_module_request_build_bvh;
      break;
    }
  }
}
