#include "modules/api.h"
#include "config.h"

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  if(parid >= 2 && parid <= 5)
  { // radii
    float oldrad = *(float*)oldval;
    float newrad = dt_module_param_float(module, parid)[0];
    return dt_api_blur_check_params(oldrad, newrad);
  }
  if(parid == 6) // subsampling changed
    return s_graph_run_all;
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

// the roi callbacks are only needed for the debug outputs. other than that
// the default implementation would work fine for us.
void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  module->connector[5].roi.wd = module->connector[5].roi.full_wd;
  module->connector[5].roi.ht = module->connector[5].roi.full_ht;
  module->connector[5].roi.scale = 1.0f;
  module->connector[6].roi.wd = module->connector[6].roi.full_wd;
  module->connector[6].roi.ht = module->connector[6].roi.full_ht;
  module->connector[6].roi.scale = 1.0f;
  module->connector[0].roi = module->connector[1].roi;
  module->connector[2].roi = module->connector[1].roi;
  module->connector[3].roi = module->connector[1].roi;
  module->connector[4].roi = module->connector[1].roi;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  module->connector[1].roi = module->connector[0].roi;
  module->connector[4].roi = module->connector[0].roi;
  module->connector[5].roi = module->connector[0].roi;
  module->connector[6].roi = module->connector[0].roi;
}

