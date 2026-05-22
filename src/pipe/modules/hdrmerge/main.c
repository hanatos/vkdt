#include "modules/api.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  module->connector[7].roi.full_wd = 1000;
  module->connector[7].roi.full_ht =  600;
  module->connector[6].roi = module->connector[0].roi; // output
}

void commit_params(dt_graph_t *graph, dt_module_t *module)
{
  int have_input2 = dt_connected(module->connector+1);
  int have_input3 = dt_connected(module->connector+2);
  int have_input4 = dt_connected(module->connector+3);
  int have_input5 = dt_connected(module->connector+4);
  int have_input6 = dt_connected(module->connector+5);
  
  const dt_image_params_t *img1_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  const dt_image_params_t *img2_param = have_input2 ? dt_module_get_input_img_param(graph, module, dt_token("input2")) : 0;
  const dt_image_params_t *img3_param = have_input3 ? dt_module_get_input_img_param(graph, module, dt_token("input3")) : 0;
  const dt_image_params_t *img4_param = have_input4 ? dt_module_get_input_img_param(graph, module, dt_token("input4")) : 0;
  const dt_image_params_t *img5_param = have_input5 ? dt_module_get_input_img_param(graph, module, dt_token("input5")) : 0;
  const dt_image_params_t *img6_param = have_input6 ? dt_module_get_input_img_param(graph, module, dt_token("input6")) : 0;

  int num_inputs = 0;
  for(int i=0;i<6;i++)
    if(module->connector[i].connected.i >= 0)
      num_inputs++;

  const float  p_pltscale = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("pltscale")))[0];
  const int    p_output   = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("output")))[0];
  const float  p_ev_push  = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("ev_push")))[0];

  float    *f =    (float *)module->committed_param;
  uint32_t *i = (uint32_t *)module->committed_param;
  
  f[0] = p_pltscale;
  i[1] = p_output;
  f[2] = p_ev_push;
  i[3] = num_inputs;
  f[4] = img1_param->exposure;
  f[5] = have_input2 ? img2_param->exposure : 0.0f;
  f[6] = have_input3 ? img3_param->exposure : 0.0f;
  f[7] = have_input4 ? img4_param->exposure : 0.0f;
  f[8] = have_input5 ? img5_param->exposure : 0.0f;
  f[9] = have_input6 ? img6_param->exposure : 0.0f;

}

int init(dt_module_t *mod)
{
  mod->committed_param_size = 10*sizeof(float);
  return 0;
}

