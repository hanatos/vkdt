#include "modules/api.h"
#include "constants.h"
#include "wb.h"
#include "pos_wb.h"

// Module connector order is public API.
enum filmsim_module_port_t
{
  s_port_input = 0,
  s_port_output,
  s_port_filmsim,
  s_port_spectra,
};

// Fill a sampler slot the shader will not read: any bound image will do.
static inline void
filmsim_bind_dummy(dt_graph_t *graph, dt_module_t *module, int dst_node, int dst_port)
{
  dt_connector_copy(graph, module, s_port_filmsim, dst_node, dst_port);
}

// Index of a named connector on an already created node.
static inline int
filmsim_conn_id(dt_graph_t *graph, int node, const char *conn)
{
  const dt_token_t connt = dt_token(conn);
  for(int c=0;c<graph->node[node].num_connectors;c++)
    if(graph->node[node].connector[c].name == connt) return c;
  assert(0 && "no such connector");
  return -1;
}

// Convert negative micrometres to film pixels.
#define FILMSIM_UM_TO_SIGMA_PX(um, iwd, iht) ((um) * MAX(iwd, iht) / FILMSIM_FRAME_WIDTH_UM)
// dt_api_blur takes 2 sigma.
#define FILMSIM_BLUR_PYR_R(sigma_px) (2.0f * (sigma_px))
// Smaller kernels are identity.
#define FILMSIM_SIGMA_IS_NOOP(sigma_px) ((sigma_px) < 1.0f/3.0f)

// Skip identity blurs; otherwise use the generic blur.
static inline int
filmsim_blur(dt_graph_t *graph, dt_module_t *module, int src, const char *conn, float sigma_px)
{
  if(FILMSIM_SIGMA_IS_NOOP(sigma_px)) return src;
  int id_out = -1;
  dt_api_blur(graph, module, src, filmsim_conn_id(graph, src, conn), 0, &id_out, FILMSIM_BLUR_PYR_R(sigma_px));
  return id_out;
}

// Mean RGB scatter widths; vkdt blur is scalar.
#define FILMSIM_SCATTER_CORE_UM 1.9333333f
#define FILMSIM_SCATTER_TAIL_UM 9.3666667f
// spektrafilm's 3-gaussian fit to exp(-r/lambda): sigma_k = ratio_k * lambda
#define FILMSIM_SCATTER_N_TAIL 3
static const float filmsim_scatter_exp_ratio[FILMSIM_SCATTER_N_TAIL] = { 0.5360f, 1.5236f, 2.7684f };

// Negative filter c requests auto white balance.
#define FILMSIM_WB_AUTO (-1.0f)

// Floats of prepared per-frame state handed from setup to the pixel passes.
#define FILMSIM_PREP_BUF_SIZE 2048

#define FILMSIM_NUM_FILMS  ((int)(sizeof(wb)    / sizeof(wb[0])))
#define FILMSIM_NUM_PAPERS ((int)(sizeof(wb[0]) / sizeof(wb[0][0])))
_Static_assert(sizeof(pos_wb)/sizeof(pos_wb[0]) == (size_t)FILMSIM_NUM_FILMS,
               "pos_wb.h is out of sync with wb.h, rerun its neutral fit (see the top of pos_wb.h)");

// the enlarge combo as a linear magnification
static inline int
filmsim_enlarge_scale(dt_module_t *module)
{
  const int pid = dt_module_get_param(module->so, dt_token("enlarge"));
  const int par = CLAMP(dt_module_param_int(module, pid)[0], 0, 2);
  return 1 << par;
}

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int s = filmsim_enlarge_scale(module);
  module->connector[s_port_input].roi = module->connector[s_port_output].roi;
  module->connector[s_port_input].roi.wd /= s;
  module->connector[s_port_input].roi.ht /= s;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int s = filmsim_enlarge_scale(module);
  module->connector[s_port_output].roi.full_wd = MIN(32768, module->connector[s_port_input].roi.full_wd * s);
  module->connector[s_port_output].roi.full_ht = MIN(32768, module->connector[s_port_input].roi.full_ht * s);
}

