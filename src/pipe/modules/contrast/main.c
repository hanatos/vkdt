#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  if(parid >= 2 && parid <= 5)
  { // radius
    float oldrad = *(float*)oldval;
    float newrad = dt_module_param_float(module, parid)[0];
    return dt_api_blur_check_params(oldrad, newrad);
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const float radius = dt_module_param_float(module, 0)[0]; // radius is the first parameter in our params file

  int guided_entry = -1, guided_exit = -1;
  dt_api_guided_filter(
      graph, module, 
      &module->connector[0].roi,
      &guided_entry,
      &guided_exit,
      radius,
      1e-2f);

  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = module->connector[0].roi,
    .connected_mi = -1,
  };
  dt_connector_t cg = {
    .name   = dt_token("coarse"),
    .type   = dt_token("read"),
    .chan   = dt_token("y"),
    .format = dt_token("f16"),
    .roi    = module->connector[0].roi,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = module->connector[1].roi,
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_comb = graph->num_nodes++;
  dt_node_t *node_comb = graph->node + id_comb;
  *node_comb = (dt_node_t) {
    .name   = dt_token("contrast"),
    .kernel = dt_token("combine"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 3,
    .connector = {
      ci, cg, co,
    },
  };

  dt_connector_copy(graph, module, 0, id_comb, 0); // rgb input
  CONN(dt_node_connect(graph, guided_exit, 2, id_comb, 1)); // luminance input

  // connect entry point of module (input rgb) to entry point of guided filter
  dt_connector_copy(graph, module, 0, guided_entry, 0);
  // connect entry point of module (input rgb) to exitnode of guided filter that needs it for reconstruction
  dt_connector_copy(graph, module, 0, guided_exit,  0);
  // connect combined rgb to exit point of module
  dt_connector_copy(graph, module, 1, id_comb, 2);
}

