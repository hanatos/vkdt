#include "modules/api.h"
#include "core/log.h"
#include "config.h"

static void generate_radius_lut_dng(
    dt_image_params_t *img_param,
    float             *lut)
{
  dt_lens_corr_dng_t *dng = &img_param->lens_corr.params.dng;

  if(dng->warp_rect_coef[0].kt0 != 0 || dng->warp_rect_coef[1].kt1 != 0
    || dng->warp_rect_coef[0].kt0 != 0 || dng->warp_rect_coef[1].kt1 != 0
    || dng->warp_rect_coef[0].kt0 != 0 || dng->warp_rect_coef[1].kt1 != 0)
  {
    dt_log(s_log_err, "[lenswarp] Ignoring WarpRectilinear tangential coefficients");
  }

  // For WarpRectilinear, radius 1 is defined as the distance from the center to
  // the furthest corner of the uncropped image (or ActiveArea?). For the LUT we
  // want to define radius 1 as the corner of the crop area.

  // pixel location of center in the uncropped image
  float cx = dng->warp_rect_center_x * img_param->uncropped[0];
  float cy = dng->warp_rect_center_y * img_param->uncropped[1];
  // distance to furthest corner of uncropped image
  float dng_dist = hypotf(
    MAX(cx, img_param->uncropped[0] - cx),
    MAX(cy, img_param->uncropped[1] - cy));
  // distance to furthest corner of cropped image
  float crop_dist = hypotf(
    MAX(cx - img_param->crop_aabb[0], img_param->crop_aabb[2] - cx),
    MAX(cy - img_param->crop_aabb[1], img_param->crop_aabb[3] - cy));
  float r_scale = crop_dist / dng_dist;

  for(int ch = 0; ch < 3; ch++)
  {
    dt_warp_rect_coef_t *c = &dng->warp_rect_coef[ch];
    for(int i = 0; i < LUT_SIZE; i++)
    {
      float r = (float)i / (LUT_SIZE - 1); // radius in the undistorted output image
      float rs2 = r * r * r_scale;
      lut[i * 4 + ch] = c->kr0 + rs2 * (c->kr1 + rs2 * (c->kr2 + rs2 * c->kr3));
    }
  }
}

static void get_center_dng(
    dt_image_params_t *img_param,
    float             *center)
{
  // TODO this might actually be relative to the ActiveArea not the full uncropped area?

  dt_lens_corr_dng_t *dng = &img_param->lens_corr.params.dng;
  // pixel location of center in the uncropped image
  float cx = dng->warp_rect_center_x * img_param->uncropped[0];
  float cy = dng->warp_rect_center_y * img_param->uncropped[1];
  // pixel location of center in the cropped image
  cx -= img_param->crop_aabb[0];
  cy -= img_param->crop_aabb[1];
  // location of center as fraction of the cropped image size
  cx /= (img_param->crop_aabb[2] - img_param->crop_aabb[0]);
  cy /= (img_param->crop_aabb[3] - img_param->crop_aabb[1]);
  center[0] = cx;
  center[1] = cy;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  switch(mod->img_param.lens_corr.type)
  {
    case s_lens_corr_dng:
      generate_radius_lut_dng(&mod->img_param, (float *)mapped);
      break;
    default:
      break;
  }
  return 0;
}

void commit_params(dt_graph_t *graph, dt_module_t *mod)
{
  float *f = (float *)mod->committed_param;
  switch(mod->img_param.lens_corr.type)
  {
    case s_lens_corr_dng:
      get_center_dng(&mod->img_param, &f[0]);
      break;
    default:
      f[0] = -1; // no-op
      break;
  }

  const float scale  = dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("scale")))[0];
  f[2] = scale;
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(float)*(3);
  return 0;
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t lutroi = (dt_roi_t){ .wd = LUT_SIZE, .ht = 1 };
  const int id_radlut = dt_node_add(graph, module, "radlut", "source",
    LUT_SIZE, 1, 1, 0, 0, 1,
    "source", "source", "rgba", "f32", &lutroi);

  const int id_main = dt_node_add(graph, module, "lenswarp", "main",
    module->connector[0].roi.wd, module->connector[0].roi.ht, 1,
    0, 0, 3,
    "input",  "read",  "rgba", "f16", &module->connector[0].roi,
    "output", "write", "rgba", "f16", &module->connector[0].roi,
    "radlut", "read",  "rgba", "f32", &lutroi);

  CONN(dt_node_connect(graph, id_radlut, 0, id_main, 2));
  dt_connector_copy(graph, module, 0, id_main, 0);
  dt_connector_copy(graph, module, 1, id_main, 1);
}