// input connectors: fixed raw file
//                   to-be-warped raw file
// output connector: warped raw file
void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int down = 4;
  const int num_levels = 4; // need more when downscaling less than 4x4 each step
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return;
  // connect each mosaic input to half, generate grey lum map for both input images
  // by a sequence of half, down4, down4, down4 kernels.
  // then compute distance (dist kernel) coarse to fine, merge best offsets (merge kernel),
  // input best coarse offsets to next finer level, and finally output offsets on finest scale.
  dt_roi_t roi[num_levels+1];
  roi[0] = module->connector[0].roi;
  const int32_t p_down = CLAMP(dt_module_param_int(module, dt_module_get_param(module->so, dt_token("sub")))[0], 1, 4);
  const int block = img_param->filters == 9u ? 3 : (img_param->filters == 0 ? p_down : 2);

  for(int i=1;i<=num_levels;i++)
  {
    int scale = i == 1 ? block : down;
    roi[i] = roi[i-1];
    roi[i].full_wd += scale-1;
    roi[i].full_wd /= scale;
    roi[i].full_ht += scale-1;
    roi[i].full_ht /= scale;
    roi[i].wd += scale-1;
    roi[i].wd /= scale;
    roi[i].ht += scale-1;
    roi[i].ht /= scale;
  }

  const char *fmt_img = "f16";// "ui8";
  const char *fmt_dst = "f16";// "ui8";

  int id_down[2][num_levels];
  uint32_t *blacki = (uint32_t *)img_param->black;
  uint32_t *whitei = (uint32_t *)img_param->white;
  for(int k=0;k<2;k++)
  {
    int pc[] = { blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        img_param->filters, block };
    id_down[k][0] = dt_node_add(graph, module, "align", "half", roi[1].wd, roi[1].ht, 1, sizeof(pc), pc, 2,
        "input",  "read", img_param->filters == 0 ? "rgba" : "rggb", dt_token_str(module->connector[k].format), roi+0,
        "output", "write", "y", fmt_img, roi+1);
    // these are the "alignsrc" and "aligndst" channels:
    dt_connector_copy(graph, module, 3-k, id_down[k][0], 0);

    for(int i=1;i<num_levels;i++)
    {
      id_down[k][i] = dt_node_add(graph, module, "align", down==4?"down4":"down2", roi[i+1].wd, roi[i+1].ht, 1, 0, 0, 2,
          "input",  "read",  "y", fmt_img, roi+i,
          "output", "write", "y", fmt_img, roi+i+1);
      CONN(dt_node_connect(graph, id_down[k][i-1], 1, id_down[k][i], 0));
    }
  }

  // connect uv-diff and merge:
  int id_offset = -1;
  int id_off[num_levels];
  for(int i=num_levels-1;i>=0;i--) // all depths/coarseness levels, starting at coarsest
  { // for all offsets in search window (using array connectors)
    int pc[] = { id_offset >= 0 ? down : 0, i };
    const int id_dist = dt_node_add(graph, module, "align", "dist", roi[i+1].wd, roi[i+1].ht, 25, sizeof(pc), pc, 4,
        "input",  "read", "y", fmt_img, roi+i+1,
        "warped", "read", "y", fmt_img, roi+i+1,
        "offset", "read", id_offset >= 0 ? "rg" : "y", id_offset >= 0 ? "f16" : fmt_img, id_offset >= 0 ? roi+i+2 : roi+i+1,
        "output", "write", "y", fmt_dst, roi+i+1);
    graph->node[id_dist].connector[3].array_length = 25;
    CONN(dt_node_connect(graph, id_down[0][i], 1, id_dist, 0));
    CONN(dt_node_connect(graph, id_down[1][i], 1, id_dist, 1));
    if(id_offset >= 0)
      CONN(dt_node_connect(graph, id_offset, 2, id_dist, 2));
    else // need to connect a dummy
      CONN(dt_node_connect(graph, id_down[0][i], 1, id_dist, 2));

    // blur output of dist node by tile size (depending on noise radius 16x16, 32x32 or 64x64?)
    // grab module parameters, would need to trigger re-create_nodes on change:
    const float blur = ((float*)module->param)[2+i];
    const int id_blur = blur > 0 ? dt_api_blur(graph, module, id_dist, 3, 0, 0, blur) : id_dist;
    // const int id_blur = blur > 0 ? dt_api_blur_sub(graph, module, id_dist, 3, 0, 0, blur, 0) : id_dist;
    // const int id_blur = blur > 0 ? dt_api_blur_small(graph, module, id_dist, 3, 0, 0, blur) : id_dist;
    const int cn_blur = blur > 0 ? 1 : 3;

    // merge output of blur node using "merge" (<off0, <off1, >merged)
    const int id_merge = dt_node_add(graph, module, "align", "merge", roi[i+1].wd, roi[i+1].ht, 1, sizeof(pc), pc, 4,
        "dist",   "read", "y", fmt_dst, roi+i+1,
        "coff",   "read", id_offset >= 0 ? "rg"  : "y", id_offset >= 0 ? "f16" : fmt_img, id_offset >= 0 ? roi+i+2 : roi+i+1,
        "merged", "write", "rg", "f16", roi+i+1,
        "resid",  "write", "r", "f16", roi+i+1);
    graph->node[id_merge].connector[0].array_length = 25;
    id_off[i] = id_merge;

    CONN(dt_node_connect(graph, id_blur, cn_blur, id_merge, 0));
    // connect coarse offset buffer from previous level:
    if(id_offset >= 0)
      CONN(dt_node_connect(graph, id_offset, 2, id_merge, 1));
    else // need to connect a dummy
      CONN(dt_node_connect(graph, id_down[1][i], 1, id_merge, 1));
    // remember our merged output as offset for next finer scale:
    id_offset = id_merge;
    // id_offset is the final merged offset buffer on this level, ready to be
    // used as input by the next finer level, if any
  }
  // id_offset is now our finest offset buffer, to be used to warp the second
  // raw image to match the first.
  // note that this buffer has dimensions roi[1], i.e. it is still half sized as compared
  // to the full raw image. it is thus large enough to move around full bayer/xtrans blocks,
  // but would need refinement if more fidelity is required (as input for
  // splatting super res demosaic for instance)

  int pc2[] = { img_param->filters, block };
  const int id_warp = dt_node_add(graph, module, "align", "warp", roi[0].wd, roi[0].ht, 1, sizeof(pc2), pc2, 5,
      "input",  "read", img_param->filters == 0 ? "rgba" : "rggb", "f16", roi+0,
      "offset", "read", "rg", "f16", roi+1,
      "output", "write", img_param->filters == 0 ? "rgba" : "rggb", "f16", roi+0,
      "visn",   "write", "rgba", "ui8", roi+0,
      "mv", "write", "rg", "f16", roi+0);
  dt_connector_copy(graph, module, 0, id_warp, 0);
  CONN(dt_node_connect(graph, id_offset, 2, id_warp, 1));
  dt_connector_copy(graph, module, 1, id_warp, 2);
  dt_connector_copy(graph, module, 5, id_warp, 3); // pass on visn
  dt_connector_copy(graph, module, 6, id_warp, 4); // pass on mv
  dt_connector_copy(graph, module, 4, id_off[0], 3);  // full res mask
  graph->node[id_off[0]].connector[3].roi = roi[1];
}
