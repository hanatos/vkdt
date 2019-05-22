#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/global.h"
#include "core/log.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
  dt_log_init(s_log_cli);
  if(argc < 2)
  {
    dt_log(s_log_cli, "usage: vkdt-cli <graph.cfg>");
    exit(1);
  }
  if(qvk_init()) exit(1);

  // TODO: create module graph from pipeline config
  // TODO: create nodes from graph
  dt_pipe_global_init();
  dt_graph_t graph;
  dt_graph_init(&graph);
  int err = dt_graph_read_config_ascii(&graph, argv[1]);
  if(err)
    dt_log(s_log_err, "could not load graph configuration!");

  qvk_cleanup();
  exit(0);
}
