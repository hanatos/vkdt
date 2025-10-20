#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

typedef struct moddata_t
{
  int id_guided;
}
moddata_t;

int init(dt_module_t *mod)
{
  moddata_t *dat = malloc(sizeof(*dat));
  mod->data = dat;
  memset(dat, 0, sizeof(*dat));
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  free(mod->data);
  mod->data = 0;
}

void commit_params(dt_graph_t *graph, dt_module_t *mod)
{ // let guided filter know we updated the push constants:
  moddata_t *dat = mod->data;
  const float *radius = dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("radius")));
  if(dat->id_guided >= 0 && dat->id_guided < graph->num_nodes &&
      graph->node[dat->id_guided].kernel == dt_token("guided2f"))
    memcpy(graph->node[dat->id_guided].push_constant, radius, sizeof(float)*2);
}

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
  const float *radius  = ((float*)module->param); // points to { radius, epsilon }

  // input image -> seven quantised zones of exposure in [0,1] (output int 0..6)
  const int id_quant = dt_node_add(graph, module, "zones", "quant", wd, ht, dp, 0, 0, 2,
      "input",  "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  // output: float image [0.0,6.0]
  // guided blur with I : zones, p : input image
  moddata_t *dat = module->data;
  const int id_guided = dt_api_guided_filter_full(
      graph, module, -1, 0, id_quant, 1,
      0, 0, &dat->id_guided, radius, 0);

  // process zone exposure correction:
  const int id_apply = dt_node_add(graph, module, "zones", "apply", wd, ht, dp, 0, 0, 3,
      "input",  "read",  "rgba", "f16", dt_no_roi,
      "zones",  "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  CONN(dt_node_connect(graph, id_guided, 2, id_apply, 1));
  dt_connector_copy(graph, module, 0, id_apply, 0);
  dt_connector_copy(graph, module, 0, id_quant, 0);
  dt_connector_copy(graph, module, 2, id_guided, 2); // dspy output
  // need to do this last to overwrite the empty roi again :(
  dt_connector_copy(graph, module, 1, id_apply, 2);
}
