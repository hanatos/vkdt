#include "modules/api.h"
#include "config.h"

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  dt_roi_t *ro = &module->connector[1].roi;
  ro->full_wd = ri->full_wd;
  ro->full_ht = ri->full_ht;
  module->img_param.filters = 0u; // after we're done there won't be any more mosaic
}

void modify_roi_in(
    dt_graph_t  *graph,
    dt_module_t *module)
{
  dt_roi_t *ri = &module->connector[0].roi;
  ri->wd = ri->full_wd;
  ri->ht = ri->full_ht;
  ri->scale = 1.0f;
}

dt_graph_run_t
check_params(
    dt_module_t *mod,
    uint32_t     parid,
    uint32_t     num,
    void        *oldval)
{
  const int pid = dt_module_get_param(mod->so, dt_token("model"));
  const int mdl = CLAMP(dt_module_param_int(mod, pid)[0], 0, 1);
  if(parid == pid) // model changed
    if(*(int*)oldval != mdl) 
      return s_graph_run_all;
  return s_graph_run_record_cmd_buf; // minimal parameter upload to uniforms is fine
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  char mdlfile[256];
  const int pid = dt_module_get_param(mod->so, dt_token("model"));
  const int mdl = CLAMP(dt_module_param_int(mod, pid)[0], 0, 1);
  const char *mdl_type[2] = {"heavy", "light"};
  snprintf(mdlfile, sizeof(mdlfile), "data/jddcnn-weights-%s.dat", mdl_type[mdl]);
  if(p->node->kernel == dt_token("weights"))
  {
    FILE *f = dt_graph_open_resource(mod->graph, 0, mdlfile, "r");
    if(f)
    { // load hardcoded name of weights
      fseek(f, 0, SEEK_END);
      size_t sz = ftell(f);
      if(p->node->connector[0].roi.wd * p->node->connector[0].roi.ht * sizeof(uint16_t) == sz)
      { // load only if allocation size is as expected
        fseek(f, 0, SEEK_SET);
        fread(mapped, sz, 1, f);
      }
      else
      { // issue a gui message
        snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
            "jddcnn: weight file has unexpected size! discarding..");
        mod->graph->gui_msg = mod->graph->gui_msg_buf;
      }
      fclose(f);
    }
    else
    {
      snprintf(mod->graph->gui_msg_buf, sizeof(mod->graph->gui_msg_buf),
          "jddcnn: could not find %s", mdlfile);
      mod->graph->gui_msg = mod->graph->gui_msg_buf;
    }
  }
  return 0;
}

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  const dt_image_params_t *img_param = dt_module_get_input_img_param(graph, module, dt_token("input"));
  if(!img_param) return; // must have disconnected input somewhere
  module->img_param.filters = 0u; // after we're done there won't be any more mosaic
  const int block = 2; // bayer only
  if(module->connector[1].roi.scale >= block)
  { // half size
    dt_roi_t roi_half = module->connector[0].roi;
    roi_half.full_wd /= block;
    roi_half.full_ht /= block;
    roi_half.wd /= block;
    roi_half.ht /= block;
    const int pc[] = { 0, 0, 0, 0, img_param->filters };
    const int id_half = dt_node_add(graph, module, "demosaic", "halfsize",
        roi_half.wd, roi_half.ht, 1, sizeof(pc), pc, 2,
        "input",  "read",  "rggb", "*",   dt_no_roi,
        "output", "write", "rgba", "f16", &roi_half);
    dt_connector_copy(graph, module, 0, id_half, 0);
    if(block != module->connector[1].roi.scale)
    { // resample to get to the rest of the resolution, only if block != scale!
      const int id_resample = dt_node_add(graph, module, "shared", "resample",
          module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
          "input",  "read",  "rgba", "f16", dt_no_roi,
          "output", "write", "rgba", "f16", &module->connector[1].roi);
      CONN(dt_node_connect(graph, id_half, 1, id_resample, 0));
      dt_connector_copy(graph, module, 1, id_resample, 1);
    }
    else dt_connector_copy(graph, module, 1, id_half, 1);
    return;
  }

