#include "modules/api.h"
#include "config.h"

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // preblend: merge samples from previous frame buffer before denoising
  assert(graph->num_nodes < graph->max_nodes);
  int id_preblend = graph->num_nodes++;
  graph->node[id_preblend] = (dt_node_t) {
    .name   = dt_token("svgf"),
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
    .name   = dt_token("svgf"),
    .kernel = dt_token("blend"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 9,
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
      .name   = dt_token("prevb"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 3
      .name   = dt_token("light"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 4
      .name   = dt_token("albedo"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 5
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    },{ // 6
      .name   = dt_token("lightout"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = module->connector[0].roi,
    },{ // 7
      .name   = dt_token("gbufp"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f32"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{ // 8
      .name   = dt_token("gbufc"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f32"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    }},
  };

  int id_eaw[4];

  for(int i=0;i<4;i++)
  {
    id_eaw[i] = graph->num_nodes++;
    graph->node[id_eaw[i]] = (dt_node_t) {
      .name   = dt_token("svgf"),
      .kernel = dt_token("eaw"),
      .module = module,
      .wd     = module->connector[0].roi.wd,
      .ht     = module->connector[0].roi.ht,
      .dp     = 1,
      .num_connectors = 4,
      .connector = {{ // 0
        .name   = dt_token("input"),
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
      },{ // 3
        .name   = dt_token("gbuf_out"),
        .type   = dt_token("write"),
        .chan   = dt_token("rg"),
        .format = dt_token("f32"),
        .roi    = module->connector[0].roi,
      }},
      .push_constant_size = 1*sizeof(uint32_t),
      .push_constant = { 1<<i },
    };
  }

  dt_connector_copy(graph, module, 0, id_preblend, 0);  // mv
  dt_connector_copy_feedback(graph, module, 1, id_preblend, 1);  // previous noisy light
  dt_connector_copy(graph, module, 1, id_preblend, 2);  // noisy light
  dt_connector_copy(graph, module, 4, id_preblend, 3);  // previous gbuffer
  dt_connector_copy(graph, module, 5, id_preblend, 4);  // current gbuffer

  CONN(dt_node_connect(graph, id_preblend, 5, id_eaw[0], 0)); // less noisy light
  // dt_connector_copy(graph, module, 1, id_eaw[0], 0);  // noisy light
  dt_connector_copy(graph, module, 5, id_eaw[0], 1);  // gbuffer for normal, depth, L moments
  for(int i=1;i<4;i++)
  {
    CONN(dt_node_connect (graph, id_eaw[i-1], 2, id_eaw[i], 0));
    CONN(dt_node_connect (graph, id_eaw[i-1], 3, id_eaw[i], 1));
  }
  CONN(dt_node_connect (graph, id_eaw[3], 2, id_blend, 3)); // denoised light

  CONN(dt_node_feedback(graph, id_blend,  6, id_blend, 1)); // denoised light, old
  CONN(dt_node_feedback(graph, id_blend,  5, id_blend, 2)); // beauty frame, old
#if SVGF_OFF==1
  dt_connector_copy(graph, module, 1, id_blend, 3); // XXX DEBUG light w/o denoising
#endif

  dt_connector_copy(graph, module, 0, id_blend, 0);  // mv
  dt_connector_copy(graph, module, 2, id_blend, 4);  // albedo
  dt_connector_copy(graph, module, 3, id_blend, 5);  // output beauty
  dt_connector_copy(graph, module, 4, id_blend, 7);  // previous gbuffer
  dt_connector_copy(graph, module, 5, id_blend, 8);  // current gbuffer
}
