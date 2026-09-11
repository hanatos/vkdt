#include "modules/api.h"

#include <jxl/encode.h>
#include <jxl/resizable_parallel_runner.h>
#include <jxl/version.h> // Annoying

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



// Mostly copying from darktable (and thus GPLv3), but also o-jpg, o-exr and o-pfm.
// A work in progress.



// Reason for the inclusion of `version.h`, this function isn’t in versions < 0.9.0. This is [copied from libjxl](https://github.com/libjxl/libjxl/blob/aea3a06e281fdee13e04815bfbf4f4132e7f59ea/lib/jxl/encode.cc#L1626).
// Copyright (c) the JPEG XL Project Authors. All rights reserved.
#if JPEGXL_NUMERIC_VERSION < JPEGXL_COMPUTE_NUMERIC_VERSION(0, 9, 0)
float JxlEncoderDistanceFromQuality(float quality)
{
  return quality >= 100.0 ? 0.0
         : quality >= 30
             ? 0.1 + (100 - quality) * 0.09
             : 53.0 / 3000.0 * quality * quality - 23.0 / 20.0 * quality + 25.0;
}
#endif



JxlEncoderStatus JxlAssert(JxlEncoderStatus code,
                           JxlEncoder *encoder,
                           int line)
{
  if(code != JXL_ENC_SUCCESS)
  {
    JxlEncoderError error = JxlEncoderGetError(encoder);
    fprintf(stderr, "[o-jxl] libjxl call failed with err %d (src/pipe/modules/o-jxl/main.c#L%d)\n", error, line);
  }
  return code;
}



void write_sink(
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)

