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

typedef enum io_method_t
{
  s_io_method_mmap,
  s_io_method_userptr,
}
io_method_t;

typedef struct buf_t
{
  char               device[256]; // opened device if any
  int                fd;
  struct v4l2_format format;      // pixel format and buffer dimensions
  io_method_t        io_method;   // userptr or mmap
  void              *buffer;      // memory mapped buffer or 0
  size_t             buffer_len;
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
  // request away, we'll get the next supported res anyways:
  dat->format.fmt.pix.width  = 1920;
  dat->format.fmt.pix.height = 1080;

  if(ioctl(dat->fd, VIDIOC_S_FMT, &dat->format) < 0)
  { // does not support YUYV, try YUV420
    dat->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dat->format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    dat->format.fmt.pix.width  = 1920;
    dat->format.fmt.pix.height = 1080;
    if(ioctl(dat->fd, VIDIOC_S_FMT, &dat->format) < 0)
    { // last try GREY
      dat->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      dat->format.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
      if(ioctl(dat->fd, VIDIOC_S_FMT, &dat->format) < 0)
      {
        perror("[i-v4l2] VIDIOC_S_FMT");
        goto error;
      }
    }
  }

  // kernel doc suggests to use outside settings intstead, as an option:
  if (-1 == ioctl(dat->fd, VIDIOC_G_FMT, &dat->format))
    perror("[i-v4l2] VIDIOC_G_FMT");

  // now wd and ht may have changed

  struct v4l2_requestbuffers bufrequest = {
    .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory = V4L2_MEMORY_USERPTR,
    .count  = 1,
  };
  dat->io_method = s_io_method_userptr;
  if(ioctl(dat->fd, VIDIOC_REQBUFS, &bufrequest) < 0)
  { // failed userptr, try mmap
    struct v4l2_requestbuffers bufrequest = {
      .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory = V4L2_MEMORY_MMAP,
      .count  = 1,
    };
    if(ioctl(dat->fd, VIDIOC_REQBUFS, &bufrequest) < 0)
    {
      perror("[i-v4l2] VIDIOC_REQBUFS");
      goto error;
    }
    dat->io_method = s_io_method_mmap;
  }

  struct v4l2_buffer bufferinfo = {
    .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .memory = dat->io_method == s_io_method_mmap ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR,
    .index  = 0,
  };
  if(ioctl(dat->fd, VIDIOC_QUERYBUF, &bufferinfo) < 0)
  {
    perror("[i-v4l2] VIDIOC_QUERYBUF");
    goto error;
  }

  if(dat->io_method == s_io_method_mmap)
  {
    dat->buffer = mmap(0, bufferinfo.length, PROT_READ | PROT_WRITE, MAP_SHARED, dat->fd, bufferinfo.m.offset);
    if(dat->buffer == MAP_FAILED)
    {
      perror("[i-v4l2] memory mapping failed");
      goto error;
    }
  }
  else dat->buffer = 0;

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
  struct v4l2_buffer buf;
  if(dat->io_method == s_io_method_mmap)
  {
    buf = (struct v4l2_buffer) {
      .type      = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory    = V4L2_MEMORY_MMAP,
      .index     = 0,
    };
  }
  else
  { // userptr
    buf = (struct v4l2_buffer) {
      .type      = V4L2_BUF_TYPE_VIDEO_CAPTURE,
      .memory    = V4L2_MEMORY_USERPTR,
      .index     = 0,
      .m         = { .userptr = (unsigned long)mapped },
      .length    = dt_connector_bufsize(mod->connector, mod->connector[0].roi.wd, mod->connector[0].roi.ht),
    };
  }
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

  if(dat->io_method == s_io_method_mmap)
    memcpy(mapped, dat->buffer, buf.bytesused);

  return 0;
}

static inline void
close_device(
    dt_module_t *mod)
{
  buf_t *dat = mod->data;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(dat->fd, VIDIOC_STREAMOFF, &type);
  if(dat->buffer) munmap(dat->buffer, dat->buffer_len);
  dat->buffer = 0;
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
  mod->flags = s_module_request_read_source;
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
  if(dat->format.fmt.pix.pixelformat == V4L2_PIX_FMT_GREY)
  {
    mod->connector[0].chan   = dt_token("r");
    mod->connector[0].format = dt_token("ui8");
  }
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const char *device = dt_module_param_string(mod, 0);
  if(open_device(mod, device)) return 1;
  return read_frame(mod, mapped);
}

void
create_nodes(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  buf_t *dat = module->data;
  if(dat->format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
  {
    dt_roi_t roi2 = module->connector[0].roi;
    roi2.full_wd /= 2;
    roi2.wd /= 2;
    const int id_in = dt_node_add(graph, module, "i-v4l2", "source", 
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 1,
        "source", "source", "rgba", "ui8", &roi2);
    graph->node[id_in].flags = s_module_request_read_source;
    int pc[] = {0};
    const int id_conv = dt_node_add(graph, module, "i-v4l2", "conv",
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1,
        sizeof(pc), pc, 2,
        "input",  "read",  "rgba", "ui8", dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[0].roi);
    dt_connector_copy(graph, module, 0, id_conv, 1);
    dt_node_connect  (graph, id_in,  0, id_conv, 0);
  }
  else if(dat->format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)
  {
    dt_roi_t roi2 = module->connector[0].roi;
    roi2.full_ht = roi2.full_ht * 3 / 2;
    roi2.ht      = roi2.ht * 3 / 2;
    const int id_in = dt_node_add(graph, module, "i-v4l2", "source", 
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 1,
        "source", "source", "r", "ui8", &roi2);
    graph->node[id_in].flags = s_module_request_read_source;
    int pc[] = {1};
    const int id_conv = dt_node_add(graph, module, "i-v4l2", "conv",
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1,
        sizeof(pc), pc, 2,
        "input",  "read",  "r",    "ui8", dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[0].roi);
    dt_connector_copy(graph, module, 0, id_conv, 1);
    dt_node_connect  (graph, id_in, 0, id_conv, 0);
  }
  else if(dat->format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)
  {
    const int id_in = dt_node_add(graph, module, "i-v4l2", "source", 
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 1,
        "source", "source", "yuv", "yuv", &module->connector[0].roi);
    graph->node[id_in].flags = s_module_request_read_source;
    int pc[] = {2};
    const int id_conv = dt_node_add(graph, module, "i-v4l2", "conv",
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1,
        sizeof(pc), pc, 2,
        "input",  "read",  "yuv",  "yuv", dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[0].roi);
    dt_connector_copy(graph, module, 0, id_conv, 1);
    dt_node_connect  (graph, id_in, 0, id_conv, 0);
  }
  else if(dat->format.fmt.pix.pixelformat == V4L2_PIX_FMT_GREY)
  {
    const int id_in = dt_node_add(graph, module, "i-v4l2", "source", 
        module->connector[0].roi.wd, module->connector[0].roi.ht, 1, 0, 0, 1,
        "source", "source", "r", "ui8", &module->connector[0].roi);
    graph->node[id_in].flags = s_module_request_read_source;
    dt_connector_copy(graph, module, 0, id_in, 0);
  }
}
