#include "modules/api.h"
#include "core/fs.h"
#include "pipe/icc-profiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

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

#define EXIF_MARKER (JPEG_APP0 + 1) /* JPEG marker code for Exif */
#define ICC_MARKER (JPEG_APP0 + 2)  /* JPEG marker code for ICC */
#define ICC_OVERHEAD_LEN 14         /* size of non-profile data in APP2 */
#define MAX_BYTES_IN_MARKER 65533   /* maximum data len of a JPEG marker */
#define MAX_DATA_BYTES_IN_MARKER (MAX_BYTES_IN_MARKER - ICC_OVERHEAD_LEN)
/*
 * stolen from darktable imageio, i.e. this is GPLv3 code:
 * This routine writes the given ICC profile data into a JPEG file.
 * It *must* be called AFTER calling jpeg_start_compress() and BEFORE
 * the first call to jpeg_write_scanlines().
 * (This ordering ensures that the APP2 marker(s) will appear after the
 * SOI and JFIF or Adobe markers, but before all else.)
 */
static void
write_icc_profile(
    j_compress_ptr cinfo,
    const uint8_t *icc_data_ptr,
    unsigned int   icc_data_len)
{
  unsigned int num_markers; /* total number of markers we'll write */
  int cur_marker = 1;       /* per spec, counting starts at 1 */

  /* Calculate the number of markers we'll need, rounding up of course */
  num_markers = icc_data_len / MAX_DATA_BYTES_IN_MARKER;
  if(num_markers * MAX_DATA_BYTES_IN_MARKER != icc_data_len) num_markers++;

  while(icc_data_len > 0)
  {
    /* length of profile to put in this marker */
    unsigned int length = icc_data_len;
    if(length > MAX_DATA_BYTES_IN_MARKER) length = MAX_DATA_BYTES_IN_MARKER;
    icc_data_len -= length;

    /* Write the JPEG marker header (APP2 code and marker length) */
    jpeg_write_m_header(cinfo, ICC_MARKER, (unsigned int)(length + ICC_OVERHEAD_LEN));

    /* Write the marker identifying string "ICC_PROFILE" (null-terminated).
     * We code it in this less-than-transparent way so that the code works
     * even if the local character set is not ASCII.
     */
    jpeg_write_m_byte(cinfo, 0x49);
    jpeg_write_m_byte(cinfo, 0x43);
    jpeg_write_m_byte(cinfo, 0x43);
    jpeg_write_m_byte(cinfo, 0x5F);
    jpeg_write_m_byte(cinfo, 0x50);
    jpeg_write_m_byte(cinfo, 0x52);
    jpeg_write_m_byte(cinfo, 0x4F);
    jpeg_write_m_byte(cinfo, 0x46);
    jpeg_write_m_byte(cinfo, 0x49);
    jpeg_write_m_byte(cinfo, 0x4C);
    jpeg_write_m_byte(cinfo, 0x45);
    jpeg_write_m_byte(cinfo, 0x0);

    /* Add the sequencing info */
    jpeg_write_m_byte(cinfo, cur_marker);
    jpeg_write_m_byte(cinfo, (int)num_markers);

    /* Add the profile data */
    while(length--)
    {
      jpeg_write_m_byte(cinfo, *icc_data_ptr);
      icc_data_ptr++;
    }
    cur_marker++;
  }
}
#undef EXIF_MARKER
#undef ICC_MARKER
#undef ICC_OVERHEAD_LEN
#undef MAX_BYTES_IN_MARKER
#undef MAX_DATA_BYTES_IN_MARKER

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-jpg] writing '%s'\n", basename);

  const int width  = module->connector[0].roi.wd;
  const int height = module->connector[0].roi.ht;
  const uint8_t *in = buf;

  char dir[512];
  snprintf(dir, sizeof(dir), "%s", basename);
  if(fs_dirname(dir)) fs_mkdir_p(dir, 0755);

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

  if(module->img_param.colour_primaries == s_colour_primaries_adobe && module->img_param.colour_trc == s_colour_trc_gamma)
    write_icc_profile(&cinfo, icc_AdobeCompat_v2, icc_AdobeCompat_v2_len);
  else if(module->img_param.colour_primaries == s_colour_primaries_2020 && module->img_param.colour_trc == s_colour_trc_709)
    write_icc_profile(&cinfo, icc_Rec2020_v2_micro, icc_Rec2020_v2_micro_len);

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

#ifndef __ANDROID__
  const int copy_exif = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("exif")))[0];
  if(copy_exif)
  {
    char src_filename[1024] = {0};
    for(int m=0;m<module->graph->num_modules;m++)
    { // locate main input module, if it is jpg or raw (these have exif)
      const dt_module_t *mod2 = module->graph->module+m;
      if((mod2->name == dt_token("i-jpg") && mod2->inst == dt_token("main")) ||
         (mod2->name == dt_token("i-raw") && mod2->inst == dt_token("main")))
      {
        const int   id   = dt_module_param_int(mod2, dt_module_get_param(mod2->so, dt_token("startid")))[0];
        const char *base = dt_module_param_string(mod2, dt_module_get_param(mod2->so, dt_token("filename")));
        if(dt_graph_get_resource_filename(mod2, base, module->graph->frame + id, src_filename, sizeof(src_filename)))
          return;
      }
    }
    if(src_filename[0] == 0) return;

    // maybe route this string in via o-jpg params (beware of the ':' and then dt_sanitize_user_string + dt_strexpand it)
    char cmd[1024];
    if(sizeof(cmd) <= snprintf(cmd, sizeof(cmd), "%s/exiftool -TagsFromFile %s \"-all:all>all:all\" -Software=\"vkdt\" -ModifyDate=\"now\" -*orientation*= -overwrite_original %s",
          dt_pipe.basedir, src_filename, filename)) return;
    // TODO find a way to access the model-based time offset in the db (cli doesn't have a db).
    // then insert "-AllDates+=%s", timeoffset before the "now" above.

    // or async if(fork()) exec(cmd); ? for cli probably staying in this thread is safer:
    FILE *f = popen(cmd, "r");
    int ret = 0;
    if(f)
    { // drain empty
      while(!feof(f) && !ferror(f) && (fgetc(f) != EOF));
      ret = pclose(f);
    }
    if(ret)
    { // issue a gui message
      snprintf(module->graph->gui_msg_buf, sizeof(module->graph->gui_msg_buf),
          "o-jpg: unable to run exiftool to copy metadata! maybe you need to install it?");
      module->graph->gui_msg = module->graph->gui_msg_buf;
    }
  }
#endif
}
