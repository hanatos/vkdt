#include "modules/api.h"

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  // only parameter is blur radius
  float oldrad = *(float*)oldval;
  float newrad = dt_module_param_float(module, parid)[0];
  return dt_api_blur_check_params(oldrad, newrad);
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *r1 = &module->connector[1].roi;
  dt_roi_t *r2 = &module->connector[2].roi;

  r1->full_wd = ri->full_wd;
  r1->full_ht = ri->full_ht;
  r2->full_wd = ri->full_wd;
  r2->full_ht = ri->full_ht;
}

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;

  ri->wd = ri->full_wd;
  ri->ht = ri->full_ht;
  ri->scale = 1.0f;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const float blur = ((float*)module->param)[0];

  // if(blur <= 0)
  // {
  //   dt_connector_bypass(graph, module, 0, 1);
  //   return;
  // }
  int id_blur_in = -1, id_blur_out = -1;
  dt_api_blur_sep(graph, module, -1, 0, &id_blur_in, &id_blur_out, blur);
  int id_blur_sub_in = -1, id_blur_sub_out = -1;
  dt_api_blur(graph, module, -1, 0, &id_blur_sub_in, &id_blur_sub_out, blur);
  // dt_api_blur(graph, module, -1, 0, &id_blur_sub_in, &id_blur_sub_out, blur);
  // dt_api_blur_sub(graph, module, -1, 0, &id_blur_in, &id_blur_out, blur, 1);
  // dt_api_blur_small(graph, module, -1, 0, &id_blur_in, &id_blur_out, 0.85);//blur); // i may believe this
  dt_connector_copy(graph, module, 0, id_blur_in,  0);
  dt_connector_copy(graph, module, 0, id_blur_sub_in,  0);
  dt_connector_copy(graph, module, 1, id_blur_out, 1);
  dt_connector_copy(graph, module, 2, id_blur_sub_out, 1);
}
