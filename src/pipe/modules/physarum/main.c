#include "modules/api.h"
#include "types.h"
#include "points_basematrix.h"
#include <inttypes.h>

void
commit_params(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // TODO: copy our shitton of cryptic parameters to uniforms
        float *p_wb  = (float*)dt_module_param_float(module, dt_module_get_param(module->so, dt_token("white")));
  const float  p_tmp = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("temp")))[0];
  const int    p_cnt = dt_module_param_int  (module, dt_module_get_param(module->so, dt_token("cnt")))[0];
  struct params_t *p = (struct params_t *)module->committed_param;
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

  int id_move = dt_node_add(graph, module, "physarum", "move",
      // XXX run on particles
      // wd, ht, 1, 0, 0, 2,
      "trails", "read", "*", "*", dt_no_roi,
      "part-cnt", "write", "r", "ui32", &module->connector[0].roi, // particle count per pixel
      "part", "write", "r", "ui32", ROI// uhm really f16 2d position, 2d heading, 2d velocity
      );
  // TODO always clear part-cnt
  |= s_conn_clear;

  int id_deposit = dt_node_add(graph, module, "physarum", "deposit", wd, ht, 1, 0, 0, 2,
      "part-cnt", "read", "r", "ui32", &module->connector[0].roi, // particle count per pixel
      "trail read"
      "trail write"
      "output"

  int id_diffusion = dt_node_add(graph, module, "physarum", "diffuse", wd, ht, 1, 0, 0, 2,
      "input",  "read",  "*",  "*",    dt_no_roi,
      "output", "write", "rg", "f16", &module->connector[0].roi);


          for (int i = 0; i < GlobalSettings::MAX_NUMBER_OF_WAVES; i++)
    {
        waveXarray[i] = GlobalSettings::SIMULATION_WIDTH / 2;
        waveYarray[i] = GlobalSettings::SIMULATION_HEIGHT / 2;
        waveTriggerTimes[i] = -12345;
        waveSavedSigmas[i] = 0.5;
    }

    for (int i = 0; i < GlobalSettings::MAX_NUMBER_OF_RANDOM_SPAWN; i++)
    {
        randomSpawnXarray[i] = GlobalSettings::SIMULATION_WIDTH / 2;
        randomSpawnYarray[i] = GlobalSettings::SIMULATION_HEIGHT / 2;
    }
}
