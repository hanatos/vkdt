#pragma once
#include "pipe/modules/bs.h"
#include "pipe/node.h"
#include "pipe/graph.h"
#include "pipe/module.h"
#include "pipe/modules/localsize.h"
#include "pipe/res.h"

#include <math.h>
#include <stdarg.h>

#ifdef VKDT_DSO_BUILD
int bs_init()
{ // need to load symbols from the dso side
  int ret = dt_module_bs_init();
  if(ret) fprintf(stderr, "bs init failed!\n");
  return ret;
}
#endif

#define dt_no_roi UINT64_C(-1)

// some module specific helpers and constants

typedef enum
{
  s_graph_run_none           = 0,
  s_graph_run_roi            = 1<<0, // pass 1: recompute regions of interest
  s_graph_run_create_nodes   = 1<<1, // pass 2: create nodes from modules
  s_graph_run_alloc          = 1<<2, // pass 3: alloc and free images
  s_graph_run_record_cmd_buf = 1<<3, // pass 4: record command buffer (uploads uniforms/params)
  s_graph_run_upload_source  = 1<<4, // final : upload new source image
  s_graph_run_download_sink  = 1<<5, // final : download sink images
  s_graph_run_wait_done      = 1<<6, // wait for fence
  s_graph_run_before_active  = 1<<7, // run all modules, even before active_module
  s_graph_run_all = -1u,
} dt_graph_run_constants_t;

// convenience function to detect inputs
static inline int
dt_connector_input(const dt_connector_t *c)
{
  return c->type == dt_token("read") || c->type == dt_token("sink");
}
static inline int
dt_connector_output(const dt_connector_t *c)
{
  return c->type == dt_token("write") || c->type == dt_token("source");
}
static inline int
dt_connector_ssbo(const dt_connector_t *c)
{
  return c->chan == dt_token("ssbo");
}
static inline int
dt_node_source(const dt_node_t *n)
{
  return n->connector[0].type == dt_token("source");
}
static inline int
dt_node_sink(const dt_node_t *n)
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
    c1->frames = MAX(c1->frames,c0->frames); // keep feedback connectors
    c1->flags  = c0->flags;
    c1->format = c0->format; // modules may be rigged from the outside, for export etc
    if(!(c1->type == dt_token("write") && c1->roi.scale > 0 && c0->roi.scale == -1))
      c1->roi = c0->roi; // don't overwrite node roi if writing node is inited and module is not
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

#ifndef __cplusplus
static inline void
dt_connector_copy_feedback(
    dt_graph_t  *graph,
    dt_module_t *module,
    int mc,    // connector id on module to copy
    int nid,   // node id
    int nc)    // connector id on node to copy to
{
  dt_connector_copy(graph, module, mc, nid, nc);
  graph->node[nid].connector[nc].flags |= s_conn_feedback;
  graph->node[nid].connector[nc].frames = 2;
  module->connector[mc].frames = 2;
}
#endif

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