// write the fitted neutral for the selected stocks onto the sliders. positives
// are balanced at scan, negatives under the enlarger.
static inline void
filmsim_apply_wb_auto(dt_module_t *module, int pid_process, int pid_f, int pid_p)
{
  const int positive = dt_module_param_int(module, pid_process)[0] == FILMSIM_PROCESS_SCAN_NEG;
  const int film  = CLAMP(dt_module_param_int(module, pid_f)[0], 0, FILMSIM_NUM_FILMS-1);
  const int paper = CLAMP(dt_module_param_int(module, pid_p)[0], 0, FILMSIM_NUM_PAPERS-1);
  const float *v = positive ? pos_wb[film] : wb[film][paper];
  const dt_token_t par[4] = {
    positive ? dt_token("ev film") : dt_token("ev paper"),
    dt_token("filter c"), dt_token("filter m"), dt_token("filter y") };
  for(int i=0;i<4;i++)
    ((float*)dt_module_param_float(module, dt_module_get_param(module->so, par[i])))[0] = v[i];
}

// Resolved graph topology and dimensions.
typedef struct filmsim_plan_t
{
  int process;
  int couplers;
  int halation;
  int hal_bounces;
  int scatter;
  int iwd, iht;
  int owd, oht;
  int use_curvewarp;
  int use_fastpath;
  float cp_sigma;
  float hal_sigma;
} filmsim_plan_t;

static filmsim_plan_t
filmsim_make_plan(dt_module_t *module)
{
  filmsim_plan_t plan = {0};
  plan.iwd = module->connector[s_port_input].roi.wd;
  plan.iht = module->connector[s_port_input].roi.ht;
  plan.owd = module->connector[s_port_output].roi.wd;
  plan.oht = module->connector[s_port_output].roi.ht;
  plan.process = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("process")))[0];
  plan.couplers = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("couplers")))[0];
  plan.halation = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("halation")))[0];
  plan.hal_bounces = CLAMP(dt_module_param_int(module, dt_module_get_param(module->so, dt_token("hal bnc")))[0], 1, FILMSIM_MAX_HAL_BOUNCES);
  plan.scatter = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("scat amt")))[0] > 0.0f;
  plan.use_curvewarp = plan.couplers > 0 && plan.process != FILMSIM_PROCESS_PRINT_NEG;
  plan.use_fastpath = plan.process != FILMSIM_PROCESS_PRINT_NEG && plan.couplers <= 0 && !plan.halation;
  plan.cp_sigma = FILMSIM_UM_TO_SIGMA_PX(dt_module_param_float(module, dt_module_get_param(module->so, dt_token("cp rad")))[0], plan.iwd, plan.iht);
  plan.hal_sigma = FILMSIM_UM_TO_SIGMA_PX(dt_module_param_float(module, dt_module_get_param(module->so, dt_token("radius")))[0], plan.iwd, plan.iht);
  return plan;
}

void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int pid_filter_c = dt_module_get_param(module->so, dt_token("filter c"));
  if(dt_module_param_float(module, pid_filter_c)[0] == FILMSIM_WB_AUTO)
  {
    int pid_process = dt_module_get_param(module->so, dt_token("process"));
    int pid_f = dt_module_get_param(module->so, dt_token("film"));
    int pid_p = dt_module_get_param(module->so, dt_token("paper"));
    filmsim_apply_wb_auto(module, pid_process, pid_f, pid_p);
  }
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  int pid_process = dt_module_get_param(module->so, dt_token("process"));
  int pid_f = dt_module_get_param(module->so, dt_token("film"));
  int pid_p = dt_module_get_param(module->so, dt_token("paper"));
  // Topology changes require a graph rebuild.
  const dt_token_t topo_par[] = {
    dt_token("process"), dt_token("enlarge"),
    dt_token("couplers"), dt_token("halation"), dt_token("hal bnc"),
  };
  for(int i=0;i<(int)(sizeof(topo_par)/sizeof(topo_par[0]));i++)
  {
    if(parid != (uint32_t)dt_module_get_param(module->so, topo_par[i])) continue;
    if(*(int*)oldval != dt_module_param_int(module, parid)[0])
    {
      // Process selects the auto white-balance table.
      if(parid == (uint32_t)pid_process)
        filmsim_apply_wb_auto(module, pid_process, pid_f, pid_p);
      return s_graph_run_all;
    }
    return s_graph_run_record_cmd_buf;
  }

  int iwd = module->connector[s_port_input].roi.wd;
  int iht = module->connector[s_port_input].roi.ht;
  const int pid_rad = dt_module_get_param(module->so, dt_token("radius"));
  const int pid_cp_rad = dt_module_get_param(module->so, dt_token("cp rad"));

  if(parid == (uint32_t)pid_rad || parid == (uint32_t)pid_cp_rad)
  {
    float s_old = FILMSIM_UM_TO_SIGMA_PX(*(float*)oldval, iwd, iht);
    float s_new = FILMSIM_UM_TO_SIGMA_PX(dt_module_param_float(module, parid)[0], iwd, iht);
    if(FILMSIM_SIGMA_IS_NOOP(s_old) && FILMSIM_SIGMA_IS_NOOP(s_new)) return s_graph_run_record_cmd_buf;
    return s_graph_run_all;
  }
  // Only zero/non-zero scatter changes topology.
  const int pid_scat = dt_module_get_param(module->so, dt_token("scat amt"));
  if(parid == (uint32_t)pid_scat)
  {
    const int hal = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("halation")))[0];
    const float o = *(float*)oldval, n = dt_module_param_float(module, pid_scat)[0];
    if(hal && (o <= 0.0f) != (n <= 0.0f)) return s_graph_run_all;
    return s_graph_run_record_cmd_buf;
  }
  if(parid == pid_f || parid == pid_p)
  { // film or paper changed, update the pre-optimised wb coeffs
    if(*(int*)oldval != dt_module_param_int(module, parid)[0])
      filmsim_apply_wb_auto(module, pid_process, pid_f, pid_p);
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms is fine
}

