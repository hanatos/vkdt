#include "modules/api.h"
#include "connector.h"

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

static inline int
open_file(
    dt_module_t *mod,
    const char  *fname)
{
  buf_t *dat = mod->data;
  if(dat && !strcmp(dat->filename, fname))
    return 0; // already open

  const char *filename = fname;
  char tmpfn[1024]; // replicate api.h:dt_graph_open_resource
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
  return mlv_get_frame(
    &dat->video, mod->graph->frame, mapped);
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
    .datetime       = "20200101",// TODO: use  video->RTCI.tm_mday video->RTCI.tm_mon video->RTCI.tm_year (hour min sec)
    .maker          = "Canon", // TODO: copy IDNT.cameraName
    .model          = "5D Mark II",
    .exposure       = dat->video.EXPO.shutterValue * 1e-6f,
    .aperture       = dat->video.LENS.aperture,
    .iso            = dat->video.EXPO.isoValue,
    .focal_length   = dat->video.LENS.focalLength,

    .noise_a = 1.0, // gauss
    .noise_b = 1.0, // poisson
  };
  // TODO: compute and write: (see ../i-raw-main.cc)
  // memcpy(.cam_to_rec2020 , )
}

int read_source(
    dt_module_t *mod,
    void        *mapped)
{
  const char *filename = dt_module_param_string(mod, 0);
  if(open_file(mod, filename)) return 1;
  return read_frame(mod, mapped);
}
