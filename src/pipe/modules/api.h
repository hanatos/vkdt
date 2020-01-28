#pragma once
#include "pipe/node.h"
#include "pipe/graph.h"
#include "pipe/module.h"

// some module specific helpers

// convenience function to detect inputs
static inline int
dt_connector_input(dt_connector_t *c)
{
  return c->type == dt_token("read") || c->type == dt_token("sink");
}
static inline int
dt_connector_output(dt_connector_t *c)
{
  return c->type == dt_token("write") || c->type == dt_token("source");
}
static inline int
dt_node_source(dt_node_t *n)
{
  return n->connector[0].type == dt_token("source");
}
static inline int
dt_node_sink(dt_node_t *n)
{
  return n->connector[0].type == dt_token("sink");
}

static inline void
dt_connector_copy(
    dt_graph_t  *graph,
    dt_module_t *module,
    int mc,    // connector id on module to copy
    int nid,   // node id
    int nc)    // connector id on node to copy to
{
  // connect the node layer on the module:
  module->connector[mc].connected_ni = nid;
  module->connector[mc].connected_nc = nc;
  // connect the node:
  dt_connector_t *c0 = module->connector + mc;
  dt_connector_t *c1 = graph->node[nid].connector + nc;
  // copy defaults if the node hasn't been inited at all:
  if(c1->name == 0)
    *c1 = *c0;
  else
  {
    c1->frames = c0->frames;
    c1->flags  = c0->flags;
    c1->format = c0->format; // modules may be rigged from the outside, for export etc
    c1->roi    = c0->roi;
    c1->connected_mi = c0->connected_mi;
    c1->connected_mc = c0->connected_mc;
    c1->connected_ni = 0;
    c1->connected_nc = 0;
  }

  // input connectors have a unique source. connect their node layer:
  if(dt_connector_input(module->connector+mc) &&
      module->connector[mc].connected_mi >= 0)
  {
    if(module->connector[mc].flags & s_conn_feedback)
    {
      // feedback connectors usually go back/form cycles in the dag.
      // this means that the module we're referring to here is often
      // not initialised. in particular we don't know which node
      // it'll be associated with. thus, we remember the module reference
      // on the node, too, and have to remember to do a repointing pass
      // before we go to the next stage after node creation.
    }
    else
    { // connect our node to the nodeid stored on the module
      graph->node[nid].connector[nc].connected_mi = graph->module[
        module->connector[mc].connected_mi].connector[
          module->connector[mc].connected_mc].connected_ni;
      graph->node[nid].connector[nc].connected_mc = graph->module[
        module->connector[mc].connected_mi].connector[
          module->connector[mc].connected_mc].connected_nc;
    }
  }
}

