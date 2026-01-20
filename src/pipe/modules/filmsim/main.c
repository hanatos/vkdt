#include "modules/api.h"
#include "wb.h"

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int pid = dt_module_get_param(module->so, dt_token("enlarge"));
  const int par = dt_module_param_int(module, pid)[0];
  int s = par == 0 ? 1 : par == 1 ? 2 : 4;

  module->connector[1].roi.scale = module->connector[1].roi.full_wd/(float)module->connector[1].roi.wd;
  module->connector[0].roi = module->connector[1].roi;
  module->connector[0].roi.wd /= s;
  module->connector[0].roi.ht /= s;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int pid = dt_module_get_param(module->so, dt_token("enlarge"));
  const int par = dt_module_param_int(module, pid)[0];
  int s = par == 0 ? 1 : par == 1 ? 2 : 4;
  module->connector[1].roi.full_wd = MIN(32768, module->connector[0].roi.full_wd * s);
  module->connector[1].roi.full_ht = MIN(32768, module->connector[0].roi.full_ht * s);
}

void commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int pid_f = dt_module_get_param(module->so, dt_token("film"));
  int pid_p = dt_module_get_param(module->so, dt_token("paper"));
  int film  = dt_module_param_int(module, pid_f)[0];
  int paper = dt_module_param_int(module, pid_p)[0];
  int pid_ev_paper = dt_module_get_param(module->so, dt_token("ev paper"));
  int pid_filter_c = dt_module_get_param(module->so, dt_token("filter c"));
  int pid_filter_m = dt_module_get_param(module->so, dt_token("filter m"));
  int pid_filter_y = dt_module_get_param(module->so, dt_token("filter y"));
  int pid_halation = dt_module_get_param(module->so, dt_token("halation"));
  int pid_process  = dt_module_get_param(module->so, dt_token("process"));
  if(dt_module_param_int(module, pid_process)[0] == 2)
    ((int*)dt_module_param_int(module, pid_halation))[0] = 0;
  if(dt_module_param_float(module, pid_filter_c)[0] == -1.0)
  { // secret flag to signal "we want auto wb please"
    ((float*)dt_module_param_float(module, pid_ev_paper))[0] = wb[film][paper][0];
    ((float*)dt_module_param_float(module, pid_filter_c))[0] = wb[film][paper][1];
    ((float*)dt_module_param_float(module, pid_filter_m))[0] = wb[film][paper][2];
    ((float*)dt_module_param_float(module, pid_filter_y))[0] = wb[film][paper][3];
  }
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  int pid_f = dt_module_get_param(module->so, dt_token("film"));
  int pid_p = dt_module_get_param(module->so, dt_token("paper"));
  int pid_halation = dt_module_get_param(module->so, dt_token("halation"));
  int halation = dt_module_param_int(module, pid_halation)[0];
  if(parid == pid_halation && *(int*)oldval != halation)
    return s_graph_run_all;
  if(parid == pid_f || parid == pid_p)
  { // film or paper changed, update the pre-optimised wb coeffs
    int oldstr = *(int*)oldval;
    int newstr = dt_module_param_int(module, parid)[0];
    int film  = dt_module_param_int(module, pid_f)[0];
    int paper = dt_module_param_int(module, pid_p)[0];
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
  const int pid_en = dt_module_get_param(module->so, dt_token("enlarge"));
  const int pid_pc = dt_module_get_param(module->so, dt_token("process"));
  if(parid == pid_en || parid == pid_pc)
  { // output res or process changed (affects kernel config)
    int oldstr = *(int*)oldval;
    int newstr = dt_module_param_int(module, parid)[0];
    if(oldstr != newstr) return s_graph_run_all;
  }
  const int pid_rad = dt_module_get_param(module->so, dt_token("radius"));
  if(parid == pid_rad)
  { // halation radius
    float oldrad = *(float*)oldval;
    float newrad = dt_module_param_float(module, parid)[0];
    return dt_api_blur_check_params(oldrad, newrad);
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms is fine
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int iwd = module->connector[0].roi.wd;
  int iht = module->connector[0].roi.ht;
  int pid_process = dt_module_get_param(module->so, dt_token("process"));
  int process = dt_module_param_int(module, pid_process)[0];
  if(process == 2)
  { // print a scanned negative as input
    // TODO: if no resize and no couplers, use the monolithic kernel w/o global memory copy!
    float scale = module->connector[0].roi.full_wd / (float)iwd;
    int pc[] = { *(int*)&scale };
    const int id_main = dt_node_add(graph, module, "filmsim", "main", iwd, iht, 1,
        sizeof(pc), pc, 4,
        "input",   "read",  "*",    "*",    dt_no_roi,
        "out",     "write", "rgba", "f16", &module->connector[0].roi,
        "filmsim", "read",  "*",    "*",    dt_no_roi,
        "spectra", "read",  "*",    "*",    dt_no_roi);
    dt_connector_copy(graph, module, 0, id_main, 0);
    dt_connector_copy(graph, module, 1, id_main, 1);
    dt_connector_copy(graph, module, 2, id_main, 2);
    dt_connector_copy(graph, module, 3, id_main, 3);
    return;
  }

  int owd = module->connector[1].roi.wd;
  int oht = module->connector[1].roi.ht;
  const int id_part0 = dt_node_add(graph, module, "filmsim", "part0", iwd, iht, 1, 0, 0, 5,
      "input",   "read",  "*",    "*",    dt_no_roi,
      "coupler", "write", "rgba", "f16", &module->connector[0].roi,
      "filmsim", "read",  "*",    "*",    dt_no_roi,
      "spectra", "read",  "*",    "*",    dt_no_roi,
      "exp",     "write", "rgba", "f16", &module->connector[0].roi);
  float scale = module->connector[0].roi.full_wd / (float)owd;
  int pid_halation = dt_module_get_param(module->so, dt_token("halation"));
  int halation = dt_module_param_int(module, pid_halation)[0];
  int pc[] = { *(int*)&scale };
  int id_part1, id_part2;
  if(halation)
  {
    id_part1 = dt_node_add(graph, module, "filmsim", "part1h", owd, oht, 1, sizeof(pc), pc, 5,
        "exp",     "read",  "*",    "*",    dt_no_roi,
        "output",  "write", "rgba", "f16", &module->connector[1].roi,
        "filmsim", "read",  "*",    "*",    dt_no_roi,
        "spectra", "read",  "*",    "*",    dt_no_roi,
        "coupler", "read",  "*",    "*",    dt_no_roi);
    id_part2 = dt_node_add(graph, module, "filmsim", "part2h", owd, oht, 1, sizeof(pc), pc, 5,
        "exp",     "read",  "*",    "*",    dt_no_roi,
        "output",  "write", "rgba", "f16", &module->connector[1].roi,
        "filmsim", "read",  "*",    "*",    dt_no_roi,
        "spectra", "read",  "*",    "*",    dt_no_roi,
        "hal",     "read",  "*",    "*",    dt_no_roi);
    // default is 200um, i.e. um_per_px = 35mm film * 1000 / 6000px
    // and so 200/6 = 33px is the *sigma* of the gauss blur here
    // or 33px is this fraction of 6000px wide:
    // float blur = 0.0055*MAX(owd, oht);
    const int   pid = dt_module_get_param(module->so, dt_token("radius"));
    const float par = dt_module_param_float(module, pid)[0];
    float blur = par*MAX(owd, oht);
    const int id_blur = blur > 0 ? dt_api_blur_sep(graph, module, id_part1, 1, 0, 0, blur) : id_part1;
    CONN(dt_node_connect_named(graph, id_blur,  "output", id_part2, "hal"));
    CONN(dt_node_connect_named(graph, id_part1, "output", id_part2, "exp"));
    dt_connector_copy(graph, module, 2, id_part2, 2);
    dt_connector_copy(graph, module, 3, id_part2, 3);
  }
  else
  {
    const int id = dt_node_add(graph, module, "filmsim", "part1", owd, oht, 1, sizeof(pc), pc, 5,
        "exp",     "read",  "*",    "*",    dt_no_roi,
        "output",  "write", "rgba", "f16", &module->connector[1].roi,
        "filmsim", "read",  "*",    "*",    dt_no_roi,
        "spectra", "read",  "*",    "*",    dt_no_roi,
        "coupler", "read",  "*",    "*",    dt_no_roi);
    id_part1 = id_part2 = id;
  }
  float blur = 0.015*MAX(owd, oht);
  const int id_blur = blur > 0 ? dt_api_blur_sep(graph, module, id_part0, 1, 0, 0, blur) : id_part0;
  const int cn_blur = blur > 0 ? 1 : 1;
  CONN(dt_node_connect(graph, id_blur, cn_blur, id_part1, 4));
  dt_connector_copy(graph, module, 0, id_part0, 0);
  dt_connector_copy(graph, module, 2, id_part0, 2);
  dt_connector_copy(graph, module, 3, id_part0, 3);
  dt_connector_copy(graph, module, 1, id_part2, 1);
  dt_connector_copy(graph, module, 2, id_part1, 2);
  dt_connector_copy(graph, module, 3, id_part1, 3);
  CONN(dt_node_connect_named(graph, id_part0, "exp", id_part1, "exp"));
}