// Optional DIR-coupler pre-warp grid.
static int
build_curvewarp_node(dt_graph_t *graph, dt_module_t *module, const filmsim_plan_t *plan)
{
  if(!plan->use_curvewarp) return -1;
  dt_roi_t roi_warp = (dt_roi_t){ .full_wd = FILMSIM_LOGEXP_GRID, .full_ht = 1,
                                  .wd = FILMSIM_LOGEXP_GRID, .ht = 1 };
  const int id_curvewarp = dt_node_add(graph, module, "filmsim", "dirlut", 1, 1, 1, 0, 0, 2,
      "output",    "write", "rgba", "f16",  &roi_warp,
      "prep",      "read",  "*",    "*",    dt_no_roi);
  return id_curvewarp;
}

// Once-per-frame prepared state.
static int
build_setup_node(dt_graph_t *graph, dt_module_t *module, int id_curvewarp, const filmsim_plan_t *plan)
{
  dt_roi_t roi_prep_buf   = (dt_roi_t){ .full_wd = FILMSIM_PREP_BUF_SIZE, .full_ht = 1, .wd = FILMSIM_PREP_BUF_SIZE, .ht = 1 };
  const int pc[] = { plan->iwd, plan->iht, plan->owd, plan->oht };
  const int id_setup = dt_node_add(graph, module, "filmsim", "setup", 1, 1, 1, sizeof(pc), pc, 3,
      "filmsim",   "read",  "*",    "*",    dt_no_roi,
      "prep",      "write", "ssbo", "f32",  &roi_prep_buf,
      "spectra",   "read",  "*",    "*",    dt_no_roi);
  filmsim_bind_dummy(graph, module, id_setup, 0);
  dt_connector_copy(graph, module, s_port_spectra, id_setup, 2);
  if (id_curvewarp >= 0) // cwarp needs prep's density-model center offset
    CONN(dt_node_connect_named(graph, id_setup, "prep", id_curvewarp, "prep"));
  return id_setup;
}

// Print an already-scanned negative.
static void
build_negprint_stage(dt_graph_t *graph, dt_module_t *module, int id_setup)
{
  int owd = module->connector[s_port_output].roi.wd;
  int oht = module->connector[s_port_output].roi.ht;
  const int id_negprint = dt_node_add(graph, module, "filmsim", "negprint", owd, oht, 1, 0, 0, 4,
      "input",     "read",  "*",    "*",    dt_no_roi,
      "spectra",   "read",  "*",    "*",    dt_no_roi,
      "prep",      "read",  "*",    "*",    dt_no_roi,
      "out",       "write", "rgba", "f16", &module->connector[s_port_output].roi);
  dt_connector_copy(graph, module, s_port_input, id_negprint, 0);
  dt_connector_copy(graph, module, s_port_spectra, id_negprint, 1);
  CONN(dt_node_connect_named(graph, id_setup, "prep", id_negprint, "prep"));
  dt_connector_copy(graph, module, s_port_output, id_negprint, 3);
}

