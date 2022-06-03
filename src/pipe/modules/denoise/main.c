#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  if(module->connector[0].chan == dt_token("rggb"))
  {
    // request the full uncropped thing, we want the borders
    module->connector[0].roi.wd = module->connector[0].roi.full_wd;
    module->connector[0].roi.ht = module->connector[0].roi.full_ht;
    module->connector[0].roi.scale = 1.0f;
  }
  else
  {
    module->connector[0].roi = module->connector[1].roi;
  }
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // copy to output
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return; // input chain disconnected
  assert(img_param);
  const uint32_t *b = img_param->crop_aabb;
  module->connector[1].roi = module->connector[0].roi;
  module->connector[1].roi.full_wd = b[2] - b[0];
  module->connector[1].roi.full_ht = b[3] - b[1];
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  if(parid == 0)
  { // strength
    float oldstr = *(float*)oldval;
    float newstr = dt_module_param_float(module, 0)[0];
    if((oldstr <= 0.0f && newstr >  0.0f) ||
       (oldstr >  0.0f && newstr <= 0.0f))
    return s_graph_run_all;// we need to update the graph topology
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return;
  assert(img_param);
  for(int k=0;k<4;k++)
  { // we will take care of this:
    module->img_param.black[k] = 0.0;
    module->img_param.white[k] = 1.0;
    module->img_param.crop_aabb[0] = 0;
    module->img_param.crop_aabb[1] = 0;
    module->img_param.crop_aabb[2] = module->connector[1].roi.full_wd;
    module->img_param.crop_aabb[3] = module->connector[1].roi.full_ht;
  }

  const float nowb[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const uint32_t *wbi = 
    (module->connector[0].chan != dt_token("rggb")) ? (uint32_t *)nowb :
    (uint32_t *)img_param->whitebalance;
  float black[4], white[4];
  uint32_t *blacki = (uint32_t *)black;
  uint32_t *whitei = (uint32_t *)white;
  const uint32_t *crop_aabb_full = img_param->crop_aabb;
  const float cs = module->connector[0].roi.wd / (float) module->connector[0].roi.full_wd;
  const uint32_t crop_aabb[] = {
    crop_aabb_full[0] *cs,
    crop_aabb_full[1] *cs,
    crop_aabb_full[2] *cs,
    crop_aabb_full[3] *cs};
  for(int k=0;k<4;k++)
    black[k] = img_param->black[k]/65535.0f;
  for(int k=0;k<4;k++)
    white[k] = img_param->white[k]/65535.0f;
  const float noise[2] = { img_param->noise_a, img_param->noise_b };
  uint32_t *noisei = (uint32_t *)noise;

#if 1
  // shortcut if no denoising is requested:
  const float strength = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("strength")))[0];
  if(strength <= 0.0f)
  {
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_noop = graph->num_nodes++;
    graph->node[id_noop] = (dt_node_t) {
      .name   = dt_token("denoise"),
      .kernel = dt_token("noop"),
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
      .push_constant_size = 12*sizeof(uint32_t),
      .push_constant = {
        crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
      },
    };
    dt_connector_copy(graph, module, 0, id_noop, 0);
    dt_connector_copy(graph, module, 1, id_noop, 1);
    return;
  }
