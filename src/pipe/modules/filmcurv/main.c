#include "modules/api.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{ // request something square for dspy output
  module->connector[2].roi.full_wd = 1024;
  module->connector[2].roi.full_ht = 1024;
  module->connector[1].roi = module->connector[0].roi; // output
}
