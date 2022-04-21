#include "connector.h"
#include "graph.h"

// "templatised" connection functions for both modules and nodes
int
dt_module_connect(
    dt_graph_t *graph,
    int m0, int c0, int m1, int c1)
{
#define connect_module
#define element module
#define num_elements num_modules
#include "connector.inc"
#undef num_elements
#undef element
#undef connect_module
}

int
dt_node_connect(
    dt_graph_t *graph,
    int m0, int c0, int m1, int c1)
{
#define element node
#define num_elements num_nodes
#include "connector.inc"
#undef num_elements
#undef element
}

int
dt_node_feedback(dt_graph_t *graph, int n0, int c0, int n1, int c1)
{
  int ret = dt_node_connect(graph, n0, c0, n1, c1);
  graph->node[n1].connector[c1].flags |= s_conn_feedback;
  graph->node[n0].connector[c0].frames = 2;
  graph->node[n1].connector[c1].frames = 2;
  return ret;
}

