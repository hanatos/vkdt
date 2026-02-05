#include "modules/api.h"
#include "core/lut.h"
#include "connector.h"

#include <stdio.h>
#include <string.h>

void write_sink(
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-lut] writing '%s'\n", basename);

  // if this varies based on parameters, we might not pick it up unless we ask directly:
  int mid = module->connector[0].connected.i;
  int cid = module->connector[0].connected.c;
  if(mid < 0) return;
  if(cid < 0) return;
  dt_token_t format = module->graph->module[mid].connector[cid].format;
  dt_token_t chan   = module->graph->module[mid].connector[cid].chan;
  int channels = dt_connector_channels(module->graph->module[mid].connector+cid);

  int datatype = format == dt_token("f32") ? dt_lut_header_f32 : dt_lut_header_f16;
  if(chan == dt_token("ssbo")) datatype += dt_lut_header_ssbo_f16;
  dt_lut_header_t header = {
    .magic    = dt_lut_header_magic,
    .version  = dt_lut_header_version,
    .channels = channels,
    .datatype = datatype,
    .wd       = module->connector[0].roi.wd,
    .ht       = module->connector[0].roi.ht,
  };

  char filename[512];
  snprintf(filename, sizeof(filename), "%s.lut", basename);
  FILE* f = fopen(filename, "wb");
  if(f)
  {
    fwrite(&header, sizeof(header), 1, f);
    // use whatever connector says because that's the real allocation (in case
    // it's just uint8 or so)
    size_t bytes = header.wd * (uint64_t)header.ht * (uint64_t)header.channels * (uint64_t)dt_connector_bytes_per_channel(module->connector+0);
    fwrite(buf, bytes, 1, f);
    dt_image_metadata_text_t *meta = dt_metadata_find(module->img_param.meta, s_image_metadata_text);
    if(meta && meta->text)
      fprintf(f, "%s\n", meta->text);
    fclose(f);
  }
}
