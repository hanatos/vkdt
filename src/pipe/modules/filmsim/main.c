#include "modules/api.h"

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int pid = dt_module_get_param(module->so, dt_token("enlarger"));
  const int par = dt_module_param_int(module, pid)[0];
  int s = par < 2 ? 1 : par == 2 ? 2 : 4;

  module->connector[1].roi.scale = module->connector[1].roi.full_wd/(float)module->connector[1].roi.wd;
  module->connector[0].roi = module->connector[1].roi;
  module->connector[0].roi.wd /= s;
  module->connector[0].roi.ht /= s;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int pid = dt_module_get_param(module->so, dt_token("enlarger"));
  const int par = dt_module_param_int(module, pid)[0];
  int s = par < 2 ? 1 : par == 2 ? 2 : 4;
  module->connector[1].roi.full_wd = MIN(32768, module->connector[0].roi.full_wd * s);
  module->connector[1].roi.full_ht = MIN(32768, module->connector[0].roi.full_ht * s);
}

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
  const int pid = dt_module_get_param(module->so, dt_token("enlarger"));
  if(parid == pid)
  { // output res changed?
    int oldstr = *(int*)oldval;
    int newstr = dt_module_param_int(module, parid)[0];
    if((oldstr <  2 && newstr >= 2) ||
       (oldstr >= 2 && newstr <  2))
      return s_graph_run_all;
  }
  // TODO look at align/main.c and copy the if-blur-radius changed logic
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms is fine
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO: if no resize and no couplers, use the monolithic kernel w/o global memory copy!
  int iwd = module->connector[0].roi.wd;
  int iht = module->connector[0].roi.ht;
  int owd = module->connector[1].roi.wd;
  int oht = module->connector[1].roi.ht;
  const int id_part0 = dt_node_add(graph, module, "filmsim", "part0", iwd, iht, 1, 0, 0, 5,
      "input",   "read",  "*",    "*",    dt_no_roi,
      "coupler", "write", "rgba", "f16", &module->connector[0].roi,
      "filmsim", "read",  "*",    "*",    dt_no_roi,
      "spectra", "read",  "*",    "*",    dt_no_roi,
      "exp",     "write", "rgba", "f16", &module->connector[0].roi);
  float scale = module->connector[0].roi.full_wd / (float)iwd;
  int pc[] = { *(int*)&scale };
  const int id_part1 = dt_node_add(graph, module, "filmsim", "part1", owd, oht, 1, sizeof(pc), pc, 5,
      "exp",     "read",  "*",    "*",    dt_no_roi,
      "output",  "write", "rgba", "f16", &module->connector[1].roi,
      "filmsim", "read",  "*",    "*",    dt_no_roi,
      "spectra", "read",  "*",    "*",    dt_no_roi,
      "coupler", "read",  "*",    "*",    dt_no_roi);
  float blur = 0.005*MAX(owd, oht);
  const int id_blur = blur > 0 ? dt_api_blur(graph, module, id_part0, 1, 0, 0, blur) : id_part0;
  const int cn_blur = blur > 0 ? 1 : 1;
  CONN(dt_node_connect(graph, id_blur, cn_blur, id_part1, 4));
  dt_connector_copy(graph, module, 0, id_part0, 0);
  dt_connector_copy(graph, module, 2, id_part0, 2);
  dt_connector_copy(graph, module, 3, id_part0, 3);
  dt_connector_copy(graph, module, 1, id_part1, 1);
  dt_connector_copy(graph, module, 2, id_part1, 2);
  dt_connector_copy(graph, module, 3, id_part1, 3);
  CONN(dt_node_connect_named(graph, id_part0, "exp", id_part1, "exp"));
}
