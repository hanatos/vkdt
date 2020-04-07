#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-print.h"
#include "pipe/graph-export.h"
#include "pipe/global.h"
#include "pipe/modules/api.h"
#include "core/log.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_cli|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();
  threads_global_init();

  const char *graphcfg = 0;
  int dump_graph = 0;
  int output_cnt = 1;
  int user_output_cnt = 0;
  float quality = 95.0f;
  dt_token_t output[10] = { dt_token("main"), dt_token("hist") };
  int ldr = 1;
  int config_start = 0; // start of arguments which are interpreted as additional config lines
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      graphcfg = argv[++i];
    else if(!strcmp(argv[i], "--quality") && i < argc-1)
      quality = atof(argv[++i]);
    else if(!strcmp(argv[i], "--dump-modules"))
      dump_graph = 1;
    else if(!strcmp(argv[i], "--dump-nodes"))
      dump_graph = 2;
    else if(!strcmp(argv[i], "--output") && i < argc-1 && ++i)
      output[user_output_cnt++] = dt_token(argv[i]);
    else if(!strcmp(argv[i], "--config"))
    { config_start = i+1; break; }

    // TODO: parse more options: filename, format related things etc
  }
  if(user_output_cnt) output_cnt = user_output_cnt;

  if(qvk_init()) exit(1);

  if(!graphcfg)
  {
    fprintf(stderr, "usage: vkdt-cli -g <graph.cfg>\n"
    "    [-d verbosity]                set log verbosity (mem,perf,pipe,cli,err,all)\n"
    "    [--dump-modules|--dump-nodes] write graphvis dot files to stdout\n"
    "    [--output <inst>]             name the instance of the output to write (can use multiple)\n"
    "    [--quality <0-100>]           jpg output quality\n"
    "    [--config]                    everything after this will be interpreted as additional cfg lines\n"
        );
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
  for(int i=config_start;i<argc;i++)
    if(dt_graph_read_config_line(&graph, argv[i]))
      dt_log(s_log_pipe|s_log_err, "failed in command line %d: '%s'", i - config_start + 1, argv[i]);

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
  if(!found_main)
  {
    int cnt = 0;
    for(;cnt<output_cnt;cnt++)
      if(dt_graph_replace_display(&graph, output[cnt], ldr))
        break;
    if(cnt == 0)
    {
      dt_log(s_log_err, "graph does not contain suitable display node %"PRItkn"!", dt_token_str(output));
      exit(2);
    }
  }
  // make sure all remaining display nodes are removed:
  dt_graph_disconnect_display_modules(&graph);

  dt_graph_export(&graph, output_cnt, output, 0, quality);

  // nodes we can only print after run() has been called:
  if(dump_graph == 2)
    dt_graph_print_nodes(&graph);

  dt_graph_cleanup(&graph);
  threads_global_cleanup();
  qvk_cleanup();
  exit(0);
}
