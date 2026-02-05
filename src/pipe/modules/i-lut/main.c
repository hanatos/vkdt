#include "modules/api.h"
#include "core/strexpand.h"
#include "core/lut.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct lutinput_buf_t
{
  char filename[PATH_MAX];
  char errormsg[256];
  dt_lut_header_t header;
  size_t data_begin;
  FILE *f;

  // for array mode:
  char        *lst_data;     // list file in one chunk
  const char **lst_filename; // pointers to lines / filenames
  int          lst_cnt;      // number of filenames in list
  uint32_t    *lst_dim;      // individual dimensions of the images
}
lutinput_buf_t;

static int 
read_header(
    dt_module_t *mod,
    uint32_t    *dim,
    const char  *pattern)
{
  if(dim) dim[0] = dim[1] = 0;
  lutinput_buf_t *lut = mod->data;

  char filename[PATH_MAX], flen[10];
  snprintf(flen, sizeof(flen), "%g", mod->graph->main_img_param.focal_length);
  const char *key[] = { "maker", "model", "flen", 0};
  const char *val[] = { mod->graph->main_img_param.maker, mod->graph->main_img_param.model, flen, 0};
  dt_strexpand(pattern, strlen(pattern), filename, sizeof(filename), key, val);
  // fprintf(stderr, "[i-lut] %"PRItkn" loading `%s'!\n", dt_token_str(mod->inst), filename);

  if(lut && !lut->lst_filename && !strcmp(lut->filename, filename))
    return 0; // already loaded
  assert(lut); // this should be inited in init()

  if(lut->f) fclose(lut->f);
  lut->f = dt_graph_open_resource(mod->graph, 0, filename, "rb");
  if(!lut->f) goto error;

  if(fread(&lut->header, sizeof(dt_lut_header_t), 1, lut->f) != 1 || lut->header.version != 2)
  {
    fclose(lut->f);
    goto error;
  }

  lut->data_begin = ftell(lut->f);

  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 1.0f;
    mod->img_param.whitebalance[k] = 1.0f;
  }
  mod->img_param.filters = 0;
  if(dim)
  {
    dim[0] = lut->header.wd;
    dim[1] = lut->header.ht;
  }

  snprintf(lut->filename, sizeof(lut->filename), "%s", filename);
  return 0;
error:
  fprintf(stderr, "[i-lut] %"PRItkn" could not load file `%s'!\n", dt_token_str(mod->inst), filename);
  if(snprintf(lut->errormsg, sizeof(lut->errormsg), "i-lut: %"PRItkn" could not load file `%s'!", dt_token_str(mod->inst), filename) >= sizeof(lut->errormsg))
    fprintf(stderr, "\n"); // fuck you gcc
  mod->graph->gui_msg = lut->errormsg;
  lut->filename[0] = 0;
  return 1;
}

static int
read_plain(
    lutinput_buf_t *lut,
    void           *out)
{
  if(!lut->f) return 1;
  fseek(lut->f, lut->data_begin, SEEK_SET);
  int datatype = lut->header.datatype;
  if(datatype >= dt_lut_header_ssbo_f16) datatype -= dt_lut_header_ssbo_f16;
  size_t sz =
    datatype == dt_lut_header_f16 ? sizeof(uint16_t) :
    datatype == dt_lut_header_ui8 ? sizeof(uint8_t) : sizeof(float);
  if(lut->header.channels == 3)
  { // need to pad to 4
    for(int k=0;k<lut->header.wd*lut->header.ht;k++)
    {
      // TODO: do that for f16 and float too:
      memset(((uint8_t*)out) + 4*sz*k, 255, 4*sz);
      fread(((uint8_t*)out) + 4*sz*k, 3, sz, lut->f);
    }
  }
  else fread(out, lut->header.wd*(uint64_t)lut->header.ht*(uint64_t)lut->header.channels, sz, lut->f);
  return 0;
}

