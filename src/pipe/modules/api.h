#pragma once
#include "pipe/node.h"
#include "pipe/graph.h"
#include "pipe/module.h"

#include <math.h>

// some module specific helpers and constants

typedef enum
{
  s_graph_run_none           = 0,
  s_graph_run_roi            = 1<<0, // pass 1: recompute regions of interest
  s_graph_run_create_nodes   = 1<<1, // pass 2: create nodes from modules
  s_graph_run_alloc          = 1<<2, // pass 3: alloc and free images
  s_graph_run_record_cmd_buf = 1<<3, // pass 4: record command buffer
  s_graph_run_upload_source  = 1<<4, // final : upload new source image
  s_graph_run_download_sink  = 1<<5, // final : download sink images
  s_graph_run_wait_done      = 1<<6, // wait for fence
  s_graph_run_before_active  = 1<<7, // run all modules, even before active_module
  s_graph_run_all = -1u,
} dt_graph_run_constants_t;

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
dt_connector_ssbo(dt_connector_t *c)
{
  return c->chan == dt_token("ssbo");
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

#ifndef __cplusplus
static inline void
dt_connector_copy(
    dt_graph_t  *graph,
    dt_module_t *module,
    int mc,    // connector id on module to copy
    int nid,   // node id
    int nc)    // connector id on node to copy to
{
  // connect the node layer on the module. these are only meaningful
  // for output connectors (others might be ambiguous):
  module->connector[mc].associated_i = nid;
  module->connector[mc].associated_c = nc;
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
    c1->array_length = c0->array_length;
    c1->array_dim    = c0->array_dim;
  }

  // node connectors need to know their counterparts.
  // unfortunately at this point not all nodes are inited.
  // thus, every node needs to remember the module id and connector
  c1->associated_i = module - graph->module;
  c1->associated_c = mc;
}

// bypass a module: instead of linking the module connectors to their
// counterparts on the node level, directly link to the node level of the next
// module.
static inline void
dt_connector_bypass(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          mc_in,
    int          mc_out)
{
  // we want to make sure the node layer of our input connector
  // is directly routed to the node layer on the output.
  dt_connector_t *c_in  = module->connector + mc_in;
  dt_connector_t *c_out = module->connector + mc_out;

  c_out->bypass_mi = module - graph->module;
  c_out->bypass_mc = mc_in;
  c_in->bypass_mi  = module - graph->module;
  c_in->bypass_mc  = mc_out;
}

static inline uint32_t
dt_api_blur_check_params(
    float oldrad,
    float newrad)
{
  // TODO: only return flags if this in fact needs a different number of passes
  if(oldrad != newrad) return s_graph_run_all; // for now just nuke it
  return s_graph_run_record_cmd_buf; // needs that anyways
}

// approximate gaussian blur by small sigma.
// this has a 3x3 kernel support and works up until sigma=0.85.
// use blur or blur_sub instead for larger blur radii.
static inline int
dt_api_blur_small(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    int         *id_blur_in,
    int         *id_blur_out,
    float        sigma)
{
  // detect pixel format on input
  const dt_connector_t *conn_input = nodeid_input >= 0 ?
    graph->node[nodeid_input].connector + connid_input :
    module->connector + connid_input;
  const uint32_t dp = conn_input->array_length > 0 ? conn_input->array_length : 1;
  const int *sigmai = (int*)&sigma;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_blur = graph->num_nodes++;
  graph->node[id_blur] = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("blurs"),
    .module = module,
    .wd     = conn_input->roi.wd,
    .ht     = conn_input->roi.ht,
    .dp     = dp,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = conn_input->chan,
      .format = conn_input->format,
      .roi    = conn_input->roi,
      .flags  = s_conn_smooth,
      .connected_mi = -1,
      .array_length = conn_input->array_length,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = conn_input->chan,
      .format = conn_input->format,
      .roi    = conn_input->roi,
      .array_length = conn_input->array_length,
    }},
    .push_constant_size = sizeof(float),
    .push_constant = { sigmai[0] },
  };
  if(id_blur_in)  *id_blur_in  = id_blur;
  if(id_blur_out) *id_blur_out = id_blur;
  if(nodeid_input >= 0)
    CONN(dt_node_connect(graph, nodeid_input, connid_input, id_blur, 0));
  else
    dt_connector_copy(graph, module, connid_input, id_blur, 0);
  return id_blur;
}

