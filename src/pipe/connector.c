#include "connector.h"
#include "graph.h"
#include "cycles.h"

// "templatised" connection functions for both modules and nodes
static inline int
dt_module_connect_internal(
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
dt_module_connect(
    dt_graph_t *graph,
    int m0, int c0, int m1, int c1)
{
  if(dt_connection_is_cyclic(graph, m0, c0, m1, c1)) return 12;
  return dt_module_connect_internal(graph, m0, c0, m1, c1);
}

int
dt_module_feedback(
    dt_graph_t *graph,
    int m0, int c0, int m1, int c1)
{
  int ret = dt_module_connect_internal(graph, m0, c0, m1, c1);
  graph->module[m1].connector[c1].flags |= s_conn_feedback;
  graph->module[m0].connector[c0].frames = 2;
  graph->module[m1].connector[c1].frames = 2;
  return ret;
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

