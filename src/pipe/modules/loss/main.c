#include "modules/api.h"
#include "core/half.h"
#include <math.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *r = &module->connector[0].roi;
  r->wd = r->full_wd;
  r->ht = r->full_ht;
  r->scale = 1.0f;
  r = &module->connector[1].roi;
  r->wd = r->full_wd;
  r->ht = r->full_ht;
  r->scale = 1.0f;
  module->connector[2].roi.wd = module->connector[2].roi.full_wd;
  module->connector[2].roi.ht = module->connector[2].roi.full_ht;
  module->connector[2].roi.scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const int sz = 2;
  module->connector[2].roi.full_wd = (module->connector[0].roi.full_wd+sz-1)/sz;
  module->connector[2].roi.full_ht = (module->connector[0].roi.full_ht+sz-1)/sz;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // one node to collect, and one to read back the sink
  assert(graph->num_nodes < graph->max_nodes);
  const int id_mse = graph->num_nodes++;
  graph->node[id_mse] = (dt_node_t) {
    .name   = dt_token("loss"),
    .kernel = dt_token("main"),
    .module = module,
    .wd     = module->connector[2].roi.wd,
    .ht     = module->connector[2].roi.ht,
    .dp     = 1,
    .num_connectors = 3,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("*"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("orig"),
      .type   = dt_token("read"),
      .chan   = dt_token("*"),
      .format = dt_token("*"),
      .roi    = module->connector[1].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("loss"),
      .type   = dt_token("write"),
      .chan   = dt_token("rg"),
      .format = dt_token("f32"),
      .roi    = module->connector[2].roi,
    }},
  };

  dt_connector_copy(graph, module, 0, id_mse, 0);
  dt_connector_copy(graph, module, 1, id_mse, 1);
  dt_connector_copy(graph, module, 2, id_mse, 2);

  // remember mse as entry point
  int node = id_mse;
  int conn = 2;

  const int sz = 2;
  dt_roi_t roi = module->connector[2].roi;
  while(roi.wd > 1 && roi.ht > 1)
  {
    int cwd = (roi.wd + sz - 1)/sz;
    int cht = (roi.ht + sz - 1)/sz;
    assert(graph->num_nodes < graph->max_nodes);
    const int id_down = graph->num_nodes++;
    graph->node[id_down] = (dt_node_t) {
      .name   = dt_token("loss"),
      .kernel = dt_token("down"),
      .module = module,
      .wd     = cwd,
      .ht     = cht,
      .dp     = 1,
      .num_connectors = 2,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("rg"),
        .format = dt_token("f32"),
        .roi    = roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rg"),
        .format = dt_token("f32"),
        .roi    = roi,
      }},
    };
    roi.wd = cwd;
    roi.ht = cht;
    graph->node[id_down].connector[1].roi = roi;
    CONN(dt_node_connect(graph, node, conn, id_down, 0));
    node = id_down;
    conn = 1;
  }

  assert(graph->num_nodes < graph->max_nodes);
  const int id_sink = graph->num_nodes++;
  graph->node[id_sink] = (dt_node_t) {
    .name   = dt_token("loss"),
    .kernel = dt_token("sink"),
    .module = module,
    .wd     = roi.wd,
    .ht     = roi.ht,
    .dp     = 1,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("sink"),
      .chan   = dt_token("rg"),
      .format = dt_token("f32"),
      .roi    = roi,
      .connected_mi = -1,
    }},
  };
  CONN(dt_node_connect(graph, node, conn, id_sink, 0));

  // dt_connector_copy(graph, module, 2, node, conn);

  module->flags |= s_module_request_write_sink;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  module->flags |= s_module_request_write_sink;
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  float *loss = 0;
  for(int p=0;p<module->so->num_params;p++)
    if(module->so->param[p]->name == dt_token("loss"))
      loss = (float *)(module->param + module->so->param[p]->offset);
  if(!loss) return;

  float *b = buf;
  loss[0] = b[0];
}
