#include "modules/api.h"

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *module)
{ // request something square for dspy output
  module->connector[2].roi.full_wd = 1024;
  module->connector[2].roi.full_ht = 1024;
  module->connector[1].roi = module->connector[0].roi; // output
}

void commit_params(dt_graph_t *graph, dt_module_t *mod)
{
  int *p_cnt = (int   *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("cnt")));
  float *p_x = (float *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("x")));
  float *p_y = (float *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("y")));

  float ddx0 = 0.0f, ddxn = 0.0f; // boundary constraints, set them according to some parameter

  // cnt is 2..8
  float alpha[8], h[8], I[8], mu[8], z[8];
  float a[8], b[8], c[8], d[8];

  // clamped cubic spline
  int n = p_cnt[0]-1;
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
  float *p_abcd = (float *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("abcd")));
  for(int i=0;i<n;i++)
  {
    p_abcd[4*i+0] = a[i];
    p_abcd[4*i+1] = b[i];
    p_abcd[4*i+2] = c[i];
    p_abcd[4*i+3] = d[i];
  }
}

dt_graph_run_t
input(
    dt_module_t             *mod,
    dt_module_input_event_t *p)
{ // mouse events on our dspy curve output
  int *p_cnt = (int   *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("cnt")));
  int *p_sel = (int   *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("sel")));
  float *p_x = (float *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("x")));
  float *p_y = (float *)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("y")));

  static int active = -1;
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
        p_x[j] = p->x;
        p_y[j] = p->y;
        p_cnt[0] = CLAMP(p_cnt[0]+1,0,8);
        active = p_sel[0] = j;
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
      p_x[active] = CLAMP(p->x, mx, Mx);
      p_y[active] = p->y;
    }
    else
    {
      p_sel[0] = -1;
      for(int i=0;i<p_cnt[0];i++)
        if(fabs(p->x - p_x[i]) < 0.05 && fabs(p->y - p_y[i]) < 0.05)
          p_sel[0] = i;
    }
    return s_graph_run_record_cmd_buf;
  }
  return 0;
}
