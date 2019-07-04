#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/global.h"
#include "core/log.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_cli|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();

  const char *graphcfg = 0;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      graphcfg = argv[++i];
  }
  if(!graphcfg)
  {
    dt_log(s_log_cli, "usage: vkdt-cli -g <graph.cfg> [-d verbosity]");
    exit(1);
  }
  if(qvk_init()) exit(1);

  dt_graph_t graph;
  dt_graph_init(&graph);
  int err = dt_graph_read_config_ascii(&graph, graphcfg);
  if(err)
  {
    dt_log(s_log_err, "could not load graph configuration from '%s'!", graphcfg);
    exit(1);
  }

  dt_graph_run(&graph, s_graph_run_all);

  dt_graph_cleanup(&graph);
  qvk_cleanup();
  exit(0);
}
