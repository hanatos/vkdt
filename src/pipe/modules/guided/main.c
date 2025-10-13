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

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const float *radius  = (float*)module->param; // points to {radius, epsilon}

  // guided blur with I : zones, p : input image
  const int id_guided = dt_api_guided_filter_full(
      graph, module, -1, 0, -1, 1,
      0, 0, 0, radius);
  dt_connector_copy(graph, module, 2, id_guided, 2);  // output
}
