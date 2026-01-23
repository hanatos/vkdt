#include "modules/api.h"
#include "denox_gbyrc.h"
#include "denox_nbyrc.h"
#include "denox_gxtrc.h"
#include "denox_nxtrc.h"
#include "denox_gbyrn.h"
#include "denox_nbyrn.h"
#include "denox_gxtrn.h"
#include "denox_nxtrn.h"

// intel and amd yet unknown:
// #include "denox_ibyrc.h"
// #include "denox_abyrc.h"
// #include "denox_ixtrc.h"
// #include "denox_axtrc.h"
// #include "denox_ibyrn.h"
// #include "denox_abyrn.h"
// #include "denox_ixtrn.h"
// #include "denox_axtrn.h"

typedef enum arch_t
{
  s_arch_none = 0,
  s_arch_gbyc,
  s_arch_gbyn,
  s_arch_ibyc,
  s_arch_ibyn,
  s_arch_abyc,
  s_arch_abyn,
  s_arch_nbyc,
  s_arch_nbyn,
  s_arch_gxtc,
  s_arch_gxtn,
  s_arch_ixtc,
  s_arch_ixtn,
  s_arch_axtc,
  s_arch_axtn,
  s_arch_nxtc,
  s_arch_nxtn,
  s_arch_cnt
} arch_t;

static inline arch_t
get_model(dt_module_t *mod)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(mod->graph, mod, dt_token("input"));
  if(!img_param) return s_arch_none; // must have disconnected input somewhere
  int xtr = img_param->filters == 9 ? 1 : 0;
  int pid_arch = dt_module_get_param(mod->so, dt_token("arch"));
  int pid_vari = dt_module_get_param(mod->so, dt_token("variant"));
  int par_arch = dt_module_param_int(mod, pid_arch)[0];
  int par_vari = dt_module_param_int(mod, pid_vari)[0];
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
  if(par_arch == 1 && !xtr && !par_vari) return s_arch_gbyc;
  if(par_arch == 1 && !xtr &&  par_vari) return s_arch_gbyn;
  if(par_arch == 1 &&  xtr && !par_vari) return s_arch_gxtc;
  if(par_arch == 1 &&  xtr &&  par_vari) return s_arch_gxtn;
  if(par_arch == 2 && !xtr && !par_vari) return s_arch_nbyc;
  if(par_arch == 2 && !xtr &&  par_vari) return s_arch_nbyn;
  if(par_arch == 2 &&  xtr && !par_vari) return s_arch_nxtc;
  if(par_arch == 2 &&  xtr &&  par_vari) return s_arch_nxtn;
  if(par_arch == 3 && !xtr && !par_vari) return s_arch_ibyc;
  if(par_arch == 3 && !xtr &&  par_vari) return s_arch_ibyn;
  if(par_arch == 3 &&  xtr && !par_vari) return s_arch_ixtc;
  if(par_arch == 3 &&  xtr &&  par_vari) return s_arch_ixtn;
  if(par_arch == 4 && !xtr && !par_vari) return s_arch_abyc;
  if(par_arch == 4 && !xtr &&  par_vari) return s_arch_abyn;
  if(par_arch == 4 &&  xtr && !par_vari) return s_arch_axtc;
  if(par_arch == 4 &&  xtr &&  par_vari) return s_arch_axtn;
  return s_arch_none;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  const int block  = module->img_param.filters == 9u ? 3 : 2;
  const float scale = ro->full_wd > 0 ? (float)ri->full_wd/(float)ro->full_wd : 1.0f;
  const int halfsize = scale >= block;
  ro->marker = ri->marker;
  if(halfsize)
  {
    ro->full_wd = (ri->full_wd+1)/2;
    ro->full_ht = (ri->full_ht+1)/2;
    if(scale >= block) ro->marker = s_roi_mark_soft_fwd; // this might be overwritten
  }
  else
  {
    ro->full_wd = ri->full_wd;
    ro->full_ht = ri->full_ht;
  }
  module->img_param.filters = 0u; // after we're done there won't be any more mosaic
}

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  ri->wd = ri->full_wd;
  ri->ht = ri->full_ht;
}

dt_graph_run_t
check_params(
    dt_module_t *mod,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int pida = dt_module_get_param(mod->so, dt_token("arch"));
  const int pidv = dt_module_get_param(mod->so, dt_token("variant"));
  const int vala = CLAMP(dt_module_param_int(mod, pida)[0], 0, 1);
  const int valv = CLAMP(dt_module_param_int(mod, pidv)[0], 0, 1);
  if(parid == pida) // arch changed
    if(*(int*)oldval != vala) 
      return s_graph_run_all;
  if(parid == pidv) // variant changed
    if(*(int*)oldval != valv) 
      return s_graph_run_all;
  return 0;
}

