#include "modules/api.h"
#include <inttypes.h>

dt_graph_run_t
check_params(
    dt_module_t *mod,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int parw = dt_module_get_param(mod->so, dt_token("wd"));
  const int parh = dt_module_get_param(mod->so, dt_token("ht"));
  if(parid == parw || parid == parh)
  { // dimensions wd or ht
    int oldsz = *(int*)oldval;
    int newsz = dt_module_param_int(mod, parid)[0];
    if(oldsz != newsz) return s_graph_run_all; // we need to update the graph topology
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *mod)
{
  const int p_wd = dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("wd")))[0];
  const int p_ht = dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("ht")))[0];
  int wd = p_wd, ht = p_ht;
  mod->connector[0].roi.full_wd = mod->connector[3].roi.full_wd = wd;
  mod->connector[0].roi.full_ht = mod->connector[3].roi.full_ht = ht;
  mod->connector[0].roi.scale   = mod->connector[3].roi.scale = 1;
  mod->img_param   = (dt_image_params_t) {
    .black            = {0, 0, 0, 0},
    .white            = {65535,65535,65535,65535},
    .whitebalance     = {1.0, 1.0, 1.0, 1.0},
    .filters          = 0, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb        = {0, 0, wd, ht},
    .colour_trc       = s_colour_trc_linear,
    .colour_primaries = s_colour_primaries_2020,
    .cam_to_rec2020   = {0.0f/0.0f, 0, 0, 0, 1, 0, 0, 0, 1},
    .noise_a          = 0.0,
    .noise_b          = 0.0,
    .orientation      = 0,
  };
}
