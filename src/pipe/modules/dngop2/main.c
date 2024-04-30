#include "modules/api.h"

// Set of four GainMaps to be applied to a Bayer mosaic image.
// There is one for each of the four filters of a Bayer block:
// 0: upper left 1: upper right 2: lower left 3: lower right
// All four have the exact same shape and are to be applied to the exact same
// region of the image.
typedef struct gain_maps_bayer_t
{
  dt_dng_gain_map_t *gm[4];
}
gain_maps_bayer_t;

typedef struct dngop2_data_t
{
  gain_maps_bayer_t gain_maps_bayer;
}
dngop2_data_t;

static int
get_gain_maps_bayer(dt_dng_opcode_list_t *op_list, int index, gain_maps_bayer_t *gmb)
{
  // We are looking for four consecutive GainMaps in the opcode list starting at
  // index which are to be applied to a Bayer mosaic image.
  if(op_list->count - index < 4)
    return 0;
  dt_dng_opcode_t *ops = op_list->ops;
  for(int i=0;i<4;i++)
    if(ops[i].id != s_dngop_gain_map)
      return 0;
  // We have four GainMaps - is there one for each Bayer filter?
  for(int i=0;i<4;i++)
    gmb->gm[i] = NULL;
  for(int i=0;i<4;i++)
  {
    dt_dng_gain_map_t *gm = (dt_dng_gain_map_t *)ops[i].data;
    // Check that this is for a Bayer filter
    if(!(gm->region.plane == 0 && gm->region.planes == 1 && gm->map_planes == 1 &&
         gm->region.row_pitch == 2 && gm->region.col_pitch == 2))
      return 0;
    // Check that this is not a 1x1 no-op
    if(gm->map_points_h < 2 && gm->map_points_v < 2)
      return 0;
    int filter = ((gm->region.top & 1) << 1) + (gm->region.left & 1);
    gmb->gm[filter] = gm;
  }
  // Check that we found all four GainMaps
  for(int i=0;i<4;i++)
    if(gmb->gm[i] == NULL)
      return 0;
  // Check that all four GainMaps have the exact same shape and the corners of
  // the image region are in the same Bayer block
  for(int i=1;i<4;i++)
  {
    if (gmb->gm[0]->map_points_h  != gmb->gm[i]->map_points_h  ||
        gmb->gm[0]->map_points_v  != gmb->gm[i]->map_points_v  ||
        gmb->gm[0]->map_spacing_h != gmb->gm[i]->map_spacing_h ||
        gmb->gm[0]->map_spacing_v != gmb->gm[i]->map_spacing_v ||
        gmb->gm[0]->map_origin_h  != gmb->gm[i]->map_origin_h  ||
        gmb->gm[0]->map_origin_v  != gmb->gm[i]->map_origin_v  ||
        gmb->gm[0]->region.top    / 2 != gmb->gm[i]->region.top    / 2 ||
        gmb->gm[0]->region.left   / 2 != gmb->gm[i]->region.left   / 2 ||
        gmb->gm[0]->region.bottom / 2 != gmb->gm[i]->region.bottom / 2 ||
        gmb->gm[0]->region.right  / 2 != gmb->gm[i]->region.right  / 2)
      return 0;
  }

  return 1;
}

static void
get_gain_maps_bayer_data(gain_maps_bayer_t *gmb, float *data)
{
  const int wd = gmb->gm[0]->map_points_h;
  const int ht = gmb->gm[0]->map_points_v;
  for(int i=0;i<ht;i++)
    for(int j=0;j<wd;j++)
      for(int c=0;c<4;c++)
        data[(i*wd+j)*4+c] = gmb->gm[c]->map_gain[(i*wd+j)];
}

int
init(dt_module_t *mod)
{
  dngop2_data_t *d = calloc(sizeof(*d), 1);
  mod->data = d;
    return 0;
}

void
cleanup(dt_module_t *mod)
{
  dngop2_data_t *d = mod->data;
  free(d);
  mod->data = 0;
}

int
read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  dngop2_data_t *d = mod->data;
  if(p->node->name == dt_token("gmdata"))
    get_gain_maps_bayer_data(&d->gain_maps_bayer, (float *)mapped);
  return 0;
}

static void
create_nodes_op_list(
    dt_graph_t  *graph,
    dt_module_t *module,
    dt_dng_opcode_list_t *op_list)
{
  dngop2_data_t *d = module->data;

  // id of the preceding opcode node's connector. -1 indicates this is the first
  // opcode and should be connected to the module input.
  int prev_nid = -1;
  int prev_nc;

  int has_gain_map = 0;
  for(int iop=0;iop<op_list->count;iop++)
  {
    gain_maps_bayer_t gain_maps_bayer;
    if(get_gain_maps_bayer(op_list, iop, &gain_maps_bayer))
    {
      if(has_gain_map)
        continue; // Only apply one set even if there are more in the file

      memcpy(&d->gain_maps_bayer, &gain_maps_bayer, sizeof(gain_maps_bayer));

      const int map_wd = gain_maps_bayer.gm[0]->map_points_h;
      const int map_ht = gain_maps_bayer.gm[0]->map_points_v;
      dt_roi_t gmdata_roi = (dt_roi_t){ .wd = map_wd, .ht = map_ht };
      const int id_gmdata = dt_node_add(graph, module, "gmdata", "source",
        map_wd, map_ht, 1,
        0, 0, 1,
        "source", "source", "rgba", "f32", &gmdata_roi);

      int32_t gm_pc[8];
      float *gm_pc_f = (float *)gm_pc;
      gm_pc[0]   = gain_maps_bayer.gm[0]->region.left;
      gm_pc[1]   = gain_maps_bayer.gm[0]->region.top;
      gm_pc[2]   = gain_maps_bayer.gm[3]->region.right;
      gm_pc[3]   = gain_maps_bayer.gm[3]->region.bottom;
      gm_pc_f[4] = gain_maps_bayer.gm[0]->map_origin_h;
      gm_pc_f[5] = gain_maps_bayer.gm[0]->map_origin_v;
      gm_pc_f[6] = 1.0 / (gain_maps_bayer.gm[0]->map_spacing_h * (map_wd - 1));
      gm_pc_f[7] = 1.0 / (gain_maps_bayer.gm[0]->map_spacing_v * (map_ht - 1));
      const int id_gainmap = dt_node_add(graph, module, "dngop2", "gainmap",
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1,
        sizeof(gm_pc), gm_pc, 3,
        "input",  "read",  "rgba", "f16", &module->connector[0].roi,
        "output", "write", "rgba", "f16", &module->connector[0].roi,
        "gmdata", "read",  "rgba", "f32", &gmdata_roi);

      CONN(dt_node_connect(graph, id_gmdata, 0, id_gainmap, 2));

      has_gain_map = 1;
      iop += 3; // Skip over all four of the GainMap opcodes

      if(prev_nid == -1)
        dt_connector_copy(graph, module, 0, id_gainmap, 0);
      else
        CONN(dt_node_connect(graph, prev_nid, prev_nc, id_gainmap, 0));
      prev_nid = id_gainmap;
      prev_nc = 1;
    }
  }

  if(prev_nid == -1)
    dt_connector_bypass(graph, module, 0, 1);
  else {
    dt_connector_copy(graph, module, 1, prev_nid, prev_nc);
  }
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  if(module->img_param.dng_opcode_lists[1] == NULL)
  {
    dt_connector_bypass(graph, module, 0, 1);
  }
  else
  {
    create_nodes_op_list(graph, module, module->img_param.dng_opcode_lists[1]);
  }
}
