#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // request the full thing, we want the borders
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.x = 0.0f;
  module->connector[0].roi.y = 0.0f;
  module->connector[0].roi.scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // copy to output
  if(module->connector[0].chan == dt_token("rggb"))
  {
    const uint32_t *b = module->img_param.crop_aabb;
    module->connector[1].roi = module->connector[0].roi;
    // TODO: double check potential rounding issues here (align to mosaic blocks etc):
    module->connector[1].roi.full_wd = b[2] - b[0];
    module->connector[1].roi.full_ht = b[3] - b[1];
  }
  else
  {
    module->connector[1].roi = module->connector[0].roi;
  }
}

void
commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  float *f = (float *)module->committed_param;
  float *g = (float*)module->param;
  f[0] = g[0];
  f[1] = g[1];
  f[2] = g[2];
  f[3] = 0.0f;// module->img_param.black[1]; // XXX TODO: set this to zero after removing black level!
  // TODO: how to do this? we can't overwrite img_param in denoise since we need it next time
  // TODO: and we can't access the (potentially multiple) modules connected to our output
  f[4] = module->img_param.noise_a;
  f[5] = module->img_param.noise_b;
}

int
init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*6;
  return 0;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int block =
    (module->connector[0].chan != dt_token("rggb")) ? 1 :
    (module->img_param.filters == 9u ? 3 : 2);
  dt_roi_t roi_half = module->connector[1].roi;
  roi_half.full_wd /= block;
  roi_half.full_ht /= block;
  roi_half.wd /= block;
  roi_half.ht /= block;
  roi_half.x  /= block;
  roi_half.y  /= block;

  const int wd = roi_half.full_wd;
  const int ht = roi_half.full_ht;

  // wire 4 scales of downsample + assembly node
  int id_down[4] = {0};

  for(int i=0;i<4;i++)
  {
    assert(graph->num_nodes < graph->max_nodes);
    id_down[i] = graph->num_nodes++;
    graph->node[id_down[i]] = (dt_node_t) {
      .name   = dt_token("dev"),
      .kernel = dt_token("down"),
      .module = module,
      .wd     = wd,
      .ht     = ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half,
      }},
      .push_constant_size = sizeof(uint32_t),
      .push_constant = { i },
    };
  }
  // wire inputs:
  for(int i=1;i<4;i++)
    CONN(dt_node_connect(graph, id_down[i-1], 1, id_down[i], 0));

  // assemble
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_assemble = graph->num_nodes++;
  graph->node[id_assemble] = (dt_node_t) {
    .name   = dt_token("dev"),
    .kernel = dt_token("assemble"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = 1,
    .num_connectors = 6,
    .connector = {{
      .name   = dt_token("s0"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s1"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s2"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s3"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("s4"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
    }},
  };

  // wire downsampled to assembly stage:
  CONN(dt_node_connect(graph, id_down[0], 1, id_assemble, 1));
  CONN(dt_node_connect(graph, id_down[1], 1, id_assemble, 2));
  CONN(dt_node_connect(graph, id_down[2], 1, id_assemble, 3));
  CONN(dt_node_connect(graph, id_down[3], 1, id_assemble, 4));

  if(module->connector[0].chan == dt_token("rggb"))
  { // raw data. need to wrap into mosaic-aware nodes:
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_half = graph->num_nodes++;
    graph->node[id_half] = (dt_node_t) {
      .name   = dt_token("dev"),
      .kernel = dt_token("half"),
      .module = module,
      .wd     = roi_half.full_wd,
      .ht     = roi_half.full_ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rggb"),
        .format = dt_token("ui16"),
        .roi    = module->connector[0].roi, // uncropped hi res
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half, // cropped lo res
      }},
      // XXX noise levels, white and black points, crop window!
      .push_constant_size = 5*sizeof(uint32_t),
      .push_constant = { 0, 0, 0, 0, module->img_param.filters },
    };
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_doub = graph->num_nodes++;
    graph->node[id_doub] = (dt_node_t) {
      .name   = dt_token("dev"),
      .kernel = dt_token("doub"),
      .module = module,
      .wd     = roi_half.full_wd, // cropped lo res
      .ht     = roi_half.full_ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half, // cropped lo res
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rggb"),
        .format = dt_token("f16"),
        .roi    = module->connector[1].roi, // cropped hi res
      }},
      .push_constant_size = 5*sizeof(uint32_t),
      .push_constant = { 0, 0, 0, 0, module->img_param.filters },
    };
    CONN(dt_node_connect(graph, id_half,     1, id_down[0],  0));
    CONN(dt_node_connect(graph, id_half,     1, id_assemble, 0));
    CONN(dt_node_connect(graph, id_assemble, 5, id_doub,     0));
    dt_connector_copy(graph, module, 0, id_half, 0);
    dt_connector_copy(graph, module, 0, id_doub, 1);
    dt_connector_copy(graph, module, 1, id_doub, 2);
  }
  else
  { // wire module i/o connectors to nodes:
    dt_connector_copy(graph, module, 0, id_down[0], 0);
    dt_connector_copy(graph, module, 0, id_assemble, 0);
    dt_connector_copy(graph, module, 1, id_assemble, 5);
  }
}

