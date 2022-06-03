#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;

  ri->wd = ri->full_wd;
  ri->ht = ri->full_ht;
  ri->scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;

  // always give full size and negotiate half or not in modify_roi_in
  ro->full_wd = ri->full_wd;
  ro->full_ht = ri->full_ht;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return; // must have disconnected input somewhere
  const int block = img_param->filters == 9u ? 3 : 2;
  module->img_param.filters = 0u; // after we're done there won't be any more mosaic
  dt_roi_t roi_full = module->connector[0].roi;
  dt_roi_t roi_half = module->connector[0].roi;
  roi_half.full_wd /= block;
  roi_half.full_ht /= block;
  roi_half.wd /= block;
  roi_half.ht /= block;
  if(module->connector[1].roi.scale >= block)
  { // half size
    // we do whatever the default implementation would have done, too:
    dt_connector_t ci = {
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rggb"),
      .format = dt_token("*"),
      .roi    = roi_full,
      .connected_mi = -1,
    };
    dt_connector_t co = {
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = roi_half,
    };
    assert(graph->num_nodes < graph->max_nodes);
    const int id_half = graph->num_nodes++;
    dt_node_t *node_half = graph->node + id_half;
    *node_half = (dt_node_t) {
      .name   = dt_token("demosaic"),
      .kernel = dt_token("halfsize"),
      .module = module,
      .wd     = roi_half.wd,
      .ht     = roi_half.ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {
        ci, co,
      },
      .push_constant_size = sizeof(uint32_t),
      .push_constant = { img_param->filters },
    };
    // resample to get to the rest of the resolution, only if block != scale!
    dt_connector_copy(graph, module, 0, id_half, 0);
    if(block != module->connector[1].roi.scale)
    {
      ci.chan   = dt_token("rgba");
      ci.format = dt_token("f16");
      ci.roi    = roi_half;
      ci.flags  = s_conn_smooth;
      co.chan   = dt_token("rgba");
      co.format = dt_token("f16");
      co.roi    = module->connector[1].roi;
      assert(graph->num_nodes < graph->max_nodes);
      const int id_resample = graph->num_nodes++;
      dt_node_t *node_resample = graph->node + id_resample;
      *node_resample = (dt_node_t) {
        .name   = dt_token("shared"),
        .kernel = dt_token("resample"),
        .module = module,
        .wd     = co.roi.wd,
        .ht     = co.roi.ht,
        .dp     = 1,
        .num_connectors = 2,
        .connector = {
          ci, co,
        },
      };
      CONN(dt_node_connect(graph, id_half, 1, id_resample, 0));
      dt_connector_copy(graph, module, 1, id_resample, 1);
    }
    else
    {
      dt_connector_copy(graph, module, 1, id_half, 1);
    }
    return;
  }

  // else: full size
  // need full size connectors and half size connectors:
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  const int dp = 1;
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rggb"),
    .format = dt_token("*"),
    .roi    = roi_full,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("y"),
    .format = dt_token("f16"),
    .roi    = roi_half,
  };
  dt_connector_t cg = {
    .name   = dt_token("gauss"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = roi_half,
    .connected_mi = -1,
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_down = graph->num_nodes++;
  dt_node_t *node_down = graph->node + id_down;
  *node_down = (dt_node_t) {
    .name   = dt_token("demosaic"),
    .kernel = dt_token("down"),
    .module = module,
    .wd     = wd/block,
    .ht     = ht/block,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
    .push_constant_size = sizeof(uint32_t),
    .push_constant = { img_param->filters },
  };
  ci.chan   = dt_token("y");
  ci.format = dt_token("f16");
  ci.roi    = roi_half;
  co.chan   = dt_token("rgba");
  co.format = dt_token("f16");
  co.roi    = roi_half;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_gauss = graph->num_nodes++;
  dt_node_t *node_gauss = graph->node + id_gauss;
  *node_gauss = (dt_node_t) {
    .name   = dt_token("demosaic"),
    .kernel = dt_token("gauss"),
    .module = module,
    .wd     = wd/block,
    .ht     = ht/block,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = roi_half,
      .connected_mi = -1,
    },{
      .name   = dt_token("orig"),
      .type   = dt_token("read"),
      .chan   = dt_token("rggb"),
      .format = dt_token("*"),
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
    .push_constant = { img_param->filters },
  };
  CONN(dt_node_connect(graph, id_down, 1, id_gauss, 0));
  dt_connector_copy(graph, module, 0, id_gauss, 1);

  ci.chan   = dt_token("rggb");
  ci.format = dt_token("*");
  ci.roi    = roi_full;
  co.chan   = dt_token("g");
  co.format = dt_token("f16");
  co.roi    = roi_full;
  cg.chan   = dt_token("rgba");
  cg.format = dt_token("f16");
  cg.roi    = roi_half;
  cg.flags  = s_conn_smooth;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_splat = graph->num_nodes++;
  graph->node[id_splat] = (dt_node_t) {
    .name   = dt_token("demosaic"),
    .kernel = dt_token("splat"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {
      ci, cg, co,
    },
    .push_constant_size = sizeof(uint32_t),
    .push_constant = { img_param->filters },
  };
  dt_connector_copy(graph, module, 0, id_splat, 0);
  CONN(dt_node_connect(graph, id_gauss, 2, id_splat, 1));
  dt_connector_copy(graph, module, 0, id_down,  0);
  // XXX DEBUG see output of gaussian params
  // dt_connector_copy(graph, module, 1, id_gauss, 2);
  // return;

  // fix colour casts
  assert(graph->num_nodes < graph->max_nodes);
  const int id_fix = graph->num_nodes++;
  graph->node[id_fix] = (dt_node_t) {
    .name   = dt_token("demosaic"),
    .kernel = dt_token("fix"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 4,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rggb"),
      .format = dt_token("*"),
      .roi    = roi_full,
      .connected_mi = -1,
    },{
      .name   = dt_token("green"),
      .type   = dt_token("read"),
      .chan   = dt_token("g"),
      .format = dt_token("*"),
      .roi    = roi_full,
      .connected_mi = -1,
    },{
      .name   = dt_token("cov"),
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
      .roi    = roi_full,
    }},
    .push_constant_size = sizeof(uint32_t),
    .push_constant = { img_param->filters },
  };
  dt_connector_copy(graph, module, 0, id_fix, 0);
  CONN(dt_node_connect(graph, id_splat, 2, id_fix, 1));
  CONN(dt_node_connect(graph, id_gauss, 2, id_fix, 2));

  if(module->connector[1].roi.scale != 1.0)
  { // add resample node to graph, copy its output instead:
    ci.chan   = dt_token("rgba");
    ci.format = dt_token("f16");
    ci.roi    = roi_full;
    ci.flags  = s_conn_smooth;
    co.chan   = dt_token("rgba");
    co.format = dt_token("f16");
    co.roi    = module->connector[1].roi;
    assert(graph->num_nodes < graph->max_nodes);
    const int id_resample = graph->num_nodes++;
    graph->node[id_resample] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("resample"),
      .module = module,
      .wd     = co.roi.wd,
      .ht     = co.roi.ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {
        ci, co,
      },
    };
    CONN(dt_node_connect(graph, id_fix, 3, id_resample, 0));
    dt_connector_copy(graph, module, 1, id_resample, 1);
  }
  else
  { // directly output demosaicing result:
    dt_connector_copy(graph, module, 1, id_fix, 3);
  }
}
