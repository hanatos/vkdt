#include "modules/api.h"

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *r = &module->connector[0].roi;
  r->wd = r->full_wd;
  r->ht = r->full_ht;
  r->x = 0;
  r->y = 0;
  r->scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // always request constant size for picked colours:
  module->connector[1].roi.full_wd = 20; // max 20 areas picked with mean rgb
  module->connector[1].roi.full_ht = 4;  // and variances etc in different lines
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO: one node to collect, and one to read back the sink
  assert(graph->num_nodes < graph->max_nodes);
  const int id_collect = graph->num_nodes++;
  graph->node[id_collect] = (dt_node_t) {
    .name   = dt_token("pick"),
    .kernel = dt_token("collect"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("*"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("picked"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f32"),
      .roi    = module->connector[1].roi,
      .flags  = s_conn_clear,
    }},
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_map = graph->num_nodes++;
  graph->node[id_map] = (dt_node_t) {
    .name   = dt_token("pick"),
    .kernel = dt_token("sink"),
    .module = module,
    .wd     = module->connector[1].roi.wd,
    .ht     = module->connector[1].roi.ht,
    .dp     = 1,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("sink"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f32"),
      .roi    = module->connector[1].roi,
      .connected_mi = -1,
    }},
  };

  // interconnect nodes:
  dt_connector_copy(graph, module, 0, id_collect, 0);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);
  dt_connector_copy(graph, module, 1, id_collect, 1);
  graph->node[id_collect].connector[1].flags = s_conn_clear;
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  // float *f32 = buf;

  // const int wd = module->connector[0].roi.wd;
  // const int ht = module->connector[0].roi.ht;

  // TODO: now read back what we averaged and write into param "picked"
}
