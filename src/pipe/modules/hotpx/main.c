#include "modules/api.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return;
  struct { int32_t crop[2], filters; float black, white, noise_a, noise_b; } pc = {
    { img_param->crop_aabb[0], img_param->crop_aabb[1] },
    img_param->filters, img_param->black[1]/65535.0f, img_param->white[1]/65535.0f,
    img_param->noise_a, img_param->noise_b };
  const int block = img_param->filters == 9 ? 3 : 2; // one invocation per block of the mosaic, and one more for a partial block at each end
  const int id = dt_node_add(graph, module, "hotpx", "main",
      module->connector[0].roi.wd / block + 2, module->connector[0].roi.ht / block + 2, 1, sizeof(pc), (const int *)&pc, 2,
      "input",  "read",  "r", "f16", dt_no_roi,
      "output", "write", "r", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, id, 0);
  dt_connector_copy(graph, module, 1, id, 1);
}
