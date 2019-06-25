#pragma once
#include "pipe/node.h"
#include "pipe/graph.h"
#include "pipe/module.h"

// some module specific helpers
// TODO: is this the best place? what to add here?

// convenience function to detect inputs
static inline int
dt_connector_input(dt_connector_t *c)
{
  return c->type == dt_token("read") || c->type == dt_token("sink");
}
static inline int
dt_connector_output(dt_connector_t *c)
{
  return c->type == dt_token("write") || c->type == dt_token("source");
}
static inline int
dt_node_source(dt_node_t *n)
{
  return n->connector[0].type == dt_token("source");
}
static inline int
dt_node_sink(dt_node_t *n)
{
  return n->connector[0].type == dt_token("sink");
}

// TODO: also need _copy_ctx() version for the context buffer!
static inline void
dt_connector_copy(
    dt_graph_t  *graph,
    dt_module_t *module,
    int nid,   // node id
    int mc,    // connector id on module to copy
    int nc)    // connector id on node to copy to
{
  // XXX FIXME: we do not account correctly for the buffer counters here!
  // connect the node layer on the module:
  module->connector[mc].connected_ni = nid;
  module->connector[mc].connected_nc = nc;
  // connect the node:
  graph->node[nid].connector[nc] = module->connector[mc];
  // input connectors have a unique source. connect their node layer:
  if(dt_connector_input(module->connector+mc) &&
      module->connector[mc].connected_mi >= 0)
  {
    // connect our node to the nodeid stored on the module
    graph->node[nid].connector[nc].connected_mi = graph->module[
      module->connector[mc].connected_mi].connector[
        module->connector[mc].connected_mc].connected_ni;
    graph->node[nid].connector[nc].connected_mc = graph->module[
      module->connector[mc].connected_mi].connector[
        module->connector[mc].connected_mc].connected_nc;
  }
}

// create new nodes, connect to given input node + connector id, perform blur
// of given pixel radius, return nodeid (output connector will be #1).
static inline int
dt_api_blur(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    int          radius)
{
  // TODO: detect pixel format on input and blur the whole thing in separate kernels
  const dt_connector_t *conn_input = graph->node[nodeid_input].connector + connid_input;
  const int wd = conn_input->roi.roi_wd;
  const int ht = conn_input->roi.roi_ht;
  const int dp = 1;
  // FIXME: currently only implemented two channel blur for guided filter:
  assert(conn_input->chan == dt_token("rg"));
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rg"),
    .format = dt_token("f32"),
    .roi    = conn_input->roi,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("rg"),
    .format = dt_token("f32"),
    .roi    = conn_input->roi,
  };
  // push and interconnect a-trous gauss blur nodes
  // TODO: iterate until radius is matched, use smaller steps to achieve sub-power-of-two radii
  const int it = 2;
  int nid_input = nodeid_input;
  int cid_input = connid_input;
  for(int i=0;i<it;i++)
  {
    // TODO: could run these on downsampled image instead
    // add nodes blur2h and blur2v
    assert(graph->num_nodes < graph->max_nodes);
    const int id_blur2h = graph->num_nodes++;
    dt_node_t *node_blur2h = graph->node + id_blur2h;
    *node_blur2h = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("blur2h"),
      .module = module,
      .wd     = wd,
      .ht     = ht,
      .dp     = dp,
      .num_connectors = 2,
      .connector = {
        ci, co,
      },
      .push_constant_size = 4,
      .push_constant = {1u<<i},
    };
    assert(graph->num_nodes < graph->max_nodes);
    const int id_blur2v = graph->num_nodes++;
    dt_node_t *node_blur2v = graph->node + id_blur2v;
    *node_blur2v = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("blur2v"),
      .module = module,
      .wd     = wd,
      .ht     = ht,
      .dp     = dp,
      .num_connectors = 2,
      .connector = {
        ci, co,
      },
      .push_constant_size = 4,
      .push_constant = {1u<<i},
    };
    // interconnect nodes:
    CONN(dt_node_connect(graph, nid_input,  cid_input, id_blur2h, 0));
    CONN(dt_node_connect(graph, id_blur2h,  1,         id_blur2v, 0));
    nid_input = id_blur2v;
    cid_input = 1;
  }
  return nid_input;
}

// implements a guided filter without guide image, i.e. p=I.
// the entry node reads connector[0], the exit writes to connector[2].
static inline void
dt_api_guided_filter(
    dt_graph_t  *graph,        // graph to add nodes to
    dt_module_t *module,
    dt_roi_t    *roi,
    int         *entry_nodeid,
    int         *exit_nodeid,
    int          radius,       // size of blur
    float        epsilon)      // tell edges from noise
{
  // const dt_connector_t *conn_input = graph->node[nodeid_input].connector + connid_input;
  const int wd = roi->roi_wd;
  const int ht = roi->roi_ht;
  const int dp = 1;
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f32"),
    .roi    = *roi,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("rg"),
    .format = dt_token("f32"),
    .roi    = *roi,
  };

  // TODO: compute grey scale image rg32 with (I, I*I)
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided1 = graph->num_nodes++;
  *entry_nodeid = id_guided1;
  dt_node_t *node_guided1 = graph->node + id_guided1;
  *node_guided1 = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided1"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };

  // then connect 1x blur:
  // mean_I = blur(I)
  // corr_I = blur(I*I)
  const int id_blur1 = dt_api_blur(graph, module, id_guided1, 1, radius);

  // connect to this node:
  // a = var_I / (var_I + eps)
  // with var_I = corr_I - mean_I * mean_I
  // b = mean_I - a * mean_I
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided2 = graph->num_nodes++;
  dt_node_t *node_guided2 = graph->node + id_guided2;
  ci.chan = dt_token("rg");
  *node_guided2 = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided2"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };

  CONN(dt_node_connect(graph, id_blur1, 1, id_guided2, 0));

  // and blur once more:
  // mean_a = blur(a)
  // mean_b = blur(b)
  const int id_blur2 = dt_api_blur(graph, module, id_guided2, 1, radius);

  // final kernel:
  // output = mean_a * I + mean_b
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided3 = graph->num_nodes++;
  dt_node_t *node_guided3 = graph->node + id_guided3;
  ci.chan = dt_token("rgba");
  co.chan = dt_token("rgba");
  dt_connector_t cm = {
    .name   = dt_token("ab"),
    .type   = dt_token("read"),
    .chan   = dt_token("rg"),
    .format = dt_token("f32"),
    .roi    = *roi,
    .connected_mi = -1,
  };
  *node_guided3 = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided3"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {
      ci, // - original image as input rgba f32
      cm, // - mean a mean b as rg f32
      co, // - output rgba f32
    },
  };
  CONN(dt_node_connect(graph, id_blur2, 1, id_guided3, 1));
  *exit_nodeid = id_guided3;
}
