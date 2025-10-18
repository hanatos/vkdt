#include "modules/api.h"
#include "pipe/graph-history.h"

// returns "edit space" from linear value, for more control in blacks for linear rgb
static inline float
edit_to_linear(float v)
{
  return v*v;
}
static inline float
linear_to_edit(float v)
{
  return sqrtf(v);
}

static const double throttle = 0.5; // throttling history in seconds

typedef struct moddata_t
{
  int id_guided_a;
  int id_guided_b;
}
moddata_t;

int init(dt_module_t *mod)
{
  moddata_t *dat = malloc(sizeof(*dat));
  mod->data = dat;
  memset(dat, 0, sizeof(*dat));
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  free(mod->data);
  mod->data = 0;
}


void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{ // request something square for dspy output
  module->connector[2].roi.full_wd = 1024;
  module->connector[2].roi.full_ht = 1024;
  module->connector[1].roi = module->connector[0].roi; // output
}

dt_graph_run_t
check_params(
    dt_module_t *mod,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int pid_channel = dt_module_get_param(mod->so, dt_token("channel"));
  const int pid_ychchan = dt_module_get_param(mod->so, dt_token("ych 3x3"));
  const int pid_mode    = dt_module_get_param(mod->so, dt_token("mode"));
  const int pid_radius  = dt_module_get_param(mod->so, dt_token("radius"));
  if(parid == pid_radius)
  { // radius
    float oldrad = *(float*)oldval;
    float newrad = dt_module_param_float(mod, pid_radius)[0];
    return dt_api_blur_check_params(oldrad, newrad);
  }
  if(parid == pid_ychchan)
  { // need to keep rgb channel in sync for gui stuff (hide deriv combo)
    int newv = dt_module_param_int(mod, pid_ychchan)[0];
    int *p = (int *)dt_module_param_int(mod, pid_channel);
    *p = newv;
  }
  if(parid == pid_mode)
  { // make sure we don't go out of range when switching back to rgb mode
    int newv = dt_module_param_int(mod, pid_mode)[0];
    int *p = (int *)dt_module_param_int(mod, pid_channel);
    int oldv = *(int*)oldval;
    if(newv != 2 && oldv == 2) *p = 0;
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void commit_params(dt_graph_t *graph, dt_module_t *mod)
{
  // let guided filter know we updated the push constants:
  moddata_t *dat = mod->data;
  const float *radius = dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("radius")));
  if(dat->id_guided_a >= 0 && dat->id_guided_a < graph->num_nodes &&
      graph->node[dat->id_guided_a].kernel == dt_token("guided2f"))
    memcpy(graph->node[dat->id_guided_a].push_constant, radius, sizeof(float)*2);
  if(dat->id_guided_b >= 0 && dat->id_guided_b < graph->num_nodes &&
      graph->node[dat->id_guided_b].kernel == dt_token("guided2f"))
    memcpy(graph->node[dat->id_guided_b].push_constant, radius, sizeof(float)*2);
  dt_token_t par[] = {
    dt_token("cntr"), dt_token("xr"), dt_token("yr"), dt_token("abcdr"), dt_token("ddr0"), dt_token("ddrn"),
    dt_token("cntg"), dt_token("xg"), dt_token("yg"), dt_token("abcdg"), dt_token("ddg0"), dt_token("ddgn"),
    dt_token("cntb"), dt_token("xb"), dt_token("yb"), dt_token("abcdb"), dt_token("ddb0"), dt_token("ddbn")};
  for(int channel=0;channel<3;channel++)
  {
    int *p_cnt = (int   *)dt_module_param_int  (mod, dt_module_get_param(mod->so, par[6*channel+0]));
    float *p_x = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, par[6*channel+1]));
    float *p_y = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, par[6*channel+2]));

    float ddx0 = dt_module_param_float(mod, dt_module_get_param(mod->so, par[6*channel+4]))[0];
    float ddxn = dt_module_param_float(mod, dt_module_get_param(mod->so, par[6*channel+5]))[0];

    int n = p_cnt[0]-1;
    if(p_cnt[0] == 2)
    { // automatic linear extension
      ddx0 = ddxn = (p_y[1] - p_y[0])/(p_x[1] - p_x[0]);
    }
    else
    { // guess ddx0 ddxn by quadratic equation
      ddx0=ddxn=-1;
      for(int k=0;k<2;k++)
      { // for both sides, left and right
        float x0 = k ? p_x[n-2] : p_x[0];
        float x1 = k ? p_x[n-1] : p_x[1];
        float x2 = k ? p_x[n-0] : p_x[2];
        float y0 = k ? p_y[n-2] : p_y[0];
        float y1 = k ? p_y[n-1] : p_y[1];
        float y2 = k ? p_y[n-0] : p_y[2];
        float x02=x0*x0, x12=x1*x1, x22 = x2*x2;
        float a = ((x1-x0)*y2+(x0-x2)*y1+(x2-x1)*y0)
          /((x1-x0)*x22+(x02-x12)*x2+x0*x12-x02*x1);
        float b = -((x12-x02)*y2+(x02-x22)*y1+(x22-x12)*y0)
          /((x1-x0)*x22+(x02-x12)*x2+x0*x12-x02*x1);
        if(k) ddxn = 2.0f*a*x2 + b;
        else  ddx0 = 2.0f*a*x0 + b;
        if( k && y2 > y1) ddxn = MAX(0, ddxn);
        if( k && y2 < y1) ddxn = MIN(0, ddxn);
        if(!k && y1 > y0) ddx0 = MAX(0, ddx0);
        if(!k && y1 < y0) ddx0 = MIN(0, ddx0);
      }
    }
    ((float*)dt_module_param_float(mod,
      dt_module_get_param(mod->so, par[6*channel+4])))[0] = ddx0;
    ((float*)dt_module_param_float(mod,
      dt_module_get_param(mod->so, par[6*channel+5])))[0] = ddxn;

    // cnt is 2..8
    float alpha[8], h[8], I[8], mu[8], z[8];
    float a[8], b[8], c[8], d[8];

    // clamped cubic spline
    for(int i=0;i<n;i++)
    {
      a[i] = p_y[i];
      h[i] = p_x[i+1] - p_x[i];
    }
    a[n] = p_y[n];
    alpha[0] = 3.0f*(a[1]-a[0])/h[0] - 3.0f*ddx0;
    alpha[n] = 3.0f*ddxn - 3.0f*(a[n]-a[n-1])/h[n-1];
    for(int i=1;i<n;i++)
      alpha[i] = 3.0f/h[i]*(a[i+1]-a[i]) - 3.0f/h[i-1]*(a[i]-a[i-1]);
    I[0]  = 2.0f*h[0];
    mu[0] = 0.5f;
    z[0] = alpha[0]/I[0];

    for(int i=1;i<n;i++)
    {
      I[i] = 2.0f*(p_x[i+1] - p_x[i-1]) - h[i-1]*mu[i-1];
      mu[i] = h[i]/I[i];
      z[i] = (alpha[i] - h[i-1]*z[i-1])/I[i];
    }
    I[n] = h[n-1]*(2.0f-mu[n-1]);
    z[n] = (alpha[n] - h[n-1]*z[n-1])/I[n];
    c[n] = z[n];
    for(int j=n-1;j>=0;j--)
    {
      c[j] = z[j] - mu[j]*c[j+1];
      b[j] = (a[j+1] - a[j])/h[j] - h[j]*(c[j+1]+2.0f*c[j])/3.0f;
      d[j] = (c[j+1]-c[j])/(3.0f*h[j]);
    }
    float *p_abcd = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, par[6*channel+3]));
    for(int i=0;i<n;i++)
    {
      p_abcd[4*i+0] = a[i];
      p_abcd[4*i+1] = b[i];
      p_abcd[4*i+2] = c[i];
      p_abcd[4*i+3] = d[i];
    }
  }

  dt_token_t tkn_abcd[] = {
    dt_token("abcd0"), dt_token("abcd1"), dt_token("abcd2"),
    dt_token("abcd3"), dt_token("abcd4"), dt_token("abcd5")};
  const int pid_v  = dt_module_get_param(mod->so, dt_token("vtx"));
  const float *p_v = dt_module_param_float(mod, pid_v);
  for(int channel=0;channel<6;channel++)
  { // now the six horizontal curves.
    int pid_abcd  = dt_module_get_param(mod->so, tkn_abcd[channel]);
    float *p_abcd = (float *)dt_module_param_float(mod, pid_abcd);
    int cnt = 0;
    for(;cnt<6;cnt++)
      if(p_v[6*channel+cnt] == -666.0f)
        break;
    for(int v=0;v<cnt;v++)
    { // init one segment per vertex pair (periodic for hues)
      const int periodic = 1;//channel == 3 || channel == 4;
      const int vp = periodic ? (v+cnt-1)%cnt : CLAMP(v-1, 0, cnt-1);
      const int vn = periodic ? (v    +1)%cnt : CLAMP(v+1, 0, cnt-1);
      const int vf = periodic ? (v    +2)%cnt : CLAMP(v+2, 0, cnt-1);
      float vp_x = p_v[6*channel+vp], vp_y = p_v[6*channel+vp+36];
      float vc_x = p_v[6*channel+v ], vc_y = p_v[6*channel+v +36];
      float vn_x = p_v[6*channel+vn], vn_y = p_v[6*channel+vn+36];
      float vf_x = p_v[6*channel+vf], vf_y = p_v[6*channel+vf+36];
      if(vp_x > vc_x) vp_x -= 1.0f; // modulo to ensure sort order of x pos
      if(vn_x < vc_x) vn_x += 1.0f;
      if(vf_x < vc_x) vf_x += 1.0f;
      // for these we forget about continuous curvature and the second derivative
      // altogether. instead, we fix the first derivatives to something a bit
      // smooth in the usual case (finite difference between adjacent vertices v_{i+1}-v_{i-1})
      // and flat for extreme values (f'=0 for peaks and dips).
      if(vn_x - vc_x < 0.001f)
      { // fallback for close vertices to avoid numerical issues
        p_abcd[4*v+0] = 0.0f;
        p_abcd[4*v+1] = 0.0f;
        p_abcd[4*v+2] = 0.0f;
        p_abcd[4*v+3] = (vn_y+vc_y)*0.5f;
      }
      else
      {
        float x = vc_x, y = vn_x;
        float A = vc_y, B = vn_y; // f(0), f(1)
        float d0 = (vc_y-vp_y)/(vc_x-vp_x), d1 = (vn_y-vc_y)/(vn_x-vc_x), d2 = (vf_y-vn_y)/(vf_x-vn_x);
        float C = 0.0f, D = 0.0f; // f'(0), f'(1)
        if(d0 > 0.0f && d1 > 0.0f) C = MIN(d0,d1);
        if(d0 < 0.0f && d1 < 0.0f) C = MAX(d0,d1);
        if(d1 > 0.0f && d2 > 0.0f) D = MIN(d1,d2);
        if(d1 < 0.0f && d2 < 0.0f) D = MAX(d1,d2);
        float x2 = x*x, x3 = x*x2, y2 = y*y, y3 = y*y2;
        // maxima says:
        // eqns :[a*x*x*x + b*x*x + c*x + d = A, a*y*y*y+b*y*y+c*y+d=B, 3*a*x*x+2*b*x+c=C, 3*a*y*y+2*b*y+c=D];
        // linsolve(eqns,[a,b,c,d]); fortran(%);
        p_abcd[4*v+0] = ((D+C)*y+(-D-C)*x-2*B+2*A)/(y3-3*x*y2+3*x2*y-x3);
        p_abcd[4*v+1] = -(((D+2*C)*y2+((D-C)*x-3*B+3*A)*y+(-(2*D)-C)*x2+(3*A-3*B)*x)/(y3-3*x*y2+3*x2*y-x3));
        p_abcd[4*v+2] = (C*y3+(2*D+C)*x*y2+((-D-2*C)*x2+(6*A-6*B)*x)*y-D*x3)/(y3-3*x*y2+3*x2*y-x3);
        p_abcd[4*v+3] = -(((C*x-A)*y3+((D-C)*x2+3*A*x)*y2+(-(D*x3)-3*B*x2)*y+B*x3)/(y3-3*x*y2+3*x2*y-x3));
      }
    }
  }
}

