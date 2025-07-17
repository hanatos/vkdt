#include "modules/api.h"
#include "types.h"
#include "points_basematrix.h"
#include <inttypes.h>

dt_graph_run_t
input(
    dt_module_t             *mod,
    dt_module_input_event_t *p)
{
  // we're not interested in mouse over widget input
  if(!p->grabbed) return 0;
  if(p->type == 1)
  { // mouse button
    int *p_wave = (int*)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("wave")));
    p_wave[0] = mod->graph->frame;
  }
  else if(p->type == 2)
  { // mouse position

  }
  return 0;
}

void
commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  // the original code says these work well:
  // const int selected_points[] = {0,5,2,15,3,4,6,1,7,8,9,10,11,12,14,16,13,17,18,19,20,21};
  // the original code picks two points (bg and cursor) and each have a current and a target point for animation.
  const int    p_pt_bg = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("back")))[0];
  const int    p_pt_cr = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("cursor")))[0];
  const int    p_col   = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("colour")))[0];
  const int    p_wave  = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("wave")))[0];
  struct params_t *p = (struct params_t *)module->committed_param;
  struct params_t def = {
    .decayFactor = 0.5,
    .time = graph->frame,
    .actionAreaSizeSigma = 0.3,
    .actionX = wd / 2.0f, // TODO gamepad?
    .actionY = ht / 2.0f,
    .moveBiasActionX = 0.1,
    .moveBiasActionY = 0.1,
    .waveXarray = { 3 * wd / 4 },
    .waveYarray = { 3 * ht / 4 },
    .waveTriggerTimes = { -666 },
    .waveSavedSigmas = { 0.5, 0.5, 0.5, 0.5 },
    .mouseXchange = 0,
    .L2Action = 0,
    .spawnParticles = 0,
    .spawnFraction = 0.1,
    .randomSpawnNumber = 0,
    .randomSpawnXarray = { 0 },
    .randomSpawnYarray = { 0 },
    .pixelScaleFactor = 1,
    .depositFactor = 0.9,
    .colorModeType = p_col,
    .numberOfColorModes = 2,
  };
  if(graph->frame == 0)
  {
    *p = def;
    memcpy(p->params+0, ParametersMatrix[p_pt_bg], sizeof(struct PointSettings));
    memcpy(p->params+1, ParametersMatrix[p_pt_cr], sizeof(struct PointSettings));
  }
  else
  {
    if(p_wave)
    {
      p->waveTriggerTimes.x = graph->frame;
      p->L2Action = 1;
      ((int*)dt_module_param_int(module, dt_module_get_param(module->so, dt_token("wave"))))[0] = 0;
    }
    p->time = graph->frame;
    p->colorModeType = p_col;
    float *f = (float *)p->params+0;
    float *g = (float *)p->params+1;
    const float t = 0.95;
    for(int i=0;i<sizeof(struct PointSettings)/sizeof(float);i++)
    {
      f[i] = t*f[i] + (1.0f-t)*ParametersMatrix[p_pt_bg][i];
      g[i] = t*g[i] + (1.0f-t)*ParametersMatrix[p_pt_cr][i];
    }
  }
}

int init(dt_module_t *mod)
{
  mod->committed_param_size = sizeof(struct params_t);
  return 0;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  // const int part_cnt = 1000000; // TODO make parameter?
  // TODO init particles somehow (at random? as sphere pointing inward?)

  // dt_roi_t roi_part = { .wd = part_cnt, .ht = 3 };
  dt_roi_t roi_part = { .wd = wd*ht, .ht = 3 };
  int id_move = dt_node_add(graph, module, "physarum", "move",
      // part_cnt, 1, 1, 0, 0, 3,
      wd*ht, 1, 1, 0, 0, 3,
      "trails",   "read",  "*",    "*",     dt_no_roi,
      "part-cnt", "write", "ssbo", "ui32", &module->connector[0].roi, // particle count per pixel
      "part",     "write", "ssbo", "ui32", &roi_part);
  graph->node[id_move].connector[1].flags |= s_conn_clear;     // clear per pixel particle counts
  graph->node[id_move].connector[2].flags |= s_conn_protected; // particle positions stay with us

  int id_deposit = dt_node_add(graph, module, "physarum", "deposit", wd, ht, 1, 0, 0, 4,
      "part-cnt", "read",  "ssbo", "ui32", &module->connector[0].roi,
      "trailr",   "read",  "*",    "*",     dt_no_roi,
      "trailw",   "write", "rg",   "f16",  &module->connector[0].roi,
      "output",   "write", "rgba", "f16",  &module->connector[0].roi);

  int id_diffuse = dt_node_add(graph, module, "physarum", "diffuse", wd, ht, 1, 0, 0, 2,
      "input",  "read",  "*",  "*",    dt_no_roi,
      "output", "write", "rg", "f16", &module->connector[0].roi);

  CONN(dt_node_connect_named (graph, id_move,    "part-cnt", id_deposit, "part-cnt"));
  CONN(dt_node_feedback_named(graph, id_diffuse, "output",   id_deposit, "trailr"));
  CONN(dt_node_feedback_named(graph, id_diffuse, "output",   id_move,    "trails"));
  CONN(dt_node_feedback_named(graph, id_deposit, "trailw",   id_diffuse, "input"));
  dt_connector_copy(graph, module, 0, id_deposit, 3); // wire output image
}
