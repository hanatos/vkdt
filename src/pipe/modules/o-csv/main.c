#include "modules/api.h"
#include "core/core.h"
#include "core/half.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct buf_t
{
  FILE *f;
}
buf_t;

int init(dt_module_t *mod)
{
  buf_t *dat = malloc(sizeof(*dat));
  memset(dat, 0, sizeof(*dat));
  mod->data = dat;
  mod->flags = s_module_request_write_sink;
  return 0;
}

void cleanup(dt_module_t *mod)
{
  if(!mod->data) return;
  buf_t *dat= mod->data;
  if(dat->f)
  {
    fclose(dat->f);
    dat->f = 0;
  }
  free(dat);
  mod->data = 0;
}

void modify_roi_out(
    dt_graph_t  *graph,
    dt_module_t *mod)
{
  buf_t *buf = mod->data;
  if(buf->f) fclose(buf->f);
  buf->f = 0; // de-init
}

void write_sink(
    dt_module_t *mod,
    void        *buf)
{
  buf_t *dat = mod->data;
  if(!dat->f)
  {
    const char *basename  = dt_module_param_string(mod, 0);
    char filename[512];
    snprintf(filename, sizeof(filename), "%s.csv", basename);
    if(dat->f) fclose(dat->f);
    dat->f = fopen(filename, "wb");
  }
  const int wd = mod->connector[0].roi.wd;
  const int ht = mod->connector[0].roi.ht;
  const int cn = dt_connector_channels(mod->connector);
  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
  {
    switch(mod->connector[0].format)
    {
      case dt_token("ui8"):
        for(int c=0;c<cn;c++) fprintf(dat->f, "%"PRIu8" ", (((uint8_t *)buf) + (j*wd + i)*cn)[c]);
        break;
      case dt_token("ui16"):
        for(int c=0;c<cn;c++) fprintf(dat->f, "%"PRIu16" ", (((uint16_t *)buf) + (j*wd + i)*cn)[c]);
        break;
      case dt_token("ui32"):
        for(int c=0;c<cn;c++) fprintf(dat->f, "%"PRIu32" ", (((uint32_t *)buf) + (j*wd + i)*cn)[c]);
        break;
      case dt_token("f16"):
        for(int c=0;c<cn;c++) fprintf(dat->f, "%g ", half_to_float((((uint16_t *)buf) + (j*wd + i)*cn)[c]));
        break;
      case dt_token("f32"):
        for(int c=0;c<cn;c++) fprintf(dat->f, "%g ", (((float *)buf) + (j*wd + i)*cn)[c]);
        break;
    }
  }
  fprintf(dat->f, "\n");
}
