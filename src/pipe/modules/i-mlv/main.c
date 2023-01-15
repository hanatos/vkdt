#include "modules/api.h"
#include "connector.h"
#include "core/core.h"
#include "adobe_coeff.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "video_mlv.c"

typedef struct buf_t
{
  char         filename[256]; // opened mlv if any
  mlv_header_t video;
}
buf_t;

int mat3inv(float *const dst, const float *const src)
{
#define A(y, x) src[(y - 1) * 3 + (x - 1)]
#define B(y, x) dst[(y - 1) * 3 + (x - 1)]

  const float det = A(1, 1) * (A(3, 3) * A(2, 2) - A(3, 2) * A(2, 3))
                  - A(2, 1) * (A(3, 3) * A(1, 2) - A(3, 2) * A(1, 3))
                  + A(3, 1) * (A(2, 3) * A(1, 2) - A(2, 2) * A(1, 3));

  const float epsilon = 1e-7f;
  if(fabsf(det) < epsilon) return 1;

  const float invDet = 1.f / det;

  B(1, 1) = invDet * (A(3, 3) * A(2, 2) - A(3, 2) * A(2, 3));
  B(1, 2) = -invDet * (A(3, 3) * A(1, 2) - A(3, 2) * A(1, 3));
  B(1, 3) = invDet * (A(2, 3) * A(1, 2) - A(2, 2) * A(1, 3));

  B(2, 1) = -invDet * (A(3, 3) * A(2, 1) - A(3, 1) * A(2, 3));
  B(2, 2) = invDet * (A(3, 3) * A(1, 1) - A(3, 1) * A(1, 3));
  B(2, 3) = -invDet * (A(2, 3) * A(1, 1) - A(2, 1) * A(1, 3));

  B(3, 1) = invDet * (A(3, 2) * A(2, 1) - A(3, 1) * A(2, 2));
  B(3, 2) = -invDet * (A(3, 2) * A(1, 1) - A(3, 1) * A(1, 2));
  B(3, 3) = invDet * (A(2, 2) * A(1, 1) - A(2, 1) * A(1, 2));
#undef A
#undef B
  return 0;
}

static inline int
open_file(
    dt_module_t *mod,
    const char  *fname)
{
  buf_t *dat = mod->data;
  if(dat && !strcmp(dat->filename, fname))
    return 0; // already open

  fprintf(stderr, "[o-mlv] opening `%s'\n", fname);

  const char *filename = fname;
  char tmpfn[2*PATH_MAX+10]; // replicate api.h:dt_graph_open_resource
  if(filename[0] != '/') // relative paths
  {
    snprintf(tmpfn, sizeof(tmpfn), "%s/%s", mod->graph->searchpath, fname);
    filename = tmpfn;
    FILE *f = fopen(filename, "rb");
    if(!f)
    {
      snprintf(tmpfn, sizeof(tmpfn), "%s/%s", mod->graph->basedir, fname);
      filename = tmpfn;
      f = fopen(filename, "rb");
      if(!f) return 1; // damn that.
    }
    fclose(f);
  }

  if(mlv_open_clip(&dat->video, filename, 0))//MLV_OPEN_PREVIEW)
    return 1;

  snprintf(dat->filename, sizeof(dat->filename), "%s", fname);
  return 0;
}

static inline int
read_frame(
    dt_module_t *mod,
    void        *mapped)
{
  buf_t *dat = mod->data;
  int frame = MIN(mod->graph->frame, dat->video.MLVI.videoFrameCount-1);
  return mlv_get_frame(
    &dat->video, frame, mapped);
}

