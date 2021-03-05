#include "quat.h"
#include "modules/api.h"
#include <inttypes.h>

typedef struct rt_t
{
  quat_t   cam_o;
  float    cam_x[3];
  uint32_t move;
}
rt_t;

void
input(
    dt_module_t             *mod,
    dt_module_input_event_t *p)
{
  static double mx = 0.0, my = 0.0;
  float *p_cam = dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("cam")));
  rt_t *rt = mod->data;
  if(p->type == 0)
  { // activate event? store mouse zero and clear all movement flags we might still have
    rt->move = 0;
    float xd[] = {1, 0, 0};
    cross(p_cam+4, xd, rt->cam_o.x);
    rt->cam_o.w = dot(p_cam+4, xd);
    rt->cam_o.w += quat_magnitude(&rt->cam_o);
    quat_normalise(&rt->cam_o);
    for(int k=0;k<3;k++) rt->cam_x[k] = p_cam[k];
    mx = my = -666.0;
    return;
  }

  if(p->type == 2)
  { // rotate camera based on mouse coordinate
    if(mx == -666.0 && my == -666.0) { mx = p->x; my = p->y; }
    const float avel = 0.001f; // angular velocity
    quat_t rot, tmp = rt->cam_o;
    quat_init_angle(&rot, (p->x-mx)*avel, 0, 0, -1);
    quat_mul(&tmp, &rot, &rt->cam_o);
    tmp = rt->cam_o;
    quat_init_angle(&rot, (p->y-my)*avel, 0, 1, 0);
    quat_mul(&tmp, &rot, &rt->cam_o);
    mx = p->x;
    my = p->y;
  }

  if(p->type == 4)
  { // keyboard
    if(p->action <= 1) // ignore key repeat
    switch(p->key)
    {
      case 'E': rt->move = (rt->move & ~(1<<0)) | (p->action<<0); break;
      case 'D': rt->move = (rt->move & ~(1<<1)) | (p->action<<1); break;
      case 'S': rt->move = (rt->move & ~(1<<2)) | (p->action<<2); break;
      case 'F': rt->move = (rt->move & ~(1<<3)) | (p->action<<3); break;
      case ' ': rt->move = (rt->move & ~(1<<4)) | (p->action<<4); break;
      case 'V': rt->move = (rt->move & ~(1<<5)) | (p->action<<5); break;
      case 'R': // reset camera
                rt->move = 0;
                memset(p_cam, 0, sizeof(float)*8);
                p_cam[4] = 1;
                dt_module_input_event_t p = { 0 };
                mod->so->input(mod, &p);
                return;
    }
  }
}

// this is called every time the command buffer is executed again, to
// upload new uniforms. only if commit_params_size > 0 on the module
// we need to actually commit params, until then it uses the default
// code path. we just abuse it as a callback for every time step.
void commit_params(dt_graph_t *graph, dt_module_t *mod)
{
  rt_t *rt = mod->data;
  float *p_cam = dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("cam")));
  // put back to params:
  float fwd[] = {1, 0, 0};
  float rgt[] = {0, 1, 0};
  float top[] = {0, 0, 1};
  quat_transform(&rt->cam_o, fwd);
  quat_transform(&rt->cam_o, rgt);
  quat_transform(&rt->cam_o, top);
  float vel = 0.1f;
  for(int k=0;k<3;k++) p_cam[4+k] = fwd[k];
  if(rt->move & (1<<0)) for(int k=0;k<3;k++) p_cam[k] += vel * fwd[k];
  if(rt->move & (1<<1)) for(int k=0;k<3;k++) p_cam[k] -= vel * fwd[k];
  if(rt->move & (1<<2)) for(int k=0;k<3;k++) p_cam[k] += vel * rgt[k];
  if(rt->move & (1<<3)) for(int k=0;k<3;k++) p_cam[k] -= vel * rgt[k];
  if(rt->move & (1<<4)) for(int k=0;k<3;k++) p_cam[k] += vel * top[k];
  if(rt->move & (1<<5)) for(int k=0;k<3;k++) p_cam[k] -= vel * top[k];
}

int init(dt_module_t *mod)
{
  rt_t *rt = calloc(sizeof(rt_t), 1);
  mod->data = rt;
  return 0;
}

int cleanup(dt_module_t *mod)
{
  free(mod->data);
  mod->data = 0;
  return 0;
}