// convenience function to add a new node to the graph.
// will return nodeid or -1 on failure
static inline int
dt_node_add(
    dt_graph_t  *graph,
    dt_module_t *module,
    const char  *name,    // module name, i.e. directory of shader
    const char  *kernel,  // file name of compute shader
    const int    wd,      // dimensions of the shader execution (will be divided by DT_LOCAL_SIZE_[XY] for workgroup numbers
    const int    ht,
    const int    dp,
    const int    pc_size, // byte size of push constants
    const int   *pc,      // push constants, cast to int*
    const int    nc,      // number of connectors
    ...)                  // info about the connectors: 4x char*:(name, type, chan, format) and one dt_roi_t*
{
  if(graph->num_nodes >= graph->max_nodes) return -1;
  const int id = graph->num_nodes++;
  graph->node[id] = (dt_node_t) {
    .name   = dt_token(name),
    .kernel = dt_token(kernel),
    .module = module,
    .num_connectors = nc,
    .flags  = module->flags,    // propagate sink/source copy requests
    .wd     = (uint32_t)wd,
    .ht     = (uint32_t)ht,
    .dp     = (uint32_t)dp,
    .push_constant_size = (size_t)pc_size,
  };
  if(pc) memcpy(graph->node[id].push_constant, pc, pc_size);
  va_list args;
  va_start(args, nc);
  for(int c=0;c<nc;c++)
  {
    const char *tmp0 = va_arg(args, const char*), *tmp1 = va_arg(args, const char*);
    const char *tmp2 = va_arg(args, const char*), *tmp3 = va_arg(args, const char*);
    dt_roi_t *roi = va_arg(args, dt_roi_t*);
    graph->node[id].connector[c].name   = dt_token(tmp0);
    graph->node[id].connector[c].type   = dt_token(tmp1);
    graph->node[id].connector[c].chan   = dt_token(tmp2);
    graph->node[id].connector[c].format = dt_token(tmp3);
    if((uint64_t)roi != dt_no_roi) // not required for input connectors, they will take on that of the output. va_arg doesn't do null pointers though :(
      graph->node[id].connector[c].roi  = *roi;
    if(dt_connector_input(graph->node[id].connector+c))
      graph->node[id].connector[c].connected_mi = -1; // mark input as disconnected
  }
  va_end(args);
  return id;
}

// connect two nodes id0->id1 by named connectors
static inline int
dt_node_connect_named(
    dt_graph_t *graph,
    const int   id0,
    const char *c0,
    const int   id1,
    const char *c1)
{
  int c0i = -1, c1i = -1;
  const dt_token_t c0t = dt_token(c0), c1t = dt_token(c1);
  for(int c=0;c<graph->node[id0].num_connectors;c++)
    if(graph->node[id0].connector[c].name == c0t) { c0i = c; break; }
  for(int c=0;c<graph->node[id1].num_connectors;c++)
    if(graph->node[id1].connector[c].name == c1t) { c1i = c; break; }
  if(c0i < 0) return -100;
  if(c1i < 0) return -101;
  return dt_node_connect(graph, id0, c0i, id1, c1i);
}

static inline int
dt_node_feedback_named(
    dt_graph_t *graph,
    const int   id0,
    const char *c0,
    const int   id1,
    const char *c1)
{
  int c0i = -1, c1i = -1;
  const dt_token_t c0t = dt_token(c0), c1t = dt_token(c1);
  for(int c=0;c<graph->node[id0].num_connectors;c++)
    if(graph->node[id0].connector[c].name == c0t) { c0i = c; break; }
  for(int c=0;c<graph->node[id1].num_connectors;c++)
    if(graph->node[id1].connector[c].name == c1t) { c1i = c; break; }
  if(c0i < 0 || c1i < 0) return -100;
  return dt_node_feedback(graph, id0, c0i, id1, c1i);
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
dt_api_blur_3x3(
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
  const uint32_t *sigmai = (uint32_t*)&sigma;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_blur = graph->num_nodes++;
  graph->node[id_blur] = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("blurs"),
    .module = module,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = conn_input->chan,
      .format = conn_input->format,
      .flags  = s_conn_smooth,
      .connected_mi = -1,
      .roi    = conn_input->roi,
      .array_length = conn_input->array_length,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = conn_input->chan,
      .format = conn_input->format,
      .roi    = conn_input->roi,
      .array_length = conn_input->array_length,
    }},
    .num_connectors = 2,
    .wd     = conn_input->roi.wd,
    .ht     = conn_input->roi.ht,
    .dp     = dp,
    .push_constant = { sigmai[0] },
    .push_constant_size = sizeof(float),
  };
  if(id_blur_in)  *id_blur_in  = id_blur;
  if(id_blur_out) *id_blur_out = id_blur;
  if(nodeid_input >= 0)
    CONN(dt_node_connect(graph, nodeid_input, connid_input, id_blur, 0));
  else
    dt_connector_copy(graph, module, connid_input, id_blur, 0);
  return id_blur;
}

