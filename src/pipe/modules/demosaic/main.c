#include "modules/api.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  ri->wd = ri->full_wd;
  ri->ht = ri->full_ht;
  ri->scale = 1.0f;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  const int method = dt_module_param_int(module, 1)[0];
  const int block  = module->img_param.filters == 9u ? 3 : 2;
  const float scale = ro->full_wd > 0 ? (float)ri->full_wd/(float)ro->full_wd : 1.0f;
  const int halfsize = (method == 2) || (scale >= block);
  if(halfsize)
  {
    ro->full_wd = (ri->full_wd+1)/2;
    ro->full_ht = (ri->full_ht+1)/2;
  }
  else
  {
    ro->full_wd = ri->full_wd;
    ro->full_ht = ri->full_ht;
  }
  module->img_param.filters = 0u; // after we're done there won't be any more mosaic
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 1)
  { // method
    int oldv = *(int*)oldval;
    int newv = dt_module_param_int(module, 1)[0];
    if(oldv != newv) return s_graph_run_all;// we need to update the graph topology
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return; // must have disconnected input somewhere

  if(!img_param->filters)
  {
    if(module->connector[0].roi.wd == module->connector[1].roi.wd &&
       module->connector[0].roi.ht == module->connector[1].roi.ht)
      return dt_connector_bypass(graph, module, 0, 1);
    const int nodeid = dt_node_add(graph, module, "resize", "main", module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
        "input",  "read",  "rgba", "*",    dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[1].roi);
    dt_connector_copy(graph, module, 0, nodeid, 0);
    dt_connector_copy(graph, module, 1, nodeid, 1);
    return;
  }

  const int block = img_param->filters == 9u ? 3 : 2;
  module->img_param.filters = 0u; // after we're done there won't be any more mosaic
  const int wd = module->connector[0].roi.wd, ht = module->connector[0].roi.ht;
  dt_roi_t roi_full = module->connector[0].roi;
  dt_roi_t roi_half = module->connector[0].roi;
  roi_half.full_wd /= block;
  roi_half.full_ht /= block;
  roi_half.wd /= block;
  roi_half.ht /= block;
  int *wbi = (int *)img_param->whitebalance;
  const int pc[] = { wbi[0], wbi[1], wbi[2], wbi[3], img_param->filters };
  const int method = dt_module_param_int(module, 1)[0];
  const int halfsize = 
    ((float)module->connector[0].roi.full_wd/(float)module->connector[1].roi.full_wd >= block)
    // (module->connector[1].roi.scale >= block)
    || (method == 2);
  if(halfsize)
  { // half size
    const int id_half = dt_node_add(graph, module, "demosaic", "halfsize",
        roi_half.wd, roi_half.ht, 1, sizeof(pc), pc, 2,
        "input",  "read",  "rggb", "*",   dt_no_roi,
        "output", "write", "rgba", "f16", &roi_half);
    dt_connector_copy(graph, module, 0, id_half, 0);
    if(block != module->connector[1].roi.scale)
    { // resample to get to the rest of the resolution, only if block != scale!
      const int id_resample = dt_node_add(graph, module, "shared", "resample",
          module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
          "input",  "read",  "rgba", "f16", dt_no_roi,
          "output", "write", "rgba", "f16", &module->connector[1].roi);
      CONN(dt_node_connect(graph, id_half, 1, id_resample, 0));
      dt_connector_copy(graph, module, 1, id_resample, 1);
    }
    else dt_connector_copy(graph, module, 1, id_half, 1);
    return;
  }

  // else: full size

  if(method == 1)
  { // bayer with RCD
    dt_roi_t hr = module->connector[0].roi;
    hr.wd /= 2;
    const int id_conv = dt_node_add(graph, module, "demosaic", "rcd_conv",
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 4,
        "cfa", "read",  "*", "*",   dt_no_roi,
        "vh",  "write", "r", "f16", &module->connector[0].roi, // could go u8 here but can't show a perf improvement
        "pq",  "write", "r", "f16",  &hr,
        "lp",  "write", "r", "f16", &hr);
    const uint32_t tile_size_x = DT_RCD_TILE_SIZE_X - 2*DT_RCD_BORDER;
    const uint32_t tile_size_y = DT_RCD_TILE_SIZE_Y - 2*DT_RCD_BORDER;
    // how many tiles will we process?
    const uint32_t num_tiles_x = (wd + tile_size_x - 1)/tile_size_x;
    const uint32_t num_tiles_y = (ht + tile_size_y - 1)/tile_size_y;
    const uint32_t invx = num_tiles_x * DT_LOCAL_SIZE_X;
    const uint32_t invy = num_tiles_y * DT_LOCAL_SIZE_Y;
    const int id_fill = dt_node_add(graph, module, "demosaic", "rcd_fill",
        invx, invy, 1, sizeof(pc), pc, 5,
        "cfa", "read", "*", "*", dt_no_roi,
        "vh",  "read", "*", "*", dt_no_roi,
        "pq",  "read", "*", "*", dt_no_roi,
        "lp",  "read", "*", "*", dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[0].roi);
    CONN(dt_node_connect_named(graph, id_conv, "vh", id_fill, "vh"));
    CONN(dt_node_connect_named(graph, id_conv, "pq", id_fill, "pq"));
    CONN(dt_node_connect_named(graph, id_conv, "lp", id_fill, "lp"));
    dt_connector_copy(graph, module, 0, id_conv, 0);
    dt_connector_copy(graph, module, 0, id_fill, 0);
    if(module->connector[1].roi.scale != 1.0)
    { // add resample node to graph, copy its output instead:
      const int id_resample = dt_node_add(graph, module, "shared", "resample",
          module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
          "input",  "read",  "rgba", "f16", dt_no_roi,
          "output", "write", "rgba", "f16", &module->connector[1].roi);
      CONN(dt_node_connect(graph, id_fill, 4, id_resample, 0));
      dt_connector_copy(graph, module, 1, id_resample, 1);
    }
    else dt_connector_copy(graph, module, 1, id_fill, 4);
    return;
  }

  // bayer or xtrans with gaussian splats
  const int id_down = dt_node_add(graph, module, "demosaic", "down",
      wd/block, ht/block, 1, sizeof(pc), pc, 2,
      "input", "read", "rggb", "*",  dt_no_roi,
      "output", "write", "y", "f16", &roi_half);
  const int id_gauss = dt_node_add(graph, module, "demosaic", "gauss",
      wd/block, ht/block, 1, sizeof(pc), pc, 3,
      "input",  "read",  "y",    "f16", dt_no_roi,
      "orig",   "read",  "rggb", "*",   dt_no_roi,
      "output", "write", "rgba", "f16", &roi_half);
  CONN(dt_node_connect(graph, id_down, 1, id_gauss, 0));
  dt_connector_copy(graph, module, 0, id_gauss, 1);

  const int id_splat = dt_node_add(graph, module, "demosaic", "splat",
      wd, ht, 1, sizeof(pc), pc, 3,
      "input",  "read",  "rggb", "*",   dt_no_roi,
      "gauss",  "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "g",    "f16", &roi_full);
  dt_connector_copy(graph, module, 0, id_splat, 0);
  CONN(dt_node_connect(graph, id_gauss, 2, id_splat, 1));
  dt_connector_copy(graph, module, 0, id_down,  0);
  // dt_connector_copy(graph, module, 1, id_gauss, 2); // XXX DEBUG see output of gaussian params
  // return;

  // fix colour casts
  const int id_fix = dt_node_add(graph, module, "demosaic", "fix",
      wd, ht, 1, sizeof(pc), pc, 4,
      "input",  "read",  "rggb", "*",   dt_no_roi,
      "green",  "read",  "g",    "*",   dt_no_roi,
      "cov",    "read",  "rgba", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &roi_full);
  dt_connector_copy(graph, module, 0, id_fix, 0);
  CONN(dt_node_connect(graph, id_splat, 2, id_fix, 1));
  CONN(dt_node_connect(graph, id_gauss, 2, id_fix, 2));

  if(module->connector[1].roi.scale != 1.0)
  { // add resample node to graph, copy its output instead:
    const int id_resample = dt_node_add(graph, module, "shared", "resample",
        module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
        "input",  "read",  "rgba", "f16", dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[1].roi);
    CONN(dt_node_connect(graph, id_fix, 3, id_resample, 0));
    dt_connector_copy(graph, module, 1, id_resample, 1);
  }
  else dt_connector_copy(graph, module, 1, id_fix, 3);
}
