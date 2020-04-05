#include "modules/api.h"

#include <jpeglib.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <setjmp.h>

typedef struct jpginput_buf_t
{
  char filename[PATH_MAX];
  uint32_t width, height;
  struct jpeg_decompress_struct dinfo;
  FILE *f;
}
jpginput_buf_t;

typedef struct jpgerr_t
{
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
}
jpgerr_t;

static void
error_exit(j_common_ptr cinfo)
{
  jpgerr_t *myerr = (jpgerr_t *)cinfo->err;
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

static int 
read_header(
    dt_module_t *mod,
    const char *filename)
{
  jpginput_buf_t *jpg = mod->data;
  if(jpg && !strcmp(jpg->filename, filename))
    return 0; // already loaded
  assert(jpg); // this should be inited in init()

  jpg->f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!jpg->f) return 1;

  jpgerr_t err;
  jpg->dinfo.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = error_exit;
  if(setjmp(err.setjmp_buffer))
  {
    jpeg_destroy_decompress(&(jpg->dinfo));
    fclose(jpg->f);
    jpg->filename[0] = 0;
    return 1;
  }
  jpeg_create_decompress(&(jpg->dinfo));
  jpeg_stdio_src(&(jpg->dinfo), jpg->f);
  // setup_read_exif(&(jpg->dinfo));
  // setup_read_icc_profile(&(jpg->dinfo));
  // jpg->dinfo.buffered_image = TRUE;
  jpeg_read_header(&(jpg->dinfo), TRUE);
  jpg->dinfo.out_color_space = JCS_RGB;
  jpg->dinfo.out_color_components = 3;
  jpg->width = jpg->dinfo.image_width;
  jpg->height = jpg->dinfo.image_height;

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 1.0f;
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.filters = 0;

  snprintf(jpg->filename, sizeof(jpg->filename), "%s", filename);
  return 0;
}

static int
read_plain(
    jpginput_buf_t *jpg, uint8_t *out)
{
  JSAMPROW row_pointer[1];
  row_pointer[0] = malloc(jpg->dinfo.output_width * jpg->dinfo.num_components);
  uint8_t *tmp = out;
  while(jpg->dinfo.output_scanline < jpg->dinfo.image_height)
  {
    if(jpeg_read_scanlines(&(jpg->dinfo), row_pointer, 1) != 1)
    {
      jpeg_destroy_decompress(&(jpg->dinfo));
      free(row_pointer[0]);
      fclose(jpg->f);
      jpg->filename[0] = 0;
      return 1;
    }
    for(unsigned int i = 0; i < jpg->dinfo.image_width; i++)
    {
      for(int k = 0; k < 3; k++) tmp[4 * i + k] = row_pointer[0][3 * i + k];
      tmp[4*i+3] = 255;
    }
    tmp += 4 * jpg->width;
  }
  free(row_pointer[0]);
  return 0;
}

static int
jpeg_read(
    jpginput_buf_t *jpg, uint8_t *out)
{
  jpgerr_t err;
  jpg->dinfo.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = error_exit;
  if(setjmp(err.setjmp_buffer))
  {
    jpeg_destroy_decompress(&(jpg->dinfo));
    fclose(jpg->f);
    jpg->filename[0] = 0;
    return 1;
  }

  (void)jpeg_start_decompress(&(jpg->dinfo));
  read_plain(jpg, out);
  (void)jpeg_finish_decompress(&(jpg->dinfo));
  // i think libjpeg doesn't want us to retain the state, at least not the way
  // by splitting here. so we'll just clean it all up:
  jpeg_destroy_decompress(&(jpg->dinfo));
  fclose(jpg->f);
  jpg->filename[0] = 0;
  return 0;
}

int init(dt_module_t *mod)
{
  jpginput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  jpginput_buf_t *jpg = mod->data;
  jpeg_destroy_decompress(&(jpg->dinfo));
  if(jpg->filename[0])
  {
    if(jpg->f) fclose(jpg->f);
    jpg->filename[0] = 0;
  }
  free(jpg);
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
  if(read_header(mod, filename)) return;
  jpginput_buf_t *jpg = mod->data;
  mod->connector[0].roi.full_wd = jpg->width;
  mod->connector[0].roi.full_ht = jpg->height;
}

int read_source(
    dt_module_t *mod,
    void *mapped)
{
  const char *filename = dt_module_param_string(mod, 0);
  if(read_header(mod, filename)) return 1;
  jpginput_buf_t *jpg = mod->data;
  jpeg_read(jpg, mapped);
  return 0;
}
