#include "modules/api.h"
#include "denox_gldr.h"
#include "denox_ildr.h"
#include "denox_aldr.h"
#include "denox_nldr.h"
#include "denox_gldrs.h"
#include "denox_ildrs.h"
#include "denox_aldrs.h"
#include "denox_nldrs.h"

typedef enum arch_t
{
  s_arch_none  = 0,
  s_arch_gldr  = 1,
  s_arch_ildr  = 2,
  s_arch_aldr  = 3,
  s_arch_nldr  = 4,
  s_arch_gldrs = 5,
  s_arch_ildrs = 6,
  s_arch_aldrs = 7,
  s_arch_nldrs = 8,
  s_arch_cnt   = 9
} arch_t;

static inline arch_t
get_model(dt_module_t *mod)
{
  int pid_arch  = dt_module_get_param(mod->so, dt_token("arch"));
  int pid_model = dt_module_get_param(mod->so, dt_token("model"));
  int par_arch  = dt_module_param_int(mod, pid_arch)[0];
  int par_model = dt_module_param_int(mod, pid_model)[0];
  if(par_arch == 0)
  {
    if     (qvk.vendorID == 0x10de) par_arch = 2; // nvidia
    else if(qvk.vendorID == 0x8086) par_arch = 3; // intel
    else if(qvk.vendorID == 0x1002) par_arch = 4; // amd
    else par_arch = 1; // uhm, sorry.
    // 0x1010 - imgtec
    // 0x13b5 - arm
    // 0x5143 - qualcomm
  }
  if(par_arch == 1 && par_model == 0) return s_arch_gldrs;
  if(par_arch == 1 && par_model == 1) return s_arch_gldr;
  if(par_arch == 2 && par_model == 0) return s_arch_nldrs;
  if(par_arch == 2 && par_model == 1) return s_arch_nldr;
  if(par_arch == 3 && par_model == 0) return s_arch_ildrs;
  if(par_arch == 3 && par_model == 1) return s_arch_ildr;
  if(par_arch == 4 && par_model == 0) return s_arch_aldrs;
  if(par_arch == 4 && par_model == 1) return s_arch_aldr;
  return s_arch_none;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  // all our parameters need to reload the weights and change the
  // graph layout/network architecture
  int par = dt_module_param_int(module, parid)[0];
  if(*(int*)oldval != par) return s_graph_run_all;
  return 0;
}

int read_source(dt_module_t *mod, void *mapped, dt_read_source_params_t *p)
{
  arch_t arch = get_model(mod);
  switch(arch)
  {
    case s_arch_gldr:  return denox_read_source_gldr (mod, mapped, p);
    case s_arch_ildr:  return denox_read_source_ildr (mod, mapped, p);
    case s_arch_aldr:  return denox_read_source_aldr (mod, mapped, p);
    case s_arch_nldr:  return denox_read_source_nldr (mod, mapped, p);
    case s_arch_gldrs: return denox_read_source_gldrs(mod, mapped, p);
    case s_arch_ildrs: return denox_read_source_ildrs(mod, mapped, p);
    case s_arch_aldrs: return denox_read_source_aldrs(mod, mapped, p);
    case s_arch_nldrs: return denox_read_source_nldrs(mod, mapped, p);
    default: return 1;
  }
}

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  const dt_roi_t input_roi = module->connector[0].roi;
  const uint32_t input_channels = 3;
  const dt_roi_t input_ssbo_roi =
  {
    .wd = input_roi.wd * input_roi.ht * input_channels,
    .ht = 1,
  };

  // convert any image rgba format into
  // a contiguous hwc ssbo format
  const int input = dt_node_add(
      graph, module, "oidn", "input", input_roi.wd, input_roi.ht, 1, 0, 0, 2,
      "input",  "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "ssbo", "f16", &input_ssbo_roi);

  // convert denox output into a rgba texture.
  const int output = dt_node_add(
      graph, module, "oidn", "output", input_roi.wd, input_roi.ht, 1, 0, 0, 2,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &input_roi);

  arch_t arch = get_model(module);
  switch(arch)
  {
    case s_arch_gldr :
      denox_create_nodes_gldr (graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    case s_arch_ildr :
      denox_create_nodes_ildr (graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    case s_arch_aldr :
      denox_create_nodes_aldr (graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    case s_arch_nldr :
      denox_create_nodes_nldr (graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    case s_arch_gldrs:
      denox_create_nodes_gldrs(graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    case s_arch_ildrs:
      denox_create_nodes_ildrs(graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    case s_arch_aldrs:
      denox_create_nodes_aldrs(graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    case s_arch_nldrs:
      denox_create_nodes_nldrs(graph, module, input_roi.ht, input_roi.wd, input,  "output", output, "input");
      break;
    default: return dt_connector_bypass(graph, module, 0, 1);
  }

  dt_connector_copy(graph, module, 0, input,  0);
  dt_connector_copy(graph, module, 1, output, 1);
}
