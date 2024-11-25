#include "modules/api.h"
#include "../i-hdr/rgbe.h"
#include "../i-hdr/rgbe.c"
#include "quat.h"
#include <inttypes.h>

typedef struct rt_t
{
  uint32_t move;

  // stuff for envmap
  char filename[PATH_MAX];
  uint32_t frame;
  uint32_t width, height;
  size_t data_begin;
  FILE *f;
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
  module->img_param   = (dt_image_params_t) {
    .black            = {0, 0, 0, 0},
    .white            = {65535,65535,65535,65535},
    .whitebalance     = {1.0, 1.0, 1.0, 1.0},
    .filters          = 0, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb        = {0, 0, wd, ht},
    .colour_trc       = s_colour_trc_linear,
    .colour_primaries = s_colour_primaries_2020,
    .cam_to_rec2020   = {0.0f/0.0f, 0, 0, 0, 1, 0, 0, 0, 1},
    .snd_samplerate   = 44100,
    .snd_format       = 2, // SND_PCM_FORMAT_S16_LE
    .snd_channels     = 2, // stereo
    .noise_a          = 1.0,
    .noise_b          = 0.0,
    .orientation      = 0,
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
  float vel = 0.5f;
  if(rt->move & (1<<0)) for(int k=0;k<3;k++) p_cam[k] += vel * fwd[k];
  if(rt->move & (1<<1)) for(int k=0;k<3;k++) p_cam[k] -= vel * fwd[k];
  if(rt->move & (1<<2)) for(int k=0;k<3;k++) p_cam[k] += vel * rgt[k];
  if(rt->move & (1<<3)) for(int k=0;k<3;k++) p_cam[k] -= vel * rgt[k];
  if(rt->move & (1<<4)) for(int k=0;k<3;k++) p_cam[k] += vel * top[k];
  if(rt->move & (1<<5)) for(int k=0;k<3;k++) p_cam[k] -= vel * top[k];
  // fprintf(stderr, "cam %g %g %g dir %g %g %g\n", p_cam[0], p_cam[1], p_cam[2], p_cam[4], p_cam[5], p_cam[6]);
}

int init(dt_module_t *mod)
{
  rt_t *rt = calloc(sizeof(rt_t), 1);
  mod->data = rt;
  return 0;
}

int cleanup(dt_module_t *mod)
{
  rt_t *rt = mod->data;
  if(rt->filename[0])
  {
    if(rt->f) fclose(rt->f);
    rt->filename[0] = 0;
  }
  free(mod->data);
  mod->data = 0;
  return 0;
}

static int 
read_header(
    dt_module_t *mod,
    uint32_t    frame,
    const char *filename)
{
  rt_t *hdr = mod->data;
  if(hdr && !strcmp(hdr->filename, filename) && hdr->frame == frame)
    return 0; // already loaded
  assert(hdr); // this should be inited in init()

  if(hdr->f) fclose(hdr->f);
  hdr->f = dt_graph_open_resource(mod->graph, frame, filename, "rb");
  if(!hdr->f) goto error;

  int wd, ht;
  rgbe_header_info info;
  if(RGBE_ReadHeader(hdr->f, &wd, &ht, &info)) goto error;
  hdr->width  = wd;
  hdr->height = ht;
  hdr->data_begin = ftell(hdr->f);

  snprintf(hdr->filename, sizeof(hdr->filename), "%s", filename);
  hdr->frame = frame;
  return 0;
error:
  fprintf(stderr, "[rt] could not load envmap file `%s'!\n", filename);
  hdr->filename[0] = 0;
  hdr->frame = -1;
  return 1;
}

static int
read_plain(
    rt_t *hdr, float *out)
{
  fseek(hdr->f, hdr->data_begin, SEEK_SET);
  return RGBE_ReadPixels_RLE(hdr->f, out, hdr->width, hdr->height);
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  rt_t *rt = module->data;
  // environment importance sampling:
  const int   id       = dt_module_param_int(module, 3)[0];
  const char *filename = dt_module_param_string(module, 2);
  if(read_header(module, graph->frame+id, filename)) {} // XXX error handling ???
  dt_roi_t roi_mip = { .wd = rt->width, .ht = rt->height };
  roi_mip.ht = roi_mip.ht + roi_mip.ht / 12 + 1; // true for 8k
  int id_env = dt_node_add(graph, module, "rt", "env",
      1, 1, 1, 0, 0, 1,
      "output", "source", "rgba", "f32", &roi_mip);

  dt_roi_t roi_gbuf = { .wd = module->connector[0].roi.wd, .ht = module->connector[0].roi.ht * 4 };
  int pc[] = { module->connector[0].roi.wd, module->connector[0].roi.ht };
  fprintf(stderr, "got wd ht %d %d \n", pc[0], pc[1]);
  int id_rt = dt_node_add(graph, module, "rt", "main", 
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, sizeof(pc), pc, 5,
      "output",   "write", "ssbo", "f32",  &roi_gbuf,                 // 0
      "blue",     "read",  "*",    "*",    dt_no_roi,                 // 1
      "tex",      "read",  "*",    "*",    dt_no_roi,                 // 2
      "aov",      "write", "rgba", "f16",  &module->connector[0].roi, // 3
      "env",      "read",  "*",    "*",    dt_no_roi);                // 4
  dt_connector_copy(graph, module, 1, id_rt, 1);
  dt_connector_copy(graph, module, 2, id_rt, 2);
  dt_connector_copy(graph, module, 3, id_rt, 3);
  CONN(dt_node_connect_named(graph, id_env, "output", id_rt, "env"));

  const int id_post = dt_node_add(graph, module, "rt", "post",
    module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 2,
      "input",  "read",  "ssbo", "f32", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi);
  // graph->node[id_rt].connector[0].flags = s_conn_clear; // clear for light tracing
  CONN(dt_node_connect_named(graph, id_rt, "output", id_post, "input"));
  dt_connector_copy(graph, module, 0, id_post, 1);
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  if(p->node->kernel != dt_token("env")) return 0;
  const int   id       = dt_module_param_int(mod, 3)[0];
  const char *filename = dt_module_param_string(mod, 2);
  if(read_header(mod, mod->graph->frame+id, filename)) return 1;
  rt_t *hdr = mod->data;
  int ret = read_plain(hdr, mapped);
  if(ret) return ret;

  // this only works for powers of two, and w = 2h kinda lat/lon maps:
  // create importance sampling mipmap in the lower part of the image

  int off = hdr->width * hdr->height;
  int wd = hdr->width / 4, ht = hdr->height / 4, m = 1;
  while(wd >= 1)
  {
    off += wd * MAX(1,wd/2);
    wd /= 2;
  }

  float *img = mapped;
  wd = hdr->width/4;
  int off_prev = off;
  while(wd >= 1)
  { // while we can still fill at least one whole scanline with the mipmap:
    off -= wd * MAX(1,ht);
    // man this is slow.
    for(int j=0;j<MAX(1,ht);j++)
    { // haul out of loop:
      float sin4[] = { sinf(((4*j+0+0.5f) / (hdr->height))*M_PI), sinf(((4*j+1+0.5f) / (hdr->height))*M_PI),
        sinf(((4*j+2+0.5f) / (hdr->height))*M_PI), sinf(((4*j+3+0.5f) / (hdr->height))*M_PI)};
      for(int i=0;i<wd;i++)
      { // for all pixels in current mip map level m
        int idx = wd*j + i;
        for(int jj=0;jj<(ht==0?1:2);jj++) for(int ii=0;ii<2;ii++)
        { // for all 4 channels of the current pixel in mip level m
          float val = 0.0f;
          if(m == 1)
          { // read original image, need jacobian + luminance
            for(int jjj=0;jjj<2;jjj++) for(int iii=0;iii<2;iii++)
            {
              int x = 4*i + 2*ii + iii, y = 4*j + 2*jj + jjj;
              float J = sin4[2*jj+jjj];
              int pxi = 4*(hdr->width*y + x);
              float lum = img[pxi] + img[pxi+1] + img[pxi+2]; // could use luminance
              val += lum * J * 0.25f;
            }
          }
          else for(int k=0;k<4;k++) // accumulate 2x2 block of previous mipmap
            val += img[4*(off_prev + 2*i+ii + 2*wd*(2*j+jj)) + k] * 0.25f;
          img[4*(off + idx) + 2*jj+ii] = val; // write current channel of current pixel
        }
        if(ht == 0) img[4*(off + idx) + 2] = img[4*(off + idx) + 3] = 0.0f;
      }
    }
    off_prev = off;
    wd /= 2; ht /= 2; m++;
  }
  return 0;
}
