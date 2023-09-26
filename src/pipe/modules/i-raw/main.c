#include "modules/api.h"
#include "mat3.h"
#include "rawloader-c/rawloader.h"

#include <stdio.h>
#include <time.h>

typedef struct rawinput_buf_t
{
  rawimage_t img;
  char filename[PATH_MAX];
  int ox, oy;
}
rawinput_buf_t;
  
void
free_raw(dt_module_t *mod)
{
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  // XXX TODO: call into rust to free stuff
  // XXX TODO: reset filename or zero the whole thing
}

int
load_raw(
    dt_module_t *mod,
    const char *filename)
{
  clock_t beg = clock();
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  assert(mod_data);
  if(!strcmp(mod_data->filename, filename))
    return 0; // already loaded
  else free_raw(mod); // maybe loaded the wrong one
// TODO load_raw: call into rust to load stuff to cache struct
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
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  free_raw(mod);
  free(mod_data);
  mod->data = 0;
}

dt_graph_run_t
check_params(
    dt_module_t *module,
    uint32_t     parid,
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
  
  if(load_raw(mod, filename)) return;
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  // we know we only have one connector called "output" (see our "connectors" file)
  mod->connector[0].roi.full_wd = mod_data->img.width;
  mod->connector[0].roi.full_ht = mod_data->img.height;

  float *noise_a = (float*)dt_module_param_float(mod, 1);
  float *noise_b = (float*)dt_module_param_float(mod, 2);
  for(int k=0;k<9;k++)
  mod->img_param.cam_to_rec2020[k] = 0.0f/0.0f; // mark as uninitialised
#ifdef VKDT_USE_EXIV2 // now essentially only for exposure time/aperture value
  dt_exif_read(&mod->img_param, filename); // FIXME: will not work for timelapses
#endif
  // set a bit of metadata from rawspeed, overwrite exiv2 because this one is more consistent:
  snprintf(mod->img_param.maker, sizeof(mod->img_param.maker), "%s", mod_data->img.clean_make);
  snprintf(mod->img_param.model, sizeof(mod->img_param.model), "%s", mod_data->img.clean_model);
#if 0
  // XXX we need this ISO value!
  mod->img_param.iso = // XXX
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
#endif

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = mod_data->img.blacklevels[k];
    mod->img_param.white[k]        = mod_data->img.whitelevels[k];
    mod->img_param.whitebalance[k] = mod_data->img.wb_coeffs[k];
    mod->img_param.crop_aabb[k]    = mod_data->img.crop_aabb[k];
  }
  // normalise wb
  mod->img_param.whitebalance[0] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[2] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[3] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[1] = 1.0f;

  if(isnanf(mod->img_param.cam_to_rec2020[0]))
  { // camera matrix not found in exif or compiled without exiv2
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
  int err = load_raw(mod, filename);
  if(err) return 1;
  uint16_t *buf = (uint16_t *)mapped;

  // dimensions of uncropped image
  rawinput_buf_t *mod_data = (rawinput_buf_t *)mod->data;
  int wd = mod_data->img.width;
  int ht = mod_data->img.height;

  // XXX TODO: do this dance in the c bindings/rust!
  int ox = mod_data->ox;
  int oy = mod_data->oy;
  wd -= ox;
  ht -= oy;
  // round down to full block size:
  const int block = mod->img_param.filters == 9u ? 3 : 2;
  wd = (wd/block)*block;
  ht = (ht/block)*block;
  size_t bufsize = (size_t)wd * ht * sizeof(uint16_t);
  // XXX here we need the ox/oy offsets + original stride to copy the right portion:
  memcpy(buf, mod_data->img.data, bufsize);
  return 0;
}
