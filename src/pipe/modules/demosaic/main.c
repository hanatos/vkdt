#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if 0
// TODO: put in header!
static inline int
FC(const size_t row, const size_t col, const uint32_t filters)
{
  return filters >> (((row << 1 & 14) + (col & 1)) << 1) & 3;
}
#endif

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  const int block = 1;//module->img_param.filters == 9u ? 3 : 2;
  ri->roi_wd = block*ro->roi_wd;
  ri->roi_ht = block*ro->roi_ht;
  ri->roi_ox = block*ro->roi_ox;
  ri->roi_oy = block*ro->roi_oy;
  ri->roi_scale = 1.0f;

  assert(ro->roi_ox == 0 && "TODO: move to block boundary");
  assert(ro->roi_oy == 0 && "TODO: move to block boundary");
#if 0
  // TODO: fix for x trans once there are roi!
  // also move to beginning of demosaic rggb block boundary
  uint32_t f = module->img_param.filters;
  int ox = 0, oy = 0;
  if(FC(ry,rx,f) == 1)
  {
    if(FC(ry,rx+1,f) == 0) ox = 1;
    if(FC(ry,rx+1,f) == 2) oy = 1;
  }
  else if(FC(ry,rx,f) == 2)
  {
    ox = oy = 1;
  }
  ri->roi_ox += ox;
  ri->roi_oy += oy;
  ri->roi_wd -= ox;
  ri->roi_ht -= oy;
#endif
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // this is just bayer 2x half size for now:
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  // this is rounding down to full bayer block size, which is good:
  const int block = 1;//module->img_param.filters == 9u ? 3 : 2;
  ro->full_wd = ri->full_wd/block;
  ro->full_ht = ri->full_ht/block;
}

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  uint32_t *i = (uint32_t *)module->committed_param;
  i[0] = module->img_param.filters;
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(uint32_t);
  mod->committed_param = malloc(mod->committed_param_size);
  return 0;
}

void cleanup(dt_module_t *mod)
{
  free(mod->committed_param);
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO: need full size connector and half size connector!
  const int block = module->img_param.filters == 9u ? 3 : 2;
  const int wd = module->connector[1].roi.roi_wd;
  const int ht = module->connector[1].roi.roi_ht;
  const int dp = 1;
  dt_roi_t roi_full = module->connector[0].roi;
  dt_roi_t roi_half = module->connector[0].roi;
  roi_half.full_wd /= block;
  roi_half.full_ht /= block;
  roi_half.roi_wd  /= block;
  roi_half.roi_ht  /= block;
  roi_half.roi_ox  /= block;
  roi_half.roi_oy  /= block;
  // TODO roi_half.roi_scale??
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rggb"),
    .format = dt_token("ui16"),
    .roi    = roi_full,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("y"),
    .format = dt_token("f32"),
    .roi    = roi_half,
  };
  dt_connector_t cg = {
    .name   = dt_token("gauss"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgb"),
    .format = dt_token("f32"),
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
  };
  ci.chan   = dt_token("y");
  ci.format = dt_token("f32");
  ci.roi    = roi_half;
  co.chan   = dt_token("rgb");
  co.format = dt_token("f32");
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
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };
  CONN(dt_node_connect(graph, id_down, 1, id_gauss, 0));

  ci.chan   = dt_token("rggb");
  ci.format = dt_token("ui16");
  ci.roi    = roi_full;
  co.chan   = dt_token("rgb");
  co.format = dt_token("f32");
  co.roi    = roi_full;
  cg.chan   = dt_token("rgb");
  cg.format = dt_token("f32");
  cg.roi    = roi_half;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_splat = graph->num_nodes++;
  dt_node_t *node_splat = graph->node + id_splat;
  *node_splat = (dt_node_t) {
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
  };
  dt_connector_copy(graph, module, id_splat,  0, 0);
  CONN(dt_node_connect(graph, id_gauss, 1, id_splat, 1));
  dt_connector_copy(graph, module, id_down,  0, 0);
  dt_connector_copy(graph, module, id_splat, 1, 2);
}
