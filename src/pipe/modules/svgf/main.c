#include "modules/api.h"
#include "config.h"

// XXX go through this and do stuff in 32 or in 16 bits please!
//
// TODO: input: straight ssbo from renderer
// TODO: input: approximate albedo/base texture
// TODO: preblend: do albedo *demodulation* (keep path tracer out of the hacky business)
void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // preblend: merge samples from previous frame buffer before denoising
  const int id_preblend = dt_node_add(graph, module, "svgf", "preblend",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 6,
      "mv",     "read",  "rg",   "*",   dt_no_roi,
      "prevl",  "read",  "rgba", "*",   dt_no_roi,
      "light",  "read",  "rgba", "*",   dt_no_roi,
      "gbufp",  "read",  "*",    "f32", dt_no_roi,
      "gbufc",  "read",  "*",    "f32", dt_no_roi,
      "output", "write", "rgba", "f32", &module->connector[0].roi);

  // blend: final taa and albedo modulation
  const int id_blend = dt_node_add(graph, module, "svgf", "blend",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 5,
      "mv",     "read",  "rg",   "*",   dt_no_roi,
      "prevb",  "read",  "rgba", "*",   dt_no_roi,
      "light",  "read",  "rgba", "*",   dt_no_roi,
      "albedo", "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "rgba", "f32", &module->connector[0].roi);

  int id_eaw[SVGF_IT];
  for(int i=0;i<SVGF_IT;i++)
  {
    int pc[] = {1<<i};
    id_eaw[i] = dt_node_add(graph, module, "svgf", "eaw",
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1, sizeof(pc), pc, 4,
        "input",    "read",  "rgba", "*",   dt_no_roi,
        "gbuf",     "read",  "*",    "f32", dt_no_roi,
        "output",   "write", "rgba", "f16", &module->connector[0].roi,
        "gbuf_out", "write", "rg",   "f32", &module->connector[0].roi);
  }

  dt_connector_copy(graph, module, 0, id_preblend, 0);  // mv
  dt_connector_copy(graph, module, 1, id_preblend, 2);  // noisy light
  dt_connector_copy(graph, module, 4, id_preblend, 3);  // previous gbuffer
  dt_connector_copy(graph, module, 5, id_preblend, 4);  // current gbuffer

  CONN(dt_node_feedback(graph, id_preblend, 5, id_preblend, 1)); // previous noisy light
  CONN(dt_node_connect (graph, id_preblend, 5, id_eaw[0],   0)); // less noisy light
  // dt_connector_copy(graph, module, 1, id_eaw[0], 0);  // noisy light
  dt_connector_copy(graph, module, 5, id_eaw[0], 1);  // gbuffer for normal, depth, L moments
  for(int i=1;i<SVGF_IT;i++)
  {
    CONN(dt_node_connect (graph, id_eaw[i-1], 2, id_eaw[i], 0));
    CONN(dt_node_connect (graph, id_eaw[i-1], 3, id_eaw[i], 1));
  }
  CONN(dt_node_connect (graph, id_eaw[SVGF_IT-1], 2, id_blend, 2)); // denoised light
  CONN(dt_node_feedback(graph, id_blend, 4, id_blend, 1)); // beauty frame, old
#if SVGF_OFF==1
  dt_connector_copy(graph, module, 1, id_blend, 2); // XXX DEBUG light w/o denoising
#endif

  dt_connector_copy(graph, module, 0, id_blend, 0);  // mv
  dt_connector_copy(graph, module, 2, id_blend, 3);  // albedo
  dt_connector_copy(graph, module, 3, id_blend, 4);  // output beauty
}
