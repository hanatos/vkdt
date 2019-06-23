#include "module.h"
#include <math.h>
#include <stdlib.h>

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO: streamline the api needed to do things like this!
  // create a box blur node:

  // TODO: in fact these should be grey scale
  // TODO dt_node_add(graph, module, "name", "kernel", "entry")
  // all three nodes need this:
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f32"),
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f32"),
  };
  // add node blur1
  assert(graph->num_nodes < graph->max_nodes);
  const int id_blur1 = graph->num_nodes++;
  dt_node_t *node_blur1 = graph->node + id_blur1;
  *node_blur1 = (dt_node_t) {
    .name   = dt_token("blur1"),
    .kernel = dt_token("main"),
    .entry  = dt_token("blur1"),
    .module = module,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };
  // add nodes blur2h and blur2v
  assert(graph->num_nodes < graph->max_nodes);
  const int id_blur2h = graph->num_nodes++;
  dt_node_t *node_blur2h = graph->node + id_blur2h;
  *node_blur2h = (dt_node_t) {
    .name   = dt_token("blur2h"),
    .kernel = dt_token("main"),
    .entry  = dt_token("blur2h"),
    .module = module,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_blur2v = graph->num_nodes++;
  dt_node_t *node_blur2v = graph->node + id_blur2v;
  *node_blur2v = (dt_node_t) {
    .name   = dt_token("blur2v"),
    .kernel = dt_token("main"),
    .entry  = dt_token("blur2v"),
    .module = module,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };

  // TODO: context buffer handling:
  // TODO: if we have a context buffer, blur it in a separate set of nodes
  //       with a reduced blur radius

  // TODO: need to deal with all module connectors. these are two
  //       that need connecting currently:
  // TODO: copy connection: module input  -- blur1  input
  // TODO: copy connection: module output -- blur2v output

  // module input
  // TODO: put this into a copy_connector function
  // TODO:  (and have corresponding copy_connector_ctx)
  // module->connected_nodeid[0] = id_blur1;
  // node_blur1->connector[0].{stuff??} = module->connector[0].{stuff??}
  // if(dt_connector_input(node->connector+0) &&
  // module->connector[0].connected_mid >= 0)
  //     node_blur1->connector[i].connected_mid = graph->module[
  //       module->connector[i].connected_mid].connected_nodeid[i];
#if 0 // XXX
  for(int i=0;i<module->num_connectors;i++)
  {
    module->connected_nodeid[i] = nodeid;
    node->connector[i] = module->connector[i];
    // update the connection node id to point to the node inside the module
    // associated with the given output connector:
    if(dt_connector_input(node->connector+i) &&
        module->connector[i].connected_mid >= 0)
      node->connector[i].connected_mid = graph->module[
        module->connector[i].connected_mid].connected_nodeid[i];
  }
#endif

  // interconnect nodes:
  dt_node_connect(graph, blur1,  blur1_output,  blur2h, blur2h_input);
  dt_node_connect(graph, blur2h, blur2h_output, blur2v, blur2v_input);

  // TODO: currently we require to init node->{wd,ht,dp} here too!
  // TODO: this would mean we'd need to re-create nodes for new roi, which indeed may be necessary for wavelet scales etc anyways
}

