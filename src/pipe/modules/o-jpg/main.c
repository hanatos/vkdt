#include "modules/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>

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

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-jpg] writing '%s'\n", basename);

  const int width  = module->connector[0].roi.wd;
  const int height = module->connector[0].roi.ht;
  const uint8_t *in = buf;

  char filename[512];
  snprintf(filename, sizeof(filename), "%s.jpg", basename);


  jpgerr_t jerr;
  struct jpeg_compress_struct cinfo;

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = error_exit;
  if(setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_compress(&cinfo);
    return;
  }
  jpeg_create_compress(&cinfo);
  FILE *f = fopen(filename, "wb");
  if(!f) return;
  jpeg_stdio_dest(&cinfo, f);

  cinfo.image_width  = width;
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  const float quality = dt_module_param_float(module, 1)[0];
  jpeg_set_quality(&cinfo, quality, TRUE);
  // same quality tradeoff as darktable
  if(quality > 90) cinfo.comp_info[0].v_samp_factor = 1;
  if(quality > 92) cinfo.comp_info[0].h_samp_factor = 1;
  if(quality > 95) cinfo.dct_method = JDCT_FLOAT;
  if(quality < 50) cinfo.dct_method = JDCT_IFAST;
  if(quality < 80) cinfo.smoothing_factor = 20;
  if(quality < 60) cinfo.smoothing_factor = 40;
  if(quality < 40) cinfo.smoothing_factor = 60;
  cinfo.optimize_coding = 1;
  cinfo.density_unit = 1;
  cinfo.X_density = 300;
  cinfo.Y_density = 300;

  jpeg_start_compress(&cinfo, TRUE);

  uint8_t *row = malloc((size_t)3 * width * sizeof(uint8_t));
  while(cinfo.next_scanline < cinfo.image_height)
  {
    JSAMPROW tmp[1];
    const uint8_t *buf = in + (size_t)cinfo.next_scanline * cinfo.image_width * 4;
    for(int i = 0; i < width; i++)
      for(int k = 0; k < 3; k++) row[3 * i + k] = buf[4 * i + k];
    tmp[0] = row;
    jpeg_write_scanlines(&cinfo, tmp, 1);
  }
  jpeg_finish_compress(&cinfo);
  free(row);
  jpeg_destroy_compress(&cinfo);
  fclose(f);
}