// blur by radius, but using a cascade of sub-sampled dispatches.
// faster than the non-sub version, but approximate.
static inline int
dt_api_blur_sub(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    int         *id_blur_in,
    int         *id_blur_out,
    uint32_t     levels,       // number of iterations, affects radius
    uint32_t    *mul,          // multiples of iterations on each scale
    uint32_t     upsample)     // set this to != 0 if you want output on the same res as the input (high quality like)
{
  // detect pixel format on input
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
  if(id_blur_in)  *id_blur_in  = -1;
  if(id_blur_out) *id_blur_out = -1;
  for(uint32_t i=0;i<levels;i++)
  {
    for(int j=0;j<mul[i];j++)
    {
      assert(graph->num_nodes < graph->max_nodes);
      const int id_blur = graph->num_nodes++;
      int hwd = j == mul[i]-1 ? (roi.wd + 1)/2 : roi.wd;
      int hht = j == mul[i]-1 ? (roi.ht + 1)/2 : roi.ht;
      graph->node[id_blur] = (dt_node_t) {
        .name   = dt_token("shared"),
        .kernel = dt_token("blur"),
        .module = module,
        .wd     = hwd,
        .ht     = hht,
        .dp     = dp,
        .num_connectors = 2,
        .connector = { ci, co },
      };
      if(id_blur_in && *id_blur_in < 0) *id_blur_in = id_blur;
      graph->node[id_blur].connector[0].roi = roi;
      roi.wd = hwd;
      roi.ht = hht;
      graph->node[id_blur].connector[1].roi = roi;
      // interconnect nodes:
      CONN(dt_node_connect(graph, nid_input, cid_input, id_blur, 0));
      nid_input = id_blur;
      cid_input = 1;
    }
  }
  if(id_blur_out) *id_blur_out = nid_input;
  if(upsample)
  { // single upsampling kernel is enough, we're using quadric reconstruction
    assert(graph->num_nodes < graph->max_nodes);
    const int id_upsample = graph->num_nodes++;
    graph->node[id_upsample] = (dt_node_t) {
      .name   = dt_token("shared"),
        .kernel = dt_token("resample"),
        .module = module,
        .wd     = conn_input->roi.wd,
        .ht     = conn_input->roi.ht,
        .dp     = dp,
        .num_connectors = 2,
        .connector = { ci, co },
    };
    graph->node[id_upsample].connector[0].roi = graph->node[nid_input].connector[cid_input].roi;
    CONN(dt_node_connect(graph, nid_input, cid_input, id_upsample, 0));
    if(id_blur_out) *id_blur_out = id_upsample;
    nid_input = id_upsample;
  }
  return nid_input;
}

