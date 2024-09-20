#include "modules/api.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return;
  int pc[] = {img_param->crop_aabb[0], img_param->crop_aabb[1], img_param->crop_aabb[2], img_param->crop_aabb[3], img_param->filters};
  const int id = dt_node_add(graph, module, "hotpx", "main", module->connector[0].roi.wd, module->connector[0].roi.ht, 1, sizeof(pc), pc, 2,
      "input",  "read",  "r", "f16", dt_no_roi,
      "output", "write", "r", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, id, 0);
  dt_connector_copy(graph, module, 1, id, 1);
}
