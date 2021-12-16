#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
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
  const float radius  = ((float*)module->param)[0];
  const float epsilon = ((float*)module->param)[1];

  // guided blur with I : zones, p : input image
  int guided_entry, guided_exit;
  dt_api_guided_filter_full(
      graph, module, &module->connector[0].roi,
      &guided_entry, &guided_exit,
      radius, epsilon);
  dt_connector_copy(graph, module, 0, guided_entry, 0); // input
  dt_connector_copy(graph, module, 1, guided_entry, 1); // guide
  dt_connector_copy(graph, module, 0, guided_exit, 0);  // input again
  dt_connector_copy(graph, module, 2, guided_exit, 2);  // output
}
