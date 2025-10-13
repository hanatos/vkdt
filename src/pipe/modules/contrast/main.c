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
  const float *radius = dt_module_param_float(module, 0); // radius is the first parameter in our params file
  // as it so happens, this is in memory directly after the radius:
  // const float edges  = dt_module_param_float(module, 1)[0];

  int guided_entry = -1, guided_exit = -1;
  dt_api_guided_filter(
      graph, module, 
      &module->connector[0].roi,
      &guided_entry,
      &guided_exit,
      radius); // this points to our params block of memory where there happens to be { radius, edges }

  assert(graph->num_nodes < graph->max_nodes);
  const int id_comb = dt_node_add(graph, module, "contrast", "combine",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 3, 
      "input",  "read", "*", "*", dt_no_roi,
      "coarse", "read", "*", "*", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);

  dt_connector_copy(graph, module, 0, id_comb, 0); // rgb input
  CONN(dt_node_connect(graph, guided_exit, 2, id_comb, 1)); // luminance input

  // connect entry point of module (input rgb) to entry point of guided filter
  dt_connector_copy(graph, module, 0, guided_entry, 0);
  // connect entry point of module (input rgb) to exitnode of guided filter that needs it for reconstruction
  dt_connector_copy(graph, module, 0, guided_exit,  0);
  // connect combined rgb to exit point of module
  dt_connector_copy(graph, module, 1, id_comb, 2);
}
