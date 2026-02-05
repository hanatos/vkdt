#include "modules/api.h"
#define SVGF_IT 4

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // preblend: merge samples from previous frame buffer before denoising
  int id_preblend = dt_node_add(graph, module, "nlr", "preblend",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 6,
      "mv",     "read",  "rg",   "*",   dt_no_roi,
      "prevl",  "read",  "rgba", "*",   dt_no_roi,
      "light",  "read",  "rgba", "*",   dt_no_roi,
      "gbufp",  "read",  "rgba", "f32", dt_no_roi,
      "gbufc",  "read",  "rgba", "f32", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);

  // blend: final taa and albedo modulation
  int id_blend = dt_node_add(graph, module, "nlr", "blend",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 5,
      "mv",     "read",  "rg",   "*",   dt_no_roi,
      "prevb",  "read",  "rgba", "*",   dt_no_roi,
      "light",  "read",  "rgba", "*",   dt_no_roi,
      "albedo", "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);

  int id_den = dt_node_add(graph, module, "nlr", "denoise",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 3,
      "light",  "read",  "rgba",  "*",  dt_no_roi,
      "gbuf",   "read",  "*",    "f32", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);

  int id_down[SVGF_IT];
  int id_comb[SVGF_IT];
  int id_up  [SVGF_IT];

  dt_roi_t roi = module->connector[0].roi;
  for(int i=0;i<SVGF_IT;i++)
  {
    dt_roi_t roi_lo = roi;
    roi_lo.wd = (roi.wd+1)/2;
    roi_lo.ht = (roi.ht+1)/2;
    id_down[i] = dt_node_add(graph, module, "nlr", "down", 
        roi_lo.wd, roi_lo.ht, 1, 0, 0, 4,
        "light",    "read",  "rgba", "*",   dt_no_roi,
        "gbuf",     "read",  "*",    "f32", dt_no_roi,
        "output",   "write", "rgba", "f16", &roi_lo,
        "gbuf_out", "write", "rg",   "f32", &roi_lo);
    id_up[i] = dt_node_add(graph, module, "nlr", "up",
        roi_lo.wd, roi_lo.ht, 1, 0, 0, 3,
        "lo",     "read",  "rgba", "*",   dt_no_roi,
        "gbuf",   "read",  "*",    "f32", dt_no_roi,
        "output", "write", "rgba", "f16", &roi);
    id_comb[i] = dt_node_add(graph, module, "nlr", "combine",
        roi.wd, roi.ht, 1, 0, 0, 5,
        "lo",      "read",  "rgba", "*",   dt_no_roi,
        "hi",      "read",  "rgba", "*",   dt_no_roi,
        "gbuf_lo", "read",  "*",    "f32", dt_no_roi,
        "gbuf_hi", "read",  "*",    "f32", dt_no_roi,
        "output",  "write", "rgba", "f16", &roi);
    roi = roi_lo;
  }

  dt_connector_copy(graph, module, 0, id_preblend, 0);  // mv
  dt_connector_copy_feedback(graph, module, 1, id_preblend, 1);  // previous noisy light
  dt_connector_copy(graph, module, 1, id_preblend, 2);  // noisy light
  dt_connector_copy(graph, module, 4, id_preblend, 3);  // previous gbuffer
  dt_connector_copy(graph, module, 5, id_preblend, 4);  // current gbuffer

  CONN(dt_node_connect(graph, id_preblend, 5, id_den, 0)); // less noisy light
  dt_connector_copy(graph, module, 5, id_den, 1);  // current gbuffer

  dt_node_connect(graph, id_den, 2, id_down[0], 0);
  dt_connector_copy(graph, module, 5, id_down[0], 1);
  // dt_node_connect(graph, id_down[SVGF_IT-1], 2, id_up[SVGF_IT-1], 0); // lowest down/up has no combine
  // dt_node_connect(graph, id_down[SVGF_IT-1], 3, id_up[SVGF_IT-1], 1);
  for(int i=0;i<SVGF_IT;i++)
  {
    if(i)
    {
      dt_node_connect(graph, id_down[i-1], 2, id_down[i], 0); // downsize
      dt_node_connect(graph, id_down[i-1], 2, id_comb[i], 1);
      dt_node_connect(graph, id_down[i-1], 3, id_down[i], 1);
      dt_node_connect(graph, id_down[i-1], 3, id_comb[i], 3);
      dt_node_connect(graph, id_comb[i], 4, id_up[i-1], 0);
    }
    else // i==0
    {
      dt_node_connect  (graph, id_den, 2, id_down[i], 0); // downsize input
      dt_node_connect  (graph, id_den, 2, id_comb[i], 1);
      dt_connector_copy(graph, module, 5, id_down[i], 1); // current gbuffer
      dt_connector_copy(graph, module, 5, id_comb[i], 3);
      dt_node_connect(graph, id_comb[i], 4, id_blend, 2);
    }
    dt_node_connect(graph, id_down[i], 2, id_up[i],   0); 
    dt_node_connect(graph, id_down[i], 3, id_up[i],   1);

    dt_node_connect(graph, id_up[i],   2, id_comb[i], 0); // col lo
    dt_node_connect(graph, id_down[i], 3, id_comb[i], 2); // gbuf lo
  }
  // CONN(dt_node_connect (graph, id_comb[0], 4, id_blend, 2)); // denoised light
  CONN(dt_node_feedback(graph, id_blend, 4, id_blend, 1)); // beauty frame, old
#if SVGF_OFF==1
  dt_connector_copy(graph, module, 1, id_blend, 2); // XXX DEBUG light w/o denoising
#endif

  dt_connector_copy(graph, module, 0, id_blend, 0);  // mv
  dt_connector_copy(graph, module, 2, id_blend, 3);  // albedo
  dt_connector_copy(graph, module, 3, id_blend, 4);  // output beauty
}
