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
    .cam_to_rec2020   = {1, 0, 0, 0, 1, 0, 0, 0, 1},
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
  float vel = 3.0f;
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
  if(read_header(module, graph->frame+id, filename)) {} // XXX ???
  dt_roi_t roi_mip = { .wd = rt->width, .ht = rt->height };
  roi_mip.ht = roi_mip.ht + roi_mip.ht / 12 + 1; // true for 8k
  int id_env = dt_node_add(graph, module, "rt", "env",
      1, 1, 1, 0, 0, 1,
      "output", "source", "rgba", "f32", &roi_mip);

  dt_roi_t roi_gbuf = { .wd = module->connector[0].roi.wd, .ht = module->connector[0].roi.ht * 4 };
  int id_rt = dt_node_add(graph, module, "rt", "main", 
      module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 5,
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

  // XXX this only works for powers of two, and w = 2h kinda lat/lon maps:
  // now create importance sampling mipmap in the lower 30% of the image

  float *img = mapped;
  int wd = hdr->width/4, ht = hdr->height/4, m = 1;
  int yoff = hdr->height, xoff = 0, yoff_prev = 0, xoff_prev = 0;
  while(wd * ht >= hdr->width)
  { // while we can still fill at least one whole scanline with the mipmap:
    int h = (wd * ht) / hdr->width;
    fprintf(stderr, "mip %d is %d x %d, yoff %d + h %d\n", m, wd, ht, yoff, h);
    for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
    { // for all pixels in current mip map level m
      // read 4 pixels at mip level-1 (2*i, 2*j, m-1) 
      int idx = wd*j + i;
      for(int jj=0;jj<2;jj++) for(int ii=0;ii<2;ii++)
      { // for all 4 channels of the current pixel in mip level m
        float val = 0.0f;
        if(m == 1)
        { // read original image, need jacobian + luminance
          for(int jjj=0;jjj<2;jjj++) for(int iii=0;iii<2;iii++)
          {
            int x = 4*i + 2*ii + iii, y = 4*j + 2*jj + jjj;
            float J = sinf(((y+0.5f) / (2.0f*ht) - 0.5f)*M_PI); // at pixel center so it won't be 0 on poles
            int pxi = 4*(hdr->width*y + x);
            float lum = img[pxi] + img[pxi+1] + img[pxi+2]; // could use luminance
            val += lum * J;
          }
        }
        else for(int k=0;k<4;k++) // accumulate 2x2 block of previous mipmap
          val += img[4*(hdr->width*yoff_prev + 4*idx+ii+2*wd*jj) + k];
        img[4*(hdr->width*yoff + idx) + 2*jj+ii] = val; // write current channel of current pixel
      }
    }
    yoff_prev = yoff;
    yoff += h;
    wd /= 2; ht /= 2; m++;
  }
  // TODO merge this loop into the above by using idxoff instead of yoff and xoff!
  // TODO: fix last iteration where we only have one pixel with two zero entries (because it samples a 2x1 block, not a 2x2 block)
  while(wd >= 1) // XXX == 1 means ht == 0 so last iteration doesn't work!
  { // the rest will now fit into one last scanline of the image
    int w = wd * ht;
    fprintf(stderr, "mip %d is %d x %d, xoff %d + w %d\n", m, wd, ht, xoff, w);
    for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
    { // for all pixels in current mip map level m
      int idx = wd*j + i;
      for(int jj=0;jj<2;jj++) for(int ii=0;ii<2;ii++)
      { // for all 4 channels of the current pixel in mip level m
        float val = 0.0f;
        for(int k=0;k<4;k++) // accumulate 2x2 block of previous mipmap
          val += img[4*(hdr->width*yoff_prev + 4*idx+ii+2*wd*jj + xoff_prev) + k];
        img[4*(hdr->width*yoff + xoff + idx) + 2*jj+ii] = val; // write current channel of current pixel
      }
      fprintf(stderr, "address: %d\n", hdr->width*(yoff-hdr->height) + xoff + idx);
    }
    xoff_prev = xoff; yoff_prev = yoff;
    xoff += w;
    wd /= 2; ht /= 2; m++;
  }
  return 0;
}