dt_graph_run_t
input(
    dt_module_t             *mod,
    dt_module_input_event_t *p)
{ // mouse events on our dspy curve output
  dt_token_t par[] = {
    dt_token("cntr"), dt_token("xr"), dt_token("yr"),
    dt_token("cntg"), dt_token("xg"), dt_token("yg"),
    dt_token("cntb"), dt_token("xb"), dt_token("yb")};
  const int pid_mode    = dt_module_get_param(mod->so, dt_token("mode"));
  const int mode        = dt_module_param_int(mod, pid_mode)[0];
  const int pid_channel = dt_module_get_param(mod->so, dt_token("channel"));
        int channel     = dt_module_param_int(mod, pid_channel)[0];
  const int pid_ychchan = dt_module_get_param(mod->so, dt_token("ych 3x3"));
  const int ychchan     = dt_module_param_int(mod, pid_ychchan)[0];
  const int pid_sel     = dt_module_get_param(mod->so, dt_token("sel"));
  const int pid_cnt     = dt_module_get_param(mod->so, par[3*channel+0]);
  const int pid_x       = dt_module_get_param(mod->so, par[3*channel+1]);
  const int pid_y       = dt_module_get_param(mod->so, par[3*channel+2]);
  const int pid_v       = dt_module_get_param(mod->so, dt_token("vtx"));
  const int pid_edit    = dt_module_get_param(mod->so, dt_token("edit"));
  int *p_sel = (int   *)dt_module_param_int  (mod, pid_sel);
  int *p_cnt = (int   *)dt_module_param_int  (mod, pid_cnt);
  float *p_x = (float *)dt_module_param_float(mod, pid_x);
  float *p_y = (float *)dt_module_param_float(mod, pid_y);
  float *p_v = (float *)dt_module_param_float(mod, pid_v);
  const int edit = dt_module_param_int(mod, pid_edit)[0];

  static int active = -1;
  
  if(mode == 2) // prepare just the same, but for ych
    channel = ychchan;
  else
    channel = CLAMP(channel, 0, 2); // make sure we don't go out of bounds

  if(mode == 2 && channel > 2)
  { // flat/relative curves
    int c = channel-3; // 6-based index
    if(p->type == 1)
    { // mouse button
      if(p->action == 1)
      { // if button pressed and point selected, mark it as active
        if(p->mbutton == 0 && p_sel[0] >= 0) active = p_sel[0];
        else if(p->mbutton == 3)
        { // double click to reset x and y values of current curve
          const dt_ui_param_t *param = mod->so->param[pid_v];
          memcpy(p_v + 6*c,      param->val + 6*c,      sizeof(float)*6);
          memcpy(p_v + 6*c + 36, param->val + 6*c + 36, sizeof(float)*6);
          dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_v, throttle);
          return s_graph_run_record_cmd_buf;
        }
      }
      else active = -1; // no mouse down no active vertex
    }
    else if(p->type == 2)
    { // mouse position
      if(active >= 0 && active < 6)
      { // move active point around
        float mx = active > 0   ? p_v[6*c+active-1]+1e-3f : 0.0f;
        float Mx = active < 6-1 ? p_v[6*c+active+1]-1e-3f : 1.0f;
        p_v[   6*c+active] = CLAMP(p->x, mx, Mx);
        p_v[36+6*c+active] = p->y-0.5f;
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_v, throttle);
        return s_graph_run_record_cmd_buf;
      }
      else
      {
        int old = p_sel[0];
        p_sel[0] = -1;
        for(int i=0;i<6;i++)
          if(fabs(p->x - p_v[6*c+i]) < 0.05 && fabs(p->y-0.5f-p_v[36+6*c+i]) < 0.05)
            p_sel[0] = i;
        if(old != p_sel[0]) return s_graph_run_record_cmd_buf;
      }
    }
    return 0;
  }

