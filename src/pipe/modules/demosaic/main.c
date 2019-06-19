#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  // TODO: make sure rounding correctly!
  ri->roi_wd = 2*ro->roi_wd;
  ri->roi_ht = 2*ro->roi_ht;
  // TODO: compute these from ro->scale and ro->ox
  ri->roi_ox = 0;
  ri->roi_oy = 0;
  ri->roi_scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // this is just bayer 2x half size for now:
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  // TODO: make sure rounding correctly!
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