// approximate gaussian blur by slightly larger sigma.
// this has a 5x5 kernel support and does only sigma=1.
// use blur or blur_sub instead for larger blur radii.
static inline int
dt_api_blur_5x5(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,
    int          connid_input,
    int         *id_blur_in,
    int         *id_blur_out)
{
  // detect pixel format on input
  const dt_connector_t *conn_input = nodeid_input >= 0 ?
    graph->node[nodeid_input].connector + connid_input :
    module->connector + connid_input;
  const uint32_t dp = conn_input->array_length > 0 ? conn_input->array_length : 1;
  assert(graph->num_nodes < graph->max_nodes);
  const int id_blur = graph->num_nodes++;
  graph->node[id_blur] = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = dt_token("blur"),
    .module = module,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = conn_input->chan,
      .format = conn_input->format,
      .flags  = s_conn_smooth,
      .connected_mi = -1,
      .roi    = conn_input->roi,
      .array_length = conn_input->array_length,
    },{
      .name   = dt_token("output"),
      .type   = dt_token("write"),
      .chan   = conn_input->chan,
      .format = conn_input->format,
      .roi    = conn_input->roi,
      .array_length = conn_input->array_length,
    }},
    .num_connectors = 2,
    .wd     = conn_input->roi.wd,
    .ht     = conn_input->roi.ht,
    .dp     = dp,
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
    .flags  = s_conn_smooth,
    .connected_mi = -1,
    .roi    = conn_input->roi,
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
  // int id_up = -1; // for multi upsampling
  if(id_blur_in)  *id_blur_in  = -1;
  if(id_blur_out) *id_blur_out = -1;
  for(uint32_t i=0;i<levels;i++)
  {
    // int owd = roi.wd, oht = roi.ht; // for multi upsampling
    for(uint32_t j=0;j<mul[i];j++)
    {
      assert(graph->num_nodes < graph->max_nodes);
      const int id_blur = graph->num_nodes++;
      uint32_t hwd = j == mul[i]-1 ? (roi.wd + 1)/2 : roi.wd;
      uint32_t hht = j == mul[i]-1 ? (roi.ht + 1)/2 : roi.ht;
      graph->node[id_blur] = (dt_node_t) {
        .name   = dt_token("shared"),
        .kernel = dt_token("blur"),
        .module = module,
        .connector = { ci, co },
        .num_connectors = 2,
        .wd     = hwd,
        .ht     = hht,
        .dp     = dp,
      };
      if(id_blur_in && *id_blur_in < 0) *id_blur_in = id_blur;
      graph->node[id_blur].connector[0].roi = roi;
      roi.full_wd = hwd;
      roi.full_ht = hht;
      roi.wd = hwd;
      roi.ht = hht;
      graph->node[id_blur].connector[1].roi = roi;
      // interconnect nodes:
      CONN(dt_node_connect(graph, nid_input, cid_input, id_blur, 0));
      nid_input = id_blur;
      cid_input = 1;
    }
#if 0 // multiple in-between upsampling steps
    if(upsample)
    {
      assert(graph->num_nodes < graph->max_nodes);
      const int id_upsample = graph->num_nodes++;
      graph->node[id_upsample] = (dt_node_t) {
        .name   = dt_token("shared"),
          .kernel = dt_token("resample"),
          .module = module,
          .wd     = owd,
          .ht     = oht,
          .dp     = dp,
          .num_connectors = 2,
          .connector = { ci, co },
      };
      graph->node[id_upsample].connector[1].roi.wd = owd;
      graph->node[id_upsample].connector[1].roi.ht = oht;
      if(i == levels-1)
      {
        graph->node[id_upsample].connector[0].roi = graph->node[nid_input].connector[cid_input].roi;
        CONN(dt_node_connect(graph, nid_input, cid_input, id_upsample, 0));
      }
      if(i)
      {
        graph->node[id_up].connector[0].roi = graph->node[id_upsample].connector[1].roi;
        CONN(dt_node_connect(graph, id_upsample, 1, id_up, 0));
      }
      if(i == 0 && id_blur_out) *id_blur_out = id_upsample;
      id_up = id_upsample;
    }
#endif
  }
  if(!upsample)
    if(id_blur_out) *id_blur_out = nid_input;
