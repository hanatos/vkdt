#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-print.h"
#include "pipe/global.h"
#include "db/thumbnails.h"
#include "core/log.h"

#include <stdlib.h>

// replace given display node instance by export module.
// returns 0 on success.
static int
replace_display(
    dt_graph_t *graph,
    dt_token_t  inst,
    int         ldr,   // TODO: output format and params of all sorts
    const char *filename)
{
  const int mid = dt_module_get(graph, dt_token("display"), inst);
  if(mid < 0) return 1; // no display node by that name

  // get module the display is connected to:
  int cid = dt_module_get_connector(graph->module+mid, dt_token("input"));
  int m0 = graph->module[mid].connector[cid].connected_mi;
  int c0 = graph->module[mid].connector[cid].connected_mc;

  if(m0 < 0) return 2; // display input not connected

  // new module export with same inst
  // maybe new module 8-bit in between here
  dt_token_t export = ldr ? dt_token("o-jpg") : dt_token("o-pfm");
  const int m1 = dt_module_add(graph, export, inst);
  const int c1 = dt_module_get_connector(graph->module+m1, dt_token("input"));
  graph->module[m0].connector[c0].format = graph->module[m1].connector[c1].format;
  CONN(dt_module_connect(graph, m0, c0, m1, c1));

  // TODO: set output filename parameter on export module
  return 0;
}

// disconnect all remaining display nodes.
static void
disconnect_display_modules(
    dt_graph_t *graph)
{
  for(int m=0;m<graph->num_modules;m++)
    if(graph->module[m].name == dt_token("display"))
      dt_module_remove(graph, m); // disconnect and reset/ignore
}

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_cli|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();
  threads_global_init();

  const char *graphcfg = 0;
  int dump_graph = 0;
  const char *thumbnails = 0;
  dt_token_t output = dt_token("main");
  const char *filename = "output";
  int ldr = 1;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      graphcfg = argv[++i];
    else if(!strcmp(argv[i], "--dump-modules"))
      dump_graph = 1;
    else if(!strcmp(argv[i], "--dump-nodes"))
      dump_graph = 2;
    else if(!strcmp(argv[i], "--thumbnails") && i < argc-1)
      thumbnails = argv[++i];
    // TODO: parse more output: filename, format related things etc
  }

  if(qvk_init()) exit(1);

  if(thumbnails)
  {
    dt_thumbnails_t tn;
    // only width/height will matter here
    dt_thumbnails_init(&tn, 400, 400, 1000, 1ul<<30);
    dt_thumbnails_cache_directory(&tn, thumbnails);
    threads_wait(); // wait for bg threads to finish
    dt_thumbnails_cleanup(&tn);
    qvk_cleanup();
    exit(0);
  }

  if(!graphcfg)
  {
    dt_log(s_log_cli, "usage: vkdt-cli -g <graph.cfg> [-d verbosity] [--dump-modules|--dump-nodes] [--thumbnails <dir>]");
    qvk_cleanup();
    exit(1);
  }

  dt_graph_t graph;
  dt_graph_init(&graph);
  int err = dt_graph_read_config_ascii(&graph, graphcfg);
  if(err)
  {
    dt_log(s_log_err, "could not load graph configuration from '%s'!", graphcfg);
    qvk_cleanup();
    exit(1);
  }

  // dump original modules, i.e. with display modules
  if(dump_graph == 1)
    dt_graph_print_modules(&graph);

  // find non-display "main" module
  int found_main = 0;
  for(int m=0;m<graph.num_modules;m++)
  {
    if(graph.module[m].inst == dt_token("main") &&
       graph.module[m].name != dt_token("display"))
    {
      found_main = 1;
      break;
    }
  }

  // replace requested display node by export node:
  if(!found_main && replace_display(&graph, output, ldr, filename))
  {
    dt_log(s_log_err, "graph does not contain suitable display node %"PRItkn"!", dt_token_str(output));
    exit(2);
  }
  // make sure all remaining display nodes are removed:
  disconnect_display_modules(&graph);

  dt_graph_run(&graph, s_graph_run_all);

  // nodes we can only print after run() has been called:
  if(dump_graph == 2)
    dt_graph_print_nodes(&graph);

  dt_graph_cleanup(&graph);
  threads_global_cleanup();
  qvk_cleanup();
  exit(0);
}
