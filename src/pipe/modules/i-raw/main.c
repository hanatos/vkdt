#include "modules/api.h"
#include "core/mat3.h"
#include "rawloader-c/rawloader.h"
// #include "core/log.h"
#include "dng_opcode.h"

#include <stdio.h>
#include <time.h>

#include "dng_opcode_decode.c"

typedef struct rawinput_buf_t
{
  rawimage_t img;
  uint64_t len;
  char filename[PATH_MAX];
  int frame;
  int ox, oy;
  dt_dng_opcode_list_t *dng_opcode_lists[3];
  dt_image_metadata_dngop_t dngop;
}
rawinput_buf_t;
  
void
free_raw(dt_module_t *mod)
{
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  rl_deallocate(mod_data->img.data, mod_data->len);
  mod_data->img.data = 0;
  mod_data->len = 0;
  for(int i=0;i<3;i++)
  {
    dng_opcode_list_free(mod_data->dng_opcode_lists[i]);
    mod_data->dng_opcode_lists[i] = NULL;
  }
  mod_data->filename[0] = 0;
}

int
load_raw(
    dt_module_t *mod,
    int         frame,
    const char *filename)
{
  // clock_t end, beg = clock();
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  assert(mod_data);
  if(!strcmp(mod_data->filename, filename) && mod_data->frame == frame)
    return 0; // already loaded
  else free_raw(mod); // maybe loaded the wrong one
  char fname[2*PATH_MAX+10];
  if(dt_graph_get_resource_filename(mod, filename, frame, fname, sizeof(fname)))
    goto error;
  mod_data->len = rl_decode_file(fname, &mod_data->img);
  // end = clock();
  // dt_log(s_log_perf, "[rawloader] load %s in %3.0fms", filename, 1000.0*(end-beg)/CLOCKS_PER_SEC);
  // fprintf(stderr, "[rawloader] load %s in %3.0fms", filename, 1000.0*(end-beg)/CLOCKS_PER_SEC);
  snprintf(mod_data->filename, sizeof(mod_data->filename), "%s", filename);
  mod_data->frame = frame;

  int have_dngop = 0;
  for(int i=0;i<3;i++)
  {
    if(mod_data->img.dng_opcode_lists_len[i] > 0)
    {
      // decode the raw opcode list into C structures we can access directly
      mod_data->dng_opcode_lists[i] = dng_opcode_list_decode(
        mod_data->img.dng_opcode_lists[i], mod_data->img.dng_opcode_lists_len[i]);
      // free the raw opcode list now that we have decoded it
      rl_deallocate(mod_data->img.dng_opcode_lists[i], mod_data->img.dng_opcode_lists_len[i]);
      mod_data->img.dng_opcode_lists[i] = 0;
      mod_data->img.dng_opcode_lists_len[i] = 0;
      have_dngop = 1;
    }
  }
  if(have_dngop)
  {
    mod_data->dngop = (dt_image_metadata_dngop_t){
      .type = s_image_metadata_dngop,
      .ox   = mod_data->img.cfa_off_x,
      .oy   = mod_data->img.cfa_off_y,
      .op_list = {
        mod_data->dng_opcode_lists[0],
        mod_data->dng_opcode_lists[1],
        mod_data->dng_opcode_lists[2],
      },
    };
    mod->img_param.meta = dt_metadata_append(mod->img_param.meta, (void*)&mod_data->dngop);
  }
  else mod->img_param.meta = 0;
  return 0;
error:
  // dt_log(s_log_err, "[i-raw] failed to load raw file %s!\n", fname);
  // fprintf(stderr, "[i-raw] failed to load raw file %s!\n", fname);
  mod_data->filename[0] = 0;
  mod_data->frame = -1;
  return 1;
}

