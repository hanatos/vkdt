#include "modules/api.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  module->connector[0].roi.full_wd = 2350;
  module->connector[0].roi.full_ht = 1000;
}
