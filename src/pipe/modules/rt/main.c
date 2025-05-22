#include "modules/api.h"
#include "../i-hdr/rgbe.h"
#include "../i-hdr/rgbe.c"
#include "quat.h"
#include "config.h"
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

dt_graph_run_t
check_params(
    dt_module_t *mod,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int parw = dt_module_get_param(mod->so, dt_token("wd"));
  const int parh = dt_module_get_param(mod->so, dt_token("ht"));
  const int pars = dt_module_get_param(mod->so, dt_token("sampler"));
  if(parid == parw || parid == parh || parid == pars)
  { // dimensions wd or ht
    int oldsz = *(int*)oldval;
    int newsz = dt_module_param_int(mod, parid)[0];
    if(oldsz != newsz) return s_graph_run_all; // we need to update the graph topology
  }
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *mod)
{
  const int p_wd = dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("wd")))[0];
  const int p_ht = dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("ht")))[0];
  int wd = p_wd, ht = p_ht;
  mod->connector[0].roi.full_wd = mod->connector[5].roi.full_wd = mod->connector[4].roi.full_wd = wd;
  mod->connector[0].roi.full_ht = mod->connector[5].roi.full_ht = mod->connector[4].roi.full_ht = ht;
  mod->connector[0].roi.wd = mod->connector[5].roi.wd = mod->connector[4].roi.wd = wd;
  mod->connector[0].roi.ht = mod->connector[5].roi.ht = mod->connector[4].roi.ht = ht;
  mod->connector[0].roi.scale   = mod->connector[5].roi.scale   = mod->connector[4].roi.scale = 1;
  mod->connector[3].roi.scale = 1;
  mod->connector[3].roi.wd = mod->connector[3].roi.full_wd = wd;
  mod->connector[3].roi.ht = mod->connector[3].roi.full_ht = ht * 4;
  mod->img_param   = (dt_image_params_t) {
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
    { // splitkb dvorak, sorry:
      case '.': rt->move = (rt->move & ~(1<<0)) | (p->action<<0); break;
      case 'E': rt->move = (rt->move & ~(1<<1)) | (p->action<<1); break;
      case 'O': rt->move = (rt->move & ~(1<<2)) | (p->action<<2); break;
      case 'U': rt->move = (rt->move & ~(1<<3)) | (p->action<<3); break;
      case ' ': rt->move = (rt->move & ~(1<<4)) | (p->action<<4); break;
      case 'K': rt->move = (rt->move & ~(1<<5)) | (p->action<<5); break;
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

  // grab uniform stuff from camera module
  int mode = dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("cam mode")))[0];
  if(mode == 1)
  {
    dt_token_t cam_module   = dt_token(dt_module_param_string(mod,
          dt_module_get_param(mod->so, dt_token("cam_mod"))));
    dt_token_t cam_instance = dt_token(dt_module_param_string(mod,
          dt_module_get_param(mod->so, dt_token("cam_inst"))));
    if(cam_module && cam_instance) for(int m=0;m<graph->num_modules;m++)
    {
      if(graph->module[m].name == cam_module &&
         graph->module[m].inst == cam_instance)
      {
        const float *p2_cam    = dt_module_param_float(graph->module+m,
            dt_module_get_param(graph->module[m].so, dt_token("cam")));
        const float *p2_fog    = dt_module_param_float(graph->module+m,
            dt_module_get_param(graph->module[m].so, dt_token("fog")));
        const float *p2_cltime = dt_module_param_float(graph->module+m,
            dt_module_get_param(graph->module[m].so, dt_token("cltime")));
        float *p_fog = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("fog")));
        float *p_cltime = (float *)dt_module_param_float(mod, dt_module_get_param(mod->so, dt_token("cltime")));
        memcpy(p_cam, p2_cam, sizeof(float)*12);
        memcpy(p_fog, p2_fog, sizeof(float)*4);
        memcpy(p_cltime, p2_cltime, sizeof(float));
        break;
      }
    }
  }
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
  if(!filename || !filename[0]) goto error;
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
  hdr->width = hdr->height = 8;
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
  const int   sampler  = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("sampler")))[0];
  // environment importance sampling:
  const int   id       = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("startid")))[0];
  const char *filename = dt_module_param_string(module, dt_module_get_param(module->so, dt_token("envmap")));
  if(read_header(module, graph->frame+id, filename)) {}
  dt_roi_t roi_mip = { .wd = rt->width, .ht = rt->height };
  roi_mip.ht = roi_mip.ht + roi_mip.ht / 12 + 1; // true for 8k
  int id_env = dt_node_add(graph, module, "rt", "env",
      1, 1, 1, 0, 0, 1,
      "output", "source", "rgba", "f32", &roi_mip);

  const int wd = module->connector[0].roi.wd;
  const int ht = module->connector[0].roi.ht;
  dt_roi_t roi_fb  = { .wd = wd, .ht = ht * 4 };
  dt_roi_t roi_mat = { .wd = wd, .ht = ht * sizeof(struct gbuf_t) };
  int id_gbuf = dt_node_add(graph, module, "rt", "gbuf", wd, ht, 1, 0, 0, 3,
      "dn",  "write", "rg",   "f32", &module->connector[0].roi,
      "tex", "read",  "*",    "*",    dt_no_roi,
      "mat", "write", "ssbo", "u8",  &roi_mat);
  dt_connector_copy(graph, module, 1, id_gbuf, 1); // tex
  int id_main = -1;

  if(sampler == 0)
  { // std pt
    id_main = dt_node_add(graph, module, "rt", "main", wd, ht, 1, 0, 0, 7,
        "output", "write", "ssbo", "f32", &roi_fb,                    // 0
        "tex",    "read",  "*",    "*",    dt_no_roi,                 // 1
        "blue",   "read",  "*",    "*",    dt_no_roi,                 // 2
        "albedo", "write", "rgba", "f16", &module->connector[0].roi,  // 3
        "env",    "read",  "*",    "*",    dt_no_roi,                 // 4
        "dn",     "read",  "*",    "*",    dt_no_roi,                 // 5
        "mat",    "read",  "ssbo", "*",    dt_no_roi);                // 6 material/gbuf
    dt_connector_copy(graph, module, 1, id_main, 1);
    dt_connector_copy(graph, module, 2, id_main, 2);
    dt_connector_copy(graph, module, 3, id_main, 0);
    dt_connector_copy(graph, module, 4, id_main, 3);
  }
  else if(sampler == 1)
  { // mcpg: markov chain path guiding, alber 2025
    dt_roi_t roi_mc = {
      .wd = MC_ADAPTIVE_BUFFER_SIZE + MC_STATIC_BUFFER_SIZE,
      .ht = sizeof(struct MCState) };
    dt_roi_t roi_lc = {
      .wd = LIGHT_CACHE_BUFFER_SIZE,
      .ht = sizeof(struct LightCacheVertex) };
    id_main = dt_node_add(graph, module, "rt", "mcpg", wd, ht, 1, 0, 0, 9,
        "output", "write", "ssbo", "f32", &roi_fb,                    // 0
        "tex",    "read",  "*",    "*",    dt_no_roi,                 // 1
        "blue",   "read",  "*",    "*",    dt_no_roi,                 // 2
        "albedo", "write", "rgba", "f16", &module->connector[0].roi,  // 3
        "env",    "read",  "*",    "*",    dt_no_roi,                 // 4
        "dn",     "read",  "*",    "*",    dt_no_roi,                 // 5
        "mat",    "read",  "ssbo", "*",    dt_no_roi,                 // 6 material/gbuf
        "mc",  "write", "ssbo", "u8",  &roi_mc,
        "lc",  "write", "ssbo", "u8",  &roi_lc);

    dt_connector_copy(graph, module, 1, id_main, 1); // tex
    dt_connector_copy(graph, module, 2, id_main, 2); // blue
    dt_connector_copy(graph, module, 3, id_main, 0); // ssbo irradiance w/o albedo
    dt_connector_copy(graph, module, 4, id_main, 3); // albedo/aov
    // protect light cache and markov chain states for next iteration:
    graph->node[id_main].connector[7].flags |= s_conn_clear_once | s_conn_protected;
    graph->node[id_main].connector[8].flags |= s_conn_clear_once | s_conn_protected;
  }
  CONN(dt_node_connect_named(graph, id_env,  "output", id_main, "env"));
  CONN(dt_node_connect_named(graph, id_gbuf, "dn",     id_main, "dn"));
  CONN(dt_node_connect_named(graph, id_gbuf, "mat",    id_main, "mat"));
  const int id_post = dt_node_add(graph, module, "rt", "post", wd, ht, 1, 0, 0, 4,
      "input",  "read",  "ssbo", "f32", dt_no_roi,
      "albedo", "read",  "rgba", "*",   dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[0].roi,
      "dep",    "read",  "*",    "*",   dt_no_roi); // sync exec connector
  CONN(dt_node_connect_named(graph, id_main, "output", id_post, "dep"));
  CONN(dt_node_connect_named(graph, id_main, "output", id_post, "input"));
  CONN(dt_node_connect_named(graph, id_main, "albedo", id_post, "albedo"));
  dt_connector_copy(graph, module, 0, id_post, 2);
  dt_connector_copy(graph, module, 5, id_gbuf, 0); // route out gbuf
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  if(p->node->kernel != dt_token("env")) return 0;
  const int   id       = dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("startid")))[0];
  const char *filename = dt_module_param_string(mod, dt_module_get_param(mod->so, dt_token("envmap")));
  if(read_header(mod, mod->graph->frame+id, filename)) return 1;
  rt_t *hdr = mod->data;
  int ret = read_plain(hdr, mapped);
  if(ret) return ret;

  // this only works for powers of two, and w = 2h kinda lat/lon maps:
  // create importance sampling mipmap in the lower part of the image

  float *img = mapped;
  int off = hdr->width * hdr->height;
  int wd = hdr->width / 4, ht = hdr->height / 4, m = 1;
  while(wd >= 1)
  {
    off += wd * MAX(1,wd/2);
    wd /= 2;
  }

  wd = hdr->width/4;
  int off_prev = off;
  while(wd >= 1)
  { // while we can still fill at least one whole scanline with the mipmap:
    off -= wd * MAX(1,ht);
    // man this is slow.
    for(int j=0;j<MAX(1,ht);j++)
    { // haul out of loop:
      float sin4[] = {
        m==1?sinf(((4*j+0+0.5f) / (hdr->height))*M_PI):0,
        m==1?sinf(((4*j+1+0.5f) / (hdr->height))*M_PI):0,
        m==1?sinf(((4*j+2+0.5f) / (hdr->height))*M_PI):0,
        m==1?sinf(((4*j+3+0.5f) / (hdr->height))*M_PI):0};
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
