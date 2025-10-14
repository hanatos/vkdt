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

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const float *radius  = (float*)module->param; // points to {radius, epsilon}

  // guided blur with I : zones, p : input image
  moddata_t *mod = module->data;
  const int id_guided = dt_api_guided_filter_full(
      graph, module, -1, 0, -1, 1,
      0, 0, &mod->id_guided, radius);
  dt_connector_copy(graph, module, 2, id_guided, 2);  // output
}
