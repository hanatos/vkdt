#include "modules/api.h"

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
#include "wb.h"
  if(parid == 0 || parid == 3)
  { // film or paper changed, update the pre-optimised wb coeffs
    int oldstr = *(int*)oldval;
    int newstr = dt_module_param_int(module, parid)[0];
    int film   = dt_module_param_int(module, 0)[0];
    int paper  = dt_module_param_int(module, 3)[0];
    if(oldstr != newstr)
    {
      int pid_ev_paper = dt_module_get_param(module->so, dt_token("ev paper"));
      int pid_filter_c = dt_module_get_param(module->so, dt_token("filter c"));
      int pid_filter_m = dt_module_get_param(module->so, dt_token("filter m"));
      int pid_filter_y = dt_module_get_param(module->so, dt_token("filter y"));
      ((float*)dt_module_param_float(module, pid_ev_paper))[0] = wb[film][paper][0];
      ((float*)dt_module_param_float(module, pid_filter_c))[0] = wb[film][paper][1];
      ((float*)dt_module_param_float(module, pid_filter_m))[0] = wb[film][paper][2];
      ((float*)dt_module_param_float(module, pid_filter_y))[0] = wb[film][paper][3];
    }
  }
  // TODO if output res changed, return s_graph_run_all
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms is fine
}
