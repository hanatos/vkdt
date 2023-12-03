#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 0)
  { // radius
    float oldrad = *(float*)oldval;
    float newrad = dt_module_param_float(module, 0)[0];
    return dt_api_blur_check_params(oldrad, newrad);
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *r1 = &module->connector[1].roi;
  dt_roi_t *r2 = &module->connector[2].roi;
  ri->wd = r1->wd;
  ri->ht = r1->ht;
  ri->scale = r1->scale;
  // not listening to what the others say, this is just dspy
  r2->wd = r1->wd;
  r2->ht = r1->ht;
  r2->scale = r1->scale;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *r1 = &module->connector[1].roi;
  dt_roi_t *r2 = &module->connector[2].roi;
  *r1 = *ri;
  *r2 = *ri;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  const int dp = 1;
  const float radius  = ((float*)module->param)[0];
  const float epsilon = ((float*)module->param)[1];

  // input image -> seven quantised zones of exposure in [0,1] (output int 0..6)
  const int id_quant = dt_node_add(graph, module, "zones", "quant", wd, ht, dp, 0, 0, 2,
      "input",  "read",  "rgba", "f16", -1ul,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  // output: float image [0.0,6.0]
  // guided blur with I : zones, p : input image
  const int id_guided = dt_api_guided_filter_full(
      graph, module, -1, 0, id_quant, 1,
      0, 0, radius, epsilon);

  // process zone exposure correction:
  const int id_apply = dt_node_add(graph, module, "zones", "apply", wd, ht, dp, 0, 0, 3,
      "input",  "read",  "rgba", "f16", -1ul,
      "zones",  "read",  "rgba", "f16", -1ul,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  CONN(dt_node_connect(graph, id_guided, 2, id_apply, 1));
  dt_connector_copy(graph, module, 0, id_apply, 0);
  dt_connector_copy(graph, module, 0, id_quant, 0);
  dt_connector_copy(graph, module, 2, id_guided, 2); // dspy output
  // need to do this last to overwrite the empty roi again :(
  dt_connector_copy(graph, module, 1, id_apply, 2);
}
