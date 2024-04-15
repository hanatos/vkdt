#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  return s_graph_run_all; // re-run modify_roi_out to re-allocate output buffer
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  int p_chan = dt_module_param_int(mod, 0)[0];
  int p_form = dt_module_param_int(mod, 1)[0];
  mod->connector[1].chan = p_chan == 0 ? dt_token("r") : p_chan == 1 ? dt_token("rg") : dt_token("rgba");
  switch(p_form)
  {
    case 0:
      mod->connector[1].format = dt_token("f32");
      break;
    case 1:
      mod->connector[1].format = dt_token("f16");
      break;
    case 2:
      mod->connector[1].format = dt_token("ui32");
      break;
    case 3:
      mod->connector[1].format = dt_token("ui16");
      break;
    case 4:
      mod->connector[1].format = dt_token("ui8");
      break;
    default:
      break;
  }
  mod->connector[1].roi.full_wd = mod->connector[0].roi.full_wd;
  mod->connector[1].roi.full_ht = mod->connector[0].roi.full_ht;
}
