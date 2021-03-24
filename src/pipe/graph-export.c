#include "core/log.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-print.h"
#include "pipe/graph-export.h"
#include "pipe/modules/api.h"

#include <libgen.h>
#include <unistd.h>

// export convenience functions, see cli/main.c

// fine grained interface:

// replace given display node instance by export module.
// returns 0 on success.
int
dt_graph_replace_display(
    dt_graph_t *graph,
    dt_token_t  inst,
    dt_token_t  mod)
{
  if(inst == 0) inst = dt_token("main"); // default to "main" instance
  const int mid = dt_module_get(graph, dt_token("display"), inst);
  if(mid < 0) return 1; // no display node by that name

  // get module the display is connected to:
  int cid = dt_module_get_connector(graph->module+mid, dt_token("input"));
  int m0 = graph->module[mid].connector[cid].connected_mi;
  int o0 = graph->module[mid].connector[cid].connected_mc;

  if(m0 < 0) return 2; // display input not connected

  if(mod == 0) mod = dt_token("o-jpg"); // default to jpg output

  // new module export with same inst
  // maybe new module 8-bit in between here
  const int m1 = dt_module_add(graph, dt_token("f2srgb"), inst);
  const int i1 = dt_module_get_connector(graph->module+m1, dt_token("input"));
  const int o1 = dt_module_get_connector(graph->module+m1, dt_token("output"));
  const int m2 = dt_module_add(graph, mod, inst);
  const int i2 = dt_module_get_connector(graph->module+m2, dt_token("input"));
  if(graph->module[m2].connector[0].format == dt_token("ui8"))
  {
    // output buffer reading is inflexible about buffer configuration. texture units can handle it, so just push further:
    graph->module[m1].connector[o1].format = graph->module[m2].connector[i2].format;
    graph->module[m1].connector[o1].chan   = graph->module[m2].connector[i2].chan;
    CONN(dt_module_connect(graph, m0, o0, m1, i1));
    CONN(dt_module_connect(graph, m1, o1, m2, i2));
  }
  else
  {
    graph->module[m0].connector[o0].format = graph->module[m2].connector[i2].format;
    CONN(dt_module_connect(graph, m0, o0, m2, i2));
  }
  return 0;
}

// disconnect all (remaining) display modules
void
dt_graph_disconnect_display_modules(
    dt_graph_t *graph)
{
  for(int m=0;m<graph->num_modules;m++)
    if(graph->module[m].name == dt_token("display"))
      dt_module_remove(graph, m); // disconnect and reset/ignore
}

