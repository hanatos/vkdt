#include "modules/api.h"
#include "denox_nseg.h"
// TODO generate these:
static int denox_read_source_gseg(dt_module_t* mod, void* mapped, dt_read_source_params_t* p) { return 1; }
static void denox_create_nodes_gseg(dt_graph_t* graph, dt_module_t* module,
      int input_id, const char* input_connector,
      int output_id, const char* output_connector) { }

#define NET_WD 384
#define NET_HT 384
#define NET_OUT 192 // logits grid
#define NET_CH 10   // people, sky, vegetation, water, mountain, ground, building, subject,
                    // artificial ground, clothing

typedef enum arch_t
{
  s_arch_auto    = 0,
  s_arch_generic = 1,
  s_arch_nvidia  = 2,
  s_arch_intel   = 3,
  s_arch_amd     = 4,
} arch_t;

static inline arch_t
get_model(dt_module_t *mod)
{
  int pid_arch  = dt_module_get_param(mod->so, dt_token("arch"));
  int par_arch  = dt_module_param_int(mod, pid_arch)[0];
  if(par_arch < s_arch_auto || par_arch > s_arch_nvidia) par_arch = s_arch_generic;
  if(par_arch == s_arch_auto)
  {
    if     (qvk.vendorID == s_qvk_vendorid_nvidia) par_arch = s_arch_nvidia; // nvidia
    else par_arch = 1; // uhm, sorry.
  }
  if(!qvk.coopmat_supported) par_arch = 1;
  return par_arch;
}

int read_source(dt_module_t *mod, void *mapped, dt_read_source_params_t *p)
{
  arch_t arch = get_model(mod);
  switch(arch)
  {
    case s_arch_generic: return denox_read_source_gseg(mod, mapped, p);
    case s_arch_nvidia : return denox_read_source_nseg(mod, mapped, p);
    default: return 1;
  }
}

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  const dt_roi_t input_roi = module->connector[0].roi;
  const dt_roi_t input_ssbo_roi = { .wd = NET_WD * NET_HT * 3, .ht = 1 };

  // resample to the network resolution, write a contiguous hwc ssbo
  const int pc_in[] = { NET_WD, NET_HT }, pc_out[] = { NET_OUT, NET_OUT, NET_CH };
  const int input = dt_node_add(
      graph, module, "segment", "input", NET_WD, NET_HT, 1, sizeof(pc_in), pc_in, 2,
      "input",  "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "ssbo", "f16", &input_ssbo_roi);

  // convert the logits of the chosen channel to a mask at input resolution
  const int output = dt_node_add(
      graph, module, "segment", "output", input_roi.wd, input_roi.ht, 1, sizeof(pc_out), pc_out, 2,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "output", "write", "y",    "f16", &input_roi);

  arch_t arch = get_model(module);
  switch(arch)
  {
    case s_arch_generic:
      denox_create_nodes_gseg(graph, module, input, "output", output, "input");
      break;
    case s_arch_nvidia:
      denox_create_nodes_nseg(graph, module, input, "output", output, "input");
      break;
    default:
      return dt_connector_bypass(graph, module, 0, 1);
  }

  dt_connector_copy(graph, module, 0, input,  0);
  dt_connector_copy(graph, module, 1, output, 1);
}
