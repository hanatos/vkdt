#include "modules/api.h"
#include <math.h>
#include "graph-history.h"

void ui_callback(
    dt_module_t *mod,
    dt_token_t   param)
{ // button to create static counterpart has been clicked
  dt_graph_t *g = mod->graph;
  int modid = dt_module_add_with_history(g, dt_token("const"), mod->inst);
  if(modid < 0) return;
  // copy nspots and picked colour to const module set const mode to 1 (replicate pick)
  int parid_dst = dt_module_get_param(g->module[modid].so, dt_token("nspots"));
  int parid_src = dt_module_get_param(mod->so, dt_token("nspots"));
  int *p_ns_dst = (int *)dt_module_param_int(g->module+modid, parid_dst);
  const int *p_ns_src = dt_module_param_int(mod, parid_src);
  int cnt = p_ns_dst[0] = p_ns_src[0];
  dt_graph_history_append(g, modid, parid_dst, 0.0);

  parid_dst = dt_module_get_param(g->module[modid].so, dt_token("colour"));
  parid_src = dt_module_get_param(mod->so, dt_token("picked"));
  float *p_col_dst = (float *)dt_module_param_float(g->module+modid, parid_dst);
  const float *p_col_src = dt_module_param_float(mod, parid_src);
  for(int i=0;i<cnt;i++)
    for(int k=0;k<3;k++)
      p_col_dst[4*i+k] = p_col_src[3*i+k];

  parid_dst = dt_module_get_param(g->module[modid].so, dt_token("mode"));
  int *p_mode = (int *)dt_module_param_int(g->module+modid, parid_dst);
  p_mode[0] = 1;

  int mus = mod - g->module;
  for(int m=0;m<g->num_modules;m++) for(int c=0;c<g->module[m].num_connectors;c++)
  { // find connected modules and repoint them to constant module
    int mid = g->module[m].connector[c].connected.i;
    int cid = g->module[m].connector[c].connected.c;
    if(dt_connector_input(g->module[m].connector+c) &&
        mid == mus && cid == 3) // connected to picked:picked
      CONN(dt_module_connect_with_history(g, modid, 0, m, c));
  }
}

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  // don't alter the acutal input roi, we'll go with whatever comes
  module->connector[3].roi.wd = module->connector[3].roi.full_wd;
  module->connector[3].roi.ht = module->connector[3].roi.full_ht;
  module->connector[2].roi.wd = module->connector[2].roi.full_wd;
  module->connector[2].roi.ht = module->connector[2].roi.full_ht;
}

void modify_roi_out(
    dt_graph_t  *graph,
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
  const int id_collect = dt_node_add(graph, module, "pick",
      qvk.float_atomics_supported ? "collect" : "coldumb",
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 2,
      "input",  "read",  "*", "*",    dt_no_roi,
      "picked", "write", "r", "atom", &module->connector[3].roi);
  graph->node[id_collect].connector[1].flags = s_conn_clear;
  const int id_map = dt_node_add(graph, module, "pick", "sink",
      module->connector[3].roi.wd, module->connector[3].roi.ht, 1, 0, 0, 1,
      "input", "sink", "r", "atom", dt_no_roi);

  // interconnect nodes:
  dt_connector_copy(graph, module, 0, id_collect, 0);
  dt_connector_copy(graph, module, 3, id_collect, 1);
  dt_node_connect  (graph, id_collect, 1, id_map, 0);

  // now detect whether we have an input spectra lut connected
  // or not. if not, we'll just not connect the display node
  // and let outside connections to the dspy channel fail as well:
  if(dt_connected(module->connector+1))
  {
    const int id_dspy = dt_node_add(graph, module, "pick", "display",
        module->connector[2].roi.wd, module->connector[2].roi.ht, 1, 0, 0, 3,
        "input", "read",  "r",    "atom", dt_no_roi,
        "lut",   "read",  "*",    "*",    dt_no_roi,
        "dspy",  "write", "rgba", "f16",  &module->connector[2].roi);
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
    uint32_t     num,
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
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)
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
      //fprintf(stderr, "read %g %g %g ref %g %g %g\n", 
      //    picked[3*k+0], picked[3*k+1], picked[3*k+2],
      //    ref[3*k+0], ref[3*k+1], ref[3*k+2]);
      picked[3*k+0] = fabsf(picked[3*k+0] - ref[3*k+0]);
      picked[3*k+1] = fabsf(picked[3*k+1] - ref[3*k+1]);
      picked[3*k+2] = fabsf(picked[3*k+2] - ref[3*k+2]);
      if(d < de76[1])
      {
        // mi = k;
        de76[1] = d;
      }
      if(d > de76[2])
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
  // fprintf(stderr, "[pick] colour difference %g - %g - %g\n", de76[0], de76[1], de76[2]);
}