#define E2L(V) (edit ? edit_to_linear(V) : (V))
#define L2E(V) (edit ? linear_to_edit(V) : (V))
  if(p->type == 1)
  { // mouse button
    if(p->action == 1)
    { // if button pressed and point selected, mark it as active
      if(p->mbutton == 0 && p_sel[0] >= 0) active = p_sel[0];
      if(p->mbutton == 0 && p_sel[0] <  0 && p_cnt[0] < 8)
      { // insert new vertex
        int j = p_cnt[0];
        for(int i=0;i<p_cnt[0]&&j==p_cnt[0];i++)
          if(p_x[i] > p->x) j = i;
        for(int i=p_cnt[0]-1;i>=j;i--)
        { // move other vertices back, keep sort order
          p_x[i+1] = p_x[i];
          p_y[i+1] = p_y[i];
        }
        p_x[j] = E2L(p->x);
        p_y[j] = E2L(p->y);
        p_cnt[0] = CLAMP(p_cnt[0]+1,0,8);
        active = p_sel[0] = j;
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_x, throttle);
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_y, throttle);
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_cnt, throttle);
      }
      if(p->mbutton == 1 && p_sel[0] >= 0 && p_cnt[0] > 2)
      { // delete active vertex
        for(int i=p_sel[0];i<p_cnt[0]-1;i++)
        {
          p_x[i] = p_x[i+1];
          p_y[i] = p_y[i+1];
        }
        p_cnt[0] = CLAMP(p_cnt[0]-1,0,8);
        active = -1;
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_x, throttle);
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_y, throttle);
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_cnt, throttle);
      }
      else if(p->mbutton == 3)
      { // double click to reset x and y values of current curve
        const dt_ui_param_t *param;
        param = mod->so->param[pid_x];
        memcpy(p_x, param->val, sizeof(float)*2);
        param = mod->so->param[pid_y];
        memcpy(p_y, param->val, sizeof(float)*2);
        param = mod->so->param[pid_cnt];
        memcpy(p_cnt, param->val, sizeof(int));
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_x, throttle);
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_y, throttle);
        dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_cnt, throttle);
        return s_graph_run_record_cmd_buf;
      }
    }
    else active = -1; // no mouse down no active vertex
  }
  else if(p->type == 2)
  { // mouse position
    if(active >= 0 && active < p_cnt[0])
    { // move active point around
      float mx = active > 0          ? p_x[active-1]+1e-3f : 0.0f;
      float Mx = active < p_cnt[0]-1 ? p_x[active+1]-1e-3f : 1.0f;
      p_x[active] = CLAMP(E2L(p->x), mx, Mx);
      p_y[active] = E2L(p->y);
      dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_x, throttle);
      dt_graph_history_append(mod->graph, mod-mod->graph->module, pid_y, throttle);
      return s_graph_run_record_cmd_buf;
    }
    else
    {
      int old = p_sel[0];
      p_sel[0] = -1;
      for(int i=0;i<p_cnt[0];i++)
        if(fabs(p->x - L2E(p_x[i])) < 0.05 && fabs(p->y - L2E(p_y[i])) < 0.05)
          p_sel[0] = i;
      if(old != p_sel[0]) return s_graph_run_record_cmd_buf;
    }
  }
  return 0;
