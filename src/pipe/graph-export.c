#include "core/log.h"
#include "core/fs.h"
#include "pipe/global.h"
#include "pipe/graph.h"
#include "pipe/graph-io.h"
#include "pipe/graph-print.h"
#include "pipe/graph-export.h"
#include "pipe/graph-defaults.h"
#include "pipe/modules/api.h"

#include <libgen.h>
#include <unistd.h>

// export convenience functions, see cli/main.c

// fine grained interface:

// replace given display node instance by export module.
// does not in fact delete the display module but adds an output module (plus support modules if required) and
// hooks this up to the same connector as the given display.
// this means you can call it multiple times to attach several different outputs.
// returns id of the newly added output module on success, or negative on failure
static inline int
dt_graph_replace_display(
    dt_graph_t *graph,
    dt_token_t  inst,   // instance name of the display module to replace. leave 0 for default (main)
    dt_token_t  iout,   // instance name of the newly added output module, or 0 if same as inst
    dt_token_t  mod,    // module type of the output module to drop into place instead, e.g. "o-jpg". leave 0 for default (o-jpg)
    int         resize, // pass 1 to insert a resize module before output
    int         max_wd, // if > 0, limit export size
    int         max_ht,
    int         prim,   // colour primaries
    int         trc)
{
  if(inst == 0) inst = dt_token("main"); // default to "main" instance
  if(iout == 0) iout = inst;
  const int mid = dt_module_get(graph, dt_token("display"), inst);
  if(mid < 0) return -1; // no display node by that name

  // get module the display is connected to:
  int cid = dt_module_get_connector(graph->module+mid, dt_token("input"));
  int m0 = graph->module[mid].connector[cid].connected.i;
  int o0 = graph->module[mid].connector[cid].connected.c;

  if(m0 < 0) return -2; // display input not connected

  if(mod == 0) mod = dt_token("o-jpg"); // default to jpg output
  else if(mod == dt_token("o-web"))
  { // special fake output module will dispatch video for multi-frame and jpg for stills
    if(graph->frame_cnt > 1) mod = dt_token("o-vid");
    else                     mod = dt_token("o-jpg");
  }

  if(resize)
  {
    const int m1 = dt_module_add(graph, dt_token("resize"), iout);
    const int i1 = 0, o1 = 1;
    CONN(dt_module_connect(graph, m0, o0, m1, i1));
    m0 = m1;
    o0 = o1;
  }

  // new module export with same inst
  const int m1 = dt_module_add(graph, dt_token("colenc"), iout);
  const int i1 = dt_module_get_connector(graph->module+m1, dt_token("input"));
  const int o1 = dt_module_get_connector(graph->module+m1, dt_token("output"));
  const int m2 = dt_module_add(graph, mod, iout);
  const int i2 = dt_module_get_connector(graph->module+m2, dt_token("input"));
  if(graph->module[m2].connector[i2].format == dt_token("ui8") ||
     prim != s_colour_primaries_2020 || 
     trc  != s_colour_trc_linear)
  {
    // if we don't know what it is, try to ask the module:
    if(prim == s_colour_primaries_unknown) prim = graph->module[m0].img_param.colour_primaries;
    if(trc  == s_colour_trc_unknown)       trc  = graph->module[m0].img_param.colour_trc;
    dt_module_t *mod = graph->module + m1;
    *(int*)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("prim"))) = prim;
    *(int*)dt_module_param_int(mod, dt_module_get_param(mod->so, dt_token("trc" ))) = trc;
    // output buffer reading is inflexible about buffer configuration. texture
    // units can handle it, so just push further:
    graph->module[m1].connector[o1].format = graph->module[m2].connector[i2].format;
    graph->module[m1].connector[o1].chan   = graph->module[m2].connector[i2].chan;
    CONN(dt_module_connect(graph, m0, o0, m1, i1));
    CONN(dt_module_connect(graph, m1, o1, m2, i2));
  }
  else
  {
    graph->module[m0].connector[o0].format = graph->module[m2].connector[i2].format;
    graph->module[m0].connector[o0].chan   = graph->module[m2].connector[i2].chan;
    CONN(dt_module_connect(graph, m0, o0, m2, i2));
  }
  graph->module[m2].connector[i2].max_wd = max_wd;
  graph->module[m2].connector[i2].max_ht = max_ht;
  return m2;
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
      dt_token_t input_module = param->input_module;
      if(param->input_module == 0)
        input_module = dt_graph_default_input_module(param->p_cfgfile);
      char graph_cfg[PATH_MAX+100];
      if(param->p_defcfg)
        snprintf(graph_cfg, sizeof(graph_cfg), "%s", param->p_defcfg);
      else
        snprintf(graph_cfg, sizeof(graph_cfg), "default-darkroom.%"PRItkn, dt_token_str(input_module));
      err = dt_graph_read_config_ascii(graph, graph_cfg);
      char imgfilename[PATH_MAX];
      // follow link if this is a cfg in a tag collection:
      fs_realpath(param->p_cfgfile, imgfilename);
      // reading the config will reset the search path. we'll repoint it to the
      // actual image file, not the default cfg:
      dt_graph_set_searchpath(graph, imgfilename);
      int len = strlen(imgfilename);
      assert(len > 4);
      imgfilename[len-4] = 0; // cut away ".cfg"
      char *basen = fs_basename(imgfilename); // cut away path so we can relocate more easily
      int modid = dt_module_get(graph, input_module, dt_token("main"));
      if(modid < 0 || dt_module_set_param_string(graph->module + modid, dt_token("filename"), basen))
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
       graph->module[m].connector[0].type == dt_token("sink") &&
       strncmp(dt_token_str(graph->module[m].name), "i-", 2))
    {
      found_main = 1;
      break;
    }
  }

  // insert default:
  if(!param->output[0].inst) param->output[0].inst = dt_token("main");

  int mod_out[20];
  for(int i=0;i<param->output_cnt;i++) mod_out[i] = -1;
  assert(param->output_cnt <= sizeof(mod_out)/sizeof(mod_out[0]));
  if(!found_main)
  { // replace requested display node by export node:
    int cnt = 0;
    for(;cnt<param->output_cnt;cnt++)
      if(0 >= (mod_out[cnt] = dt_graph_replace_display(
            graph, param->output[cnt].inst, param->output[cnt].inst_out, param->output[cnt].mod,
            ( param->output[cnt].mod != dt_token("o-bc1")) && // no hq thumbnails
            ((param->output[cnt].max_width > 0) || (param->output[cnt].max_height > 0)),
            param->output[cnt].max_width, param->output[cnt].max_height,
            param->output[cnt].colour_primaries, param->output[cnt].colour_trc
            )))
        break;
    if(cnt != param->output_cnt)
    {
      dt_log(s_log_err, "graph does not contain suitable display node %"PRItkn"!", dt_token_str(param->output[cnt].inst));
      return VK_INCOMPLETE;
    }
  }
  // make sure all remaining display nodes are removed:
  dt_graph_disconnect_display_modules(graph);

  // read extra arguments after replacing display, so we can access the additional modules
  for(int i=0;i<param->extra_param_cnt;i++)
    if(dt_graph_read_config_line(graph, param->p_extra_param[i]))
      dt_log(s_log_pipe|s_log_err, "failed to read extra params %d: '%s'", i + 1, param->p_extra_param[i]);
  for(int m=0;m<graph->num_modules;m++) dt_module_keyframe_post_update(graph->module+m);

  graph->frame = 0;
  char filename[256];
  if(found_main) for(int i=0;i<param->output_cnt;i++) for(int m=0;m<graph->num_modules;m++)
  {
    dt_token_t inst = param->output[i].inst_out;
    if(!inst)  inst = param->output[i].inst;
    if(graph->module[m].inst == inst &&
       graph->module[m].name == param->output[i].mod)
    {
      mod_out[i] = m;
      graph->module[m].connector[0].max_wd = param->output[i].max_width;
      graph->module[m].connector[0].max_ht = param->output[i].max_height;
      break;
    }
  }

  for(int i=0;i<param->output_cnt;i++)
  {
    if(mod_out[i] < 0) continue; // not a known output module (display node requested on command line)
    if(graph->frame_cnt > 1 && !param->last_frame_only)
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
    if(param->output[i].p_pdata)
      memcpy(graph->module[mod_out[i]].param, param->output[i].p_pdata, dt_module_total_param_size(graph->module[mod_out[i]].so - dt_pipe.module));
    dt_module_set_param_string(
        graph->module+mod_out[i], dt_token("filename"),
        filename);
    if(param->output[i].quality > 0)
      dt_module_set_param_float(graph->module+mod_out[i], dt_token("quality"), param->output[i].quality);
  }

  int audio_mod = -1;
  uint16_t *audio_samples;
  for(int i=0;i<graph->num_modules;i++)
    if(graph->module[i].name && graph->module[i].so->audio) { audio_mod = i; break; }
  FILE *audio_f = 0;
  if(param->output[0].p_audio && audio_mod >= 0) audio_f = fopen(param->output[0].p_audio, "wb");

  // TODO: want bytes per sample from snd header!
  const int audio_bps = audio_mod < 0 ? 0 : graph->module[audio_mod].img_param.snd_channels * sizeof(int16_t);
  const int audio_spf = audio_mod < 0 ? 0 : graph->module[audio_mod].img_param.snd_samplerate / graph->frame_rate;
  uint64_t audio_pos = 0;

  if(graph->frame_cnt > 1)
  {
    VkResult res = VK_SUCCESS;
    dt_graph_apply_keyframes(graph);
    dt_graph_run(graph, s_graph_run_all
        ^(param->last_frame_only ? s_graph_run_download_sink : 0));
    if(audio_f)
    {
      for(int audio_cnt=audio_spf;audio_cnt;)
      {
        int delta = graph->module[audio_mod].so->audio(graph->module+audio_mod, audio_pos, audio_cnt, &audio_samples);
        if(delta) fwrite(audio_samples, audio_bps, delta, audio_f);
        else break;
        if(graph->frame_rate == 0) break; // no fixed frame rate have to provide the right amount of audio
        audio_cnt -= delta; audio_pos += delta;
      }
    }
    for(int f=1;f<graph->frame_cnt;f++)
    {
      graph->frame = f;
      for(int i=0;i<param->output_cnt;i++)
      {
        if(mod_out[i] < 0) continue; // not a known output module
        if(param->last_frame_only)
        {
          if(param->output[i].p_filename)
            snprintf(filename, sizeof(filename), "%s", param->output[i].p_filename);
          else
            snprintf(filename, sizeof(filename), "%"PRItkn, dt_token_str(param->output[i].inst));
        }
        else
        {
          if(param->output[i].p_filename)
            snprintf(filename, sizeof(filename), "%s_%04d", param->output[i].p_filename, f);
          else
            snprintf(filename, sizeof(filename), "%"PRItkn"_%04d", dt_token_str(param->output[i].inst), f);
        }
        dt_module_set_param_string(
            graph->module+mod_out[i], dt_token("filename"),
            filename);
      }
      dt_graph_apply_keyframes(graph);
      graph->double_buffer = 1-graph->double_buffer;
      res = dt_graph_run(graph,
          s_graph_run_record_cmd_buf | 
          ((!param->last_frame_only || (f == graph->frame_cnt-1)) ?
          s_graph_run_download_sink : 0) |
          s_graph_run_wait_done);
      if(res != VK_SUCCESS) goto done;
      if(audio_f)
      {
        for(int audio_cnt=audio_spf;audio_cnt;)
        {
          int delta = graph->module[audio_mod].so->audio(graph->module+audio_mod, audio_pos, audio_cnt, &audio_samples);
          if(delta) fwrite(audio_samples, audio_bps, delta, audio_f);
          else break;
          if(graph->frame_rate == 0) break; // no fixed frame rate have to provide the right amount of audio
          audio_cnt -= delta; audio_pos += delta;
        }
      }
      if(param->print_progress)
        fprintf(stderr, "\r[export] processing frame %d/%d", graph->frame, graph->frame_cnt-1);
    }
    if(param->print_progress) fprintf(stderr, "\n");
done:
    if(audio_f) fclose(audio_f);
    return res;
  }
  else
  {
    return dt_graph_run(graph, s_graph_run_all);
  }
}