int init(dt_module_t *mod)
{
  buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  mod->flags = s_module_request_read_source;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  buf_t *dat= mod->data;
  if(dat->filename[0])
  {
    mlv_header_cleanup(&dat->video);
    dat->filename[0] = 0;
  }
  free(dat);
  mod->data = 0;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load image if not happened yet
  const char *filename = dt_module_param_string(mod, 0);
  if(open_file(mod, filename)) return;
  buf_t *dat = mod->data;
  mod->connector[0].roi.full_wd = dat->video.RAWI.xRes;
  mod->connector[0].roi.full_ht = dat->video.RAWI.yRes;
  float b = dat->video.RAWI.raw_info.black_level;
  float w = dat->video.RAWI.raw_info.white_level;
  mod->img_param = (dt_image_params_t) {
    .black          = {b, b, b, b},
    .white          = {w, w, w, w},
    .whitebalance   = {1.0, 1.0, 1.0, 1.0},
    .filters        = 0x5d5d5d5d, // anything not 0 or 9 will be bayer starting at R
    .crop_aabb      = {0, 0, dat->video.RAWI.xRes, dat->video.RAWI.yRes},
    .cam_to_rec2020 = {1, 0, 0, 0, 1, 0, 0, 0, 1},

    .orientation    = 0,
    .exposure       = dat->video.EXPO.shutterValue * 1e-6f,
    .aperture       = dat->video.LENS.aperture * 0.01f,
    .iso            = dat->video.EXPO.isoValue,
    .focal_length   = dat->video.LENS.focalLength,
    .snd_samplerate = dat->video.frame_rate,
    .snd_format     = 2, // ==SND_PCM_FORMAT_S16_LE, // XXX use dat->video.WAVI.bytesPerSample
    .snd_channels   = dat->video.WAVI.channels,

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation" // gcc does not understand dates
  snprintf(mod->img_param.datetime, sizeof(mod->img_param.datetime), "%4d%2d%2d %2d:%2d%2d",
      dat->video.RTCI.tm_year, dat->video.RTCI.tm_mon, dat->video.RTCI.tm_mday,
      dat->video.RTCI.tm_hour, dat->video.RTCI.tm_min, dat->video.RTCI.tm_sec);
#pragma GCC diagnostic pop
  snprintf(mod->img_param.model, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  snprintf(mod->img_param.maker, sizeof(mod->img_param), "%s", dat->video.IDNT.cameraName);
  for(int i=0;i<sizeof(mod->img_param.maker);i++) if(mod->img_param.maker[i] == ' ') mod->img_param.maker[i] = 0;
  mod->graph->frame_cnt  = dat->video.MLVI.videoFrameCount;
  mod->graph->frame_rate = dat->video.frame_rate;

  // load noise profile:
  char pname[512];
  snprintf(pname, sizeof(pname), "data/nprof/%s-%s-%d.nprof",
      mod->img_param.maker,
      mod->img_param.model,
      (int)mod->img_param.iso);
  FILE *f = dt_graph_open_resource(graph, 0, pname, "rb");
  if(f)
  {
    float a = 0.0f, b = 0.0f;
    int num = fscanf(f, "%g %g", &a, &b);
    if(num == 2)
    {
      mod->img_param.noise_a = a;
      mod->img_param.noise_b = b;
    }
    fclose(f);
  }
  
  // load colour matrix
  float xyz_to_cam[12], mat[9] = {0};
  if(dt_dcraw_adobe_coeff(mod->img_param.model, (float(*)[12]) xyz_to_cam))
    mat[0] = mat[4] = mat[8] = 1.0;
  else mat3inv(mat, xyz_to_cam);

  // compute matrix camrgb -> rec2020 d65
  double cam_to_xyz[] = {
    mat[0], mat[1], mat[2],
    mat[3], mat[4], mat[5],
    mat[6], mat[7], mat[8]};
  // find white balance baked into matrix:
  mod->img_param.whitebalance[0] = (cam_to_xyz[0]+cam_to_xyz[1]+cam_to_xyz[2]);
  mod->img_param.whitebalance[1] = (cam_to_xyz[3]+cam_to_xyz[4]+cam_to_xyz[5]);
  mod->img_param.whitebalance[2] = (cam_to_xyz[6]+cam_to_xyz[7]+cam_to_xyz[8]);
  mod->img_param.whitebalance[0] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[2] /= mod->img_param.whitebalance[1];
  mod->img_param.whitebalance[1]  = 1.0f;

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
  const char *filename = dt_module_param_string(mod, 0);
  if(open_file(mod, filename)) return 1;
  return read_frame(mod, mapped);
}

int audio(
    dt_module_t  *mod,
    const int     frame,
    uint8_t     **samples)
{
  buf_t *dat = mod->data;
  if(!dat || !dat->filename[0] || !dat->video.audio_data || !dat->video.audio_size)
    return 0;
  uint64_t bytes_per_frame = dat->video.WAVI.bytesPerSecond / dat->video.frame_rate;
  *samples = dat->video.audio_data + frame * bytes_per_frame;

  bytes_per_frame = MIN(bytes_per_frame, 
    MAX(0, dat->video.audio_data + dat->video.audio_size - *samples));

  // samples: stereo and 2 bytes/channel => /4
  return bytes_per_frame/4;
}
