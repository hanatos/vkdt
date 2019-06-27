#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: put in header!
static inline int
FC(const size_t row, const size_t col, const uint32_t filters)
{
  return filters >> (((row << 1 & 14) + (col & 1)) << 1) & 3;
}

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  ri->roi_wd = 2*ro->roi_wd;
  ri->roi_ht = 2*ro->roi_ht;
  int rx = ri->roi_ox = 2*ro->roi_ox;
  int ry = ri->roi_oy = 2*ro->roi_oy;
  ri->roi_scale = 1.0f;

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
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // this is just bayer 2x half size for now:
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  // this is rounding down to full bayer block size, which is good:
  ro->full_wd = ri->full_wd/2;
  ro->full_ht = ri->full_ht/2;
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
