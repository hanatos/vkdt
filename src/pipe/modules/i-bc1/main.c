#include "modules/api.h"

#if defined(__ANDROID__) // XXX windows or macintosh too?
#include <stdio.h>
#else
#include <bsd/stdio.h>
#endif
#include <stdlib.h>
#include <zlib.h>

#define CHUNK 16384
typedef struct gzresFILE
{
  dt_graph_t *g;
  z_stream strm;
  unsigned char in[CHUNK];
  FILE *f;
} gzresFILE;

static int gzread_res(void* cookie, char* buf, int size)
{ // from the zlib example code zpipe.c:
  gzresFILE *gf = (gzresFILE *)cookie;
  int wpos = 0;
  do { // outer loop reads more input chunks into our buffer
    if(gf->strm.avail_in == 0)
    { // no more input data, read from our stream
      gf->strm.avail_in = fread(gf->in, 1, CHUNK, gf->f);
      if (ferror(gf->f)) return -1;
      gf->strm.next_in = gf->in;
    }
    if (gf->strm.avail_in == 0) return -1;

    do { // inner loop: write to our output buffer until in empty or out full
      gf->strm.avail_out = size - wpos;
      gf->strm.next_out  = (uint8_t*)buf + wpos;
      int ret = inflate(&gf->strm, Z_NO_FLUSH);
      switch (ret) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          return -1;
        default:;
      }
      wpos = size - gf->strm.avail_out;
      if (ret == Z_STREAM_END) return wpos;
    } while (gf->strm.avail_in > 0 && gf->strm.avail_out > 0);
  } while (gf->strm.avail_out > 0);
  return size;
}

static int gzwrite_res(void* cookie, const char* buf, int size) {
  return EACCES; // can't provide write access to the apk
}

static off_t gzseek_res(void* cookie, off_t offset, int whence) {
  return -1;
}

int gzclose_res(void *cookie)
{
  gzresFILE *gf = (gzresFILE *)cookie;
  (void)inflateEnd(&gf->strm);
  fclose(gf->f);
  free(gf);
  return 0;
}

FILE *gzopen_res(FILE *f, dt_graph_t *g)
{
  gzresFILE *gf = malloc(sizeof(*gf));
  gf->g = g;
  /* allocate inflate state */
  gf->strm.zalloc = Z_NULL;
  gf->strm.zfree = Z_NULL;
  gf->strm.opaque = Z_NULL;
  gf->strm.avail_in = 0;
  gf->strm.next_in = Z_NULL;
  int ret = inflateInit2(&gf->strm, 32+15);//16);
  if (ret != Z_OK) return 0;
  gf->f = f; // remember inner so we can close it
  return funopen(gf, gzread_res, gzwrite_res, gzseek_res, gzclose_res);
}

// this callback is responsible to set the full_{wd,ht} dimensions on the
// regions of interest on all "write"|"source" channels
void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  // load only header
  const char *filename = dt_module_param_string(mod, 0);
  uint32_t header[4] = {0};
  FILE *inner = dt_graph_open_resource(mod->graph, 0, filename, "rb");
  FILE *f = 0; // will close inner fd when closing f
  int ret = 0;
  if(!inner ||
     !(f = gzopen_res(inner, graph)) ||
     (ret = fread(header, 1, sizeof(uint32_t)*4, f)) != sizeof(uint32_t)*4)
  {
    fprintf(stderr, "[i-bc1] %s: can't open file!\n", filename);
    snprintf(mod->graph->gui_msg_buf, 100, "[i-bc1] %s: can't open file! ret %d\n", filename, ret);
    if(f) fclose(f);
    return;
  }
  if(header[0] != dt_token("bc1z") || header[1] != 1)
  {
    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
    snprintf(mod->graph->gui_msg_buf, 100, "[i-bc1] %s: wrong magic number or version %d\n", filename, header[0]);
    if(f) fclose(f);
    return;
  }
  const uint32_t wd = 4*(header[2]/4), ht = 4*(header[3]/4);
  mod->connector[0].roi.full_wd = wd;
  mod->connector[0].roi.full_ht = ht;
  mod->img_param.colour_primaries = s_colour_primaries_2020;
  mod->img_param.colour_trc       = s_colour_trc_srgb;
  if(f) fclose(f);
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  const char *filename = dt_module_param_string(mod, 0);
  uint32_t header[4] = {0};
  FILE *inner = dt_graph_open_resource(mod->graph, 0, filename, "rb");
  FILE *f = 0; // will close inner fd when closing
  if(!inner || !(f = gzopen_res(inner, mod->graph)) || fread(header, 1, sizeof(uint32_t)*4, f) != sizeof(uint32_t)*4)
  {
    fprintf(stderr, "[i-bc1] %s: can't open file!\n", filename);
    if(f) fclose(f);
    return 1;
  }
  if(header[0] != dt_token("bc1z") || header[1] != 1)
  {
    fprintf(stderr, "[i-bc1] %s: wrong magic number or version!\n", filename);
    fclose(f);
    return 1;
  }
  const uint32_t wd = 4*(header[2]/4), ht = 4*(header[3]/4);
  // fprintf(stderr, "[i-bc1] %s magic %"PRItkn" version %u dim %u x %u\n",
  //     filename, dt_token_str(header[0]),
  //     header[1], header[2], header[3]);
  fread(mapped, 1, sizeof(uint8_t)*8*(wd/4)*(ht/4), f);
  fclose(f);
  return 0;
}
