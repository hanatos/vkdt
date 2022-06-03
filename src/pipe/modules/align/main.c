#include "modules/api.h"
#include "config.h"

// TODO: redo timing. but it seems down4 is a lot more precise (less pyramid approx?)
#define DOWN 4
#if DOWN==4
#define NUM_LEVELS 4
#else
#define NUM_LEVELS 6
#endif

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
  // connect each mosaic input to half, generate grey lum map for both input images
  // by a sequence of half, down4, down4, down4 kernels.
  // then compute distance (dist kernel) coarse to fine, merge best offsets (merge kernel),
  // input best coarse offsets to next finer level, and finally output offsets on finest scale.
  dt_roi_t roi[NUM_LEVELS+1] = {module->connector[0].roi};
#ifdef DT_BURST_HRES_FIT // 2x2 down
  const int block = module->img_param.filters == 9u ? 3 : (module->img_param.filters == 0 ? 2 : 2);
#elif defined(DT_BURST_QRES_FIT) // 4x4 down
  const int block = module->img_param.filters == 9u ? 3 : (module->img_param.filters == 0 ? 4 : 2);
#else // full res
  const int block = module->img_param.filters == 9u ? 3 : (module->img_param.filters == 0 ? 1 : 2);
#endif
  for(int i=1;i<=NUM_LEVELS;i++)
  {
    int scale = i == 1 ? block : DOWN;
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

  dt_token_t fmt_img = dt_token("f16");//ui8");
  dt_token_t fmt_dst = dt_token("f16");//ui8");

  int id_down[2][NUM_LEVELS] = {0};
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return;
  uint32_t *blacki = (uint32_t *)img_param->black;
  uint32_t *whitei = (uint32_t *)img_param->white;
  for(int k=0;k<2;k++)
  {
    assert(graph->num_nodes < graph->max_nodes);
    id_down[k][0] = graph->num_nodes++;
    graph->node[id_down[k][0]] = (dt_node_t) {
      .name   = dt_token("align"),
      .kernel = dt_token("half"),
      .module = module,
      .wd     = roi[1].wd,
      .ht     = roi[1].ht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = module->img_param.filters == 0 ? dt_token("rgba") : dt_token("rggb"),
        .format = module->connector[k].format,
        .roi    = roi[0],
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("y"),
        .format = fmt_img,
        .roi    = roi[1],
      }},
      .push_constant_size = 9*sizeof(uint32_t),
      .push_constant = { blacki[0], blacki[1], blacki[2], blacki[3],
        whitei[0], whitei[1], whitei[2], whitei[3],
        img_param->filters },
    };
    // these are the "alignsrc" and "aligndst" channels:
    dt_connector_copy(graph, module, 3-k, id_down[k][0], 0);

    for(int i=1;i<NUM_LEVELS;i++)
    {
      assert(graph->num_nodes < graph->max_nodes);
      id_down[k][i] = graph->num_nodes++;
      graph->node[id_down[k][i]] = (dt_node_t) {
        .name   = dt_token("align"),
#if DOWN==4
        .kernel = dt_token("down4"),
#else
        .kernel = dt_token("down2"),
#endif
        .module = module,
        .wd     = roi[i+1].wd,
        .ht     = roi[i+1].ht,
        .dp     = 1,
        .num_connectors = 2,
        .connector = {{
          .name   = dt_token("input"),
          .type   = dt_token("read"),
          .chan   = dt_token("y"),
          .format = fmt_img,
          .roi    = roi[i],
          .connected_mi = -1,
        },{
          .name   = dt_token("output"),
          .type   = dt_token("write"),
          .chan   = dt_token("y"),
          .format = fmt_img,
          .roi    = roi[i+1],
        }},
      };
      CONN(dt_node_connect(graph, id_down[k][i-1], 1, id_down[k][i], 0));
    }
  }

  int id_off[NUM_LEVELS] = {0};

  // connect uv-diff and merge:
  int id_offset = -1;
  for(int i=NUM_LEVELS-1;i>=0;i--) // all depths/coarseness levels, starting at coarsest
  { // for all offsets in search window (using array connectors)
    assert(graph->num_nodes < graph->max_nodes);
    int id_dist = graph->num_nodes++;
    graph->node[id_dist] = (dt_node_t) {
      .name   = dt_token("align"),
      .kernel = dt_token("dist"),
      .module = module,
      .wd     = roi[i+1].wd,
      .ht     = roi[i+1].ht,
      .dp     = 25,
      .num_connectors = 4,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("y"),
        .format = fmt_img,
        .roi    = roi[i+1],
        .connected_mi = -1,
      },{
        .name   = dt_token("warped"),
        .type   = dt_token("read"),
        .chan   = dt_token("y"),
        .format = fmt_img,
        .roi    = roi[i+1],
        .connected_mi = -1,
      },{
        .name   = dt_token("offset"),
        .type   = dt_token("read"),
        .chan   = id_offset >= 0 ? dt_token("rg")  : dt_token("y"),
        .format = id_offset >= 0 ? dt_token("f16") : fmt_img,
        .roi    = id_offset >= 0 ? roi[i+2] : roi[i+1],
        .flags  = s_conn_smooth,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("y"),
        .format = fmt_dst,
        .roi    = roi[i+1],
        .array_length = 25,
      }},
      .push_constant_size = 2*sizeof(uint32_t),
      .push_constant = { id_offset >= 0 ? DOWN: 0, i },
    };
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
    assert(graph->num_nodes < graph->max_nodes);
    const int id_merge = graph->num_nodes++;
    graph->node[id_merge] = (dt_node_t) {
      .name   = dt_token("align"),
      .kernel = dt_token("merge"),
      .module = module,
      .wd     = roi[i+1].wd,
      .ht     = roi[i+1].ht,
      .dp     = 1,
      .num_connectors = 4,
      .connector = {{
        .name   = dt_token("dist"),
        .type   = dt_token("read"),
        .chan   = dt_token("y"),
        .format = fmt_dst,
        .roi    = roi[i+1],
        .flags  = s_conn_smooth,
        .connected_mi = -1,
        .array_length = 25,
      },{
        .name   = dt_token("coff"),
        .type   = dt_token("read"),
        .chan   = id_offset >= 0 ? dt_token("rg")  : dt_token("y"),
        .format = id_offset >= 0 ? dt_token("f16") : fmt_img,
        .roi    = id_offset >= 0 ? roi[i+2] : roi[i+1],
        .flags  = s_conn_smooth,
        .connected_mi = -1,
      },{
        .name   = dt_token("merged"),
        .type   = dt_token("write"),
        .chan   = dt_token("rg"),
        .format = dt_token("f16"),
        .roi    = roi[i+1],
      },{
        .name   = dt_token("resid"),
        .type   = dt_token("write"),
        .chan   = dt_token("r"),
        .format = dt_token("f16"),
        .roi    = roi[i+1],
      }},
      .push_constant_size = 2*sizeof(uint32_t),
      .push_constant = { id_offset >= 0 ? DOWN: 0, i },
    };
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

  assert(graph->num_nodes < graph->max_nodes);
  const int id_warp = graph->num_nodes++;
  graph->node[id_warp] = (dt_node_t) {
    .name   = dt_token("align"),
    .kernel = dt_token("warp"),
    .module = module,
    .wd     = roi[0].wd,
    .ht     = roi[0].ht,
    .dp     = 1,
    .num_connectors = 5,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = module->img_param.filters == 0 ? dt_token("rgba") : dt_token("rggb"),
      .format = dt_token("f16"),
      .roi    = roi[0],
      .connected_mi = -1,
    },{
      .name   = dt_token("offset"),
      .type   = dt_token("read"),
      .chan   = dt_token("rg"),
      .format = dt_token("f16"),
      .roi    = roi[1],
      .flags  = s_conn_smooth,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = module->img_param.filters == 0 ? dt_token("rgba") : dt_token("rggb"),
      .format = dt_token("f16"),
      .roi    = roi[0],
    },{
      .name   = dt_token("visn"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("ui8"),
      .roi    = roi[0],
    },{
      .name   = dt_token("mv"),
      .type   = dt_token("write"),
      .chan   = dt_token("rg"),
      .format = dt_token("f16"),
      .roi    = roi[0],
    }},
    .push_constant_size = 2*sizeof(uint32_t),
    .push_constant = {
      module->img_param.filters,
      block,
    },
  };
  dt_connector_copy(graph, module, 0, id_warp, 0);
  CONN(dt_node_connect(graph, id_offset, 2, id_warp, 1));
  dt_connector_copy(graph, module, 1, id_warp, 2);
  dt_connector_copy(graph, module, 5, id_warp, 3); // pass on visn
  dt_connector_copy(graph, module, 6, id_warp, 4); // pass on mv