#undef E2L
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  moddata_t *dat = module->data;
  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t hroi = (dt_roi_t){.wd = 16+1, .ht = 16 };
  const int id_hist = dt_node_add(graph, module, "curves", "hist", (wd+15)/16 * DT_LOCAL_SIZE_X, (ht+15)/16 * DT_LOCAL_SIZE_Y, 1, 0, 0, 2,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "hist",   "write", "ssbo", "u32", &hroi);

  const int id_prep = dt_node_add(graph, module, "curves", "prep", wd, ht, 1, 0, 0, 4,
      "input",  "read",  "*", "*",   dt_no_roi,
      // XXX add single-channel option to guided filter to save a lot of memory!
      "a",      "write", "rgba", "f16", &module->connector[1].roi,
      "b",      "write", "rgba", "f16", &module->connector[1].roi,
      "L",      "write", "r", "f16", &module->connector[1].roi);

  const int id_curv = dt_node_add(graph, module, "curves", "main", wd, ht, 1, 0, 0, 4,
      "input",  "read",  "*",    "*",   dt_no_roi,
      "a",      "read",  "*",    "*",   dt_no_roi,
      "b",      "read",  "*",    "*",   dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);

  // this points to our params {radius, edges} and will also update during param update :)
  const float *radius = dt_module_param_float(module, dt_module_get_param(module->so, dt_token("radius")));
  // int id_blur = dt_api_blur_5x5(graph, module, id_prep, 1, 0, 0);
  // int id_blur = dt_api_blur(graph, module, id_prep, 1, 0, 0, 14);
  // connector 2 will be the output here:
  int id_blra = dt_api_guided_filter_full(graph, module,
      id_prep, 3, // input, determines buffer sizes
      id_prep, 1, // guide
      0, 0, &dat->id_guided_a,
      radius);
  int id_blrb = dt_api_guided_filter_full(graph, module,
      id_prep, 3, // input, determines buffer sizes
      id_prep, 2, // guide
      0, 0, &dat->id_guided_b,
      radius);

  const int id_dspy = dt_node_add(graph, module, "curves", "dspy",
      module->connector[2].roi.wd, module->connector[2].roi.ht, 1, 0, 0, 2,
      "hist", "read",  "ssbo", "u32", dt_no_roi,
      "dspy", "write", "rgba", "f16", &module->connector[2].roi);

  dt_connector_copy(graph, module, 0, id_hist, 0);
  dt_connector_copy(graph, module, 0, id_curv, 0);
  dt_connector_copy(graph, module, 0, id_prep, 0);
  dt_connector_copy(graph, module, 1, id_curv, 3);
  dt_connector_copy(graph, module, 2, id_dspy, 1);
  dt_node_connect(graph, id_hist, 1, id_dspy, 0);
  // dt_node_connect(graph, id_blur, 1, id_curv, 1);
  dt_node_connect_named(graph, id_blra, "output", id_curv, "a");
  dt_node_connect_named(graph, id_blrb, "output", id_curv, "b");
  // dt_node_connect(graph, id_prep, 1, id_curv, 1);
  // dt_node_connect(graph, id_prep, 2, id_curv, 2);
  graph->node[id_hist].connector[1].flags |= s_conn_clear; // clear histogram buffer
}
