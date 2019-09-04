#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO: streamline the api needed to do things like this!

  int guided_entry = -1, guided_exit = -1;
  dt_api_guided_filter(
      graph, module, 
      &module->connector[0].roi,
      &guided_entry,
      &guided_exit,
      20,  // TODO: not used! put in push constants?
      1e-2f);

  // TODO: could put that into shared/guided3 in one dispatch:
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = module->connector[0].roi,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = module->connector[1].roi,
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_comb = graph->num_nodes++;
  dt_node_t *node_comb = graph->node + id_comb;
  *node_comb = (dt_node_t) {
    .name   = dt_token("contrast"),
    .kernel = dt_token("combine"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };

  dt_connector_copy(graph, module, 0, id_comb, 0); // rgb input
  CONN(dt_node_connect(graph, guided_exit, 2, id_comb, 1)); // luminance input

  // connect entry point of module (input rgb) to entry point of guided filter
  dt_connector_copy(graph, module, 0, guided_entry, 0);
  // connect entry point of module (input rgb) to exitnode of guided filter that needs it for reconstruction
  dt_connector_copy(graph, module, 0, guided_exit,  0);
  // connect combined rgb to exit point of module
  dt_connector_copy(graph, module, 1, id_comb, 2);
}

