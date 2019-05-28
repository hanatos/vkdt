#include "module.h"

#include <stdio.h>
#include <string.h>

// TODO: params struct


void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
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
  ro->full_wd = ri->full_wd/2;
  ro->full_ht = ri->full_ht/2;
}
