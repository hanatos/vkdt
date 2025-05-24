#include "modules/api.h"

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int pid_iter = dt_module_get_param(module->so, dt_token("iter"));
  if(parid == pid_iter)
  { // number of iterations changes pipeline
    int oldv = *(int*)oldval;
    int newv = dt_module_param_int(module, parid)[0];
    if(oldv != newv) return s_graph_run_all;
  }
  return s_graph_run_record_cmd_buf;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *mod)
{ // grab output dimensions from albedo image
  mod->connector[3].roi.scale = 1;
  mod->connector[3].roi.full_wd = mod->connector[2].roi.full_wd;
  mod->connector[3].roi.full_ht = mod->connector[2].roi.full_ht;
}

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *mod)
{ // need irradiance with 4 stride
  mod->connector[0].roi.wd = mod->connector[3].roi.wd; // mv
  mod->connector[0].roi.ht = mod->connector[3].roi.ht;
  mod->connector[1].roi.wd = mod->connector[3].roi.wd; // irr
  mod->connector[1].roi.ht = mod->connector[3].roi.ht * 4;
  mod->connector[2].roi.wd = mod->connector[3].roi.wd; // albedo
  mod->connector[2].roi.ht = mod->connector[3].roi.ht;
  mod->connector[4].roi.wd = mod->connector[3].roi.wd; // dn prev
  mod->connector[4].roi.ht = mod->connector[3].roi.ht;
  mod->connector[5].roi.wd = mod->connector[3].roi.wd; // dn curr
  mod->connector[5].roi.ht = mod->connector[3].roi.ht;
}

// XXX go through this and do stuff in 32 or in 16 bits please!
void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // preblend: merge samples from previous frame buffer before denoising
  const int id_preblend = dt_node_add(graph, module, "svgf", "preblend",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 6,
      "mv",     "read",  "rg",   "*",   dt_no_roi,
      "irrp",   "read",  "rgba", "f32", dt_no_roi,
      "irrc",   "read",  "ssbo", "f32", dt_no_roi,
      "gbufp",  "read",  "*",    "f32", dt_no_roi,
      "gbufc",  "read",  "*",    "f32", dt_no_roi,
      "output", "write", "rgba", "f32", &module->connector[0].roi);
  graph->node[id_preblend].connector[5].flags |= s_conn_clear_once;

  // blend: final taa and albedo modulation
  const int id_blend = dt_node_add(graph, module, "svgf", "blend",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 5,
      "mv",     "read",  "rg",   "*",   dt_no_roi,
      "prevb",  "read",  "rgba", "*",   dt_no_roi,
      "light",  "read",  "rgba", "*",   dt_no_roi,
      "albedo", "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "rgba", "f32", &module->connector[0].roi);
  graph->node[id_blend].connector[4].flags |= s_conn_clear_once;

  const int pid_iter = dt_module_get_param(module->so, dt_token("iter"));
  const int num_it = CLAMP(dt_module_param_int(module, pid_iter)[0], 0, 5);
  int id_eaw[5]; // we only have 5 max iterations in the ui
  for(int i=0;i<num_it;i++)
  {
    int pc[] = {1<<i};
    id_eaw[i] = dt_node_add(graph, module, "svgf", "eaw",
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1, sizeof(pc), pc, 4,
        "input",    "read",  "rgba", "*",   dt_no_roi,
        "gbuf",     "read",  "*",    "f32", dt_no_roi,
        "output",   "write", "rgba", "f16", &module->connector[0].roi,
        "gbuf_out", "write", "rg",   "f32", &module->connector[0].roi);
  }

  dt_connector_copy(graph, module, 0, id_preblend, 0);  // mv
  dt_connector_copy(graph, module, 1, id_preblend, 2);  // noisy light
  dt_connector_copy(graph, module, 4, id_preblend, 3);  // previous gbuffer
  dt_connector_copy(graph, module, 5, id_preblend, 4);  // current gbuffer

  CONN(dt_node_feedback(graph, id_preblend, 5, id_preblend, 1)); // previous noisy light
  if(num_it > 0)
  {
    CONN(dt_node_connect (graph, id_preblend, 5, id_eaw[0],   0)); // less noisy light
    // dt_connector_copy(graph, module, 1, id_eaw[0], 0);  // noisy light
    dt_connector_copy(graph, module, 5, id_eaw[0], 1);  // gbuffer for normal, depth, L moments
    for(int i=1;i<num_it;i++)
    {
      CONN(dt_node_connect (graph, id_eaw[i-1], 2, id_eaw[i], 0));
      CONN(dt_node_connect (graph, id_eaw[i-1], 3, id_eaw[i], 1));
    }
    CONN(dt_node_connect (graph, id_eaw[num_it-1], 2, id_blend, 2)); // denoised light
  }
  else
  {
    CONN(dt_node_connect(graph, id_preblend, 5, id_blend, 2));
  }
  CONN(dt_node_feedback(graph, id_blend, 4, id_blend, 1)); // beauty frame, old

  dt_connector_copy(graph, module, 0, id_blend, 0);  // mv
  dt_connector_copy(graph, module, 2, id_blend, 3);  // albedo
  dt_connector_copy(graph, module, 3, id_blend, 4);  // output beauty
}
