#include "modules/api.h"
#include "core/fs.h"

#include <stdio.h>
#include <string.h>

// TODO ask for really small output frame buffer, to go faster

void write_sink(
    dt_module_t            *module,
    void                   *buf,
    dt_write_sink_params_t *p)
{
  dt_graph_t *graph = module->graph;
  char *basename = (char *)dt_module_param_string(module, 0);
  fprintf(stdout, "[o-copy] copying the original to `%s'\n", basename);
  const int copy_mode = dt_module_param_string(module, 1)[0];

  if(copy_mode == 1)
  { // export bundle with all external dependencies
    // in this mode, the orginal file name stays, and all external resources are exported as relative file names too
    // does not support frame numbers in folder names, only in file names
    if(!graph || !graph->ext_files) return;
    dt_stringpool_t *sp = graph->ext_files;
    int cut_basename = fs_dirname(basename);
    char newdir[PATH_MAX];
    for(int i=0;i<sp->buf_max;)
    {
      int len = strlen(sp->buf+i);
      if(len)
      {
        uint32_t pos = dt_stringpool_get(sp, sp->buf+i, len, -1u, 0);
        if(pos != -1u)
        { // got next relative resource file name:
          char *relative_res = sp->buf+i;
          FILE *f = dt_graph_open_resource(graph, graph->frame, relative_res, "r");
          if(f)
          {
            if(relative_res[0] == '/')
            { // absolute path
              snprintf(newdir, sizeof(newdir), "%s/%s", basename, fs_basename(relative_res));
            }
            else if(fs_dirname(relative_res))
            { // relative with path
              snprintf(newdir, sizeof(newdir), "%s/%s", basename, relative_res);
              fs_mkdir_p(newdir, 0775);
              relative_res[strlen(relative_res)] = '/';
              snprintf(newdir, sizeof(newdir), "%s/%s", basename, relative_res);
            }
            else snprintf(newdir, sizeof(newdir), "%s/%s", basename, relative_res);
            fprintf(stdout, "[o-copy] exporting `%s'\n", newdir);
            fs_copy_file(newdir, f);
            fclose(f);
          }
        }
      }
      i += len+1; // eat terminating 0, too
    }
    // dt_graph_print_external_resources(module->graph);
    // re-instate basename (probably not needed):
    if(cut_basename) basename[strlen(basename)] = '/';
  }
  else
  { // export just the source file
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
}
