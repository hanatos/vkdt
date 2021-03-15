#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

// request everything
void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // request the full thing, we'll rescale
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.scale = 1.0f;
}

#if 0
// XXX do we need this? i think only for magnification
void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // copy to output
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  assert(img_param);
  const uint32_t *b = img_param->crop_aabb;
  module->connector[1].roi = module->connector[0].roi;
  module->connector[1].roi.full_wd = b[2] - b[0];
  module->connector[1].roi.full_ht = b[3] - b[1];
}
#endif

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  if(module->connector[0].roi.wd == module->connector[1].roi.wd &&
     module->connector[0].roi.ht == module->connector[1].roi.ht)
  {
    dt_connector_bypass(graph, module, 0, 1);
  }
  else
  {
    assert(graph->num_nodes < graph->max_nodes);
    const int nodeid = graph->num_nodes++;
    graph->node[nodeid] = (dt_node_t) {
      .name   = module->name,
      .kernel = dt_token("main"),
      .module = module,
      .wd     = module->connector[1].roi.wd,
      .ht     = module->connector[1].roi.ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"), // will be overwritten soon
        .format = dt_token("f16"),
        .roi    = module->connector[0].roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"), // will be overwritten soon
        .format = dt_token("f16"),
        .roi    = module->connector[1].roi,
      }},
    };
    dt_connector_copy(graph, module, 0, nodeid, 0);
    dt_connector_copy(graph, module, 1, nodeid, 1);
  }
}

