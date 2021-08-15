#include "modules/api.h"

#include <jpeglib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <setjmp.h>

typedef struct lst_t
{
  char        *data;     // list file in one chunk
  const char **filename; // pointers to lines
  int          cnt;      // number of files in list
  uint32_t    *dim;      // dimensions of the images
}
lst_t;

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

static void
read_header(
    dt_module_t *mod,
    uint32_t    *dim,
    const char  *filename)
{
  dim[0] = dim[1] = 0;
  FILE *f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!f) return;

  struct jpeg_decompress_struct dinfo;
  jpgerr_t err;
  dinfo.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = error_exit;
  if(setjmp(err.setjmp_buffer)) goto error;
  jpeg_create_decompress(&dinfo);
  jpeg_stdio_src(&dinfo, f);
  jpeg_read_header(&dinfo, TRUE);
  dinfo.out_color_space = JCS_RGB;
  dinfo.out_color_components = 3;
  dim[0] = dinfo.image_width;
  dim[1] = dinfo.image_height;
error:
  jpeg_destroy_decompress(&dinfo);
  fclose(f);
}

static void
read_full(
    dt_module_t *mod,
    const char  *filename,
    uint8_t     *out)
{
  FILE *f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!f) return;

  struct jpeg_decompress_struct dinfo;
  jpgerr_t err;
  dinfo.err = jpeg_std_error(&err.pub);
  err.pub.error_exit = error_exit;
  if(setjmp(err.setjmp_buffer)) goto error;
  jpeg_create_decompress(&dinfo);
  jpeg_stdio_src(&dinfo, f);
  jpeg_read_header(&dinfo, TRUE);
  dinfo.out_color_space = JCS_RGB;
  dinfo.out_color_components = 3;

  (void)jpeg_start_decompress(&dinfo);
  JSAMPROW row_pointer[1] = { malloc(dinfo.output_width * dinfo.num_components) };
  uint8_t *tmp = out;
  while(dinfo.output_scanline < dinfo.image_height)
  {
    if(jpeg_read_scanlines(&dinfo, row_pointer, 1) != 1) goto error;
    for(unsigned int i = 0; i < dinfo.image_width; i++)
    {
      for(int k = 0; k < 3; k++) tmp[4 * i + k] = row_pointer[0][3 * i + k];
      tmp[4*i+3] = 255;
    }
    tmp += 4 * dinfo.image_width;
  }
  free(row_pointer[0]);
  (void)jpeg_finish_decompress(&dinfo);
error:
  jpeg_destroy_decompress(&dinfo);
  fclose(f);
  return;
}

int init(dt_module_t *mod)
{
  lst_t *lst = calloc(sizeof(lst_t), 1);
  mod->data = lst;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  lst_t *lst = mod->data;
  if(lst->data)     free(lst->data);
  if(lst->dim)      free(lst->dim);
  if(lst->filename) free(lst->filename);
  free(lst);
  mod->data = 0;
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  lst_t *lst = mod->data;
  // load list of images, keep number of files and the dimension of their images.
  const char *filename = dt_module_param_string(mod, 0);
  FILE *f = dt_graph_open_resource(mod->graph, filename, "rb");
  if(!f) return;
  fseek(f, 0, SEEK_END);
  uint64_t size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if(lst->data)     free(lst->data);
  if(lst->dim)      free(lst->dim);
  if(lst->filename) free(lst->filename);
  lst->data = malloc(size);
  fread(lst->data, size, 1, f);
  fclose(f);
  int cnt = 0;
  for(int i=0;i<size;i++)
    if(lst->data[i] == '\n') { lst->data[i] = 0; cnt++; }

  lst->cnt = cnt;
  lst->dim = calloc(2*sizeof(uint32_t), cnt);
  lst->filename = calloc(sizeof(const char*), cnt+1);
  lst->filename[0] = lst->data;
  cnt = 0;
  for(int i=0;i<size;i++)
    if(lst->data[i] == 0)
      lst->filename[++cnt] = lst->data + i + 1;

  // for each one image, open the header and find the size of the image
  for(int i=0;i<lst->cnt;i++)
    read_header(mod, lst->dim + 2*i, lst->filename[i]);

  uint32_t max_wd = 0, max_ht = 0;
  for(int i=0;i<lst->cnt;i++)
  {
    max_wd = MAX(max_wd, lst->dim[2*i+0]);
    max_ht = MAX(max_ht, lst->dim[2*i+1]);
  }

  mod->connector[0].roi.full_wd = max_wd; // will be used to allocate staging buffer
  mod->connector[0].roi.full_ht = max_ht;
  mod->connector[0].roi.wd = max_wd;
  mod->connector[0].roi.ht = max_ht;
  mod->connector[0].roi.scale = 1;
  mod->connector[0].array_length = cnt;
  // instruct the connector that we have an array with different image resolution for every element:
  mod->connector[0].array_dim = lst->dim;
  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 1.0f;
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.filters = 0;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  lst_t *lst = mod->data;
  read_full(mod, lst->filename[p->a], mapped);
  return 0;
}