VkResult
dt_graph_export(
    dt_graph_t        *graph,  // graph to run, will overwrite filename param
    dt_graph_export_t *param)
{
  if(param->p_cfgfile)
  {
    int err = dt_graph_read_config_ascii(graph, param->p_cfgfile);
    if(err)
    {
      dt_log(s_log_pipe, "could not open config file '%s'.", param->p_cfgfile);
      dt_token_t input_module = param->input_module;
      if(param->input_module == 0) input_module = dt_token("i-raw");
      char graph_cfg[256];
      if(param->p_defcfg)
        snprintf(graph_cfg, sizeof(graph_cfg), "%s", param->p_defcfg);
      else
        snprintf(graph_cfg, sizeof(graph_cfg), "%s/default-darkroom.%"PRItkn, dt_pipe.basedir, dt_token_str(input_module));
      err = dt_graph_read_config_ascii(graph, graph_cfg);
      char imgfilename[256];
      // follow link if this is a cfg in a tag collection:
      ssize_t linklen = readlink(param->p_cfgfile, imgfilename, sizeof(imgfilename));
      if(linklen == -1) snprintf(imgfilename, sizeof(imgfilename), "%s", param->p_cfgfile);
      else imgfilename[linklen] = 0;
      // reading the config will reset the search path. we'll repoint it to the
      // actual image file, not the default cfg:
      dt_graph_set_searchpath(graph, imgfilename);
      int len = strlen(imgfilename);
      assert(len > 4);
      imgfilename[len-4] = 0; // cut away ".cfg"
      char *basen = basename(imgfilename); // cut away path o we can relocate more easily
      int modid = dt_module_get(graph, input_module, dt_token("main"));
      if(modid < 0 ||
          dt_module_set_param_string(graph->module + modid, dt_token("filename"), basen))
      {
        dt_log(s_log_err, "config '%s' has no valid %"PRItkn" input module!", graph_cfg, dt_token_str(input_module));
        return VK_INCOMPLETE;
      }
    }
  }

  // dump original modules, i.e. with display modules
  if(param->dump_modules)
    dt_graph_print_modules(graph);

  // find non-display non-input "main" module
  int found_main = 0;
  for(int m=0;m<graph->num_modules;m++)
  {
    if(graph->module[m].inst == dt_token("main") &&
       graph->module[m].name != dt_token("display") &&
       strncmp(dt_token_str(graph->module[m].name), "i-", 2))
    {
      found_main = 1;
      break;
    }
  }

  // insert default:
  if(!param->output[0].inst) param->output[0].inst = dt_token("main");

  // replace requested display node by export node:
  if(!found_main)
  {
    int cnt = 0;
    for(;cnt<param->output_cnt;cnt++)
      if(dt_graph_replace_display(graph, param->output[cnt].inst, param->output[cnt].mod))
        break;
    if(cnt != param->output_cnt)
    {
      dt_log(s_log_err, "graph does not contain suitable display node %"PRItkn"!", dt_token_str(param->output[cnt].inst));
      return VK_INCOMPLETE;
    }
  }
  // make sure all remaining display nodes are removed:
  dt_graph_disconnect_display_modules(graph);

  // read extra arguments after replacing display, so we can access the additional f2srgb
  for(int i=0;i<param->extra_param_cnt;i++)
    if(dt_graph_read_config_line(graph, param->p_extra_param[i]))
      dt_log(s_log_pipe|s_log_err, "failed to read extra params %d: '%s'", i + 1, param->p_extra_param[i]);

  graph->frame = 0;
  dt_module_t *mod_out[20] = {0};
  assert(param->output_cnt <= sizeof(mod_out)/sizeof(mod_out[0]));
  char filename[256];
  for(int i=0;i<param->output_cnt;i++) for(int m=0;m<graph->num_modules;m++)
  {
    if(graph->module[m].inst == param->output[i].inst &&
        dt_token_str(graph->module[m].name)[0] == 'o' &&
        dt_token_str(graph->module[m].name)[1] == '-')
    {
      mod_out[i] = graph->module+m;
      break;
    }
  }

  for(int i=0;i<param->output_cnt;i++)
  {
    if(mod_out[i] == 0) continue; // not a known output module
    if(param->output[i].inst == dt_token("main"))
    { // set bounds to drive roi computation to main output if we find one
      // can this be generalised some more?
      // we assume that the "main" output is the last display in the config (driving the main roi)
      // which will also be confined by the user supplied max dimensions. we can explicitly introduce
      // resampling nodes. this may be useful for more high quality resampling in the future.
      if(param->output[i].max_width  > 0) graph->output_wd = param->output[i].max_width;
      if(param->output[i].max_height > 0) graph->output_ht = param->output[i].max_height;
    }
    if(graph->frame_cnt > 1)
    {
      if(param->output[i].p_filename)
        snprintf(filename, sizeof(filename), "%s_%04d", param->output[i].p_filename, 0);
      else
        snprintf(filename, sizeof(filename), "%"PRItkn"_%04d", dt_token_str(param->output[i].inst), 0);
    }
    else
    {
      if(param->output[i].p_filename)
        snprintf(filename, sizeof(filename), "%s", param->output[i].p_filename);
      else
        snprintf(filename, sizeof(filename), "%"PRItkn, dt_token_str(param->output[i].inst));
    }
    dt_module_set_param_string(
        mod_out[i], dt_token("filename"),
        filename);
    if(param->output[i].quality > 0)
      if(mod_out[i]->name == dt_token("o-jpg"))
        dt_module_set_param_float(mod_out[i], dt_token("quality"), param->output[i].quality);
  }

  if(graph->frame_cnt > 1)
  {
    dt_graph_run(graph, s_graph_run_all);
    for(int f=1;f<graph->frame_cnt;f++)
    {
      graph->frame = f;
      for(int i=0;i<param->output_cnt;i++)
      {
        if(mod_out[i] == 0) continue; // not a known output module
        if(param->output[i].p_filename)
          snprintf(filename, sizeof(filename), "%s_%04d", param->output[i].p_filename, f);
        else
          snprintf(filename, sizeof(filename), "%"PRItkn"_%04d", dt_token_str(param->output[i].inst), f);
        dt_module_set_param_string(
            mod_out[i], dt_token("filename"),
            filename);
      }
      VkResult res = dt_graph_run(graph,
          s_graph_run_record_cmd_buf | 
          s_graph_run_download_sink  |
          s_graph_run_wait_done);
      if(res != VK_SUCCESS) return res;
    }
    return VK_SUCCESS;
  }
  else
  {
    return dt_graph_run(graph, s_graph_run_all);
  }
}