static inline int
dt_api_blur_flat(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    int          *id_blur_in,
    int          *id_blur_out,
    uint32_t     radius)
{
  // detect pixel format on input and blur the whole thing in separable kernels
  const dt_connector_t *conn_input = nodeid_input >= 0 ?
    graph->node[nodeid_input].connector + connid_input :
    module->connector + connid_input;
  const uint32_t dp = conn_input->array_length > 0 ? conn_input->array_length : 1;
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = conn_input->chan,
    .format = conn_input->format,
    .roi    = conn_input->roi,
    .flags  = s_conn_smooth,
    .connected_mi = -1,
    .array_length = conn_input->array_length,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = conn_input->chan,
    .format = conn_input->format,
    .roi    = conn_input->roi,
    .array_length = conn_input->array_length,
  };
  dt_roi_t roi = conn_input->roi;
  int nid_input = nodeid_input;
  int cid_input = connid_input;
  int it = 1;
  if(id_blur_in)  *id_blur_in  = -1;
  if(id_blur_out) *id_blur_out = -1;
  // uint32_t rad = it;
  // while(rad<radius) { it++; rad+=2*it; }
  while(2*(1u<<it) < radius) it++;
  for(uint32_t i=0;i<it;i++)
  {
    // add nodes blurh and blurv
    assert(graph->num_nodes < graph->max_nodes);
    const int id_blurh = graph->num_nodes++;
    graph->node[id_blurh] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("blurd"),
      .module = module,
      .wd     = i ? roi.wd : roi.wd/2,
      .ht     = roi.ht,
      .dp     = dp,
      .num_connectors = 2,
      .connector = { ci, co },
      .push_constant_size = 3*sizeof(uint32_t),
      .push_constant = { 1, 0, i },
    };
    if(id_blur_in && *id_blur_in < 0) *id_blur_in = id_blurh;
    graph->node[id_blurh].connector[0].roi = roi;
    if(i == 0) roi.wd /= 2;
    graph->node[id_blurh].connector[1].roi = roi;
    assert(graph->num_nodes < graph->max_nodes);
    const int id_blurv = graph->num_nodes++;
    graph->node[id_blurv] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("blurd"),
      .module = module,
      .wd     = roi.wd,
      .ht     = i ? roi.ht : roi.ht/2,
      .dp     = dp,
      .num_connectors = 2,
      .connector = { ci, co },
      .push_constant_size = 3*sizeof(uint32_t),
      .push_constant = { 0, 1, i },
    };
    if(id_blur_out && i == it-1) *id_blur_out = id_blurv;
    graph->node[id_blurv].connector[0].roi = roi;
    if(i == 0) roi.ht /= 2;
    graph->node[id_blurv].connector[1].roi = roi;
    // interconnect nodes:
    CONN(dt_node_connect(graph, nid_input,  cid_input, id_blurh, 0));
    CONN(dt_node_connect(graph, id_blurh,   1,         id_blurv, 0));
    nid_input = id_blurv;
    cid_input = 1;
  }
#if 0
  // now let's upsample at least a bit:
  // for(int i=0;i<it;i++) { }
    assert(graph->num_nodes < graph->max_nodes);
    const int id_upsample = graph->num_nodes++;
    roi = conn_input->roi;
    roi.wd /= 2;
    roi.ht /= 2;
    graph->node[id_upsample] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("resample"),
      .module = module,
      .wd     = roi.wd,
      .ht     = roi.ht,
      .dp     = dp,
      .num_connectors = 2,
      .connector = { ci, co },
      .push_constant_size = 3*sizeof(uint32_t),
      .push_constant = { 1, 0, 0 },
    };
    graph->node[id_upsample].connector[0].roi = graph->node[nid_input].connector[1].roi;
    graph->node[id_upsample].connector[1].roi = roi;
    CONN(dt_node_connect(graph, nid_input,  cid_input, id_upsample, 0));
    return id_upsample;
#endif
  return nid_input;
}

// create new nodes, connect to given input node + connector id, perform blur
// of given pixel radius, return nodeid (output connector will be #1).
static inline int
dt_api_blur(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    uint32_t     radius)
{
  // detect pixel format on input and blur the whole thing in separable kernels
  const dt_connector_t *conn_input = graph->node[nodeid_input].connector + connid_input;
  const uint32_t wd = conn_input->roi.wd;
  const uint32_t ht = conn_input->roi.ht;
  const uint32_t dp = conn_input->array_length > 0 ? conn_input->array_length : 1;
  // TODO: is this necessary? or could we always go for [0]?
  dt_token_t blurh = dp > 1 ? dt_token("blurah") : dt_token("blurh");
  dt_token_t blurv = dp > 1 ? dt_token("blurav") : dt_token("blurv");
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = conn_input->chan,
    .format = dt_token("f16"),
    .roi    = conn_input->roi,
    .connected_mi = -1,
    .array_length = conn_input->array_length,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = conn_input->chan,
    .format = dt_token("f16"),
    .roi    = conn_input->roi,
    .array_length = conn_input->array_length,
  };
  // push and interconnect a-trous gauss blur nodes
  // perf: could make faster by using a decimated scheme, see llap reduce for instance.
  // TODO: iterate until radius is matched, use smaller steps to achieve sub-power-of-two radii
  int it = 1;
  while(2*(1u<<it) < radius) it++;
  int nid_input = nodeid_input;
  int cid_input = connid_input;
  for(int i=0;i<it;i++)
  {
    // add nodes blurh and blurv
    assert(graph->num_nodes < graph->max_nodes);
    const int id_blurh = graph->num_nodes++;
    graph->node[id_blurh] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = blurh,
      .module = module,
      .wd     = wd,
      .ht     = ht,
      .dp     = dp,
      .num_connectors = 2,
      .connector = { ci, co },
      .push_constant_size = 4,
      .push_constant = {1u<<i},
    };
    assert(graph->num_nodes < graph->max_nodes);
    const int id_blurv = graph->num_nodes++;
    graph->node[id_blurv] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = blurv,
      .module = module,
      .wd     = wd,
      .ht     = ht,
      .dp     = dp,
      .num_connectors = 2,
      .connector = { ci, co },
      .push_constant_size = 4,
      .push_constant = {1u<<i},
    };
    // interconnect nodes:
    CONN(dt_node_connect(graph, nid_input,  cid_input, id_blurh, 0));
    CONN(dt_node_connect(graph, id_blurh,   1,         id_blurv, 0));
    nid_input = id_blurv;
    cid_input = 1;
  }
  return nid_input;
}

