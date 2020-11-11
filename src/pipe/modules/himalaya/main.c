#include "modules/api.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  #if 1
  module->connector[2].roi.full_wd = 2350;
  module->connector[2].roi.full_ht = 1000;
  #else
  // double res  
  module->connector[2].roi.full_wd = 4700;
  module->connector[2].roi.full_ht = 2000;
  #endif
}
