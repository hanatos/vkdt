#include "modules/api.h"
#include <math.h>
#include <stdlib.h>


dt_graph_run_t
input(
    dt_module_t             *mod,
    dt_module_input_event_t *p)
{ // mouse events on our dspy curve output
  float *l = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("dspyl")));
  float *c = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("dspyc")));

  static bool active = false;

  if(p->type == 1)
  { // mouse button
    if(p->action == 1)
    { // if button pressed and point selected, mark it as active
      active = true;
    }
    else if(p->action == 0 && p->mbutton == 0 && active)
    { // released after selection
      active = false;
    }
    else active = false; // no mouse down no active vertex
  }
  else if(p->type == 2)
  { // mouse position
    if(active)
    { // move active point around
      float l_max = 1.0f;
      float l_min = 0.0f;
      float c_max = 0.5f;
      float c_min = 0.0f;
      *l = CLAMP(p->y * (l_max - l_min) + l_min, 0.0f, 1.0f);
      *c = CLAMP(p->x * (c_max - c_min) + c_min, 0.0f, 1.0f);
      return s_graph_run_record_cmd_buf;
    }
  }
  return 0;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{ // request something square for dspy output
  module->connector[3].roi.full_wd = 1024;
  module->connector[3].roi.full_ht = 1024;
  module->connector[1].roi = module->connector[0].roi; // output
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int have_pick = dt_connected(module->connector+2);
  const int pc[] = { have_pick };

  dt_roi_t roi = module->connector[0].roi;
  const int id_okgmtclp = dt_node_add(graph, module, "okgmtclp", "main", 
    roi.wd, roi.ht, 1, 0, 0, 2,
    "input",   "read",  "rgba", "f16",  dt_no_roi,
    "output",  "write", "rgba", "f16",  &roi);
  dt_connector_copy(graph, module, 0, id_okgmtclp, 0);
  dt_connector_copy(graph, module, 1, id_okgmtclp, 1);

  const int id_dspy = dt_node_add(graph, module, "okgmtclp", "dspy",
    module->connector[3].roi.wd, module->connector[3].roi.ht, 1, sizeof(pc), pc, 3,
    "input",  "read",  "rgba", "f16", dt_no_roi,
    "dspy",   "write", "rgba", "f16", &module->connector[3].roi,
    "picked", "read",  "*",    have_pick ? dt_token_str(module->connector[2].format) : "f16", dt_no_roi);
  dt_connector_copy(graph, module, 0, id_dspy, 0);
  dt_connector_copy(graph, module, 3, id_dspy, 1);
  if(have_pick)  dt_connector_copy(graph, module, 2, id_dspy, 2);
  else           dt_connector_copy(graph, module, 0, id_dspy, 2); // dummy
}
