#include "qvk/qvk.h"
#include "pipe/graph.h"
#include "pipe/asciiio.h"
#include "pipe/graph-io.h"
#include "pipe/graph-export.h"
#include "pipe/global.h"
#include "pipe/modules/api.h"
#include "core/log.h"
#include "core/solve.h"

#include <stdlib.h>
#include <float.h>
#include <signal.h>

// this limits the number of module parameters to optimise for.
// each parameter may hold an array of values though.
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

static int user_abort = 0;
static int frame = 0;

void print_state(int signal)
{
  user_abort = 1; // flag so we can do async signal unsave things
}

void evaluate_f(double *p, double *f, int m, int n, void *data)
{
  opt_dat_t *dat = data;
  for(int i=1;i<dat->param_cnt;i++) // [0] is the target parameter array
    for(int j=0;j<dat->cnt[i];j++)
    {
      // fprintf(stderr, "p %d = %f\n", j, *p);
      dat->par[i][j] = *(p++);
    }

  // apply animation as stochastic gradient descent:
  dat->graph.frame = frame;
  dt_graph_apply_keyframes(&dat->graph);
  VkResult res = dt_graph_run(&dat->graph,
      s_graph_run_all |
      s_graph_run_wait_done);
  
  if(res)
  {
    dt_log(s_log_err, "failed to run graph!");
    exit(2); // no time for cleanup
  }

  for(int i=0;i<n;i++) // copy back results
    f[i] = dat->par[0][i];
  // for(int i=0;i<n;i++) fprintf(stderr, "f[%d] = %g\n", i, f[i]);
}

double loss(double *p, void *data)
{
  double f;
  evaluate_f(p, &f, 1, 1, data);
  return f;
}

void evaluate_J(double *p, double *J, int m, int n, void *data)
{
  double p2[m], f1[n], f2[n];
  memcpy(p2, p, sizeof(p2));
  evaluate_f(p2, f1, m, n, data);
  for(int j=0;j<m;j++)
  {
    const double s = xrand() >= 0.5 ? 1.0 : -1.0;
    const double h = s * (1e-10 + xrand()*1e-4);
    // memcpy(p2, p, sizeof(p2));
    // p2[j] = p[j] + h;
    // evaluate_f(p2, f1, m, n, data);
    memcpy(p2, p, sizeof(p2));
    p2[j] = p[j] + h;
    evaluate_f(p2, f2, m, n, data);
    // for(int k=0;k<n;k++) J[m*k + j] = CLAMP((f1[k] - f2[k]) / (2.0*h), -1e10, 1e10);
    for(int k=0;k<n;k++) J[m*k + j] = CLAMP((f2[k] - f1[k]) / h, -1e10, 1e10);
    // for(int k=0;k<n;k++) J[m*k + j] += (1.0-2.0*xrand())*1e-8; // XXX DEBUG
    // for(int k=0;k<n;k++) fprintf(stderr, "J[%d][%d] = %g\n", j, k, J[m*k+j]);
  }
  opt_dat_t *dat = data;
  frame = (frame + 1) % dat->graph.frame_cnt;
}

