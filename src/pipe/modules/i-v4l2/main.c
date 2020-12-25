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

#include <linux/videodev2.h>

typedef struct buf_t
{
  char device[256];
  int fd;
  struct v4l2_format format;
}
buf_t;

static inline int
open_device(
    dt_module_t *mod,
    const char *device)
{
  buf_t *dat = mod->data;
  if(dat && !strcmp(dat->device, device))
    return 0; // already open

  if((dat->fd = open(device, O_RDWR)) < 0)
  {
    perror("[i-v4l2] open");
    return 1; // fd not open yet
  }

  struct v4l2_capability cap;
  if(ioctl(dat->fd, VIDIOC_QUERYCAP, &cap) < 0)
  {
    perror("[i-v4l2] VIDIOC_QUERYCAP");
    goto error;
  }

  if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
  {
    fprintf(stderr, "[i-v4l2] the device does not handle video capture\n");
    goto error;
  }

  if(!(cap.capabilities & V4L2_CAP_STREAMING))
  {
    fprintf(stderr, "[i-v4l2] the device does not handle streaming\n");
    goto error;
  }

  dat->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  dat->format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  dat->format.fmt.pix.width = 1280;
  dat->format.fmt.pix.height = 720;

  if(ioctl(dat->fd, VIDIOC_S_FMT, &dat->format) < 0)
  {
    perror("[i-v4l2] VIDIOC_S_FMT");
    goto error;
  }
  // kernel doc suggests to use outside settings intstead, as an option:
  // if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
  // errno_exit("VIDIOC_G_FMT");

  // now wd and ht may have changed

  struct v4l2_requestbuffers bufrequest = {
    .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory = V4L2_MEMORY_USERPTR,
    .count  = 1,
  };
  if(ioctl(dat->fd, VIDIOC_REQBUFS, &bufrequest) < 0)
  {
    perror("[i-v4l2] VIDIOC_REQBUFS");
    goto error;
  }

  struct v4l2_buffer bufferinfo = {
    .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory = V4L2_MEMORY_USERPTR,
    .index  = 0,
  };
  if(ioctl(dat->fd, VIDIOC_QUERYBUF, &bufferinfo) < 0)
  {
    perror("[i-v4l2] VIDIOC_QUERYBUF");
    goto error;
  }

  // linux docs say to do QBUF before STREAMON

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if(ioctl(dat->fd, VIDIOC_STREAMON, &type) < 0)
  {
    perror("[i-v4l2] VIDIOC_STREAMON");
    goto error;
  }

  snprintf(dat->device, sizeof(dat->device), "%s", device);
  return 0;
error:
  close(dat->fd);
  dat->fd = -1;
  return 1;
}

static inline int
read_frame(
    dt_module_t *mod,
    void        *mapped)
{
  buf_t *dat = mod->data;

  // enqueue buffer:
  struct v4l2_buffer buf = {
    .type      = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory    = V4L2_MEMORY_USERPTR,
    .index     = 0,
    .m = {
      .userptr = (unsigned long)mapped,
    },
    .length    = dt_connector_bufsize(mod->connector),
  };
  if(ioctl(dat->fd, VIDIOC_QBUF, &buf) < 0)
  {
    fprintf(stderr, "[i-v4l2] could not enqueue buffer!\n");
    return 1;
  }

  // dequeue buffer (blocks until data arrives)
  if(ioctl(dat->fd, VIDIOC_DQBUF, &buf) < 0)
  {
    fprintf(stderr, "[i-v4l2] could not dequeue buffer!\n");
    return 1;
  }

  return 0;
}

static inline void
close_device(
    dt_module_t *mod)
{
  buf_t *dat = mod->data;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(dat->fd, VIDIOC_STREAMOFF, &type);
  if(dat->fd != -1) close(dat->fd);
  dat->fd = -1;
  dat->device[0] = 0;
}

int init(dt_module_t *mod)
{
  buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  dat->fd = -1;
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  buf_t *dat= mod->data;
  if(dat->device[0])
  {
    close_device(mod);
    dat->device[0] = 0;
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
  const char *device = dt_module_param_string(mod, 0);
  if(open_device(mod, device)) return;
  buf_t *dat = mod->data;
  mod->connector[0].roi.full_wd = dat->format.fmt.pix.width;
  mod->connector[0].roi.full_ht = dat->format.fmt.pix.height;
}

int read_source(
    dt_module_t *mod,
    void *mapped)
{
  const char *device = dt_module_param_string(mod, 0);
  if(open_device(mod, device)) return 1;
  return read_frame(mod, mapped);
}
