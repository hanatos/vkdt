#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/io.h"
#include "pipe/graph-io.h"
#include "pipe/global.h"
#include "pipe/modules/api.h"
#include "core/log.h"
#include "core/solve.h"

#include <stdlib.h>
#include <float.h>

#define OPT_MAX_PAR 20

typedef struct opt_dat_t
{
  uint32_t   param_cnt;         // total number of parameters. slot[0] is the target.
  dt_token_t mod[OPT_MAX_PAR];  // module name
  dt_token_t mid[OPT_MAX_PAR];  // module instance
  dt_token_t pid[OPT_MAX_PAR];  // parameter name
  uint32_t   cnt[OPT_MAX_PAR];  // number of elements in this parameter
  float     *par[OPT_MAX_PAR];  // cached pointer to start of param on graph

  dt_graph_t graph;
}
opt_dat_t;

void evaluate_f(double *p, double *f, int m, int n, void *data)
{
  opt_dat_t *dat = data;
  for(int i=1;i<dat->param_cnt;i++) // [0] is the target parameter array
    for(int j=0;j<dat->cnt[i];j++)
      dat->par[i][j] = *(p++);

  VkResult res = dt_graph_run(&dat->graph,
      s_graph_run_record_cmd_buf | 
      s_graph_run_download_sink  |
      s_graph_run_wait_done);
  
  if(res)
  {
    dt_log(s_log_err, "failed to run graph!");
    exit(2); // no time for cleanup
  }

  for(int i=0;i<n;i++) // copy back results
    f[i] = dat->par[0][i];
}

void evaluate_J(double *p, double *J, int m, int n, void *data)
{
  double p2[m], f1[n], f2[n];
  const double h = 1e-10;
  for(int j=0;j<m;j++)
  {
    memcpy(p2, p, sizeof(p2));
    p2[j] = p[j] + h;
    evaluate_f(p2, f1, m, n, data);
    p2[j] = p[j] - h;
    evaluate_f(p2, f2, m, n, data);
    for(int k=0;k<n;k++) J[m*k + j] = (f1[k] - f2[k]) / (2.0*h);
  }
}

static inline int
init_param(
    opt_dat_t  *dat,
    char       *line,
    const int   p)
{
  dt_graph_t *graph = &dat->graph;
  dat->param_cnt = MAX(dat->param_cnt, p+1);

  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  // grab count from declaration in module_so_t:
  int modid = dt_module_get(graph, name, inst);
  if(modid < 0 || modid > graph->num_modules)
  {
    dt_log(s_log_err|s_log_pipe, "no such module/instance %"PRItkn"/%"PRItkn,
        dt_token_str(name), dt_token_str(inst));
    return 1;
  }
  int parid = dt_module_get_param(graph->module[modid].so, parm);
  if(parid < 0)
  {
    dt_log(s_log_err|s_log_pipe, "no such parameter name %"PRItkn, dt_token_str(parm));
    return 2;
  }
  const dt_ui_param_t *pui = graph->module[modid].so->param[parid];
  if(pui->type != dt_token("float"))
  {
    dt_log(s_log_err|s_log_pipe, "only supporting float params now %"PRItkn, dt_token_str(parm));
    return 3;
  }
  dat->par[p] = (float *)(graph->module[modid].param + pui->offset);
  dat->cnt[p] = pui->cnt;
  return 0;
}

int main(int argc, char *argv[])
{
  // init global things, log and pipeline:
  dt_log_init(s_log_cli|s_log_pipe);
  dt_log_init_arg(argc, argv);
  dt_pipe_global_init();
  threads_global_init();

  opt_dat_t dat = {0};

  int config_start = 0; // start of arguments which are interpreted as additional config lines
  const char *graph_cfg = 0;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      graph_cfg = argv[++i];
    else if(!strcmp(argv[i], "--target"))
    { if(init_param(&dat, argv[++i], 0)) exit(1); }
    else if(!strcmp(argv[i], "--param"))
    { if(init_param(&dat, argv[++i], dat.param_cnt+1)) exit(1); }
    else if(!strcmp(argv[i], "--config"))
    { config_start = i+1; break; }
  }

  if(qvk_init()) exit(1);

  if(!graph_cfg)
  {
    fprintf(stderr, "usage: vkdt-fit -g <graph.cfg>\n"
    "    [-d verbosity]                set log verbosity (mem,perf,pipe,cli,err,all)\n"
    "    [--param m:i:p]               add a parameter line to optimise. has to be float.\n"
    "    [--target m:i:p]              set the given module:inst:param as target for optimisation\n"
    "    [--config]                    everything after this will be interpreted as additional cfg lines\n"
        );
    threads_global_cleanup();
    qvk_cleanup();
    exit(1);
  }

  dt_graph_init(&dat.graph);
  VkResult err = dt_graph_read_config_ascii(&dat.graph, graph_cfg);
  if(err)
  {
    dt_log(s_log_err, "failed to load config file '%s'", graph_cfg);
    dt_graph_cleanup(&dat.graph);
    threads_global_cleanup();
    qvk_cleanup();
    exit(1);
  }

  for(int i=config_start;i<argc;i++)
    if(dt_graph_read_config_line(&dat.graph, argv[i]))
      dt_log(s_log_pipe|s_log_err, "failed to read extra params %d: '%s'", i - config_start, argv[i]);

  int num_params = 0;
  for(int i=1;i<dat.param_cnt;i++) num_params += dat.cnt[i];
  int num_target = dat.cnt[0];

  double p[num_params], t[num_target];
  double *pp = p; // set initial parameters
  for(int i=1;i<dat.param_cnt;i++)
    for(int j=0;j<dat.cnt[i];j++)
      *(pp++) = dat.par[i][j];
  pp = t; // also set target from cfg file
  for(int j=0;j<dat.cnt[0];j++)
    *(pp++) = dat.par[0][j];

  // init lower and upper bounds
  double lb[num_params], ub[num_params];
  for(int i=0;i<num_params;i++) lb[i] = -DBL_MAX;
  for(int i=0;i<num_params;i++) ub[i] =  DBL_MAX;

  const int num_it = 1000;
  dt_gauss_newton_cg(evaluate_f, evaluate_J,
    p, t, num_params, num_target,
    lb, ub, num_it, &dat);

  // TODO: output target + parameters (or just full cfg?)

  dt_graph_cleanup(&dat.graph);
  threads_global_cleanup();
  qvk_cleanup();
  exit(0);
}
