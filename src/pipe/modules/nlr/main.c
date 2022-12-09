#include "modules/api.h"
#define SVGF_IT 4

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // preblend: merge samples from previous frame buffer before denoising
  assert(graph->num_nodes < graph->max_nodes);
  int id_preblend = graph->num_nodes++;
  graph->node[id_preblend] = (dt_node_t) {
    .name   = dt_token("nlr"),
    .kernel = dt_token("preblend"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 6,
    .connector = {{ // 0
      .name   = dt_token("mv"),
      .type   = dt_token("read"),
      .chan   = dt_token("rg"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 1
      .name   = dt_token("prevl"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 2
      .name   = dt_token("light"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 3
      .name   = dt_token("gbufp"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f32"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 4
      .name   = dt_token("gbufc"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f32"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 5
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };

  // blend: final taa and albedo modulation
  assert(graph->num_nodes < graph->max_nodes);
  int id_blend = graph->num_nodes++;
  graph->node[id_blend] = (dt_node_t) {
    .name   = dt_token("nlr"),
    .kernel = dt_token("blend"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 5,
    .connector = {{ // 0
      .name   = dt_token("mv"),
      .type   = dt_token("read"),
      .chan   = dt_token("rg"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 1
      .name   = dt_token("prevb"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 2
      .name   = dt_token("light"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 3
      .name   = dt_token("albedo"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 4
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };

  int id_den = graph->num_nodes++;
  graph->node[id_den] = (dt_node_t) {
    .name   = dt_token("nlr"),
    .kernel = dt_token("denoise"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 3,
    .connector = {{ // 0
      .name   = dt_token("light"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 1
      .name   = dt_token("gbuf"),
      .type   = dt_token("read"),
      .chan   = dt_token("*"),
      .format = dt_token("f32"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 2
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    }},
  };

  int id_down[SVGF_IT];
  int id_comb[SVGF_IT];
  int id_up  [SVGF_IT];

  dt_roi_t roi = module->connector[0].roi;
  for(int i=0;i<SVGF_IT;i++)
  {
    dt_roi_t roi_lo = roi;
    roi_lo.wd = (roi.wd+1)/2;
    roi_lo.ht = (roi.ht+1)/2;
    id_down[i] = graph->num_nodes++;
    graph->node[id_down[i]] = (dt_node_t) {
      .name   = dt_token("nlr"),
      .kernel = dt_token("down"),
      .module = module,
      .wd     = roi_lo.wd,
      .ht     = roi_lo.ht,
      .dp     = 1,
      .num_connectors = 4,
      .connector = {{ // 0
        .name   = dt_token("light"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("*"),
        .roi    = roi,
        .connected_mi = -1,
      },{ // 1
        .name   = dt_token("gbuf"),
        .type   = dt_token("read"),
        .chan   = dt_token("*"),
        .format = dt_token("f32"),
        .roi    = roi,
        .connected_mi = -1,
      },{ // 2
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi_lo,
      },{ // 3
        .name   = dt_token("gbuf_out"),
        .type   = dt_token("write"),
        .chan   = dt_token("rg"),
        .format = dt_token("f32"),
        .roi    = roi_lo,
      }},
    };
    id_up[i] = graph->num_nodes++;
    graph->node[id_up[i]] = (dt_node_t) {
      .name   = dt_token("nlr"),
      .kernel = dt_token("up"),
      .module = module,
      .wd     = roi_lo.wd,
      .ht     = roi_lo.ht,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{ // 0
        .name   = dt_token("lo"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("*"),
        .roi    = roi_lo,
        .connected_mi = -1,
      },{ // 1
        .name   = dt_token("gbuf"),
        .type   = dt_token("read"),
        .chan   = dt_token("*"),
        .format = dt_token("f32"),
        .roi    = roi_lo,
        .connected_mi = -1,
      },{ // 2
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi,
      }},
    };
    id_comb[i] = graph->num_nodes++;
    graph->node[id_comb[i]] = (dt_node_t) {
      .name   = dt_token("nlr"),
      .kernel = dt_token("combine"),
      .module = module,
      .wd     = roi.wd,
      .ht     = roi.ht,
      .dp     = 1,
      .num_connectors = 5,
      .connector = {{ // 0
        .name   = dt_token("lo"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("*"),
        .roi    = roi_lo,
        .connected_mi = -1,
      },{ // 1
        .name   = dt_token("hi"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("*"),
        .roi    = roi,
        .connected_mi = -1,
      },{ // 2
        .name   = dt_token("gbuf_lo"),
        .type   = dt_token("read"),
        .chan   = dt_token("*"),
        .format = dt_token("f32"),
        .roi    = roi_lo,
        .connected_mi = -1,
      },{ // 3
        .name   = dt_token("gbuf_hi"),
        .type   = dt_token("read"),
        .chan   = dt_token("*"),
        .format = dt_token("f32"),
        .roi    = roi,
        .connected_mi = -1,
      },{ // 4
        .name   = dt_token("output"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = roi,
      }},
    };
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