// Fast path: expose, develop, and finalize in one dispatch.
static void
build_fastpath_stage(dt_graph_t *graph, dt_module_t *module, int id_setup,
    const filmsim_plan_t *plan)
{
  const int id_dev = dt_node_add(graph, module, "filmsim", "fast", plan->owd, plan->oht, 1, 0, 0, 4,
      "input",     "read",  "*",    "*",    dt_no_roi,
      "prep",      "read",  "*",    "*",    dt_no_roi,
      "spectra",   "read",  "*",    "*",    dt_no_roi,
      "output",    "write", "rgba", "f16", &module->connector[s_port_output].roi);
  dt_connector_copy(graph, module, s_port_input, id_dev, 0);
  CONN(dt_node_connect_named(graph, id_setup, "prep", id_dev, "prep"));
  dt_connector_copy(graph, module, s_port_spectra, id_dev, 2);
  dt_connector_copy(graph, module, s_port_output, id_dev, 3);
}

static int
build_expose_stage(dt_graph_t *graph, dt_module_t *module, int id_setup,
    const filmsim_plan_t *plan)
{
  dt_roi_t roi_cp_stub = (dt_roi_t){ .full_wd = 1, .full_ht = 1, .wd = 1, .ht = 1 };
  dt_roi_t *roi_cp = plan->couplers > 0 ? &module->connector[s_port_input].roi : &roi_cp_stub;
  const int id_expose = dt_node_add(graph, module, "filmsim", "expose", plan->iwd, plan->iht, 1, 0, 0, 5,
      "input",     "read",  "*",    "*",    dt_no_roi,
      "spectra",   "read",  "*",    "*",    dt_no_roi,
      "prep",      "read",  "*",    "*",    dt_no_roi,
      "coupler",   "write", "rgba", "f16", roi_cp,
      "exp",       "write", "rgba", "f16", &module->connector[s_port_input].roi);
  dt_connector_copy(graph, module, s_port_input, id_expose, 0);
  dt_connector_copy(graph, module, s_port_spectra, id_expose, 1);
  CONN(dt_node_connect_named(graph, id_setup, "prep", id_expose, "prep"));
  return id_expose;
}

// Apply DIR correction before optional halation.
static int
build_postexpose_stage(dt_graph_t *graph, dt_module_t *module, int id_curvewarp, int id_setup,
    int id_expose, const filmsim_plan_t *plan)
{
  int id_dev1;
  if(plan->halation)
    id_dev1 = dt_node_add(graph, module, "filmsim", "halin", plan->iwd, plan->iht, 1, 0, 0, 4,
        "exp",       "read",  "*",    "*",    dt_no_roi,
        "prep",      "read",  "*",    "*",    dt_no_roi,
        "coupler",   "read",  "*",    "*",    dt_no_roi,
        "output",    "write", "rgba", "f16", &module->connector[s_port_input].roi);
  else
  {
    assert(id_curvewarp >= 0);
    id_dev1 = dt_node_add(graph, module, "filmsim", "develop", plan->owd, plan->oht, 1, 0, 0, 5,
        "exp",       "read",  "*",    "*",    dt_no_roi,
        "prep",      "read",  "*",    "*",    dt_no_roi,
        "cwarp",     "read",  "*",    "*",    dt_no_roi,
        "coupler",   "read",  "*",    "*",    dt_no_roi,
        "output",    "write", "rgba", "f16", &module->connector[s_port_output].roi);
  }
  CONN(dt_node_connect_named(graph, id_setup, "prep", id_dev1, "prep"));
  if(!plan->halation)
    CONN(dt_node_connect_named(graph, id_curvewarp, "output", id_dev1, "cwarp"));

  const int id_cp = plan->couplers > 0 ?
    filmsim_blur(graph, module, id_expose, "coupler", plan->cp_sigma) : id_expose;
  if(id_cp != id_expose)
    CONN(dt_node_connect_named(graph, id_cp, "output", id_dev1, "coupler"));
  else
    CONN(dt_node_connect_named(graph, id_expose, "coupler", id_dev1, "coupler"));

  return id_dev1;
}

