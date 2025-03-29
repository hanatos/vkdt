#include "modules/api.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{ // request something square for dspy output
  module->connector[2].roi.full_wd = 1024;
  module->connector[2].roi.full_ht = 1024;
  module->connector[1].roi = module->connector[0].roi; // output
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