#if 0
  dt_connector_copy(graph, module, 4, id_off[1], 3);  // lo res mask
  graph->node[id_off[1]].connector[3].roi = roi[2];   // XXX FIXME: connector_copy should probably respect ROI
#else
  dt_connector_copy(graph, module, 4, id_off[0], 3);  // full res mask
  graph->node[id_off[0]].connector[3].roi = roi[1];   // XXX FIXME: connector_copy should probably respect ROI
#endif
#if 0 // connect extra mask creation kernel based on aovs:
  assert(graph->num_nodes < graph->max_nodes);
  const int id_mask = graph->num_nodes++;
  graph->node[id_mask] = (dt_node_t) {
    .name   = dt_token("align"),
    .kernel = dt_token("mask"),
    .module = module,
    .wd     = roi[0].wd,
    .ht     = roi[0].ht,
    .dp     = 1,
    .num_connectors = 4,
    .connector = {{
      .name   = dt_token("src"),
      .type   = dt_token("read"),
      .chan   = module->img_param.filters == 0 ? dt_token("rgba") : dt_token("rggb"),
      .format = dt_token("f16"),
      .roi    = roi[0],
      .connected_mi = -1,
    },{
      .name   = dt_token("dst"),
      .type   = dt_token("read"),
      .chan   = module->img_param.filters == 0 ? dt_token("rgba") : dt_token("rggb"),
      .format = dt_token("f16"),
      .roi    = roi[0],
      .connected_mi = -1,
    },{
      .name   = dt_token("offset"),
      .type   = dt_token("read"),
      .chan   = dt_token("rg"),
      .format = dt_token("f16"),
      .roi    = roi[1],
      .flags  = s_conn_smooth,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("r"),
      .format = dt_token("f16"),
      .roi    = roi[0],
    }},
  };
  dt_connector_copy(graph, module, 2, id_mask, 0);
  dt_connector_copy(graph, module, 3, id_mask, 1);
  CONN(dt_node_connect(graph, id_offset, 2, id_mask, 2));
  dt_connector_copy(graph, module, 4, id_mask, 3);
#endif
#undef NUM_LEVELS
}