#endif

  const int block =
    (module->connector[0].chan != dt_token("rggb")) ? 1 :
    (img_param->filters == 9u ? 3 : 2);
  dt_roi_t roi_half = module->connector[1].roi;
  roi_half.full_wd /= block;
  roi_half.full_ht /= block;
  roi_half.wd /= block;
  roi_half.ht /= block;

  const int wd = roi_half.wd;
  const int ht = roi_half.ht;

  // wire 4 scales of downsample + assembly node
  int id_down[4] = {0};

  for(int i=0;i<4;i++)
  {
    int cov = (module->connector[0].chan == dt_token("rggb")) && (i==0);
    assert(graph->num_nodes < graph->max_nodes);
    id_down[i] = graph->num_nodes++;
    graph->node[id_down[i]] = (dt_node_t) {
      .name   = dt_token("denoise"),
      .kernel = cov ?
        dt_token("downcov") :
        dt_token("down"),
      .module = module,
      .wd     = wd,
      .ht     = ht,
      .dp     = 1,
      .num_connectors = cov ? 3 : 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = (i==0 && block==1) ? module->connector[0].roi : roi_half,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half,
      },{
        .name   = dt_token("cov"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half,
      }},
      .push_constant_size = 20*sizeof(uint32_t),
      .push_constant = {
        wbi[0], wbi[1], wbi[2], wbi[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        (i == 0 && block == 1) ? crop_aabb[0] : 0,
        (i == 0 && block == 1) ? crop_aabb[1] : 0,
        (i == 0 && block == 1) ? crop_aabb[2] : 0,
        (i == 0 && block == 1) ? crop_aabb[3] : 0,
        noisei[0], noisei[1],
        i, block },
    };
  }
  // wire inputs:
  for(int i=1;i<4;i++)
    CONN(dt_node_connect(graph, id_down[i-1], 1, id_down[i], 0));

  // assemble
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_assemble = graph->num_nodes++;
  graph->node[id_assemble] = (dt_node_t) {
    .name   = dt_token("denoise"),
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
      .roi    = block == 1 ? module->connector[0].roi : roi_half,
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
    .push_constant_size = 19*sizeof(uint32_t),
    .push_constant = {
      wbi[0], wbi[1], wbi[2], wbi[3],
      blacki[0], blacki[1], blacki[2], blacki[3],
      whitei[0], whitei[1], whitei[2], whitei[3],
      block == 1 ? crop_aabb[0] : 0,
      block == 1 ? crop_aabb[1] : 0,
      block == 1 ? crop_aabb[2] : 0,
      block == 1 ? crop_aabb[3] : 0,
      noisei[0], noisei[1],
      img_param->filters },
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
      .name   = dt_token("denoise"),
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
      .push_constant_size = 17*sizeof(uint32_t),
      .push_constant = {
        wbi[0], wbi[1], wbi[2], wbi[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
        img_param->filters },
    };
    assert(graph->num_nodes < graph->max_nodes);
    const int id_doub = graph->num_nodes++;
    graph->node[id_doub] = (dt_node_t) {
      .name   = dt_token("denoise"),
      .kernel = dt_token("doub"),
      .module = module,
      .wd     = module->connector[1].roi.wd,
      .ht     = module->connector[1].roi.ht,
      .dp     = 1,
      .num_connectors = 4,
      .connector = {{
        .name   = dt_token("orig"),
        .type   = dt_token("read"),
        .chan   = dt_token("rggb"),
        .format = dt_token("f16"),
        .roi    = module->connector[0].roi, // original rggb input
        .connected_mi = -1,
      },{
        .name   = dt_token("crs0"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half, // cropped lo res
        .connected_mi = -1,
      },{
        .name   = dt_token("crs1"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_half, // cropped lo res
        .flags  = s_conn_smooth,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rggb"),
        .format = dt_token("f16"),
        .roi    = module->connector[1].roi, // cropped hi res
      }},
      .push_constant_size = 19*sizeof(uint32_t),
      .push_constant = {
        wbi[0], wbi[1], wbi[2], wbi[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
        img_param->filters, noisei[0], noisei[1]
      },
    };
    CONN(dt_node_connect(graph, id_assemble, 5, id_doub, 1));
    CONN(dt_node_connect(graph, id_half,     1, id_doub, 2));
    dt_connector_copy(graph, module, 0, id_doub,  0);
    dt_connector_copy(graph, module, 1, id_doub, 3);

    CONN(dt_node_connect(graph, id_half,     1, id_down[0],  0));
    CONN(dt_node_connect(graph, id_half,     1, id_assemble, 0));
    dt_connector_copy(graph, module, 0, id_half, 0);
  }
  else
  { // wire module i/o connectors to nodes:
    dt_connector_copy(graph, module, 0, id_down[0], 0);
    dt_connector_copy(graph, module, 0, id_assemble, 0);
    dt_connector_copy(graph, module, 1, id_assemble, 5);
  }
}