// create new nodes, connect to given input node + connector id, perform blur
// of given pixel radius, return nodeid (output connector will be #1).
// this uses separated h/v passes.
static inline int
dt_api_blur_sep(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    int         *id_blur_in,
    int         *id_blur_out,
    uint32_t     radius)        // blur radius in pixels
{
  // detect pixel format on input and blur the whole thing in separable kernels
  const dt_connector_t *conn_input = nodeid_input >= 0 ?
    graph->node[nodeid_input].connector + connid_input :
    module->connector + connid_input;
  const uint32_t wd = conn_input->roi.wd;
  const uint32_t ht = conn_input->roi.ht;
  const uint32_t dp = conn_input->array_length > 0 ? conn_input->array_length : 1;
  dt_token_t blurh = dt_token("blurh");
  dt_token_t blurv = dt_token("blurv");
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = conn_input->chan,
    .format = conn_input->format,
    .roi    = conn_input->roi,
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
    .push_constant_size = sizeof(uint32_t),
    .push_constant = {radius},
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
    .push_constant_size = sizeof(uint32_t),
    .push_constant = {radius},
  };
  // interconnect nodes:
  CONN(dt_node_connect(graph, nodeid_input,  connid_input, id_blurh, 0));
  CONN(dt_node_connect(graph, id_blurh,      1,            id_blurv, 0));
  if(id_blur_in ) *id_blur_in  = id_blurh;
  if(id_blur_out) *id_blur_out = id_blurv;
  return id_blurv;
}

// generic blur, selecting some mix of separable/small/subsampled blur
// to best reach the given radius goal
static inline int
dt_api_blur(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    int         *id_blur_in,
    int         *id_blur_out,
    float        radius)        // 2 sigma of the requsted blur, in pixels
{
  // XXX DEBUG
    // return dt_api_blur_sep(graph, module, nodeid_input, connid_input,
        // id_blur_in, id_blur_out, radius);
  // let's separate the blur in several passes.
  // we'll do as many steps of subsampled blur as we can, because
  // these are fast.
  // subsampling does 5x5 blurs, i.e. radius=2 on powers of two
  // i.e. 5x5, 9x9, 17x17, .. (radii 2 4 8 ..)
  float sig2_req = radius*radius*0.25; // requested sigma^2
  float sig2 = 0.0f;
  int it = 0; // sub blur iterations needed
  uint32_t mul[10] = {0};
  for(;it<10;it++)
  {
    float sig = 1<<it; // sigma of this iteration
    // the combined sigma of two consecutively executed blurs does not quite sum up:
    // fprintf(stderr, "it %d sig2 %g sig*sig %g vs. req %g\n", it, sig2, sig*sig, sig2_req);
    if(sig2 + sig*sig > sig2_req)
      break;
    sig2 += sig*sig;  // combined sigma^2
    mul[it] = 1;
  }
  for(int j=it;j>=0;j--)
  {
    float sig = 1<<j;
    while(sig2 + sig*sig < sig2_req)
    { // record iteration j as multiple
      sig2 += sig*sig;
      mul[j]++;
    }
  }
  // remaining sigma
  float sig_rem = sqrtf(MAX(0, sig2_req - sig2));
  // fprintf(stderr, "radius: %g levels: %d remaining sigma %g\n", radius, it, sig_rem);
  // fprintf(stderr, "multiplicity: %d %d %d %d %d %d %d %d\n",
  //     mul[0], mul[1], mul[2], mul[3],
  //     mul[4], mul[5], mul[6], mul[7]);

#if 1
  if(it && (sig_rem == 0 || it >= 2))
  { // for large blurs you'll not notice a little underblur anyways, leave the small kernel.
    return dt_api_blur_sub(graph, module, nodeid_input, connid_input,
        id_blur_in, id_blur_out, it, mul, 1);
  }
  else
#endif
  if(it)
  { // use both to get more precise correspondence
    int id_blur_small_in = -1, id_blur_small_out = -1;
    dt_api_blur_sep(graph, module, nodeid_input, connid_input,
        &id_blur_small_in, &id_blur_small_out, 2.0f*sig_rem);
    return dt_api_blur_sub(graph, module, id_blur_small_out, 1,
        id_blur_in, id_blur_out, it, mul, 1);
  }
  else
  {
    return dt_api_blur_small(graph, module, nodeid_input, connid_input,
        id_blur_in, id_blur_out, 0.5f*radius);
  }
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
    float        radius,       // size of blur as fraction of input width
    float        epsilon)      // tell edges from noise
{
  const uint32_t wd = roi->wd;
  const uint32_t ht = roi->ht;
  const uint32_t dp = 1;
  const float radius_px = radius * wd;

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
  const int id_blur1 = dt_api_blur(graph, module, id_guided1, 2, 0, 0, radius_px);

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
  const int id_blur = dt_api_blur(graph, module, id_guided2, 1, 0, 0, radius_px);
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
    float        radius,       // size of blur as fraction of input width
    float        epsilon)      // tell edges from noise
{
  // const dt_connector_t *conn_input = graph->node[nodeid_input].connector + connid_input;
  const uint32_t wd = roi->wd;
  const uint32_t ht = roi->ht;
  const uint32_t dp = 1;
  const float radius_px = radius * wd;
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
  const int id_blur1 = dt_api_blur(graph, module, id_guided1, 1, 0, 0, radius_px);

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
  const int id_blur = dt_api_blur(graph, module, id_guided2, 1, 0, 0, radius_px);

  // final kernel:
  // output = mean_a * I + mean_b
  assert(graph->num_nodes < graph->max_nodes);
  const int id_guided3 = graph->num_nodes++;
  dt_node_t *node_guided3 = graph->node + id_guided3;
  ci.chan = dt_token("rgba");
  co.chan = dt_token("y");
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

#endif // not defined cplusplus

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
    return (const float *)(module->param + module->so->param[paramid]->offset);
  return 0;
}

