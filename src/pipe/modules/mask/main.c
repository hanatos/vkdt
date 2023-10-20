#include "modules/api.h"

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int pid = dt_module_get_param(module->so, dt_token("envelope"));
  if(parid == pid)
  {
    float *p_env = (float *)dt_module_param_float(module, pid);
    for(int i=0;i<4;i++)
    { // make sure the sort order stays put
      if(i < num && p_env[i] > p_env[num]) p_env[i] = p_env[num];
      if(i > num && p_env[i] < p_env[num]) p_env[i] = p_env[num];
    }
  }
  return s_graph_run_record_cmd_buf;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t hroi = (dt_roi_t){.wd = 16+1, .ht = 16 };
  const int id_hist = dt_node_add(graph, module, "mask", "hist", (wd+15)/16 * DT_LOCAL_SIZE_X, (ht+15)/16 * DT_LOCAL_SIZE_Y, 1, 0, 0, 2,
      "input",  "read",  "*",    "*",   -1ul,
      "hist",   "write", "ssbo", "u32", &hroi);

  const int id_mask = dt_node_add(graph, module, "mask", "main", wd, ht, 1, 0, 0, 2,
      "input", "read",  "*",    "*",   -1ul,
      "mask",  "write", "rgba", "f16", &module->connector[1].roi);

  const int id_dspy = dt_node_add(graph, module, "mask", "dspy",
      module->connector[2].roi.wd, module->connector[2].roi.ht, 1, 0, 0, 2,
      "hist", "read",  "ssbo", "u32", -1ul,
      "dspy", "write", "rgba", "f16", &module->connector[2].roi);

  dt_connector_copy(graph, module, 0, id_hist, 0);
  dt_connector_copy(graph, module, 0, id_mask, 0);
  dt_connector_copy(graph, module, 1, id_mask, 1);
  dt_connector_copy(graph, module, 2, id_dspy, 1);
  dt_node_connect(graph, id_hist, 1, id_dspy, 0);
  graph->node[id_hist].connector[1].flags |= s_conn_clear; // clear histogram buffer
}
