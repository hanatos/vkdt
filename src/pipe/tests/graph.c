#include "pipe/graph.h"
#include "pipe/global.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  dt_pipe_global_init();
  dt_graph_t graph;
  dt_graph_init(&graph);
  int err = dt_graph_read_config_ascii(&graph, "tests/pipe.cfg");
  assert(!err);
  // TODO: perform some consistency checks
  dt_graph_cleanup(&graph);
  dt_pipe_global_cleanup();
  exit(0);
}
