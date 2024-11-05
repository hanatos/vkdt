#include "modules/api.h"
#include "quat.h"
#include <inttypes.h>

typedef struct rt_t
{
  uint32_t move;
}
rt_t;

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  const int wd = 2048, ht = 1152;
  module->connector[0].roi.full_wd = module->connector[3].roi.full_wd = wd;
  module->connector[0].roi.full_ht = module->connector[3].roi.full_ht = ht;
  module->connector[0].roi.scale = module->connector[3].roi.scale = 1;
  module->img_param = (dt_image_params_t) {
    .black          = {0, 0, 0, 0},
    .white          = {65535,65535,65535,65535},
    .whitebalance   = {1.0, 1.0, 1.0, 1.0},
    .filters        = 0, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb      = {0, 0, wd, ht},
    .cam_to_rec2020 = {1, 0, 0, 0, 1, 0, 0, 0, 1},
    .snd_samplerate = 44100,
    .snd_format     = 2, // SND_PCM_FORMAT_S16_LE
    .snd_channels   = 2, // stereo
    .noise_a        = 1.0,
    .noise_b        = 0.0,
    .orientation    = 0,
  };
}

void
input(
    dt_module_t             *mod,
    dt_module_input_event_t *p)
{
  static double mx = 0.0, my = 0.0;
  float *p_cam = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("cam")));
  rt_t *rt = mod->data;
  if(p->type == 0)
  { // activate event? store mouse zero and clear all movement flags we might still have
    rt->move = 0;
    mx = my = -666.0;
    return;
  }

  if(p->type == 2)
  { // rotate camera based on mouse coordinate
    if(mx == -666.0 && my == -666.0) { mx = p->x; my = p->y; }
    const float avel = 0.001f; // angular velocity
    quat_t rotx, roty, tmp;
    float rgt[3], top[] = {0,0,1};
    cross(top, p_cam+4, rgt);
    quat_init_angle(&rotx, (p->x-mx)*avel, 0, 0, -1);
    quat_init_angle(&roty, (p->y-my)*avel, rgt[0], rgt[1], rgt[2]);
    quat_mul(&rotx, &roty, &tmp);
    quat_transform(&tmp, p_cam+4);
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
  float *p_cam = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("cam")));
  // put back to params:
  float fwd[] = {p_cam[4], p_cam[5], p_cam[6]};
  float top[] = {0, 0, 1};
  float rgt[3]; cross(top, fwd, rgt);
  float vel = 3.0f;
  if(rt->move & (1<<0)) for(int k=0;k<3;k++) p_cam[k] += vel * fwd[k];
  if(rt->move & (1<<1)) for(int k=0;k<3;k++) p_cam[k] -= vel * fwd[k];
  if(rt->move & (1<<2)) for(int k=0;k<3;k++) p_cam[k] += vel * rgt[k];
  if(rt->move & (1<<3)) for(int k=0;k<3;k++) p_cam[k] -= vel * rgt[k];
  if(rt->move & (1<<4)) for(int k=0;k<3;k++) p_cam[k] += vel * top[k];
  if(rt->move & (1<<5)) for(int k=0;k<3;k++) p_cam[k] -= vel * top[k];
  fprintf(stderr, "cam %g %g %g dir %g %g %g\n", p_cam[0], p_cam[1], p_cam[2], p_cam[4], p_cam[5], p_cam[6]);
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

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  int id_rt = dt_node_add(graph, module, "rt", "main", 
    module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 4,
      "output",   "write", "rgba", "f16",  &module->connector[0].roi, // 0
      "blue",     "read",  "*",    "*",    dt_no_roi,                 // 1
      "tex",      "read",  "*",    "*",    dt_no_roi,                 // 2
      "aov",      "write", "rgba", "f16",  &module->connector[0].roi);// 3
  dt_connector_copy(graph, module, 0, id_rt, 0);
  dt_connector_copy(graph, module, 1, id_rt, 1);
  dt_connector_copy(graph, module, 2, id_rt, 2);
  dt_connector_copy(graph, module, 3, id_rt, 3);
}