static inline const int32_t *dt_module_param_int(
    const dt_module_t *module,
    int paramid)
{
  if(paramid >= 0 && paramid < module->so->num_params)
    return (const int32_t *)(module->param + module->so->param[paramid]->offset);
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

static inline int dt_module_set_param_float(
    const dt_module_t *module,
    dt_token_t         param,
    const float        val)
{
  for(int p=0;p<module->so->num_params;p++)
  {
    if(module->so->param[p]->name == param)
    {
      ((float *)(module->param + module->so->param[p]->offset))[0] = val;
      return 0;
    }
  }
  return 1;
}

static inline int dt_module_get_param(dt_module_so_t *so, dt_token_t param)
{
  for(int i=0;i<so->num_params;i++)
    if(so->param[i]->name == param) return i;
  return -1;
}

static inline int dt_module_set_param_float_n(
    const dt_module_t *module,
    dt_token_t         param,
    const float       *val,
    int                cnt)
{
  for(int p=0;p<module->so->num_params;p++)
  {
    if(module->so->param[p]->name == param)
    {
      for(int i=0;i<cnt;i++)
        ((float *)(module->param + module->so->param[p]->offset))[i] = val[i];
      return 0;
    }
  }
  return 1;
}

// returns a pointer to the metadata parameters of the raw image
// coming to our module from the given input connector name.
// if you don't mess with the metadata (such as replacing black point
// after subtraction), this will yield equivalent data to mod->img_param.
static inline const dt_image_params_t*
dt_module_get_input_img_param(
    dt_graph_t  *graph,
    dt_module_t *module,
    dt_token_t   input)
{
  for(int c=0;c<module->num_connectors;c++)
  {
    if(module->connector[c].name == input)
    {
      int mid = module->connector[c].connected_mi;
      if(mid < 0) return 0;
      return &graph->module[mid].img_param;
    }
  }
  return 0;
}

// open a file pointer, consider search paths specific to this graph
static inline FILE*
dt_graph_open_resource(
    dt_graph_t *graph,
    const char *fname,
    const char *mode)
{
  if(fname[0] == '/')
    return fopen(fname, mode); // absolute path
  char filename[1024]; // for relative paths, add search path
  if(graph)
  {
    snprintf(filename, sizeof(filename), "%s/%s", graph->searchpath, fname);
    FILE *f = fopen(filename, mode);
    if(f) return f;
    // if we can't open it in the graph specific search path, try the global one:
    snprintf(filename, sizeof(filename), "%s/%s", graph->basedir, fname);
    return fopen(filename, mode);
  }
  return 0;
}