{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-jxl] writing '%s'\n", basename);
  const uint16_t *p16 = buf;
  
  const size_t width  = module->connector[0].roi.wd;
  const size_t height = module->connector[0].roi.ht;

  const dt_colour_primaries_t primaries =  module->img_param.colour_primaries;
  const dt_colour_trc_t trc = module->img_param.colour_trc;

  char filename[512];
  snprintf(filename, sizeof(filename), "%s.jxl", basename);


  int error = 0;
  // Initialising these three here so I can always `goto end` and free them.
  uint16_t *pixels = NULL;
  uint8_t *out_buf = NULL;
  FILE *out_file = NULL;

  JxlEncoder *encoder = JxlEncoderCreate(NULL);

  const unsigned num_threads = JxlResizableParallelRunnerSuggestThreads(width, height);
  void *runner = JxlResizableParallelRunnerCreate(NULL);
  JxlResizableParallelRunnerSetThreads(runner, num_threads);
  if(JxlAssert(JxlEncoderSetParallelRunner(encoder,
                                           JxlResizableParallelRunner,
                                           runner),
               encoder,
               __LINE__))
  {
    error = 1;
    goto end;
  }



  JxlPixelFormat pixel_format = { 3, JXL_TYPE_FLOAT16, JXL_NATIVE_ENDIAN, 0 };

  // Set encoder basic info, just f16 for now.
  // To my understanding, JXL always stores at f32 precision in lossy mode.
  JxlBasicInfo basic_info;
  JxlEncoderInitBasicInfo(&basic_info);
  basic_info.xsize = width;
  basic_info.ysize = height;
  basic_info.bits_per_sample = 16;
  basic_info.exponent_bits_per_sample = 5;


  // Container isn’t necessary but has minimal overhead, and is needed for EXIF data.
  // exiftool can add it later, but it issues a minor error.
  if(JxlAssert(JxlEncoderUseContainer(encoder,
                                   JXL_TRUE),
               encoder,
               __LINE__))
  {
    error = 1;
    goto end;
  }



  // Automatically freed when we destroy the encoder
  JxlEncoderFrameSettings *frame_settings = JxlEncoderFrameSettingsCreate(encoder, NULL);

  const float quality = dt_module_param_float(module, 1)[0];
  // JXL natively uses ‘distance’ a [0:25] value. This aims to estimate a distance
  // roughly equivalent to what would be obtained with libjpeg-turbo with the same quality parameter.
  const float distance = JxlEncoderDistanceFromQuality(quality);

  if(quality == 100)
  {
    if(JxlAssert(JxlEncoderSetFrameLossless(frame_settings,
                                            JXL_TRUE),
                 encoder,
                 __LINE__))
    {
      error = 1;
      goto end;
    }
    basic_info.uses_original_profile = 1;
  }
  else
  {
    if(JxlAssert(JxlEncoderSetFrameDistance(frame_settings,
                                            distance),
                 encoder,
                 __LINE__))
    {
      error = 1;
      goto end;
    }
  }

  // Codestream level should be chosen automatically given the settings
  if(JxlAssert(JxlEncoderSetBasicInfo(encoder,
                                      &basic_info),
               encoder,
               __LINE__))
  {
    error = 1;
    goto end;
  }

  const int effort = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("effort")))[0];
  if(JxlAssert(JxlEncoderFrameSettingsSetOption(frame_settings,
                                                JXL_ENC_FRAME_SETTING_EFFORT,
                                                effort),
               encoder,
               __LINE__))
  {
    error = 1;
    goto end;
  }



  // Only currently support the options shown in the export GUI, except ‘custom’ as I don’t know where the custom values come from,
  // and XYZ, because of what seems to be a libjxl bug.
  JxlColorEncoding colour_encoding;

  colour_encoding.color_space = JXL_COLOR_SPACE_RGB;
  
  JxlPrimaries nativePrimaries = 0;
  switch(primaries)
  {
    case s_colour_primaries_srgb:   nativePrimaries = JXL_PRIMARIES_SRGB;
                                    colour_encoding.white_point = JXL_WHITE_POINT_D65;
                                    break;
    case s_colour_primaries_P3:     nativePrimaries = JXL_PRIMARIES_P3;
                                    colour_encoding.white_point = JXL_WHITE_POINT_D65;
                                    break;
    case s_colour_primaries_2020:   nativePrimaries = JXL_PRIMARIES_2100;
                                    colour_encoding.white_point = JXL_WHITE_POINT_D65;
                                    break;
                                    // Derived from section §4.3.1.1 of [Adobe® RGB (1998) Color Image Encoding]
                                    // (https://www.adobe.com/digitalimag/pdfs/AdobeRGB1998.pdf).
    case s_colour_primaries_adobe:  nativePrimaries = JXL_PRIMARIES_CUSTOM;
                                    colour_encoding.primaries_red_xy[0] = 0.64;
                                    colour_encoding.primaries_red_xy[1] = 0.33;
                                    colour_encoding.primaries_green_xy[0] = 0.21;
                                    colour_encoding.primaries_green_xy[1] = 0.71;
                                    colour_encoding.primaries_blue_xy[0] = 0.15;
                                      colour_encoding.primaries_blue_xy[1] = 0.06;
                                    colour_encoding.white_point = JXL_WHITE_POINT_D65;
                                    break;
                                    // Values from §8.1 of ITU-T H.273 (V4) (07/2024). Because I wasn’t sure!
                                    // But not working because [0.0 not an allowed primary?](https://github.com/libjxl/libjxl/blob/e4b66d30278df6050137a4529e5efde5ef691f32/lib/jxl/cms/color_encoding_cms.h#L402_)
                                    // Escpecially weird becuase libjxl (does exactly this)[https://github.com/libjxl/libjxl/blob/e4b66d30278df6050137a4529e5efde5ef691f32/lib/extras/dec/apng.cc#L171].
                                    // Disabled for now.
    /* case s_colour_primaries_XYZ:    nativePrimaries = JXL_PRIMARIES_CUSTOM;
                                    colour_encoding.primaries_red_xy[0] = 1;
                                    colour_encoding.primaries_red_xy[1] = 0;
                                    colour_encoding.primaries_green_xy[0] = 0;
                                    colour_encoding.primaries_green_xy[1] = 1;
                                    colour_encoding.primaries_blue_xy[0] = 0;
                                    colour_encoding.primaries_blue_xy[1] = 0;
                                    colour_encoding.white_point = JXL_WHITE_POINT_E;
                                    break; */
    default:                        snprintf(module->graph->gui_msg_buf,
                                             sizeof(module->graph->gui_msg_buf),
                                             "[o-jxl] Recieved primaries currently not supported for export! Aborting…");
                                    module->graph->gui_msg = module->graph->gui_msg_buf;
                                    {
                                      error = 1;
                                      goto end;
                                    }
  }
  colour_encoding.primaries = nativePrimaries;

  JxlTransferFunction nativeTRC = 0;
  switch(trc)
  {
    case s_colour_trc_linear:       nativeTRC = JXL_TRANSFER_FUNCTION_LINEAR;
                                    break;
    case s_colour_trc_709:          nativeTRC = JXL_TRANSFER_FUNCTION_709;
                                    break;
    case s_colour_trc_srgb:         nativeTRC = JXL_TRANSFER_FUNCTION_SRGB;
                                    break;
    case s_colour_trc_PQ:           nativeTRC = JXL_TRANSFER_FUNCTION_PQ;
                                    break;
    case s_colour_trc_DCI:          nativeTRC = JXL_TRANSFER_FUNCTION_DCI;
                                    break;
    case s_colour_trc_HLG:          nativeTRC = JXL_TRANSFER_FUNCTION_HLG;
                                    break;
    case s_colour_trc_gamma:        nativeTRC = JXL_TRANSFER_FUNCTION_GAMMA;
                                    // Then set whatever gamma value. But using ~2.2 because s_colour_trc_gamma is currently only for AdobeRGB(?)
                                    // Derived from section §4.3.1.2 of [Adobe® RGB (1998) Color Image Encoding]
                                    // (https://www.adobe.com/digitalimag/pdfs/AdobeRGB1998.pdf).
                                    // colour_encoding.gamma = 256.0 / 563.0;
                                    // Actually elsewhere in the codebase this approximation is used, so for consistency:
                                    colour_encoding.gamma = 1.0 / 2.2;
                                    break;
                                    // Not sure if this is what I should do?
    case s_colour_trc_unknown:      nativeTRC = JXL_TRANSFER_FUNCTION_UNKNOWN;
                                    break;
    default:                        snprintf(module->graph->gui_msg_buf,
                                             sizeof(module->graph->gui_msg_buf),
                                             "[o-jxl] Recieved trc currently not supported for export! Aborting…");
                                    module->graph->gui_msg = module->graph->gui_msg_buf;
                                    {
                                      error = 1;
                                      goto end;
                                    }
  }
  colour_encoding.transfer_function = nativeTRC;

  // Hardcoding as relative for now.
  // ISO 15076-1:2010, but don’t currently know what they really do and I can’t find any reference to them within vkdt.
  // (They are user selectable in darktable but might not be important enough to warrant that).
  colour_encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
  
  if(JxlAssert(JxlEncoderSetColorEncoding(encoder,
                                          &colour_encoding),
               encoder,
               __LINE__))
  {
    error = 1;
    goto end;
  }


  // Pretty much straight from darktable. Don’t really understand what’s going on.
  // Switched to uint16_t which hopefully has the same characteristics as f16.

  // Fix pixel stride
  const size_t pixels_size = width * height * 3 * sizeof(uint16_t);
  pixels = malloc(pixels_size);
  if(!pixels)
    fprintf(stderr, "could not allocate output pixel buffer of size %zu", pixels_size);

  for(size_t y = 0; y < height; ++y)
  {
    for(size_t x = 0; x < width; ++x)
    {
      const uint16_t *in_pixel = p16 + 4 * ((y * width) + x);
      uint16_t *out_pixel = pixels + 3 * ((y * width) + x);

      out_pixel[0] = in_pixel[0];
      out_pixel[1] = in_pixel[1];
      out_pixel[2] = in_pixel[2];
    }
  }

  if(JxlAssert(JxlEncoderAddImageFrame(frame_settings,
                                       &pixel_format,
                                       pixels,
                                       pixels_size),
               encoder,
               __LINE__))
  {
    error = 1;
    goto end;
  }

  // No more image frames nor metadata boxes to add
  JxlEncoderCloseInput(encoder);

  // Write the image codestream to a buffer, starting with a chunk of 64 KiB.
  // TODO: Can we better estimate what the optimal size of chunks is for this image?
  size_t chunk_size = 1 << 16;
  size_t out_len = chunk_size;
  out_buf = malloc(out_len);
  if(!out_buf) printf("could not allocate codestream buffer of size %zu", out_len);
  uint8_t *out_cur = out_buf;
  size_t out_avail = out_len;

  JxlEncoderStatus out_status = JXL_ENC_NEED_MORE_OUTPUT;
  while(out_status == JXL_ENC_NEED_MORE_OUTPUT) {
    out_status = JxlEncoderProcessOutput(encoder, &out_cur, &out_avail);
    if(out_status == JXL_ENC_NEED_MORE_OUTPUT) {
      const size_t offset = out_cur - out_buf;
      if(chunk_size < 1 << 20)
        chunk_size *= 2;

      out_len += chunk_size;
      out_buf = realloc(out_buf, out_len);
      if(!out_buf)
        fprintf(stderr, "could not reallocate codestream buffer to size %zu", out_len);
      out_cur = out_buf + offset;
      out_avail = out_len - offset;

    }
  }

  // Update actual length of codestream written
  out_len = out_cur - out_buf;

  // Write codestream contents to file
  out_file = fopen(filename, "wb");
  if(fwrite(out_buf, sizeof(uint8_t), out_len, out_file) != out_len)
    printf("could not write bytes to `%s'", filename);



  end:
  if(runner)
    JxlResizableParallelRunnerDestroy(runner);
  if(encoder)
    JxlEncoderDestroy(encoder);
  if(out_file)
    fclose(out_file);
  free(pixels);
  free(out_buf);



  // Straight from o-jpg, with a bad error check thing because the `end` has to run before this and I don’t know how to do error handling properly, and [o-jpg] changed to [o-jxl]. 
#ifndef __ANDROID__
  const int copy_exif = dt_module_param_int(module, dt_module_get_param(module->so, dt_token("exif")))[0];
  if(copy_exif && !error)
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
    if(sizeof(cmd) <= snprintf(cmd, sizeof(cmd), "\"%s/exiftool\" -TagsFromFile \"%s\" \"-all:all>all:all\" -Software=\"vkdt\" -ModifyDate=\"now\" -*image*= -*orientation*= -orientation=normal -overwrite_original \"%s\"",
          dt_pipe.basedir, src_filename, filename)) return;
#ifdef _WIN64
    // sometimes another set of quotes is needed
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "\"%s\"", cmd);
    snprintf(cmd, sizeof(cmd), "%s", tmp);
#endif
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
          "o-jxl: unable to run exiftool to copy metadata! maybe you need to install it?");
      module->graph->gui_msg = module->graph->gui_msg_buf;
    }
  }
#endif
}

