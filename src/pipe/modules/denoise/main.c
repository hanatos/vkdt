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
    uint32_t     num,
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

  const uint32_t gainmap = 0;
  const uint32_t gainmap_sx = 0, gainmap_sy = 0, gainmap_ox = 0, gainmap_oy = 0;

  // shortcut if no denoising is requested:
  const float strength = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("strength")))[0];
  if(strength <= 0.0f)
  {
    const int32_t pc[] = {
      crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
      blacki[0], blacki[1], blacki[2], blacki[3],
      whitei[0], whitei[1], whitei[2], whitei[3],
      gainmap_ox, gainmap_oy, gainmap_sx, gainmap_sy,
      img_param->filters, gainmap
    };
    const uint32_t id_noop = dt_node_add(graph, module, "denoise", "noop", 
      module->connector[1].roi.wd, module->connector[1].roi.ht, 1, sizeof(pc), pc, 3,
      "input",   "read",  "rgba", "f16", dt_no_roi,
      "output",  "write", "rgba", "f16", &module->connector[1].roi,
      "gainmap", "read",  "rgba", "f16", dt_no_roi);
    dt_connector_copy(graph, module, 0, id_noop, 0);
    dt_connector_copy(graph, module, 0, id_noop, 2);
    dt_connector_copy(graph, module, 1, id_noop, 1);
    return;
  }

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
     const int32_t pc[] = {
        wbi[0], wbi[1], wbi[2], wbi[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        (i == 0 && block == 1) ? crop_aabb[0] : 0,
        (i == 0 && block == 1) ? crop_aabb[1] : 0,
        (i == 0 && block == 1) ? crop_aabb[2] : 0,
        (i == 0 && block == 1) ? crop_aabb[3] : 0,
        noisei[0], noisei[1],
        i, block };
    int cov = (module->connector[0].chan == dt_token("rggb")) && (i==0);
    id_down[i] = dt_node_add(graph, module, "denoise", cov ? "downcov" : "down",
        wd, ht, 1, sizeof(pc), pc, cov ? 3 : 2,
        "input",  "read",  "rgba", "f16", dt_no_roi,
        "output", "write", "rgba", "f16", &roi_half,
        "cov",    "write", "rgba", "f16", &roi_half);
  }
  // wire inputs:
  for(int i=1;i<4;i++)
    CONN(dt_node_connect(graph, id_down[i-1], 1, id_down[i], 0));

  // assemble
  const int32_t pcas[] = {
    wbi[0], wbi[1], wbi[2], wbi[3],
    blacki[0], blacki[1], blacki[2], blacki[3],
    whitei[0], whitei[1], whitei[2], whitei[3],
    block == 1 ? crop_aabb[0] : 0,
    block == 1 ? crop_aabb[1] : 0,
    block == 1 ? crop_aabb[2] : 0,
    block == 1 ? crop_aabb[3] : 0,
    noisei[0], noisei[1],
    img_param->filters };
  const uint32_t id_assemble = dt_node_add(graph, module, "denoise", "assemble", wd, ht, 1,
      sizeof(pcas), pcas, 6,
      "s0", "read", "rgba", "f16", dt_no_roi,
      "s1", "read", "rgba", "f16", dt_no_roi,
      "s2", "read", "rgba", "f16", dt_no_roi,
      "s3", "read", "rgba", "f16", dt_no_roi,
      "s4", "read", "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &roi_half);
  // wire downsampled to assembly stage:
  CONN(dt_node_connect(graph, id_down[0], 1, id_assemble, 1));
  CONN(dt_node_connect(graph, id_down[1], 1, id_assemble, 2));
  CONN(dt_node_connect(graph, id_down[2], 1, id_assemble, 3));
  CONN(dt_node_connect(graph, id_down[3], 1, id_assemble, 4));

  if(module->connector[0].chan == dt_token("rggb"))
  { // raw data. need to wrap into mosaic-aware nodes:
    int32_t pch[] = {
      wbi[0], wbi[1], wbi[2], wbi[3],
      blacki[0], blacki[1], blacki[2], blacki[3],
      whitei[0], whitei[1], whitei[2], whitei[3],
      crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
      img_param->filters };
    const uint32_t id_half = dt_node_add(graph, module, "denoise", "half", roi_half.full_wd, roi_half.full_ht, 1, sizeof(pch), pch, 2,
        "input",  "read",  "rggb", "ui16", dt_no_roi,
        "output", "write", "rgba", "f16", &roi_half);
    int32_t pc[] = {
        wbi[0], wbi[1], wbi[2], wbi[3],
        blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        crop_aabb[0], crop_aabb[1], crop_aabb[2], crop_aabb[3],
        img_param->filters, noisei[0], noisei[1],
        gainmap, gainmap_ox, gainmap_oy, gainmap_sx, gainmap_sy
    };
    const int id_doub = dt_node_add(graph, module, "denoise", "doub", 
      module->connector[1].roi.wd, module->connector[1].roi.ht, 1, sizeof(pc), pc, 5,
      "orig", "read", "rggb", "f16", dt_no_roi,
      "crs0", "read", "rgba", "f16", dt_no_roi,
      "crs1", "read", "rgba", "f16", dt_no_roi,
      "output", "write", "rggb", "f16", &module->connector[1].roi,
      "gainmap", "read", "rgba", "f16", dt_no_roi);
    CONN(dt_node_connect(graph, id_assemble, 5, id_doub, 1));
    CONN(dt_node_connect(graph, id_half,     1, id_doub, 2));
    CONN(dt_node_connect(graph, id_half,     1, id_doub, 4)); // connect dummy gainmap
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
