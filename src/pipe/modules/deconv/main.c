#include "modules/api.h"

// this is an initial implementation of some simplistic iterative, non-blind R/L deconvolution.
// it has a lot of problems because i didn't spend time on it:
// - hardcoded radius
// - hardcoded iteration count
// - radius is not really precisely respected because the blur just approximately blurs
//   as much as it thinks is necessary. for this use case we'll probably want something more precise.
// - the division kernel has an epsilon safeguard num/(den+eps) which i'm not sure about.

// TODO: check params based on radius

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // super simple non-blind richardson-lucy deconvolution.
  // we assume a ~gaussian blur kernel K (with the adjoint K=K')
  // one iteration computes:
  // I(i+1) = I(i) * ( K' x (input / (I(i) x K)) )
  const int it_cnt = 2;
  const dt_roi_t *roi = &module->connector[0].roi;

  const float radius = 2;

  uint32_t I_i_id = -1u; // first iteration we'll copy the module connector
  uint32_t I_i_cn =  0u;
  for(int i=0;i<it_cnt;i++)
  {
    // take input and blur it
    int32_t id_blur_in = -1;
    const uint32_t IxK_id = dt_api_blur_sub(graph, module, I_i_id, I_i_cn, &id_blur_in, 0, radius);
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
    const uint32_t KpxF_id = dt_api_blur_sub(graph, module, id_div, 2, 0, 0, radius);
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
}
