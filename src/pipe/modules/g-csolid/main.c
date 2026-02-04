#include "modules/api.h"

void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  for(int n=0;n<graph->num_nodes;n++)
  {
    if(graph->node[n].name == dt_token("g-csolid") &&
       graph->node[n].kernel == dt_token("main") &&
       graph->node[n].module->inst == module->inst)
    {
      const dt_connector_t *c = graph->node[n].connector;
      // max vertex count, i.e. buffer size:
      int vtx_cnt = !dt_cid_unset(c->connected) ?
        graph->node[c->connected.i].connector[c->connected.c].roi.wd-2 : 0;
      // number of actually initialised vertices:
      int vtx_end = !dt_cid_unset(c->connected) ?
        graph->node[c->connected.i].connector[c->connected.c].roi.full_wd-2 : 0;
      if(vtx_cnt <= 0) vtx_cnt = 2;
      if(vtx_end <= 0) vtx_end = 2;
      graph->node[n].connector[1].roi.full_wd = 6*vtx_cnt;
      graph->node[n].wd = (vtx_cnt + 31)/32*DT_LOCAL_SIZE_X;
      graph->node[n].push_constant[0] = vtx_cnt;
      graph->node[n].push_constant[1] = vtx_end;
      break;
    }
  }
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int vtx_cnt = module->connector[0].roi.full_wd-2;
  if(vtx_cnt <= 0) vtx_cnt = 2;
  int tri_cnt = 6*vtx_cnt;
  module->connector[1].roi = (dt_roi_t){ .wd = tri_cnt, .full_wd = tri_cnt, .ht = 1, .full_ht = 1 };
}

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.scale = 1;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int vtx_cnt = module->connector[0].roi.wd-2;
  if(vtx_cnt <= 0) vtx_cnt = 2;
  int tri_cnt = 6*vtx_cnt;
  dt_roi_t roi = { .wd = tri_cnt, .full_wd = tri_cnt, .ht = 1, .full_ht = 1 };
  int pc[] = { vtx_cnt, vtx_cnt };
  int id_geo = dt_node_add(graph, module, "g-csolid", "main", 
      (vtx_cnt+31)/32*DT_LOCAL_SIZE_X, 1, 1, sizeof(pc), pc, 2,
      "input",  "read",  "ssbo", "ui32", dt_no_roi,
      "output", "write", "ssbo", "tri",  &roi);
  dt_connector_copy(graph, module, 0, id_geo, 0);
  dt_connector_copy(graph, module, 1, id_geo, 1);
}
