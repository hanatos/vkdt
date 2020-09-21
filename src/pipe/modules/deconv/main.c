#include "modules/api.h"
#include "config.h"
#include "modules/localsize.h"

// this is an initial implementation of some simplistic iterative, non-blind R/L deconvolution.
// it has a lot of problems because i didn't spend time on it:
// - hardcoded iteration count
// - the division kernel has an epsilon safeguard num/max(den, eps) which i'm not sure about.

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  // yea sorry we're stupid and need push constants which we diligently don't update
  // until we recreate the nodes. should change that i suppose.
  // options:
  // - secretly pick up parameter in the api blur call
  // - make blurs a custom node without api wrapping and access params
  // - implement commit_params() to alter the push constant
  return s_graph_run_all;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_roi_t *roi = &module->connector[0].roi;
  assert(graph->num_nodes < graph->max_nodes);
  const uint32_t id_deconv = graph->num_nodes++;
  // for dimensions, reverse
  // (wd + DT_LOCAL_SIZE_X - 1) / DT_LOCAL_SIZE_X
  // such that it'll result in
  // (wd + DECONV_WD - 2B - 1) / (DECONV_WD-2B)
  // this seems to be very wrong, at least the rounding is off by a fair bit:
  const uint32_t wd = ((roi->wd - DT_DECONV_TILE_WD - 2*DT_DECONV_BORDER -1)/(DT_DECONV_TILE_WD-2*DT_DECONV_BORDER)
    - DT_LOCAL_SIZE_X + 1) * DT_LOCAL_SIZE_X;
  const uint32_t ht = ((roi->ht - DT_DECONV_TILE_HT - 2*DT_DECONV_BORDER -1)/(DT_DECONV_TILE_HT-2*DT_DECONV_BORDER)
    - DT_LOCAL_SIZE_Y + 1) * DT_LOCAL_SIZE_Y;
  graph->node[id_deconv] = (dt_node_t) {
    .name   = dt_token("deconv"),
    .kernel = dt_token("deconv"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
    }},
  };
  dt_connector_copy(graph, module, 0, id_deconv, 0);
  dt_connector_copy(graph, module, 1, id_deconv, 1);

#if 0 // multi-node slow version. every iteration is an extra kernel call and goes back to global memory :(
  // super simple non-blind richardson-lucy deconvolution.
  // we assume a ~gaussian blur kernel K (with the adjoint K=K')
  // one iteration computes:
  // I(i+1) = I(i) * ( K' x (input / (I(i) x K)) )
  const int it_cnt = 20;
  const dt_roi_t *roi = &module->connector[0].roi;
  const float radius = dt_module_param_float(module, 0)[0];

  uint32_t I_i_id = -1u; // first iteration we'll copy the module connector
  uint32_t I_i_cn =  0u;
  for(int i=0;i<it_cnt;i++)
  {
    // take input and blur it
    int32_t id_blur_in = -1;
    // const uint32_t IxK_id = dt_api_blur_sub(graph, module, I_i_id, I_i_cn, &id_blur_in, 0, radius);
    const uint32_t IxK_id = dt_api_blur_small(graph, module, I_i_id, I_i_cn, &id_blur_in, 0, radius);
    if(i==0) dt_connector_copy(graph, module, 0, id_blur_in, 0);
    // divide original image by IxK
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_div = graph->num_nodes++;
    graph->node[id_div] = (dt_node_t) {
      .name   = dt_token("deconv"),
      .kernel = dt_token("div"),
      .module = module,
      .wd     = roi->wd,
      .ht     = roi->ht,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("orig"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = *roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("IxK"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .flags  = s_conn_smooth,
        .roi    = *roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = *roi,
      }},
    };
    dt_connector_copy(graph, module, 0, id_div, 0);
    CONN(dt_node_connect(graph, IxK_id, 1, id_div, 1));
    // convolve resulting fraction by adjoint kernel
    // const uint32_t KpxF_id = dt_api_blur_sub(graph, module, id_div, 2, 0, 0, radius);
    const uint32_t KpxF_id = dt_api_blur_small(graph, module, id_div, 2, 0, 0, radius);
    // multiply last iteration by this result
    assert(graph->num_nodes < graph->max_nodes);
    const uint32_t id_mul = graph->num_nodes++;
    graph->node[id_mul] = (dt_node_t) {
      .name   = dt_token("deconv"),
      .kernel = dt_token("mul"),
      .module = module,
      .wd     = roi->wd,
      .ht     = roi->ht,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("f0"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = *roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("f1"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .flags  = s_conn_smooth,
        .roi    = *roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = *roi,
      }},
    };
    CONN(dt_node_connect(graph, KpxF_id, 1, id_mul, 0));
    if(i)
      CONN(dt_node_connect(graph, I_i_id, I_i_cn, id_mul, 1));
    else
      dt_connector_copy(graph, module, 0, id_mul, 1);
    // wire output of this back into loop as I(i+1)
    I_i_id = id_mul;
    I_i_cn = 2;
  }
  // copy last I(i) to module output
  dt_connector_copy(graph, module, 1, I_i_id, I_i_cn);
#endif
}