// full guided filter:
// input image      I (rgb) -> entry node [0]
// input image      I (rgb) -> exit  node [0]
// input edge image p (rgb) -> entry node [1]
// output filtered    (rgb) <- exit  node [2]
static inline void
dt_api_guided_filter_full(
    dt_graph_t  *graph,        // graph to add nodes to
    dt_module_t *module,
    dt_roi_t    *roi,
    int         *entry_nodeid,
    int         *exit_nodeid,
    float        radius,       // size of blur
    float        epsilon)      // tell edges from noise
{
  const uint32_t wd = roi->wd;
  const uint32_t ht = roi->ht;
  const uint32_t dp = 1;

  // compute (I,p,I*I,I*p)
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided1 = graph->num_nodes++;
  *entry_nodeid = id_guided1;
  graph->node[id_guided1] = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided1f"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("edges"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
    }},
  };

  // mean_I  = blur(I)
  // mean_p  = blur(p)
  // corr_I  = blur(I*I)
  // corr_Ip = blur(I*p)
  // const int id_blur1 = dt_api_blur(graph, module, id_guided1, 2, radius);
  const int id_blur1 = dt_api_blur_flat(graph, module, id_guided1, 2, 0, 0, radius);

  // var_I   = corr_I - mean_I*mean_I
  // cov_Ip  = corr_Ip - mean_I*mean_p
  // a = cov_Ip / (var_I + epsilon)
  // b = mean_p - a * mean_I
  const int id_guided2 = graph->num_nodes++;
  graph->node[id_guided2] = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided2f"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .flags  = s_conn_smooth,
      .roi    = *roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rg"),
      .format = dt_token("f16"),
      .roi    = *roi,
    }},
  };
  CONN(dt_node_connect(graph, id_blur1, 1, id_guided2, 0));

  // this is the same as in the p=I case below:
  // mean_a = blur(a)
  // mean_b = blur(b)
  // const int id_blur = dt_api_blur(graph, module, id_guided2, 1, radius);
  const int id_blur = dt_api_blur_flat(graph, module, id_guided2, 1, 0, 0, radius);
  // final kernel:
  // output = mean_a * I + mean_b
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided3 = graph->num_nodes++;
  graph->node[id_guided3] = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided3"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {{ // original image as input rgba f16
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
      .connected_mi = -1,
    },{ // mean ab
      .name   = dt_token("ab"),
      .type   = dt_token("read"),
      .chan   = dt_token("rg"),
      .format = dt_token("f16"),
      .flags  = s_conn_smooth,
      .roi    = *roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = dt_token("rgba"),
      .format = dt_token("f16"),
      .roi    = *roi,
    }},
  };
  CONN(dt_node_connect(graph, id_blur, 1, id_guided3, 1));
  *exit_nodeid = id_guided3;
}

