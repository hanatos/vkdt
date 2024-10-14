#include "modules/api.h"
#include "core/fs.h"

#include <stdio.h>
#include <string.h>

void write_sink(
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  const char *basename = dt_module_param_string(module, 0);
  fprintf(stderr, "[o-copy] copying the original to '%s'\n", basename);

  // now forget all this and copy the main input image
  for(int m=0;m<module->graph->num_modules;m++)
  {
    const dt_module_t *mod = module->graph->module + m;
    if(mod->inst == dt_token("main") && dt_connector_output(mod->connector))
    {
      const int p_id       = dt_module_get_param(mod->so, dt_token("startid"));
      const int p_filename = dt_module_get_param(mod->so, dt_token("filename"));
      const int   id       = p_id >= 0 ? dt_module_param_int(mod, p_id)[0] : 0;
      const char *filename = p_filename >= 0 ? dt_module_param_string(mod, p_filename) : 0;
      char filename_in[512], filename_out[512];
      if(!filename || dt_graph_get_resource_filename(mod, filename, id + mod->graph->frame, filename_in, sizeof(filename_in)))
      {
        fprintf(stderr, "[o-copy] could not reconstruct filename of the input file %s!\n", filename);
        return;
      }
      const char *ext = filename + strlen(filename);
      for(;ext[0] != '.' && ext > filename;ext--) {}
      if(ext[0] == '.') ext++;
      else
      {
        fprintf(stderr, "[o-copy] could not determine file extension of the input file %s!\n", filename);
        return;
      }
      snprintf(filename_out, sizeof(filename_out), "%s.%s", basename, ext);
      fs_copy(filename_out, filename_in);
      return;
    }
  }
}
