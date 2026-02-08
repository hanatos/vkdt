#include "modules/api.h"
#include "core/core.h"
#include <math.h>
#include <stdlib.h>

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  module->connector[1].roi = module->connector[0].roi;
  const int wd = dt_module_param_int(module, 0)[0];
  const int ht = dt_module_param_int(module, 1)[0];
  if(!wd || !ht)
  { // if not explicitly set, be soft about roi:
    module->connector[1].roi.scale = -1.0f;
    return;
  }
  double scale = MIN(1.0, MIN(
      wd / (double)module->connector[1].roi.full_wd,
      ht / (double)module->connector[1].roi.full_ht));
  if(scale < 1.0)
  {
    module->connector[1].roi.full_wd = (int)(module->connector[0].roi.full_wd * scale + 0.5);
    module->connector[1].roi.full_ht = (int)(module->connector[0].roi.full_ht * scale + 0.5);
  }
}

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{ // request the full thing, we'll rescale
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  // we *don't* set the scale factor. this is used internally as
  // proof that the roi is initialised, but the only use is disambiguation
  // when one output is connected to several chains. by *not* setting the scale
  // here, we suggest a new resolution but if any other chain is more opinionated
  // than us, we let them override our numbers.
  // module->connector[0].roi.scale = 1.0f;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 0 || parid == 1)
  { // wd or ht
    int oldv = *(int*)oldval;
    int newv = dt_module_param_int(module, parid)[0];
    if(oldv != newv) return s_graph_run_all; // re-run buffer allocations
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  if(module->connector[0].roi.wd == module->connector[1].roi.wd &&
     module->connector[0].roi.ht == module->connector[1].roi.ht)
  return dt_connector_bypass(graph, module, 0, 1);
  const int nodeid = dt_node_add(graph, module, "resize", "main",
      module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
      "input",  "read",  "rgba", "*",    dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, nodeid, 0);
  dt_connector_copy(graph, module, 1, nodeid, 1);
}