int read_source(dt_module_t *mod, void *mapped, dt_read_source_params_t *p)
{
  arch_t arch = get_model(mod);
  switch(arch)
  {
    case s_arch_gbyc: return denox_read_source_gbyrc(mod, mapped, p);
    case s_arch_gbyn: return denox_read_source_gbyrn(mod, mapped, p);
    // case s_arch_ibyc: return denox_read_source_ibyrc(mod, mapped, p);
    // case s_arch_ibyn: return denox_read_source_ibyrn(mod, mapped, p);
    // case s_arch_abyc: return denox_read_source_abyrc(mod, mapped, p);
    // case s_arch_abyn: return denox_read_source_abyrn(mod, mapped, p);
    case s_arch_nbyc: return denox_read_source_nbyrc(mod, mapped, p);
    case s_arch_nbyn: return denox_read_source_nbyrn(mod, mapped, p);
    case s_arch_gxtc: return denox_read_source_gxtrc(mod, mapped, p);
    case s_arch_gxtn: return denox_read_source_gxtrn(mod, mapped, p);
    // case s_arch_ixtc: return denox_read_source_ixtrc(mod, mapped, p);
    // case s_arch_ixtn: return denox_read_source_ixtrn(mod, mapped, p);
    // case s_arch_axtc: return denox_read_source_axtrc(mod, mapped, p);
    // case s_arch_axtn: return denox_read_source_axtrn(mod, mapped, p);
    case s_arch_nxtc: return denox_read_source_nxtrc(mod, mapped, p);
    case s_arch_nxtn: return denox_read_source_nxtrn(mod, mapped, p);
    default: return 1;
  }
}

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return; // must have disconnected input somewhere
  module->img_param.filters = 0u; // after we're done there won't be any more mosaic
  int block = img_param->filters == 9 ? 3 : 2;
  const float scale = (float)module->connector[0].roi.wd/(float)module->connector[1].roi.wd;
  const int halfsize = scale >= block;
  if(halfsize)
  { // half size
    dt_roi_t roi_half = module->connector[0].roi;
    roi_half.full_wd /= block;
    roi_half.full_ht /= block;
    roi_half.wd /= block;
    roi_half.ht /= block;
    const int pc[] = { 0, 0, 0, 0, img_param->filters };
    const int id_half = dt_node_add(graph, module, "demosaic", "halfsize",
        roi_half.wd, roi_half.ht, 1, sizeof(pc), pc, 2,
        "input",  "read",  "rggb", "*",   dt_no_roi,
        "output", "write", "rgba", "f16", &roi_half);
    dt_connector_copy(graph, module, 0, id_half, 0);
    if(block != scale)
    { // resample to get to the rest of the resolution, only if block != scale!
      const int id_resample = dt_node_add(graph, module, "shared", "resample",
          module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
          "input",  "read",  "rgba", "f16", dt_no_roi,
          "output", "write", "rgba", "f16", &module->connector[1].roi);
      CONN(dt_node_connect(graph, id_half, 1, id_resample, 0));
      dt_connector_copy(graph, module, 1, id_resample, 1);
    }
    else dt_connector_copy(graph, module, 1, id_half, 1);
    return;
  }

  const dt_roi_t input_roi = module->connector[0].roi;
  dt_roi_t input_ssbo_roi = input_roi;
  if(img_param->filters == 9)
    input_ssbo_roi.ht *= 4;

  // take care of bayer re-swizzling to HWC with C=block*block, apply pu:
  const int pc[] = { img_param->filters };
  const int input = dt_node_add(
      graph, module, "jddcnn", "input", input_roi.wd, input_roi.ht, 1, sizeof(pc), pc, 2,
      "input",  "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "ssbo", "f16", &input_ssbo_roi);

  const int output = dt_node_add(
      graph, module, "jddcnn", "output", input_roi.wd, input_roi.ht, 1, sizeof(pc), pc, 2,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &input_roi);

  arch_t arch = get_model(module);
  switch(arch)
  {
    case s_arch_gbyc:
    case s_arch_gbyn:
      denox_create_nodes_gbyrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
      break;
    // case s_arch_ibyc:
    // case s_arch_ibyn:
    //   denox_create_nodes_ibyrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
    //   break;
    // case s_arch_abyc:
    // case s_arch_abyn:
    //   denox_create_nodes_abyrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
    //   break;
    case s_arch_nbyc:
    case s_arch_nbyn:
      denox_create_nodes_nbyrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
      break;
    case s_arch_gxtc:
    case s_arch_gxtn:
      denox_create_nodes_gxtrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
      break;
    // case s_arch_ixtc:
    // case s_arch_ixtn:
    //   denox_create_nodes_ixtrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
    //   break;
    // case s_arch_axtc:
    // case s_arch_axtn:
    //   denox_create_nodes_axtrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
    //   break;
    case s_arch_nxtc:
    case s_arch_nxtn:
      denox_create_nodes_nxtrc(graph, module, (input_roi.ht+1)/2, (input_roi.wd+1)/2, input, "output", output, "input");
      break;
    default: return dt_connector_bypass(graph, module, 0, 1);
  }
  dt_connector_copy(graph, module, 0, input,  0);
  dt_connector_copy(graph, module, 1, output, 1);
}