#if 1 // single upsampling step at the end
  if(upsample)
  { // single upsampling kernel is enough, we're using quadric reconstruction
    assert(graph->num_nodes < graph->max_nodes);
    const int id_upsample = graph->num_nodes++;
    graph->node[id_upsample] = (dt_node_t) {
      .name   = dt_token("shared"),
        .kernel = dt_token("resample"),
        .module = module,
        .connector = { ci, co },
        .num_connectors = 2,
        .wd     = conn_input->roi.wd,
        .ht     = conn_input->roi.ht,
        .dp     = dp,
    };
    graph->node[id_upsample].connector[0].roi = graph->node[nid_input].connector[cid_input].roi;
    CONN(dt_node_connect(graph, nid_input, cid_input, id_upsample, 0));
    if(id_blur_out) *id_blur_out = id_upsample;
    nid_input = id_upsample;
  }
#endif
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
    float        radius)        // blur radius in pixels, = 3 sigma (will be rounded down to int)
{
  // detect pixel format on input and blur the whole thing in separable kernels
  const dt_connector_t *conn_input = nodeid_input >= 0 ?
    graph->node[nodeid_input].connector + connid_input :
    module->connector + connid_input;
  const uint32_t wd = conn_input->roi.wd;
  const uint32_t ht = conn_input->roi.ht;
  const uint32_t dp = conn_input->array_length > 0 ? conn_input->array_length : 1;
  uint32_t *irad = (uint32_t *)&radius;
  dt_token_t blurh = dt_token("blurh");
  dt_token_t blurv = dt_token("blurv");
  dt_connector_t ci = {
    .name   = dt_token("input"),
    .type   = dt_token("read"),
    .chan   = conn_input->chan,
    .format = conn_input->format,
    .connected_mi = -1,
    .roi    = conn_input->roi,
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
    .connector = { ci, co },
    .num_connectors = 2,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .push_constant = {irad[0]},
    .push_constant_size = sizeof(float),
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_blurv = graph->num_nodes++;
  graph->node[id_blurv] = (dt_node_t) {
    .name   = dt_token("shared"),
    .kernel = blurv,
    .module = module,
    .connector = { ci, co },
    .num_connectors = 2,
    .wd     = wd,
    .ht     = ht,
    .dp     = dp,
    .push_constant = {irad[0]},
    .push_constant_size = sizeof(float),
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
  if(radius < 2)
    return dt_api_blur_3x3(graph, module, nodeid_input, connid_input,
        id_blur_in, id_blur_out, 0.5f*radius);
  if(radius == 2)
    return dt_api_blur_5x5(graph, module, nodeid_input, connid_input, id_blur_in, id_blur_out);
  // XXX DEBUG XXX frustratingly that is much faster even for relatively large blurs it seems
  // TODO: probably go straight here for smaller radii
    // return dt_api_blur_sep(graph, module, nodeid_input, connid_input,
     //    id_blur_in, id_blur_out, radius);
  // let's separate the blur in several passes.
  // we'll do as many steps of subsampled blur as we can, because
  // these are fast.
  // subsampling does 5x5 blurs, i.e. radius=2 on powers of two
  // i.e. 5x5, 9x9, 17x17, .. (radii 2 4 8 ..)
  radius = CLAMP(radius, 1, graph->node[nodeid_input].connector[connid_input].roi.wd/2);
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
  // also avoid highres resampling
  for(int j=it;j>=1;j--) // XXX this is only faster if the highest res level is avoided here
  {
    float sig = 1<<j;
    for(int k=0;k<10;k++)
    {
      if(sig2 + sig*sig < sig2_req)
      { // record iteration j as multiple
        sig2 += sig*sig;
        mul[j]++;
      }
      else break;
    }
  }
  // remaining sigma
  float sig_rem = sqrtf(MAX(0, sig2_req - sig2));
  // fprintf(stderr, "radius: %g levels: %d remaining sigma %g\n", radius, it, sig_rem);
  // fprintf(stderr, "multiplicity: %d %d %d %d %d %d %d %d\n",
  //     mul[0], mul[1], mul[2], mul[3],
  //     mul[4], mul[5], mul[6], mul[7]);

  if(it && (sig_rem == 0 || it >= 2))
  { // for large blurs you'll not notice a little underblur anyways, leave the small kernel.
    return dt_api_blur_sub(graph, module, nodeid_input, connid_input,
        id_blur_in, id_blur_out, it, mul, 1);
  }
  else if(it)
  { // use both to get more precise correspondence
    int id_blur_small_in = -1, id_blur_small_out = -1;
    dt_api_blur_sep(graph, module, nodeid_input, connid_input,
        &id_blur_small_in, &id_blur_small_out, 3.0f*sig_rem);
    return dt_api_blur_sub(graph, module, id_blur_small_out, 1,
        id_blur_in, id_blur_out, it, mul, 1);
  }
  else
  {
    return dt_api_blur_3x3(graph, module, nodeid_input, connid_input,
        id_blur_in, id_blur_out, 0.5f*radius);
  }
}

// full guided filter:
// input image      I (rgb) -> entry node [0]
// input image      I (rgb) -> exit  node [0]
// input edge image p (rgb) -> entry node [1]
// output filtered    (rgb) <- exit  node [2]
// as blurs do, this will connect the input node (or module if node==-1) as needed.
// will return the output node id, the output connector is number 2, name is "output"
static inline int
dt_api_guided_filter_full(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input, // pass -1 if the connector only exists on the module so far
    int          connid_input, // connector id to grab input and connect to all appropriate places
    int          nodeid_guide,
    int          connid_guide,
    int         *id_in,        // can be 0, or will be set to the input node
    int         *id_out,       // can be 0, or well be set to the output node
    int         *id_pc,        // can be 0, or well be set to the node that has push constants
    const float *params,       // {radius (fraction of input wd), epsilon}, persist with cmd, will be pc
    int          grey)         // set to 1 if the input and output buffers are single-channel
{
  // detect pixel format on input
  const dt_connector_t *conn_input = nodeid_input >= 0 ?
    graph->node[nodeid_input].connector + connid_input :
    module->connector + connid_input;
  const dt_roi_t *roi = &conn_input->roi;
  const uint32_t wd = roi->wd;
  const uint32_t ht = roi->ht;
  const uint32_t dp = 1;
  const float radius_px = params[0] * wd;
  dt_token_t format = conn_input->format;

  // compute (I,p,I*I,I*p)
  const int id_guided1 = dt_node_add(graph, module, "shared", "guided1f", wd, ht, dp,
      sizeof(int), &grey, 3,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "guide",  "read",  "*",    "*",   dt_no_roi,
      "output", "write", "rgba", dt_token_str(format), roi);
  if(id_in) *id_in = id_guided1;

  // mean_I  = blur(I)
  // mean_p  = blur(p)
  // corr_I  = blur(I*I)
  // corr_Ip = blur(I*p)
  const int id_blur1 = dt_api_blur(graph, module, id_guided1, 2, 0, 0, radius_px);

  // var_I   = corr_I - mean_I*mean_I
  // cov_Ip  = corr_Ip - mean_I*mean_p
  // a = cov_Ip / (var_I + epsilon)
  // b = mean_p - a * mean_I
  const int id_guided2 = dt_node_add(graph, module, "shared", "guided2f", wd, ht, dp,
      sizeof(float)*2, (const int*)params, 2,
      "input", "read",  "*",  "*",   dt_no_roi,
      "ab",    "write", "rg", dt_token_str(format), roi);
  CONN(dt_node_connect_named(graph, id_blur1, "output", id_guided2, "input"));
  if(id_pc) *id_pc = id_guided2;

  // this is the same as in the p=I case below:
  // mean_a = blur(a)
  // mean_b = blur(b)
  const int id_blur = dt_api_blur(graph, module, id_guided2, 1, 0, 0, radius_px);
  // final kernel:
  // output = mean_a * I + mean_b
  const int id_guided3 = dt_node_add(graph, module, "shared", "guided3", wd, ht, dp,
      sizeof(int), &grey, 3,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "ab",     "read",  "*",    "*",   dt_no_roi,
      "output", "write", grey ? "r" : "rgba", dt_token_str(format), roi);
  CONN(dt_node_connect_named(graph, id_blur, "output", id_guided3, "ab"));
  if(id_out) *id_out = id_guided3;
  if(nodeid_guide >= 0)
    CONN(dt_node_connect(graph, nodeid_guide, connid_guide, id_guided1, 1));
  else
    dt_connector_copy(graph, module, connid_guide, id_guided1, 1);
  if(nodeid_input >= 0)
  {
    CONN(dt_node_connect(graph, nodeid_input, connid_input, id_guided1, 0));
    CONN(dt_node_connect(graph, nodeid_input, connid_input, id_guided3, 0));
  }
  else
  {
    dt_connector_copy(graph, module, connid_input, id_guided1, 0);
    dt_connector_copy(graph, module, connid_input, id_guided3, 0);
  }
  return id_guided3;
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
    int         *id_pc,        // which needs updated push constants
    const float *params)       // {radius (fraction of input wd), epsilon}, persist with cmd, will be pc
{
  // const dt_connector_t *conn_input = graph->node[nodeid_input].connector + connid_input;
  const uint32_t wd = roi->wd;
  const uint32_t ht = roi->ht;
  const float radius_px = params[0] * wd;

  // compute grey scale image rg32 with (I, I*I)
  const int id_guided1 = dt_node_add(graph, module, "shared", "guided1", wd, ht, 1, 0, 0, 2,
      "input",  "read",  "*",  "*",   dt_no_roi,
      "output", "write", "rg", "f16", roi);
  if(entry_nodeid) *entry_nodeid = id_guided1;

  // then connect 1x blur:
  // mean_I = blur(I)
  // corr_I = blur(I*I)
  const int id_blur1 = dt_api_blur(graph, module, id_guided1, 1, 0, 0, radius_px);

  // connect to this node:
  // a = var_I / (var_I + eps)
  // with var_I = corr_I - mean_I * mean_I
  // b = mean_I - a * mean_I
  const int id_guided2 = dt_node_add(graph, module, "shared", "guided2", wd, ht, 1,
      sizeof(float)*2, (const int*)params, 2,
      "input",  "read",  "*",  "*",   dt_no_roi,
      "output", "write", "rg", "f16", roi);
  if(id_pc) *id_pc = id_guided2;

  CONN(dt_node_connect(graph, id_blur1, 1, id_guided2, 0));

  // and blur once more:
  // mean_a = blur(a)
  // mean_b = blur(b)
  const int id_blur = dt_api_blur(graph, module, id_guided2, 1, 0, 0, radius_px);

  // final kernel:
  // output = mean_a * I + mean_b
  int pc = 0;
  const int id_guided3 = dt_node_add(graph, module, "shared", "guided3", wd, ht, 1,
      sizeof(int), &pc, 3,
      "input",  "read",  "*",     "*",  dt_no_roi,
      "ab",     "read",  "*",     "*",  dt_no_roi,
      "output", "write", "rgba", "f16", roi);
  CONN(dt_node_connect(graph, id_blur, 1, id_guided3, 1));
  if(exit_nodeid) *exit_nodeid = id_guided3;
}

// return modid or -1
static inline int
dt_module_get(
    const dt_graph_t *graph,
    dt_token_t name,
    dt_token_t inst)
{
  // TODO: some smarter way? hash table? benchmark how much we lose
  // through this stupid O(n) approach. after all the comparisons are
  // fast and we don't have all that many modules.
  for(uint32_t i=0;i<graph->num_modules;i++)
    if(graph->module[i].name == name &&
       graph->module[i].inst == inst)
      return i;
  return -1;
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

#ifndef __cplusplus
// radix sort an array of integers.
// this will output an offset table to be used by the following nodes.
// input connector id on id_radix_in will be 0, output connector id on id_radix_out will be 3.
static inline int
dt_api_radix_sort(
    dt_graph_t  *graph,
    dt_module_t *module,
    int          nodeid_input,  // node and connector of an ssbo/ui32 list of elements
    int          connid_input,
    int         *id_radix_in,   // if !0, will be filled with input and output node ids of this operation
    int         *id_radix_out,
    uint32_t     stride,        // how many uints form one element in the input array? first one will be sorting key
    uint32_t     max_count)     // this determines the number of iterations/bits we need
{
  int s = (32 - __builtin_clz(max_count))/4;
  uint32_t count = max_count;
  int id_in = nodeid_input;
  int cn_in = connid_input;
  for(int l=0;l<s;l++)
  { // for a max of 8 x 4 bits, radix sort iteratively:
    int ncnt = (count + 31)/32;
    assert(graph->num_nodes < graph->max_nodes);
    const int id_count = graph->num_nodes++;
    graph->node[id_count] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("count"),
      .module = module,
      .wd     = (count + 31)/32 * DT_LOCAL_SIZE_X,
      .ht     = 1,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("ssbo"),
        .format = dt_token("ui32"),
        .roi    = { .wd = count, .ht = stride, .full_wd = count, .full_ht = stride},
        .connected_mi = -1,
      },{
        .name   = dt_token("off0"),
        .type   = dt_token("write"),
        .chan   = dt_token("ssbo"),
        .format = dt_token("ui32"),
        .roi    = { .wd = ncnt, .ht = 16, .full_wd = ncnt, .full_ht = 16},
      },{
        .name   = dt_token("off1"),
        .type   = dt_token("write"),
        .chan   = dt_token("ssbo"),
        .format = dt_token("ui32"),
        .roi    = { .wd = count, .ht = 1, .full_wd = count, .full_ht = 1},
      }},
      .push_constant_size = 3*sizeof(uint32_t),
      .push_constant = { count, l, stride },
    };
    CONN(dt_node_connect(graph, id_in, cn_in, id_count, 0));
    if(!l && id_radix_in) *id_radix_in = id_count;

    int id_scan_in = id_count;
    int cn_scan_in = 1;
    int id_add_out = id_count;
    int cn_add_out = 1;
    int id_add_in  = -1;

    while(ncnt > 32)
    { // create a prefix sum
      int cnt = ncnt;
      ncnt = (cnt+31)/32;
      assert(graph->num_nodes < graph->max_nodes);
      const int id_scan = graph->num_nodes++;
      graph->node[id_scan] = (dt_node_t) {
        .name   = dt_token("shared"),
        .kernel = dt_token("scan"),
        .module = module,
        .wd     = (cnt + 31)/32 * DT_LOCAL_SIZE_X,
        .ht     = 1,
        .dp     = 1,
        .num_connectors = 2,
        .connector = {{
          .name   = dt_token("input"),
          .type   = dt_token("read"),
          .chan   = dt_token("ssbo"),
          .format = dt_token("ui32"),
          .roi    = { .wd = cnt, .ht = 16, .full_wd = cnt, .full_ht = 16},
          .connected_mi = -1,
        },{
          .name   = dt_token("output"),
          .type   = dt_token("write"),
          .chan   = dt_token("ssbo"),
          .format = dt_token("ui32"),
          .roi    = { .wd = ncnt, .ht = 16, .full_wd = ncnt, .full_ht = 16},
        }},
        .push_constant_size = sizeof(uint32_t),
        .push_constant = { cnt },
      };
      assert(graph->num_nodes < graph->max_nodes);
      const int id_add = graph->num_nodes++;
      graph->node[id_add] = (dt_node_t) {
        .name   = dt_token("shared"),
        .kernel = dt_token("add"),
        .module = module,
        .wd     = (cnt + 31)/32 * DT_LOCAL_SIZE_X,
        .ht     = 1,
        .dp     = 1,
        .num_connectors = 2,
        .connector = {{
          .name   = dt_token("input"),
          .type   = dt_token("read"),
          .chan   = dt_token("ssbo"),
          .format = dt_token("ui32"),
          .roi    = { .wd = ncnt, .ht = 16, .full_wd = ncnt, .full_ht = 16},
          .connected_mi = -1,
        },{
          .name   = dt_token("output"),
          .type   = dt_token("write"),
          .chan   = dt_token("ssbo"),
          .format = dt_token("ui32"),
          .roi    = { .wd = cnt, .ht = 16, .full_wd = cnt, .full_ht = 16},
        }},
        .push_constant_size = sizeof(uint32_t),
        .push_constant = { cnt },
      };
      // deal with output of our newly created adder. it either goes to the next coarser (previous) level:
      if(id_add_in >= 0)
        CONN(dt_node_connect(graph, id_add, 1, id_add_in, 0));
      else
      { // or, first iteration, it is remembered as output of this scan cascade
        id_add_out = id_add;
        cn_add_out = 1;
      }
      id_add_in = id_add;
      CONN(dt_node_connect(graph, id_scan_in, cn_scan_in, id_scan, 0));
      if(ncnt <= 32) // last iteration, connect scan and add cascades:
        CONN(dt_node_connect(graph, id_scan, 1, id_add, 0));
      id_scan_in = id_scan; cn_scan_in = 1;
    }
    ncnt = (count + 31)/32;
    assert(graph->num_nodes < graph->max_nodes);
    const int id_scatter = graph->num_nodes++;
    graph->node[id_scatter] = (dt_node_t) {
      .name   = dt_token("shared"),
      .kernel = dt_token("scatter"),
      .module = module,
      .wd     = (count + 31)/32 * DT_LOCAL_SIZE_X,
      .ht     = 1,
      .dp     = 1,
      .num_connectors = 4,
      .connector = {{
        .name   = dt_token("input"), // input element array
        .type   = dt_token("read"),
        .chan   = dt_token("ssbo"),
        .format = dt_token("ui32"),
        .roi    = { .wd = count, .ht = 1, .full_wd = count, .full_ht = 1},
        .connected_mi = -1,
      },{
        .name   = dt_token("off0"),  // coarse offsets per radix
        .type   = dt_token("read"),
        .chan   = dt_token("ssbo"),
        .format = dt_token("ui32"),
        .roi    = { .wd = ncnt, .ht = 16, .full_wd = ncnt, .full_ht = 16},
        .connected_mi = -1,
      },{
        .name   = dt_token("off1"),  // fine offsets for each element
        .type   = dt_token("read"),
        .chan   = dt_token("ssbo"),
        .format = dt_token("ui32"),
        .roi    = { .wd = count, .ht = 1, .full_wd = count, .full_ht = 1},
        .connected_mi = -1,
      },{
        .name   = dt_token("output"), // sorted output will be written here
        .type   = dt_token("write"),
        .chan   = dt_token("ssbo"),
        .format = dt_token("ui32"),
        .roi    = { .wd = count, .ht = stride, .full_wd = count, .full_ht = stride},
      }},
      .push_constant_size = 3*sizeof(uint32_t),
      .push_constant = { count, l, stride },
    };
    CONN(dt_node_connect(graph, id_in, cn_in, id_scatter, 0)); // input elements
    CONN(dt_node_connect(graph, id_add_out, cn_add_out, id_scatter, 1)); // coarse offsets
    CONN(dt_node_connect(graph, id_count, 2, id_scatter, 2)); // fine offsets
    id_in = id_scatter; // to be reconnected to next iteration
    cn_in = 3;
  }
  if(id_radix_out) *id_radix_out = id_in;
  return id_in;
}
#endif