// Optional in-emulsion scatter before halation.
static int
build_scatter_stage(dt_graph_t *graph, dt_module_t *module, int id_dev1, const filmsim_plan_t *plan)
{
  if(!plan->scatter) return id_dev1;

  const int id_scatter = dt_node_add(graph, module, "filmsim", "scatter", plan->iwd, plan->iht, 1, 0, 0, 6,
      "input",     "read",  "*",    "*",    dt_no_roi,
      "output",    "write", "rgba", "f16", &module->connector[s_port_input].roi,
      "core",      "read",  "*",    "*",    dt_no_roi,
      "tail0",     "read",  "*",    "*",    dt_no_roi,
      "tail1",     "read",  "*",    "*",    dt_no_roi,
      "tail2",     "read",  "*",    "*",    dt_no_roi);
  CONN(dt_node_connect_named(graph, id_dev1, "output", id_scatter, "input"));

  const float sc_um[1+FILMSIM_SCATTER_N_TAIL] = {
    FILMSIM_SCATTER_CORE_UM,
    FILMSIM_SCATTER_TAIL_UM * filmsim_scatter_exp_ratio[0],
    FILMSIM_SCATTER_TAIL_UM * filmsim_scatter_exp_ratio[1],
    FILMSIM_SCATTER_TAIL_UM * filmsim_scatter_exp_ratio[2],
  };
  const char *sc_conn[1+FILMSIM_SCATTER_N_TAIL] = { "core", "tail0", "tail1", "tail2" };
  for(int k=0; k<1+FILMSIM_SCATTER_N_TAIL; k++)
  {
    const int id_b = filmsim_blur(graph, module, id_dev1, "output",
        FILMSIM_UM_TO_SIGMA_PX(sc_um[k], plan->iwd, plan->iht));
    CONN(dt_node_connect_named(graph, id_b, "output", id_scatter, sc_conn[k]));
  }
  return id_scatter;
}

// Blur exposure bounces and finalize halation.
static int
build_halation_stage(dt_graph_t *graph, dt_module_t *module, int id_curvewarp, int id_setup,
    int id_dev1, const filmsim_plan_t *plan)
{
  const int id_dev2 = dt_node_add(graph, module, "filmsim", "halout", plan->owd, plan->oht, 1, 0, 0, 10,
      "exp",       "read",  "*",    "*",    dt_no_roi,
      "prep",      "read",  "*",    "*",    dt_no_roi,
      "cwarp",     "read",  "*",    "*",    dt_no_roi,
      "output",    "write", "rgba", "f16", &module->connector[s_port_output].roi,
      "hal0",      "read",  "*",    "*",    dt_no_roi,
      "hal1",      "read",  "*",    "*",    dt_no_roi,
      "hal2",      "read",  "*",    "*",    dt_no_roi,
      "hal3",      "read",  "*",    "*",    dt_no_roi,
      "hal4",      "read",  "*",    "*",    dt_no_roi,
      "hal5",      "read",  "*",    "*",    dt_no_roi);
  CONN(dt_node_connect_named(graph, id_setup, "prep", id_dev2, "prep"));
  if (id_curvewarp >= 0)
    CONN(dt_node_connect_named(graph, id_curvewarp, "output", id_dev2, "cwarp"));
  else
    filmsim_bind_dummy(graph, module, id_dev2, 2);

  const int id_hal_src = build_scatter_stage(graph, module, id_dev1, plan);

  static const char *hal_conn[] = { "hal0", "hal1", "hal2", "hal3", "hal4", "hal5" };
  int id_hal[FILMSIM_MAX_HAL_BOUNCES];
  for(int k=0; k<FILMSIM_MAX_HAL_BOUNCES; k++)
  {
    if(k < plan->hal_bounces)
      id_hal[k] = filmsim_blur(graph, module, id_hal_src, "output", plan->hal_sigma*sqrtf(k+1.0f));
    else id_hal[k] = id_hal[0];
    CONN(dt_node_connect_named(graph, id_hal[k], "output", id_dev2, hal_conn[k]));
  }

  CONN(dt_node_connect_named(graph, id_hal_src, "output", id_dev2, "exp"));
  return id_dev2;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const filmsim_plan_t plan = filmsim_make_plan(module);

  const int id_curvewarp = build_curvewarp_node(graph, module, &plan);
  const int id_setup = build_setup_node(graph, module, id_curvewarp, &plan);

  if(plan.process == FILMSIM_PROCESS_PRINT_NEG)
  {
    build_negprint_stage(graph, module, id_setup);
    return;
  }

  if(plan.use_fastpath)
  {
    build_fastpath_stage(graph, module, id_setup, &plan);
    return;
  }

  const int id_expose = build_expose_stage(graph, module, id_setup, &plan);
  const int id_dev1 = build_postexpose_stage(graph, module, id_curvewarp, id_setup,
      id_expose, &plan);
  const int id_dev2 = plan.halation ?
    build_halation_stage(graph, module, id_curvewarp, id_setup, id_dev1, &plan) :
    id_dev1;

  CONN(dt_node_connect_named(graph, id_expose, "exp", id_dev1, "exp"));
  dt_connector_copy(graph, module, s_port_output, id_dev2, filmsim_conn_id(graph, id_dev2, "output"));
}