int init(dt_module_t *mod)
{
  rawinput_buf_t *dat = calloc(1, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  rawinput_buf_t *mod_data = mod->data;
  free_raw(mod);
  free(mod_data);
  mod->data = 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  if(parid == 1 || parid == 2) // noise model
  {
    const float noise_a = dt_module_param_float(module, 1)[0];
    const float noise_b = dt_module_param_float(module, 2)[0];
    module->img_param.noise_a = noise_a;
    module->img_param.noise_b = noise_b;
    return s_graph_run_all; // need no do modify_roi_out again to read noise model from file
  }
  return s_graph_run_record_cmd_buf;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const int   id    = dt_module_param_int(mod, 3)[0];
  const char *fname = dt_module_param_string(mod, 0);
  char        filename[2*PATH_MAX+10];
  if(dt_graph_get_resource_filename(mod, fname, id, filename, sizeof(filename))) return;

  if(strstr(fname, "%"))
  { // reading a sequence of raws as a timelapse animation
    mod->flags = s_module_request_read_source;
  }
  
  if(load_raw(mod, id + graph->frame, filename)) return;
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  // we know we only have one connector called "output" (see our "connectors" file)
  mod->connector[0].roi.full_wd = mod_data->img.width;
  mod->connector[0].roi.full_ht = mod_data->img.height;

  mod->img_param.colour_primaries = s_colour_primaries_custom;
  mod->img_param.colour_trc       = s_colour_trc_linear;

  // set a bit of metadata
  snprintf(mod->img_param.maker, sizeof(mod->img_param.maker), "%s", mod_data->img.clean_maker);
  snprintf(mod->img_param.model, sizeof(mod->img_param.model), "%s", mod_data->img.clean_model);
  mod->img_param.iso = mod_data->img.iso;
  mod->img_param.aperture = mod_data->img.aperture;
  mod->img_param.exposure = mod_data->img.exposure;
  mod->img_param.focal_length = mod_data->img.focal_length;
  mod->img_param.orientation = mod_data->img.orientation;
  size_t r = snprintf(mod->img_param.datetime, sizeof(mod->img_param.datetime), "%s", mod_data->img.datetime);
  if(r >= sizeof(mod->img_param.datetime)) mod->img_param.datetime[sizeof(mod->img_param.datetime)-1] = 0;
  float *noise_a = (float*)dt_module_param_float(mod, 1);
  float *noise_b = (float*)dt_module_param_float(mod, 2);
  if(noise_a[0] == 0.0f && noise_b[0] == 0.0f)
  {
    char pname[512];
    snprintf(pname, sizeof(pname), "nprof/%s-%s-%d.nprof",
        mod->img_param.maker,
        mod->img_param.model,
        (int)mod->img_param.iso);
    FILE *f = dt_graph_open_resource(graph, id, pname, "rb");
    if(f)
    {
      float a = 0.0f, b = 0.0f;
      int num = fscanf(f, "%g %g", &a, &b);
      if(num == 2)
      {
        noise_a[0] = mod->img_param.noise_a = a;
        noise_b[0] = mod->img_param.noise_b = b;
      }
      fclose(f);
    }
  }
  else
  {
    mod->img_param.noise_a = noise_a[0];
    mod->img_param.noise_b = noise_b[0];
  }

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = mod_data->img.blacklevels[k];
    mod->img_param.white[k]        = mod_data->img.whitelevels[k];
    mod->img_param.whitebalance[k] = mod_data->img.wb_coeffs[k];
    mod->img_param.crop_aabb[k]    = mod_data->img.crop_aabb[k];
  }
  if(mod->img_param.crop_aabb[2] >= INT_MAX || mod->img_param.crop_aabb[3] >= INT_MAX)
  { // this can't be right
    mod->img_param.crop_aabb[0] = 0;
    mod->img_param.crop_aabb[1] = 0;
    mod->img_param.crop_aabb[2] = mod_data->img.width;
    mod->img_param.crop_aabb[3] = mod_data->img.height;
  }
  // normalise wb
  mod->img_param.whitebalance[0] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[2] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[3] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[1] = 1.0f;
  mod->img_param.filters = mod_data->img.filters;

  if(mod_data->img.cpp == 3)
  {
    mod->connector[0].chan = dt_token("rgba");
    mod->img_param.filters = 0;
  }

  // XXX TODO CAT from img.illuminant if it's not D65
  // uint32_t illuminant; // D65 D50 D55 D75 A or we don't know
  float xyz_to_cam[12], mat[9] = {0};
  // get d65 camera matrix from rawloader
  for(int j=0;j<3;j++) for(int i=0;i<3;i++)
    xyz_to_cam[3*j+i] = mod_data->img.xyz_to_cam[j][i];
  mat3inv(mat, xyz_to_cam);

  // compute matrix camrgb -> rec2020 d65
  double cam_to_xyz[] = {
    mat[0], mat[1], mat[2],
    mat[3], mat[4], mat[5],
    mat[6], mat[7], mat[8]};

  const float xyz_to_rec2020[] = {
    1.7166511880, -0.3556707838, -0.2533662814,
    -0.6666843518,  1.6164812366,  0.0157685458,
    0.0176398574, -0.0427706133,  0.9421031212
  };
  float cam_to_rec2020[9] = {0.0f};
  for(int j=0;j<3;j++) for(int i=0;i<3;i++) for(int k=0;k<3;k++)
    cam_to_rec2020[3*j+i] +=
      xyz_to_rec2020[3*j+k] * cam_to_xyz[3*k+i];
  for(int k=0;k<9;k++)
    mod->img_param.cam_to_rec2020[k] = cam_to_rec2020[k];
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const int   id    = dt_module_param_int(mod, 3)[0];
  const char *fname = dt_module_param_string(mod, 0);
  char        filename[2*PATH_MAX+10];
  if(dt_graph_get_resource_filename(mod, fname, id + mod->graph->frame, filename, sizeof(filename)))
    return 1;
  int err = load_raw(mod, id + mod->graph->frame, filename);
  if(err) return 1;
  // TODO: if img.data_type == 1 it's an f32 buffer instead.
  uint16_t *buf = (uint16_t *)mapped;

  // dimensions of uncropped image
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  int wd = mod_data->img.width;
  int ht = mod_data->img.height;

  int icpp = mod_data->img.cpp;
  int ocpp = icpp == 3 ? 4 : 1;
  int ox = mod_data->img.cfa_off_x;
  int oy = mod_data->img.cfa_off_y;
  int stride = mod_data->img.stride * icpp;
  if(icpp == 1)
    for(int j=0;j<ht;j++)
      memcpy(buf + j*wd, ((uint16_t*)mod_data->img.data) + (j+oy)*stride + ox, sizeof(uint16_t)*wd);
  if(ocpp == 4)
  {
    for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
    {
      for(int k=0;k<3;k++)
        buf[4*(j*wd + i)+k] = ((uint16_t*)mod_data->img.data)[(j+oy)*stride + icpp*(ox + i) + k];
      buf[4*(j*wd + i)+3] = 1.0;
    }
  }
  return 0;
}