int init(dt_module_t *mod)
{
  lutinput_buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  lutinput_buf_t *lut = mod->data;
  if(lut->filename[0])
  {
    if(lut->f) fclose(lut->f);
    lut->filename[0] = 0;
  }
  if(lut->lst_filename) free(lut->lst_filename);
  if(lut->lst_data)     free(lut->lst_data);
  if(lut->lst_dim)      free(lut->lst_dim);
  free(lut);
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
  lutinput_buf_t *lut = mod->data;

  int len = strlen(filename);
  const char *c = filename + strlen(filename);
  if(len > 4 && !strncmp(c-4, ".txt", 4))
  { // this is not a lut but in fact a list of texture filenames as text.
    // load list of images, keep number of files and the dimension of their images.
    FILE *f = dt_graph_open_resource(mod->graph, 0, filename, "rb");
    if(!f) return;
    fseek(f, 0, SEEK_END);
    uint64_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(lut->lst_data)     free(lut->lst_data);
    if(lut->lst_dim)      free(lut->lst_dim);
    if(lut->lst_filename) free(lut->lst_filename);
    lut->lst_data = malloc(size);
    fread(lut->lst_data, size, 1, f);
    fclose(f);
    int cnt = 0;
    for(int i=0;i<size;i++)
      if(lut->lst_data[i] == '\n') { lut->lst_data[i] = 0; cnt++; }

    lut->lst_cnt = cnt;
    lut->lst_dim = calloc(2*sizeof(uint32_t), cnt);
    lut->lst_filename = calloc(sizeof(const char*), cnt+1);
    lut->lst_filename[0] = lut->lst_data;
    cnt = 0;
    for(int i=0;i<size;i++)
      if(lut->lst_data[i] == 0)
        lut->lst_filename[++cnt] = lut->lst_data + i + 1;

    // for each one image, open the header and find the size of the image
    for(int i=0;i<lut->lst_cnt;i++)
      read_header(mod, lut->lst_dim + 2*i, lut->lst_filename[i]);

    uint32_t max_wd = 0, max_ht = 0;
    for(int i=0;i<lut->lst_cnt;i++)
    {
      max_wd = MAX(max_wd, lut->lst_dim[2*i+0]);
      max_ht = MAX(max_ht, lut->lst_dim[2*i+1]);
    }
    // lut->header holds last loaded file specs, will init channel config below
    mod->connector[0].roi.full_wd = max_wd; // will be used to allocate staging buffer
    mod->connector[0].roi.full_ht = max_ht;
    mod->connector[0].roi.wd = max_wd;
    mod->connector[0].roi.ht = max_ht;
    mod->connector[0].roi.scale = 1;
    mod->connector[0].array_length = cnt;
    // instruct the connector that we have an array with different image resolution for every element:
    mod->connector[0].array_dim = lut->lst_dim;
  }
  else
  { // regular single lut file
    if(read_header(mod, 0, filename))
    {
      mod->connector[0].roi.full_wd = 32;
      mod->connector[0].roi.full_ht = 32;
      return;
    }
    mod->connector[0].roi.full_wd = lut->header.wd;
    mod->connector[0].roi.full_ht = lut->header.ht;
  }
  // adjust output connector channels:
  if(lut->header.channels == 1) mod->connector[0].chan = dt_token("r");
  if(lut->header.channels == 2) mod->connector[0].chan = dt_token("rg");
  if(lut->header.channels == 3) mod->connector[0].chan = dt_token("rgba");
  if(lut->header.channels == 4) mod->connector[0].chan = dt_token("rgba");
  int dtype = lut->header.datatype;
  if(dtype >= dt_lut_header_ssbo_f16)
  {
    mod->connector[0].chan = dt_token("ssbo");
    dtype -= dt_lut_header_ssbo_f16;
  }
  if(dtype == dt_lut_header_ui8) mod->connector[0].format = dt_token("ui8");
  if(dtype == dt_lut_header_f16) mod->connector[0].format = dt_token("f16");
  if(dtype == dt_lut_header_f32) mod->connector[0].format = dt_token("f32");
  for(int k=0;k<4;k++)
  {
    mod->img_param.black[k]        = 0.0f;
    mod->img_param.white[k]        = 1.0f;
    mod->img_param.whitebalance[k] = 1.0f;
  }
  // we don't know any better:
  mod->img_param.colour_primaries = s_colour_primaries_2020;
  mod->img_param.colour_trc       = s_colour_trc_linear;
  mod->img_param.filters = 0;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  lutinput_buf_t *lut = mod->data;
  if(lut->lst_filename)
  {
    if(read_header(mod, 0, lut->lst_filename[p->a])) return 1;
    return read_plain(lut, mapped);
  }
  else
  {
    const char *filename = dt_module_param_string(mod, 0);
    if(read_header(mod, 0, filename)) return 1;
    return read_plain(lut, mapped);
  }
}