// init optimisation target as keyframe
static inline int
init_keyframe(
    opt_dat_t  *dat,
    char       *line,
    const int   p)
{
  dt_graph_t *graph = &dat->graph;
  dat->param_cnt = MAX(dat->param_cnt, p+1);

  int frame = dt_read_int(line, &line);
  dt_token_t name = dt_read_token(line, &line);
  dt_token_t inst = dt_read_token(line, &line);
  dt_token_t parm = dt_read_token(line, &line);
  // grab count from declaration in module_so_t:
  int modid = dt_module_get(graph, name, inst);
  if(modid < 0 || modid > graph->num_modules)
  {
    dt_log(s_log_err|s_log_pipe, "no such module:instance %"PRItkn":%"PRItkn,
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
  // TODO: find keyframe by stupidly iterating
  int ki = -1;
  for(int i=0;ki<0&&i<graph->module[modid].keyframe_cnt;i++)
    if(graph->module[modid].keyframe[i].frame == frame &&
       graph->module[modid].keyframe[i].param == parm) ki = i;
  if(ki < 0)
  {
    dt_log(s_log_err|s_log_pipe, "no such keyframe %d %"PRItkn, frame, dt_token_str(parm));
    return 4;
  }
  int beg = graph->module[modid].keyframe[ki].beg;
  int end = graph->module[modid].keyframe[ki].end;
  dat->par[p] = (float *)(graph->module[modid].keyframe[ki].data + dt_ui_param_size(pui->type, 1) * beg);
  dat->cnt[p] = end - beg;
    dt_log(s_log_cli, "initing param[%d] keyframe %d module:instance:param %"PRItkn":%"PRItkn":%"PRItkn,
        p,
        frame,
        dt_token_str(name),
        dt_token_str(inst),
        dt_token_str(parm)
        );
  return 0;
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
    dt_log(s_log_err|s_log_pipe, "no such module:instance %"PRItkn":%"PRItkn,
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
  dat->mod[p] = name;
  dat->mid[p] = inst;
  dat->pid[p] = parm;
  dat->par[p] = (float *)(graph->module[modid].param + pui->offset);
  dat->cnt[p] = pui->cnt;
    dt_log(s_log_cli, "initing param[%d] module:instance:param %"PRItkn":%"PRItkn":%"PRItkn,
        p,
        dt_token_str(name),
        dt_token_str(inst),
        dt_token_str(parm)
        );
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
  dat.param_cnt = 1;

  int config_start = 0; // start of arguments which are interpreted as additional config lines
  char *graph_cfg = 0;
  char *parstr[OPT_MAX_PAR] = {0};
  int keyframe[OPT_MAX_PAR] = {0};
  int optimiser = 0; // lu
  double adam_eps = 1e-8, adam_beta1 = 0.9, adam_beta2 = 0.999, adam_alpha = 0.01;
  for(int i=0;i<argc;i++)
  {
    if(!strcmp(argv[i], "-g") && i < argc-1)
      graph_cfg = argv[++i];
    else if(!strcmp(argv[i], "--target"))
      parstr[0] = argv[++i];
    else if(!strcmp(argv[i], "--param"))
      parstr[dat.param_cnt++] = argv[++i];
    else if(!strcmp(argv[i], "--keyframe"))
    { keyframe[dat.param_cnt] = 1; parstr[dat.param_cnt++] = argv[++i]; }
    else if(!strcmp(argv[i], "--adam") && i < argc-4)
    { optimiser = 1; adam_eps = atof(argv[++i]); adam_beta1 = atof(argv[++i]); adam_beta2 = atof(argv[++i]); adam_alpha = atof(argv[++i]); }
    else if(!strcmp(argv[i], "--nelder-mead"))
      optimiser = 2;
    else if(!strcmp(argv[i], "--bogosearch"))
      optimiser = 3;
    else if(!strcmp(argv[i], "--config"))
    { config_start = i+1; break; }
  }

  if(qvk_init(0, -1, 0, 0, 0)) exit(1);

  if(!graph_cfg || !dat.param_cnt)
  {
    fprintf(stderr, "usage: vkdt-fit -g <graph.cfg>\n"
    "    [-d verbosity]                 set log verbosity (none,mem,perf,pipe,cli,err,all)\n"
    "    [--param m:i:p]                add a parameter line to optimise. has to be float, need at least one\n"
    "    [--keyframe f:m:i:p]           add a keyframe to optimise, which is already present in the input cfg\n"
    "    [--target m:i:p]               set the given module:inst:param as target for optimisation\n"
    "    [--adam eps beta1 beta2 alpha] set the parameters of the adam optimiser\n"
    "    [--nelder-mead]                use nelder mead optimiser\n"
    "    [--config]                     everything after this will be interpreted as additional cfg lines\n"
        );
    threads_global_cleanup();
    qvk_cleanup();
    exit(1);
  }

  dt_graph_init(&dat.graph, s_queue_compute);
  snprintf(dat.graph.searchpath, sizeof(dat.graph.searchpath), ".");
  VkResult err = dt_graph_read_config_ascii(&dat.graph, graph_cfg);
  if(err)
  {
    dt_log(s_log_err, "failed to load config file '%s'", graph_cfg);
    dt_graph_cleanup(&dat.graph);
    threads_global_cleanup();
    qvk_cleanup();
    exit(1);
  }

  if(config_start)
    for(int i=config_start;i<argc;i++)
      if(dt_graph_read_config_line(&dat.graph, argv[i]))
        dt_log(s_log_pipe|s_log_err, "failed to read extra params %d: '%s'", i - config_start, argv[i]);

  dt_graph_disconnect_display_modules(&dat.graph);

  // cache data pointers for target and parameters:
  for(int i=0;i<dat.param_cnt;i++)
    if     ( keyframe[i] && init_keyframe(&dat, parstr[i], i)) exit(1);
    else if(!keyframe[i] && init_param   (&dat, parstr[i], i)) exit(1);

  int num_params = 0;
  for(int i=1;i<dat.param_cnt;i++) num_params += dat.cnt[i];
  int num_target = dat.cnt[0];

  if(adam_alpha > 0.0)
    dat.cnt[0] = num_target = 1; // adam supports only a single scalar loss value

  double p[num_params], t[num_target];
  double *pp = p; // set initial parameters
  for(int i=1;i<dat.param_cnt;i++)
    for(int j=0;j<dat.cnt[i];j++)
      *(pp++) = dat.par[i][j];
  pp = t; // also set target from cfg file
  for(int j=0;j<num_target;j++)
    *(pp++) = dat.par[0][j];

  signal(SIGINT, print_state); // ctrl-c

  dt_graph_run(&dat.graph, s_graph_run_all); // run once to init nodes

  // init lower and upper bounds
  double lb[num_params], ub[num_params];
  for(int i=0;i<num_params;i++) lb[i] = -DBL_MAX;
  for(int i=0;i<num_params;i++) ub[i] =  DBL_MAX;
  {
    int k=0;
    for(int i=1;i<dat.param_cnt;i++)
    {
      int modid = dt_module_get(&dat.graph, dat.mod[i], dat.mid[i]);
      if(modid < 0) break;
      int pid = dt_module_get_param(dat.graph.module[modid].so, dat.pid[i]);
      if(pid < 0) break;
      const dt_ui_param_t *param = dat.graph.module[modid].so->param[pid];
      if(!param) break;
      for(int j=0;j<dat.cnt[i];j++)
      {
        lb[k] = param->widget.min;
        ub[k] = param->widget.max;
        k++;
      }
    }
  }

  if(optimiser == 1)
    fprintf(stderr, "using the adam optimiser with eps %g beta1 %g beta2 %g alpha %g\n",
        adam_eps, adam_beta1, adam_beta2, adam_alpha);
  fprintf(stderr, "pre-opt params: ");
  for(int i=0;i<num_params;i++) fprintf(stderr, "%g ", p[i]);
  fprintf(stderr, "\n");
  // for(int i=7;i<num_params;i++) p[i] += 1e-5*(1.0-2.0*xrand()); // randomly perturb the initial guess

  double resid = DBL_MAX;
  if(optimiser == 0)
  {
    const int num_it = 400;
    resid = dt_gauss_newton_cg(evaluate_f, evaluate_J,
        p, t, num_params, num_target,
        lb, ub, num_it, &dat);
  }
  else if(optimiser == 1)
  {
    const int num_it = 20000;
    resid = dt_adam(evaluate_f, evaluate_J,
        p, t, num_params, num_target,
        lb, ub, num_it, &dat,
        adam_eps, adam_beta1, adam_beta2, adam_alpha,
        &user_abort);
  }
  else if(optimiser == 2)
  {
    resid = dt_nelder_mead(p, num_params, 20000, loss, lb, ub, &dat, &user_abort);
  }
  else if(optimiser == 3)
  {
    resid = dt_bogosearch(p, num_params, 2000, loss, lb, ub, &dat, &user_abort);
  }

  fprintf(stderr, "post-opt params: ");
  for(int i=0;i<num_params;i++) fprintf(stderr, "%g ", p[i]);
  fprintf(stderr, "\n");
  fprintf(stderr, "post-opt loss: %g\n", resid);

  pp = p;
  for(int i=1;i<dat.param_cnt;i++) // [0] is the target parameter array
    for(int j=0;j<dat.cnt[i];j++)
      dat.par[i][j] = *(pp++);

  // output full cfg to stdout
  dt_graph_write_config_ascii(&dat.graph, "/dev/stdout");

  dt_graph_cleanup(&dat.graph);
  threads_global_cleanup();
  qvk_cleanup();
  exit(0);
}
