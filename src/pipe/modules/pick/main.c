#include "modules/api.h"
#include <math.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *r = &module->connector[0].roi;
  r->wd = r->full_wd;
  r->ht = r->full_ht;
  r->scale = 1.0f;
  module->connector[3].roi.wd = module->connector[3].roi.full_wd;
  module->connector[3].roi.ht = module->connector[3].roi.full_ht;
  module->connector[2].roi.wd = module->connector[2].roi.full_wd;
  module->connector[2].roi.ht = module->connector[2].roi.full_ht;
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  // always request constant size for picked colours:
  module->connector[3].roi.full_wd = 24; // max 24 areas picked with mean rgb
  module->connector[3].roi.full_ht = 4;  // and variances etc in different lines
  // rasterise spectra at this resolution
  module->connector[2].roi.full_wd = 700;
  module->connector[2].roi.full_ht = 350;
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  // one node to collect, and one to read back the sink
  assert(graph->num_nodes < graph->max_nodes);
  const int id_collect = graph->num_nodes++;
  graph->node[id_collect] = (dt_node_t) {
    .name   = dt_token("pick"),
    .kernel = graph->float_atomics_supported ? dt_token("collect") : dt_token("coldumb"),
    .module = module,
    .wd     = module->connector[0].roi.wd,
    .ht     = module->connector[0].roi.ht,
    .dp     = 1,
    .num_connectors = 2,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("read"),
      .chan   = dt_token("*"),
      .format = dt_token("*"),
      .roi    = module->connector[0].roi,
      .connected_mi = -1,
    },{
      .name   = dt_token("picked"),
      .type   = dt_token("write"),
      .chan   = dt_token("r"),
      .format = dt_token("atom"),
      .roi    = module->connector[3].roi,
      .flags  = s_conn_clear,
    }},
  };
  assert(graph->num_nodes < graph->max_nodes);
  const int id_map = graph->num_nodes++;
  graph->node[id_map] = (dt_node_t) {
    .name   = dt_token("pick"),
    .kernel = dt_token("sink"),
    .module = module,
    .wd     = module->connector[3].roi.wd,
    .ht     = module->connector[3].roi.ht,
    .dp     = 1,
    .num_connectors = 1,
    .connector = {{
      .name   = dt_token("input"),
      .type   = dt_token("sink"),
      .chan   = dt_token("r"),
      .format = dt_token("atom"),
      .roi    = module->connector[3].roi,
      .connected_mi = -1,
    }},
  };

  // interconnect nodes:
  dt_connector_copy(graph, module, 0, id_collect, 0);
  dt_connector_copy(graph, module, 3, id_collect, 1);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);

  // now detect whether we have an input spectra lut connected
  // or not. if not, we'll just not connect the display node
  // and let outside connections to the dspy channel fail as well:
  if(module->connector[1].connected_mi >= 0 &&
     module->connector[1].connected_mc >= 0)
  {
    assert(graph->num_nodes < graph->max_nodes);
    const int id_dspy = graph->num_nodes++;
    graph->node[id_dspy] = (dt_node_t) {
      .name   = dt_token("pick"),
      .kernel = dt_token("display"),
      .module = module,
      .wd     = module->connector[2].roi.wd,
      .ht     = module->connector[2].roi.ht,
      .dp     = 1,
      .num_connectors = 3,
      .connector = {{
        .name   = dt_token("input"),
        .type   = dt_token("read"),
        .chan   = dt_token("r"),
        .format = dt_token("atom"),
        .roi    = module->connector[3].roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("lut"),
        .type   = dt_token("read"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = module->connector[1].roi,
        .connected_mi = -1,
      },{
        .name   = dt_token("dspy"),
        .type   = dt_token("write"),
        .chan   = dt_token("rgba"),
        .format = dt_token("f16"),
        .roi    = module->connector[2].roi,
      }},
    };
    dt_node_connect  (graph, id_collect, 1, id_dspy, 0);
    dt_connector_copy(graph, module, 1, id_dspy, 1);
    dt_connector_copy(graph, module, 2, id_dspy, 2);
    module->connector[2].name = dt_token("dspy");
  }
  else
  {
    dt_connector_bypass(graph, module, 0, 2);     // just pass on input buffer as a dummy
    module->connector[2].name = dt_token("dsp_"); // disable temp display output
  }

  graph->node[id_collect].connector[1].flags = s_conn_clear; // restore after connect
  if(dt_module_param_int(module, dt_module_get_param(module->so, dt_token("grab")))[0] == 1)
    module->flags |= s_module_request_write_sink;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    void        *oldval)
{
  if((parid >= 0 && parid < module->so->num_params) &&
      module->so->param[parid]->name == dt_token("grab"))
  {
    int grab = dt_module_param_int(module, parid)[0];
    if(grab == 1) // live grabbing
      module->flags |= s_module_request_write_sink;
    else
      module->flags = 0;
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}


static inline void
_rec2020_to_lab(
    float *rgb,
    float *Lab)
{
  const float rec2020_to_xyz[] = {
    6.36958048e-01, 2.62700212e-01, 4.20575872e-11,
    1.44616904e-01, 6.77998072e-01, 2.80726931e-02,
    1.68880975e-01, 5.93017165e-02, 1.06098506e+00};
  float xyz[3] = {0.0f};
  for(int j=0;j<3;j++)
    for(int i=0;i<3;i++)
      xyz[j] += rec2020_to_xyz[3*j+i] * rgb[i];

  const float epsilon = 216.0f / 24389.0f;
  const float kappa = 24389.0f / 27.0f;
#define labf(x) ((x > epsilon) ? cbrtf(x) : (kappa * x + 16.0f) / 116.0f)
  float f[3] = { 0.0f };
  for(int i = 0; i < 3; i++) f[i] = labf(xyz[i]);
  Lab[0] = 116.0f * f[1] - 16.0f;
  Lab[1] = 500.0f * (f[0] - f[1]);
  Lab[2] = 200.0f * (f[1] - f[2]);
#undef labf
}

static inline float
_cie_de76(
    float *rgb0,
    float *rgb1)
{
  float Lab0[3], Lab1[3];
  _rec2020_to_lab(rgb0, Lab0);
  _rec2020_to_lab(rgb1, Lab1);
  return sqrtf(
      (Lab0[0] - Lab1[0])*(Lab0[0] - Lab1[0])+
      (Lab0[1] - Lab1[1])*(Lab0[1] - Lab1[1])+
      (Lab0[2] - Lab1[2])*(Lab0[2] - Lab1[2]));
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  float *f32 = buf;
  const int wd = module->connector[3].roi.wd;
  const int ht = module->connector[3].roi.ht; // we only read the first moment

  // now read back what we averaged and write into param "picked"
  float *picked = 0, *ref = 0, *de76 = 0;
  int cnt = 0, show = 0;
  for(int p=0;p<module->so->num_params;p++)
  {
    if(module->so->param[p]->name == dt_token("nspots"))
      cnt = dt_module_param_int(module, p)[0];
    if(module->so->param[p]->name == dt_token("picked"))
      picked = (float *)(module->param + module->so->param[p]->offset);
    if(module->so->param[p]->name == dt_token("ref"))
      ref = (float *)(module->param + module->so->param[p]->offset);
    if(module->so->param[p]->name == dt_token("de76"))
      de76 = (float *)(module->param + module->so->param[p]->offset);
    if(module->so->param[p]->name == dt_token("show"))
      show = dt_module_param_int(module, p)[0];
  }
  if(!picked || !ref) return;
  if(cnt > 24) cnt = 24; // sanitize, we don't have more memory than this
  if(cnt > wd) cnt = wd;
  if(ht < 4) return;
  de76[1] = 1e38f; de76[0] = 0.0f, de76[2] = 0.0f;
  // int mi = -1, Mi = -1;
  for(int k=0;k<cnt;k++)
  {
    picked[3*k+0] = f32[k+0*wd];
    picked[3*k+1] = f32[k+1*wd];
    picked[3*k+2] = f32[k+2*wd];
    if(show == 2)
    {
      const float d = _cie_de76(picked+3*k, ref+3*k);
      picked[3*k+0] -= ref[3*k+0];
      picked[3*k+1] -= ref[3*k+1];
      picked[3*k+2] -= ref[3*k+2];
      if(d < de76[1])
      {
        // mi = k;
        de76[1] = d;
      }
      else if(d > de76[2])
      {
        // Mi = k;
        de76[2] = d;
      }
      de76[0] += d/cnt;
    }
    else if(show == 1)
    {
      picked[3*k+0] = ref[3*k+0];
      picked[3*k+1] = ref[3*k+1];
      picked[3*k+2] = ref[3*k+2];
    }
    // unfortunately this fixed point thing leads to a bit of numeric jitter.
    // let's hope the vulkan 1.2 extension with floating point atomics makes
    // it to amd at some point, too.
    // fprintf(stderr, "read %d: %g %g %g\n", k, picked[3*k+0], picked[3*k+1], picked[3*k+2]);
  }
  // fprintf(stderr, "[pick] colour difference %g (%d) - %g - %g (%d)\n", de76[0], mi, de76[1], de76[2], Mi);
}
