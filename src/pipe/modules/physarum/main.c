#include "modules/api.h"
#include "types.h"
#include "points_basematrix.h"
#include <inttypes.h>

void
commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // the original code says these work well:
  // const int selected_points[] = {0,5,2,15,3,4,6,1,7,8,9,10,11,12,14,16,13,17,18,19,20,21};
  // the original code picks two points (bg and cursor) and each have a current and a target point for animation.
  const int    p_pt_bg = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("back")))[0];
  const int    p_pt_cr = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("cursor")))[0];
  struct params_t *p = (struct params_t *)module->committed_param;
  *p = (struct params_t) {
    .decayFactor = 0.5,
    .time = graph->frame,
    .actionAreaSizeSigma = 0.3,
    .actionX = 0, // TODO gamepad?
    .actionY = 0,
    .moveBiasActionX = 0,
    .moveBiasActionY = 0,
    .waveXarray = { 0 }, // wd / 2?
    .waveYarray = { 0 }, // ht / 2?
    .waveTriggerTimes = { -12345 }, // ???
    .waveSavedSigmas = { 0.5, 0.5, 0.5, 0.5, 0.5},
    .mouseXchange = 0,
    .L2Action = 0,
    .spawnParticles = 0,
    .spawnFraction = 0.1,
    .randomSpawnNumber = 0,
    .randomSpawnXarray = { 0 },
    .randomSpawnYarray = { 0 },
    .pixelScaleFactor = 1,
    .depositFactor = 0.9,
    .colorModeType = 1,
    .numberOfColorModes = 2,
  };
  memcpy(p->params+0, ParametersMatrix[p_pt_bg], sizeof(struct PointSettings));
  memcpy(p->params+1, ParametersMatrix[p_pt_cr], sizeof(struct PointSettings));
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
  const int part_cnt = 1000000; // TODO make parameter?
  // TODO init particles somehow (at random? as sphere pointing inward?)

  dt_roi_t roi_part = { .wd = part_cnt, .ht = 3 };
  int id_move = dt_node_add(graph, module, "physarum", "move",
      part_cnt, 1, 1, 0, 0, 3,
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

  CONN(dt_node_connect_named (graph, id_move,    "part-cnt", id_deposit, "part_cnt"));
  CONN(dt_node_feedback_named(graph, id_diffuse, "output",   id_deposit, "trailr"));
  CONN(dt_node_feedback_named(graph, id_diffuse, "output",   id_move,    "trails"));
  CONN(dt_node_feedback_named(graph, id_deposit, "trailw",   id_diffuse, "input"));
  dt_connector_copy(graph, module, 0, id_deposit, 3); // wire output image
}