// implements a guided filter without guide image, i.e. p=I.
// the entry node reads connector[0], the exit writes to connector[2].
static inline void
dt_api_guided_filter(
    dt_graph_t  *graph,        // graph to add nodes to
    dt_module_t *module,
    dt_roi_t    *roi,
    int         *entry_nodeid,
    int         *exit_nodeid,
    int          radius,       // size of blur
    float        epsilon)      // tell edges from noise
{
  // const dt_connector_t *conn_input = graph->node[nodeid_input].connector + connid_input;
  const uint32_t wd = roi->wd;
  const uint32_t ht = roi->ht;
  const uint32_t dp = 1;
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = dt_token("rgba"),
    .format = dt_token("f16"),
    .roi    = *roi,
    .connected_mi = -1,
  };
  dt_connector_t co = {
    .name   = dt_token("output"),
    .type   = dt_token("write"),
    .chan   = dt_token("rg"),
    .format = dt_token("f16"),
    .roi    = *roi,
  };

  // compute grey scale image rg32 with (I, I*I)
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided1 = graph->num_nodes++;
  *entry_nodeid = id_guided1;
  dt_node_t *node_guided1 = graph->node + id_guided1;
  *node_guided1 = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided1"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };

  // then connect 1x blur:
  // mean_I = blur(I)
  // corr_I = blur(I*I)
  const int id_blur1 = dt_api_blur(graph, module, id_guided1, 1, radius);

  // connect to this node:
  // a = var_I / (var_I + eps)
  // with var_I = corr_I - mean_I * mean_I
  // b = mean_I - a * mean_I
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided2 = graph->num_nodes++;
  dt_node_t *node_guided2 = graph->node + id_guided2;
  ci.chan = dt_token("rg");
  *node_guided2 = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided2"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {
      ci, co,
    },
  };

  CONN(dt_node_connect(graph, id_blur1, 1, id_guided2, 0));

  // and blur once more:
  // mean_a = blur(a)
  // mean_b = blur(b)
  const int id_blur = dt_api_blur(graph, module, id_guided2, 1, radius);

  // final kernel:
  // output = mean_a * I + mean_b
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided3 = graph->num_nodes++;
  dt_node_t *node_guided3 = graph->node + id_guided3;
  ci.chan = dt_token("rgba");
  co.chan = dt_token("rgba");
  dt_connector_t cm = {
    .name   = dt_token("ab"),
    .type   = dt_token("read"),
    .chan   = dt_token("rg"),
    .format = dt_token("f16"),
    .roi    = *roi,
    .connected_mi = -1,
  };
  *node_guided3 = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("guided3"),
    .module = module,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .num_connectors = 3,
    .connector = {
      ci, // - original image as input rgba f16
      cm, // - mean a mean b as rg f16
      co, // - output rgba f16
    },
  };
  CONN(dt_node_connect(graph, id_blur, 1, id_guided3, 1));
  *exit_nodeid = id_guided3;
}

static inline const uint32_t *dt_module_param_uint32(
    const dt_module_t *module,
    int paramid)
{
  if(paramid >= 0 && paramid < module->so->num_params)
    return (uint32_t *)(module->param + module->so->param[paramid]->offset);
  return 0;
}

static inline const float *dt_module_param_float(
    const dt_module_t *module,
    int paramid)
{
  if(paramid >= 0 && paramid < module->so->num_params)
    return (float *)(module->param + module->so->param[paramid]->offset);
  return 0;
}

static inline const char *dt_module_param_string(
    const dt_module_t *module,
    int paramid)
{
  if(paramid >= 0 && paramid < module->so->num_params)
    return (const char *)(module->param + module->so->param[paramid]->offset);
  return 0;
}

static inline int
dt_module_set_param_string(
    const dt_module_t *module,
    dt_token_t         param,
    const char        *val)
{
  char *param_str = 0;
  int param_len = 0;
  for(int p=0;p<module->so->num_params;p++)
  {
    if(module->so->param[p]->name == param)
    {
      param_str = (char *)(module->param + module->so->param[p]->offset);
      param_len = module->so->param[p]->cnt;
      break;
    }
  }
  if(!param_str) return 1;
  snprintf(param_str, param_len, "%s", val);
  return 0;
}
