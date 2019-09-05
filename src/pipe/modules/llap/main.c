#include "config.h"
#include "modules/api.h"
#include <math.h>
#include <stdlib.h>

// TODO: make curve specific stuff an api function for reuse?
void
commit_params(
    dt_graph_t  *graph,
    dt_node_t   *node)
{
  float *f = (float *)node->push_constant;
  float *p = (float *)node->module->param;

  if(node->kernel == dt_token("assemble"))
  {
    f[1] = dt_module_param_float(node->module, 8)[0];
    return;
  }

  // this is for node name film curve
  // copy x and y
  for(int k=0;k<8;k++) f[k] = p[k];
  // init tangent
  const int n=4;
  float m[5], d[5];
  for(int i=0;i<n-1;i++)
    d[i] = (p[i+5] - p[i+4])/(p[i+1] - p[i]);
  d[n-1] = d[n-2];
  for(int i=1;i<n-1;i++)
    if(d[i-1]*d[i] <= 0.0f)
      m[i] = 0.0f;
    else
      m[i] = (d[i-1] + d[i])*.5f;
  // for extrapolation: keep curvature constant
  m[n-1] = fmaxf(m[n-2] + d[n-2] - d[n-3], 0.0f);
  m[0]   = fmaxf(m[1] + d[0] - d[1], 0.0f);

  // monotone hermite clamping:
  for(int i=0;i<n;i++)
  {
    if(fabsf(d[i]) <= 1e-8f)
      m[i] = m[i+1] = 0.0f;
    else
    {
      const float alpha = m[i]   / d[i];
      const float beta  = m[i+1] / d[i];
      const float tau   = alpha * alpha + beta * beta;
      if(tau > 9.0f)
      {
        m[i]   = 3.0f * m[i]   / sqrtf(tau);
        m[i+1] = 3.0f * m[i+1] / sqrtf(tau);
      }
    }
  }
  for(int k=0;k<4;k++) f[8+k] = m[k];
  f[12] = p[8];
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  const int dp = 1;

  // input
  //  |
  //  v
  // curve -> curve0.. curve5  gray
  //           |    ..  |       |
  //           v    ..  v       v
  //          reduce   reduce  reduce       (on all levels)
  //           |                |
  //           v   ..           v
  //           C                C -> resample context buf + large scale tone curve/log (one level deeper than the others)
  //
  //      assemble  on all levels, with inputs all buffers from corresponding level

  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = module->connector[0].roi,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("y"),
    .format = dt_token("f16"),
    .roi    = module->connector[0].roi,
  };
#ifdef LLAP_HQ
  const int num_gamma = 10;
#else
  const int num_gamma = 6;
