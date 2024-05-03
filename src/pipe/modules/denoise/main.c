#include "modules/api.h"
#include "pipe/dng_opcode.h"
#include <math.h>
#include <stdlib.h>

typedef struct mod_data_t
{
  dt_dng_gain_map_t *gm[4];
}
mod_data_t;

static int
get_gain_maps_bayer(dt_dng_opcode_list_t *op_list, int index, mod_data_t *dat)
{
  if(op_list->count - index < 4) return 0;
  dt_dng_opcode_t *ops = op_list->ops;
  for(int i=0;i<4;i++) if(ops[i].id != s_dngop_gain_map) return 0;
  for(int i=0;i<4;i++) dat->gm[i] = 0;
  for(int i=0;i<4;i++)
  {
    dt_dng_gain_map_t *gm = (dt_dng_gain_map_t *)ops[i].data;
    if(!(gm->region.plane == 0 && gm->region.planes == 1 && gm->map_planes == 1 &&
         gm->region.row_pitch == 2 && gm->region.col_pitch == 2))
      return 0;
    if(gm->map_points_h < 2 && gm->map_points_v < 2) return 0;
    int filter = ((gm->region.top & 1) << 1) + (gm->region.left & 1);
    dat->gm[filter] = gm;
  }
  for(int i=0;i<4;i++) if(!dat->gm[i]) return 0;
  for(int i=1;i<4;i++)
    if (dat->gm[0]->map_points_h  != dat->gm[i]->map_points_h  ||
        dat->gm[0]->map_points_v  != dat->gm[i]->map_points_v  ||
        dat->gm[0]->map_spacing_h != dat->gm[i]->map_spacing_h ||
        dat->gm[0]->map_spacing_v != dat->gm[i]->map_spacing_v ||
        dat->gm[0]->map_origin_h  != dat->gm[i]->map_origin_h  ||
        dat->gm[0]->map_origin_v  != dat->gm[i]->map_origin_v  ||
        dat->gm[0]->region.top    / 2 != dat->gm[i]->region.top    / 2 ||
        dat->gm[0]->region.left   / 2 != dat->gm[i]->region.left   / 2 ||
        dat->gm[0]->region.bottom / 2 != dat->gm[i]->region.bottom / 2 ||
        dat->gm[0]->region.right  / 2 != dat->gm[i]->region.right  / 2)
      return 0;
  return 1;
}

int
init(dt_module_t *mod)
{
  mod_data_t *d = calloc(sizeof(*d), 1);
  mod->data = d;
  return 0;
}

void
cleanup(dt_module_t *mod)
{
  mod_data_t *d = mod->data;
  free(d);
  mod->data = 0;
}

int
read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  mod_data_t *dat = mod->data;
  if(p->node->kernel == dt_token("gainmap"))
  {
    const int wd = dat->gm[0]->map_points_h, ht = dat->gm[0]->map_points_v;
    for(int i=0;i<ht;i++)
      for(int j=0;j<wd;j++)
        for(int c=0;c<4;c++)
          ((float*)mapped)[(i*wd+j)*4+c] = dat->gm[c]->map_gain[(i*wd+j)];
  }
  return 0;
}

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

  uint32_t gainmap = 0;
  uint32_t gainmap_sx = 0, gainmap_sy = 0, gainmap_ox = 0, gainmap_oy = 0;
  mod_data_t *dat = module->data;
  const dt_image_metadata_dngop_t *dngop = dt_metadata_find(module->img_param.meta, s_image_metadata_dngop);
  dt_dng_opcode_list_t *op_list = dngop->op_list[1];
  if(op_list)
    for(int op=0;op<op_list->count&&!gainmap;op++)
      gainmap = get_gain_maps_bayer(op_list, op, dat);
  int id_gmdata = -1;
  if(gainmap)
  {
    const int map_wd = dat->gm[0]->map_points_h;
    const int map_ht = dat->gm[0]->map_points_v;
    dt_roi_t gmdata_roi = (dt_roi_t){ .wd = map_wd, .ht = map_ht };
    id_gmdata = dt_node_add(graph, module, "denoise", "gainmap", map_wd, map_ht, 1, 0, 0, 1,
        "source", "source", "rgba", "f32", &gmdata_roi);
    float ox = dat->gm[0]->map_origin_h;
    float oy = dat->gm[0]->map_origin_v;
    gainmap_ox = *(uint32_t*)&ox;
    gainmap_oy = *(uint32_t*)&oy;
    float sx = 1.0 / (dat->gm[0]->map_spacing_h * map_wd);
    float sy = 1.0 / (dat->gm[0]->map_spacing_v * map_ht);
    gainmap_sx = *(uint32_t*)&sx;
    gainmap_sy = *(uint32_t*)&sy;
  }

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
      "gainmap", "read",  "rgba", "f32", dt_no_roi);
    dt_connector_copy(graph, module, 0, id_noop, 0);
    if(gainmap) CONN(dt_node_connect(graph, id_gmdata, 0, id_noop, 2));
    else dt_connector_copy(graph, module, 0, id_noop, 2);
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
      "gainmap", "read", "rgba", "f32", dt_no_roi);
    CONN(dt_node_connect(graph, id_assemble, 5, id_doub, 1));
    CONN(dt_node_connect(graph, id_half,     1, id_doub, 2));
    if(gainmap) CONN(dt_node_connect(graph, id_gmdata, 0, id_doub, 4));
    else CONN(dt_node_connect(graph, id_half, 1, id_doub, 4)); // connect dummy gainmap
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
