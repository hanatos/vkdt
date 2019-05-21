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
  // TODO: perform some more exhaustive consistency checks
  // output dot file, build with
  // ./tests/graph > graph.dot
  // dot -Tpdf graph.dot -o graph.pdf
  fprintf(stdout, "digraph G {\n");
  // for all nodes, print all incoming edges (outgoing don't have module ids)
  for(int m=0;m<graph.num_modules;m++)
  {
    for(int c=0;c<graph.module[m].num_connectors;c++)
    {
      if((graph.module[m].connector[c].type == dt_token("read") ||
          graph.module[m].connector[c].type == dt_token("sink")) &&
          graph.module[m].connector[c].connected_mid >= 0)
      {
        fprintf(stdout, "%"PRItkn" -> %"PRItkn"\n",
            dt_token_str(graph.module[
            graph.module[m].connector[c].connected_mid
            ].name),
            dt_token_str(graph.module[m].name));
      }
    }
  }
  fprintf(stdout, "}\n");


  dt_graph_setup_pipeline(&graph);
  // TODO: debug rois
  // TODO: create mini pipeline with demosaic
  // TODO: write something out

  dt_graph_cleanup(&graph);
  dt_pipe_global_cleanup();
  exit(0);
}
