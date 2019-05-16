#include "connector.h"

// "templatised" connection functions for both modules and nodes
int
dt_module_connect(
    dt_module_graph_t *graph,
    int m0, int c0, int m1, int c1)
{
#define element module
#define num_elements num_modules
#include "connector.inc"
#undef modtype
}

int
dt_node_connect(
    dt_graph_t *graph,
    int m0, int c0, int m1, int c1)
{
#define element node
#define num_elements num_nodes
#include "connector.inc"
#undef modtype
}
