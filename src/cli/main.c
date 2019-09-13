#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-print.h"
#include "pipe/global.h"
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
  if(ldr)
  {
    const int m1 = dt_module_add(graph, dt_token("f2srgb8"),  inst);
    const int c1 = dt_module_get_connector(graph->module+m1, dt_token("input"));
    const int co = dt_module_get_connector(graph->module+m1, dt_token("output"));
    const int m2 = dt_module_add(graph, dt_token("export8"), inst);
    const int c2 = dt_module_get_connector(graph->module+m2, dt_token("input"));

    if(graph->module[m0].name == dt_token("f2srgb"))
    { // detect and skip f2srgb (replaced by f2srgb8)
      cid = dt_module_get_connector(graph->module+m0, dt_token("input"));
      c0 = graph->module[m0].connector[cid].connected_mc;
      m0 = graph->module[m0].connector[cid].connected_mi;
    }
    // connect: source (m0, c0) -> destination (m1, c1)
    CONN(dt_module_connect(graph, m0, c0, m1, c1));
    CONN(dt_module_connect(graph, m1, co, m2, c2));
  }
  else
  {
    const int m1 = dt_module_add(graph, dt_token("export"), inst);
    const int c1 = dt_module_get_connector(graph->module+m1, dt_token("input"));
    CONN(dt_module_connect(graph, m0, c0, m1, c1));
  }
  // TODO: set output filename parameter on export modules
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

  const char *graphcfg = 0;
  int dump_graph = 0;
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
    // TODO: parse more output: filename, format related things etc
  }
  if(!graphcfg)
  {
    dt_log(s_log_cli, "usage: vkdt-cli -g <graph.cfg> [-d verbosity] [--dump-modules|--dump-nodes]");
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

  // dump original modules, i.e. with display modules
  if(dump_graph == 1)
    dt_graph_print_modules(&graph);

  // replace requested display node by export node:
  err = replace_display(&graph, output, ldr, filename);
  if(err)
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
  qvk_cleanup();
  exit(0);
}