#define layers_cnt 4
  const int pid = dt_module_get_param(module->so, dt_token("model"));
  const int mdl = CLAMP(dt_module_param_int(module, pid)[0], 0, 1);
  char shader[10];
  const char *mdl_suffix[2] = { "", "16" };

  int id_encoder[layers_cnt];   // convolution layer nodes
  int id_decoder[layers_cnt];   // decoder with skip connections as input
  int id_convolv[layers_cnt];   // extra convolution on each level of the decoder
  int id_convola[layers_cnt];   // additional encoder convolution
  int id_convolb[layers_cnt];   // additional decoder convolution
  int index_weights_buffer = 0; // beginning of the weights of the next convolution

  // starting with bayer planes, i.e. 2x2 downsampled
  int wd[layers_cnt+2] = {module->connector[0].roi.wd, (module->connector[0].roi.wd+1)/2};
  int ht[layers_cnt+2] = {module->connector[0].roi.ht, (module->connector[0].roi.ht+1)/2};

  int i_cnt = 0;
  int o_cnt = 0;
  for(int i=0;i<layers_cnt;i++)
  {
    wd[i+2] = (wd[i+1]+1)/2;
    ht[i+2] = (ht[i+1]+1)/2;
    {
      i_cnt = i ? o_cnt : 4;
      o_cnt = i ? i_cnt : (mdl ? 16 : FUNIT);
      dt_roi_t roi_out = { .wd = wd[i+1] * ht[i+1], .ht = o_cnt };
      int pc[] = { index_weights_buffer, wd[i+1], ht[i+1] };

      snprintf(shader, sizeof(shader), "con%da%s", i, mdl_suffix[mdl]);
      id_convola[i] = dt_node_add(
          graph, module, "jddcnn", shader,
          (ht[i+1]+7) / 8 * DT_LOCAL_SIZE_X, (wd[i+1]+7) / 8 * DT_LOCAL_SIZE_Y,
          1, sizeof(pc), pc, 3,
          "weights", "read",  "ssbo", "f16", dt_no_roi,
          "output",  "write", "ssbo", "f16", &roi_out,
          "input",   "read",  "ssbo", "f16", dt_no_roi);
    }
    index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
    // fprintf(stderr, "encoder %s %d [%d %d %d %d] running on %d x %d output %d x %d\n", shader, i, o_cnt, i_cnt, 3, 3, wd[i+1], ht[i+1], wd[i+1], ht[i+1]);

    i_cnt  = o_cnt;
    o_cnt *= 2;
    {
      int pc[] = { index_weights_buffer, wd[i+1], ht[i+1] };
      dt_roi_t roi_out = { .wd = wd[i+2] * ht[i+2], .ht = o_cnt };
      snprintf(shader, sizeof(shader), "enc%d%s", i, mdl_suffix[mdl]);
      id_encoder[i] = dt_node_add(
          graph, module, "jddcnn", shader,
          (ht[i+1]+7) / 8 * DT_LOCAL_SIZE_X, (wd[i+1]+7) / 8 * DT_LOCAL_SIZE_Y,
          1, sizeof(pc), pc, 3,
          "weights", "read",  "ssbo", "f16", dt_no_roi,
          "output",  "write", "ssbo", "f16", &roi_out,
          "input",   "read",  "ssbo", "f16", dt_no_roi);
    }

    // fprintf(stderr, "encoder %s  %d [%d %d %d %d] running on %d x %d output %d x %d\n", shader, i, o_cnt, i_cnt, 3, 3, wd[i+1], ht[i+1], wd[i+2], ht[i+2]);
    index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
  }

  int id_bottom;
  i_cnt = o_cnt;
  { // bottom level
    int pc[] = { index_weights_buffer, wd[layers_cnt+1], ht[layers_cnt+1] };
    dt_roi_t roi_out = { .wd = wd[layers_cnt+1] * ht[layers_cnt+1], .ht = o_cnt };
    snprintf(shader, sizeof(shader), "con%da%s", layers_cnt, mdl_suffix[mdl]);
    id_bottom = dt_node_add(
        graph, module, "jddcnn", shader,
        (ht[layers_cnt+1]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt+1]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 3,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi);
    // fprintf(stderr, "bottom  %s   [%d %d %d %d] running on %d x %d output %d x %d\n", shader, o_cnt, i_cnt, 3, 3, wd[layers_cnt+1], ht[layers_cnt+1], wd[layers_cnt+1], ht[layers_cnt+1]);
    index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
  }

  for(int i=0;i<layers_cnt;i++)
  {
    { // convolution, upsampling and reducing channels, no skip connection
      o_cnt = i_cnt / 2;
      dt_roi_t roi_out = { .wd = wd[layers_cnt-i] * ht[layers_cnt-i], .ht = o_cnt };
      int pc[] = { index_weights_buffer, wd[layers_cnt-i], ht[layers_cnt-i] };
      snprintf(shader, sizeof(shader), "con%d%s", i, mdl_suffix[mdl]);
      id_convolv[i] = dt_node_add(
          graph, module, "jddcnn", shader,
          (ht[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_Y,
          1, sizeof(pc), pc, 3,
          "weights", "read",  "ssbo", "f16", dt_no_roi,
          "output",  "write", "ssbo", "f16", &roi_out,
          "input",   "read",  "ssbo", "f16", dt_no_roi);
      index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
      // fprintf(stderr, "decoder %s  %d [%d %d %d %d] running on %d x %d output %d x %d\n", shader, i, o_cnt, i_cnt, 3, 3, wd[layers_cnt-i+1], ht[layers_cnt-i+1], wd[layers_cnt-i], ht[layers_cnt-i]);
    }
    { // decoder convolution, no upsampling but we get the skip connection
      i_cnt = 2*o_cnt; // we get the skip connection too, but reduce to current o_cnt
      if(i == layers_cnt-1) i_cnt = 4+o_cnt; // skip connection comes from input directly
      dt_roi_t roi_out = { .wd = wd[layers_cnt-i] * ht[layers_cnt-i], .ht = o_cnt };
      int pc[] = { index_weights_buffer, wd[layers_cnt-i], ht[layers_cnt-i] };
      snprintf(shader, sizeof(shader), "dec%d%s", i, mdl_suffix[mdl]);
      id_decoder[i] = dt_node_add(
          graph, module, "jddcnn", shader,
          (ht[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_Y,
          1, sizeof(pc), pc, 4,
          "weights", "read",  "ssbo", "f16", dt_no_roi,
          "output",  "write", "ssbo", "f16", &roi_out,
          "input",   "read",  "ssbo", "f16", dt_no_roi,  // low res inputs, to be upsampled first
          "skip",    "read",  "ssbo", "f16", dt_no_roi); // these are the skip connections on high res
      index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
      // fprintf(stderr, "decoder %s  %d [%d %d %d %d] running on %d x %d output %d x %d\n", shader, i, o_cnt, i_cnt, 3, 3, wd[layers_cnt-i], ht[layers_cnt-i], wd[layers_cnt-i], ht[layers_cnt-i]);
    }
    { // extra convolution conXb
      i_cnt = o_cnt;
      if(i == layers_cnt-1) o_cnt = 12;
      dt_roi_t roi_out = { .wd = wd[layers_cnt-i] * ht[layers_cnt-i], .ht = o_cnt };
      int pc[] = { index_weights_buffer, wd[layers_cnt-i], ht[layers_cnt-i] };
      snprintf(shader, sizeof(shader), "con%db%s", i, mdl_suffix[mdl]);
      id_convolb[i] = dt_node_add(
          graph, module, "jddcnn", shader,
          (ht[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_Y,
          1, sizeof(pc), pc, 3,
          "weights", "read",  "ssbo", "f16", dt_no_roi,
          "output",  "write", "ssbo", "f16", &roi_out,
          "input",   "read",  "ssbo", "f16", dt_no_roi);
      index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
      // fprintf(stderr, "decoder %s %d [%d %d %d %d] running on %d x %d output %d x %d\n", shader, i, o_cnt, i_cnt, 3, 3, wd[layers_cnt-i], ht[layers_cnt-i], wd[layers_cnt-i], ht[layers_cnt-i]);
    }
  }

  // fprintf(stderr, "weights %lu bytes\n", sizeof(uint16_t)*index_weights_buffer);

  // wire weights from file
  dt_roi_t roi_weights = { .wd = index_weights_buffer, .ht = 1, .full_wd = index_weights_buffer, .full_ht = 1, .scale = 1 };
  const int id_lut = dt_node_add(graph, module, "jddcnn", "weights", 1, 1, 1, 0, 0, 1,
      "weights", "source", "ssbo", "f16", &roi_weights);
  graph->module[id_lut].flags = s_module_request_read_source; // read once
  for(int i=0;i<layers_cnt;i++)
  {
    dt_node_connect_named(graph, id_lut, "weights", id_encoder[i], "weights");
    dt_node_connect_named(graph, id_lut, "weights", id_decoder[i], "weights");
    dt_node_connect_named(graph, id_lut, "weights", id_convolv[i], "weights");
    dt_node_connect_named(graph, id_lut, "weights", id_convola[i], "weights");
    dt_node_connect_named(graph, id_lut, "weights", id_convolb[i], "weights");
    dt_node_connect_named(graph, id_convola[i], "output", id_encoder[i], "input");
    dt_node_connect_named(graph, id_convolv[i], "output", id_decoder[i], "input");
    dt_node_connect_named(graph, id_decoder[i], "output", id_convolb[i], "input");
    if(i < layers_cnt-1)
    {
      // fprintf(stderr, "skip connection enc%d -> dec%d\n", layers_cnt-i-2, i);
      dt_node_connect_named(graph, id_encoder[layers_cnt-i-2], "output", id_decoder[i], "skip");
    }
  }
  dt_node_connect_named(graph, id_encoder[layers_cnt-1], "output", id_bottom, "input");
  dt_node_connect_named(graph, id_bottom, "output",  id_convolv[0], "input");
  dt_node_connect_named(graph, id_lut,    "weights", id_bottom, "weights");
  for(int i=0;i<layers_cnt-1;i++)
  {
    dt_node_connect_named(graph, id_encoder[i], "output", id_convola[i+1], "input");
    dt_node_connect_named(graph, id_convolb[i], "output", id_convolv[i+1], "input");
  }

  dt_roi_t roi_out = (dt_roi_t){ .wd = wd[1] * ht[1], .ht = 4 };
  const int id_input = dt_node_add(graph, module, "jddcnn", "input", wd[0], ht[0], 1, 0, 0, 2,
      "input",  "read",  "rggb", "*",   dt_no_roi,
      "output", "write", "ssbo", "f16", &roi_out);
  const int id_output = dt_node_add(graph, module, "jddcnn", "output", wd[0], ht[0], 1, 0, 0, 2,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  dt_node_connect_named(graph, id_input, "output", id_decoder[layers_cnt-1], "skip");
  // fprintf(stderr, "skip connection input -> dec%d\n", layers_cnt-1);
  // fprintf(stderr, "output node width %d %d x %d %d, full %d x %d\n",
  //     wd[0], module->connector[1].roi.wd,
  //     ht[0], module->connector[1].roi.ht,
  //     module->connector[1].roi.full_wd,
  //     module->connector[1].roi.full_ht);
  dt_connector_copy(graph, module, 0, id_input,  0);
  if(module->connector[1].roi.scale != 1.0)
  { // add resample node to graph, copy its output instead:
    const int id_resample = dt_node_add(graph, module, "shared", "resample",
        module->connector[1].roi.wd, module->connector[1].roi.ht, 1, 0, 0, 2,
        "input",  "read",  "rgba", "f16", dt_no_roi,
        "output", "write", "rgba", "f16", &module->connector[1].roi);
    CONN(dt_node_connect(graph, id_output, 1, id_resample, 0));
    dt_connector_copy(graph, module, 1, id_resample, 1);
  }
  else dt_connector_copy(graph, module, 1, id_output, 1);
  dt_node_connect_named(graph, id_input, "output", id_convola[0], "input");
  dt_node_connect_named(graph, id_convolb[layers_cnt-1], "output", id_output, "input");
}
#undef layers_cnt