#endif

  assert(graph->num_nodes < graph->max_nodes);
  const int id_curve = graph->num_nodes++;
  dt_node_t *node_curve = graph->node + id_curve;
  *node_curve = (dt_node_t) {
    .name   = dt_token("llap"),
    .kernel = dt_token("curve"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2 + num_gamma,
    .connector = {
      ci, co, co, co,
      co, co, co, co,
#ifdef LLAP_HQ
      co, co, co, co,
#endif
    },
  };

  dt_roi_t rf = module->connector[0].roi;
  dt_roi_t rc = module->connector[0].roi;
  rc.wd /= 2;
  rc.ht /= 2;
  rc.full_wd /= 2;
  rc.full_ht /= 2;

  const int nl = 8;
  int id_reduce[nl][num_gamma+1] = {{-1}};
  int cn_reduce[nl][num_gamma+1] = {{-1}};
  for(int k=0;k<num_gamma+1;k++)
  {
    id_reduce[0][k] = id_curve;
    cn_reduce[0][k] = 1+k;
  }
  int id_assemble[nl] = {-1};
  for(int l=1;l<nl;l++)
  { // for all coarseness levels
    dt_connector_t cif = {
      .name   = dt_token("inhi"),
      .type   = dt_token("read"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = rf,
      .connected_mi = -1,
    };
    dt_connector_t cic = cif;
    cic.roi  = rc;
    cic.name = dt_token("inlo");
    dt_connector_t cof = {
      .name   = dt_token("outhi"),
      .type   = dt_token("write"),
      .chan   = dt_token("y"),
      .format = dt_token("f16"),
      .roi    = rf,
    };
    dt_connector_t coc = cof;
    coc.roi = rc;
    coc.name = dt_token("outlo");

    for(int k=0;k<num_gamma+1;k++)
    { // for all brightness levels gamma:
      // one reduce node taking this gamma one coarser
      assert(graph->num_nodes < graph->max_nodes);
      id_reduce[l][k] = graph->num_nodes++;
      cn_reduce[l][k] = 1;
      dt_node_t *node_reduce = graph->node + id_reduce[l][k];
      *node_reduce = (dt_node_t) {
        .name   = dt_token("llap"),
        .kernel = dt_token("reduce"),
        .module = module,
        .wd     = rc.wd,
        .ht     = rc.ht,
        .dp     = dp,
        .num_connectors = 2,
        .connector = {
          cif, coc,
        },
      };
      // wire input:
      CONN(dt_node_connect(graph, id_reduce[l-1][k], cn_reduce[l-1][k], id_reduce[l][k], 0));
    }
    // one assemble node taking as input:
    // - coarse output pyramid (start with coarsest of gray after global tonecurve)
    // - fine level of original input colour for laplacian selection
    // - all 6x2 adjacent gamma levels (i/o to reduce)
    // output:
    // - next finer output pyramid
    const float scale = l/(nl-1.0);
    assert(graph->num_nodes < graph->max_nodes);
    id_assemble[l] = graph->num_nodes++;
    dt_node_t *node_assemble = graph->node + id_assemble[l];
    *node_assemble = (dt_node_t) {
      .name   = dt_token("llap"),
      .kernel = dt_token("assemble"),
      .module = module,
      .wd     = rf.wd,
      .ht     = rf.ht,
      .dp     = dp,
      .num_connectors = 2*num_gamma + 3,
      .connector = {
        cif, cic,
        cif, cic, cif, cic, cif, cic, cif, cic, cif, cic, cif, cic,
#ifdef LLAP_HQ
        cif, cic, cif, cic, cif, cic, cif, cic,
#endif
        cof,
      },
      .push_constant_size = 8,
      .push_constant = { *(uint32_t*)(&scale), 0 }, 
    };
    node_assemble->connector[0].name = dt_token("orighi");
    node_assemble->connector[1].name = dt_token("currlo");
    for(int k=0;k<num_gamma;k++)
    { // connect fine and coarse levels of curve processed buffers:
      CONN(dt_node_connect(graph, id_reduce[l-1][k], cn_reduce[l-1][k], id_assemble[l], 2+2*k  ));
      CONN(dt_node_connect(graph, id_reduce[l  ][k], cn_reduce[l  ][k], id_assemble[l], 2+2*k+1));
    }
    // connect fine reference, unprocessed:
    CONN(dt_node_connect(graph, id_reduce[l-1][num_gamma], cn_reduce[l-1][num_gamma], id_assemble[l], 0));

    rf = rc;
    rc.wd /= 2;
    rc.ht /= 2;
    rc.full_wd /= 2;
    rc.full_ht /= 2;
  }


  // connect ouput fine buffer [14] (input to coarse [1] on next finer level)
  for(int l=1;l<nl-1;l++)
    CONN(dt_node_connect(graph, id_assemble[l+1], 2*num_gamma + 2, id_assemble[l], 1));

  // wire into some recolouration node:
  dt_connector_t cy = ci;
  ci.chan = dt_token("rgba");
  ci.name = dt_token("inrgb");
  cy.chan = dt_token("y");
  cy.name = dt_token("inlum");
  co.chan = dt_token("rgba");
  co.name = dt_token("output");
  assert(graph->num_nodes < graph->max_nodes);
  const int id_col = graph->num_nodes++;
  dt_node_t *node_col = graph->node + id_col;
  *node_col = (dt_node_t) {
    .name   = dt_token("llap"),
    .kernel = dt_token("colour"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {
      cy, ci, co,
    },
  };
  CONN(dt_node_connect(graph, id_assemble[1], 2*num_gamma+2, id_col, 0));

  // connect input coarse buffer [1] (coarsest reduced unprocessed or from coarser assemble level)
#if 1
  // TODO: insert tone curve in between here:
  // XXX tried and doesn't work super well. put this back:
  CONN(dt_node_connect(graph, id_reduce[nl-1][num_gamma], cn_reduce[nl-1][num_gamma], id_assemble[nl-1], 1));
#else
  // XXX TODO: need to fill push constants in commit_params and grab some code from
  // XXX filmcurve!
  // TODO add filmcurve node and connect it!
  // XXX
  // dt_connector_copy(graph, module, 1, id_reduce[nl-1][6], cn_reduce[nl-1][6]);
  // dt_connector_copy(graph, module, 2, id_assemble[nl-1], 1);
  ci.chan = dt_token("y");
  ci.name = dt_token("input");
  co.chan = dt_token("y");
  co.name = dt_token("output");
  ci.roi  = graph->node[id_assemble[nl-1]].connector[1].roi;
  co.roi  = graph->node[id_assemble[nl-1]].connector[1].roi;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_film = graph->num_nodes++;
  dt_node_t *node_film = graph->node + id_film;
  *node_film = (dt_node_t) {
    .name   = dt_token("filmcurv"),
    .kernel = dt_token("main"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
    .push_constant_size = 13*sizeof(float),
  };
  CONN(dt_node_connect(graph, id_reduce[nl-1][6], cn_reduce[nl-1][6], id_film, 0));
  CONN(dt_node_connect(graph, id_film, 1, id_assemble[nl-1], 1));
#endif

  // wire module i/o connectors to nodes:
  dt_connector_copy(graph, module, 0, id_curve, 0);
  dt_connector_copy(graph, module, 0, id_col,   1);
  dt_connector_copy(graph, module, 1, id_col,   2);
}