void create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_roi_t roi     = module->connector[0].roi;
  const dt_roi_t roi_out = module->connector[6].roi;

  int have_input2 = dt_connected(module->connector+1);
  int have_input3 = dt_connected(module->connector+2);
  int have_input4 = dt_connected(module->connector+3);
  int have_input5 = dt_connected(module->connector+4);
  int have_input6 = dt_connected(module->connector+5);

  if (!have_input2) {
    snprintf(module->graph->gui_msg_buf, sizeof(module->graph->gui_msg_buf),
    "hdrmerge: only one input connected, bypassing");
    module->graph->gui_msg = module->graph->gui_msg_buf;
    return dt_connector_bypass(graph, module, 0, 6); // need to have at least two inputs
  }
  
  const dt_image_params_t *img1_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  const dt_image_params_t *img2_param = have_input2 ? dt_module_get_input_img_param(graph, module, dt_token("input2")) : 0;
  const dt_image_params_t *img3_param = have_input3 ? dt_module_get_input_img_param(graph, module, dt_token("input3")) : 0;
  const dt_image_params_t *img4_param = have_input4 ? dt_module_get_input_img_param(graph, module, dt_token("input4")) : 0;
  const dt_image_params_t *img5_param = have_input5 ? dt_module_get_input_img_param(graph, module, dt_token("input5")) : 0;
  const dt_image_params_t *img6_param = have_input6 ? dt_module_get_input_img_param(graph, module, dt_token("input6")) : 0;

  // check if param filters are the same, if not, we can't merge and need to bypass:
  if(img1_param->filters != img2_param->filters)
    return dt_connector_bypass(graph, module, 0, 6);
  if(have_input3 && img1_param->filters != img3_param->filters)
    return dt_connector_bypass(graph, module, 0, 6);
  if(have_input4 && img1_param->filters != img4_param->filters)
    return dt_connector_bypass(graph, module, 0, 6);
  if(have_input5 && img1_param->filters != img5_param->filters)
    return dt_connector_bypass(graph, module, 0, 6);
  if(have_input6 && img1_param->filters != img6_param->filters)
    return dt_connector_bypass(graph, module, 0, 6);

  // ensure images are same size
  for (int i=1;i<6;i++) {
    if(dt_connected(module->connector+i) && (roi.wd != module->connector[i].roi.wd ||
                                            roi.ht != module->connector[i].roi.ht)) {
      printf("hdrmerge: input %d has different dimensions, bypassing\n", i+1);
      snprintf(module->graph->gui_msg_buf, sizeof(module->graph->gui_msg_buf),
          "hdrmerge: input %d has different dimensions, bypassing", i+1);
      module->graph->gui_msg = module->graph->gui_msg_buf;
      return dt_connector_bypass(graph, module, 0, 6);
    }
  }

  const int id_main = dt_node_add(graph, module, "hdrmerge", "linear",
      roi.wd, roi.ht, 1, 0, 0, 8,
      "input",  "read",  "rgba", "f16",  dt_no_roi,
      "input2", "read",  "rgba", "f16",  dt_no_roi,
      "input3", "read",  "rgba", "f16",  dt_no_roi,
      "input4", "read",  "rgba", "f16",  dt_no_roi,
      "input5", "read",  "rgba", "f16",  dt_no_roi,
      "input6", "read",  "rgba", "f16",  dt_no_roi,
      "output", "write", "rgba", "f16",  &module->connector[6].roi,
      "output", "write", "r",    "ui32", &roi_out);
    graph->node[id_main].connector[7].flags = s_conn_clear;

    const int id_map = dt_node_add(graph, module, "hdrmerge", "map", 
    roi_out.wd, roi_out.ht, 1, 0, 0, 2,
    "input",  "read",  "r",    "ui32", dt_no_roi,
    "output", "write", "rgba", "f16",  &roi_out);

  dt_connector_copy(graph, module,     0, id_main, 0); // input
  dt_connector_copy(graph, module,     1, id_main, 1); // input2
  dt_connector_copy(graph, module,     6, id_main, 6); // output
  dt_node_connect  (graph, id_main,    7, id_map,  0); // dspy
  dt_connector_copy(graph, module,     7, id_map,  1); // output

  if(have_input3 && have_input4 && have_input5 && have_input6) {
    dt_connector_copy(graph, module,     2, id_main, 2);
    dt_connector_copy(graph, module,     3, id_main, 3);
    dt_connector_copy(graph, module,     4, id_main, 4);
    dt_connector_copy(graph, module,     5, id_main, 5);
  } else if (have_input3 && have_input4 && have_input5) {
    dt_connector_copy(graph, module,     2, id_main, 2);
    dt_connector_copy(graph, module,     3, id_main, 3);
    dt_connector_copy(graph, module,     4, id_main, 4);
    dt_connector_copy(graph, module,     0, id_main, 5); // dummy
  } else if (have_input3 && have_input4) {
    dt_connector_copy(graph, module,     2, id_main, 2);
    dt_connector_copy(graph, module,     3, id_main, 3);
    dt_connector_copy(graph, module,     0, id_main, 4); // dummy
    dt_connector_copy(graph, module,     0, id_main, 5); // dummy
  } else if (have_input3) {
    dt_connector_copy(graph, module,     2, id_main, 2);
    dt_connector_copy(graph, module,     0, id_main, 3); // dummy
    dt_connector_copy(graph, module,     0, id_main, 4); // dummy
    dt_connector_copy(graph, module,     0, id_main, 5); // dummy
  } else {
    dt_connector_copy(graph, module,     0, id_main, 2); // dummy
    dt_connector_copy(graph, module,     0, id_main, 3); // dummy
    dt_connector_copy(graph, module,     0, id_main, 4); // dummy
    dt_connector_copy(graph, module,     0, id_main, 5); // dummy
  }
}
