#include "modules/api.h"

static inline int round16(const int x)
{ // rounds x to the next multiple of 16
  return 16 * ((x+15)/16);
}

void modify_roi_out(
    dt_graph_t *graph,
    dt_module_t *module)
{
  module->connector[1].roi = module->connector[0].roi;
}

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{ // we work on full res/bayer
  module->connector[0].roi.wd = module->connector[0].roi.full_wd;
  module->connector[0].roi.ht = module->connector[0].roi.full_ht;
  module->connector[1].roi.wd = module->connector[0].roi.full_wd;
  module->connector[1].roi.ht = module->connector[0].roi.full_ht;
  module->connector[0].roi.scale = 1.0f;
}

int read_source(
    dt_module_t             *mod,
    void                    *mapped,
    dt_read_source_params_t *p)
{
  if(p->node->kernel == dt_token("weights"))
  {
    FILE *f = dt_graph_open_resource(mod->graph, 0, "data/jddcnn-weights.dat", "r");
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
      {
        // TODO gui message
        fprintf(stderr, "weight file has unexpected size! discarding..\n");
      }
      fclose(f);
    }
    else fprintf(stderr, "could not find data/jddcnn-weights.dat!\n");
  }
  return 0;
}

// TODO: something commit params and replace black and white values

void create_nodes(dt_graph_t *graph, dt_module_t *module)
{
  fprintf(stderr, "bw %f %f\n", module->img_param.black[0], module->img_param.white[0]);
  fprintf(stderr, "mat\n%f %f %f\n%f %f %f\n%f %f %f\n",
      module->img_param.cam_to_rec2020[0], module->img_param.cam_to_rec2020[1], module->img_param.cam_to_rec2020[2],
      module->img_param.cam_to_rec2020[3], module->img_param.cam_to_rec2020[4], module->img_param.cam_to_rec2020[5],
      module->img_param.cam_to_rec2020[6], module->img_param.cam_to_rec2020[7], module->img_param.cam_to_rec2020[8]);
#define layers_cnt 6
  // const int featenc[] = {32, 43, 57, 76, 101, 101};
  // const int featdec[] = {101, 76, 57, 43, 16, 12};
  const int featenc[] = {32, 48, 64, 80, 112, 112};
  const int featdec[] = {112, 80, 64, 48, 16, 12};
  char shader[10];

  int id_encoder[layers_cnt];   // convolution layer nodes
  int id_decoder[layers_cnt];   // decoder with skip connections as input
  int id_convolv[layers_cnt];   // extra convolution on each level of the decoder
  int id_convola[layers_cnt];   // additional decoder convolution
  int index_weights_buffer = 0; // beginning of the weights of the next convolution

  // starting with bayer planes, i.e. 2x2 downsampled
  int wd[layers_cnt+2] = {module->connector[0].roi.wd, (module->connector[0].roi.wd+1)/2};
  int ht[layers_cnt+2] = {module->connector[0].roi.ht, (module->connector[0].roi.ht+1)/2};

  for(int i=0;i<layers_cnt;i++)
  {
    const int i_cnt = i == 0 ? 4 : featenc[i-1];
    const int o_cnt = featenc[i];

    wd[i+2] = (wd[i+1]+1)/2;
    ht[i+2] = (ht[i+1]+1)/2;
    dt_roi_t roi_out = { .wd = wd[i+2] * ht[i+2], .ht = round16(o_cnt) };
    int pc[] = { index_weights_buffer, wd[i+1], ht[i+1] };

    snprintf(shader, sizeof(shader), "enc%d", i);
    id_encoder[i] = dt_node_add(
        graph, module, "jddcnn", shader,
        (ht[i+1]+7) / 8 * DT_LOCAL_SIZE_X, (wd[i+1]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 3,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi);

    fprintf(stderr, "encoder conv %d [%d %d %d %d] running on %d x %d output %d x %d\n", i, o_cnt, i_cnt, 3, 3, wd[i+1], ht[i+1], wd[i+2], ht[i+2]);
    index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
  }

  for(int i=0;i<layers_cnt;i++)
  {
    const int i_cnt = ((i == layers_cnt-1) ?  4 : featenc[layers_cnt - 2 - i] ) + (i ? featdec[i-1] : featenc[layers_cnt-1]);
    const int o_cnt = featdec[i];

    // layers upsample their inputs first thing when running. so the resolution to run the kernel here is the larger one,
    // i.e. the input resolution of the corresponding encoder layer.
    dt_roi_t roi_out = { .wd = wd[layers_cnt-i] * ht[layers_cnt-i], .ht = round16(o_cnt) };
    int pc[] = { index_weights_buffer, wd[layers_cnt-i], ht[layers_cnt-i] };

    // TODO: can this go faster if we parallelise workgroups over feature channels too? i.e. z up to o_cnt/16
    snprintf(shader, sizeof(shader), "dec%d", i);
    id_decoder[i] = dt_node_add(
        graph, module, "jddcnn", shader,
        (ht[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 4,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi,  // low res inputs, to be upsampled first
        "skip",    "read",  "ssbo", "f16", dt_no_roi); // these are the skip connections on high res

    if(i==layers_cnt-1) roi_out = (dt_roi_t){ .wd = wd[layers_cnt-i] * ht[layers_cnt-i], .ht = o_cnt };
    fprintf(stderr, "decoder conv %d [%d %d %d %d] running on %d x %d output %d x %d\n", i, o_cnt, i_cnt, 3, 3, wd[layers_cnt-i], ht[layers_cnt-i], wd[layers_cnt-i], ht[layers_cnt-i]);
    index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
    snprintf(shader, sizeof(shader), "con%d", i);
    pc[0] = index_weights_buffer;
    id_convolv[i] = dt_node_add(
        graph, module, "jddcnn", shader,
        (ht[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 3,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi);
    index_weights_buffer += 9 * o_cnt * o_cnt + o_cnt;
  }

  for(int i=0;i<layers_cnt;i++)
  {
    const int o_cnt = featdec[i];
    dt_roi_t roi_out = { .wd = wd[layers_cnt-i] * ht[layers_cnt-i], .ht = round16(o_cnt) };
    int pc[] = { index_weights_buffer, wd[layers_cnt-i], ht[layers_cnt-i] };

    if(i==layers_cnt-1) roi_out = (dt_roi_t){ .wd = wd[layers_cnt-i] * ht[layers_cnt-i], .ht = o_cnt };
    pc[0] = index_weights_buffer;
    id_convola[i] = dt_node_add(
        graph, module, "jddcnn", shader,
        (ht[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt-i]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 3,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi);

    fprintf(stderr, "decoder cnva %d [%d %d %d %d] running on %d x %d output %d x %d\n", i, o_cnt, o_cnt, 3, 3, wd[layers_cnt-i], ht[layers_cnt-i], wd[layers_cnt-i], ht[layers_cnt-i]);
    index_weights_buffer += 9 * o_cnt * o_cnt + o_cnt;
  }
#if 0
  // final convolution, upsampling to full res
  const int o_cnt = 3;
  dt_roi_t roi_out = { .wd = wd[0] * ht[0], .ht = round16(o_cnt) };
  int pc[] = { index_weights_buffer, wd[0], ht[0] };

  snprintf(shader, sizeof(shader), "dec%d", layers_cnt);
  const int id_last = dt_node_add(
        graph, module, "jddcnn", shader,
        (ht[0]+7) / 8 * DT_LOCAL_SIZE_X, (wd[0]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 4,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi,  // low res inputs, to be upsampled first
        "skip",    "read",  "ssbo", "f16", dt_no_roi); // these are the skip connections on high res
  index_weights_buffer += 9 * 16 * o_cnt + o_cnt;
#endif

  fprintf(stderr, "weights %lu bytes\n", sizeof(uint16_t)*index_weights_buffer);

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
    dt_node_connect_named(graph, id_decoder[i], "output", id_convolv[i], "input");
    dt_node_connect_named(graph, id_convolv[i], "output", id_convola[i], "input");
  }

  // dt_node_connect_named(graph, id_lut, "weights", id_last, "weights");
  // dt_node_connect_named(graph, id_convola[layers_cnt-1], "output", id_last, "input");
  // dt_node_connect_named(graph, id_convola[layers_cnt-1], "output", id_last, "skip"); // dummy

  dt_roi_t roi_out = (dt_roi_t){ .wd = wd[1] * ht[1], .ht = 4 };
  const int id_input = dt_node_add(graph, module, "jddcnn", "input", wd[0], ht[0], 1, 0, 0, 2,
      "input",  "read",  "rggb", "*",   dt_no_roi,
      "output", "write", "ssbo", "f16", &roi_out);
  const int id_output = dt_node_add(graph, module, "jddcnn", "output", wd[0], ht[0], 1, 0, 0, 2,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  fprintf(stderr, "output node width %d %d x %d %d, full %d x %d\n",
      wd[0], module->connector[1].roi.wd,
      ht[0], module->connector[1].roi.ht,
      module->connector[1].roi.full_wd,
      module->connector[1].roi.full_ht);
  dt_connector_copy(graph, module, 0, id_input,  0);
  dt_connector_copy(graph, module, 1, id_output, 1);
  dt_node_connect_named(graph, id_input, "output", id_encoder[0], "input");
  // dt_node_connect_named(graph, id_last,  "output", id_output,     "input");
  // XXX DEBUG
  // dt_node_connect_named(graph, id_decoder[0], "output", id_output, "input");
  // dt_node_connect_named(graph, id_encoder[4], "output", id_output, "input");
  dt_node_connect_named(graph, id_convola[layers_cnt-1], "output", id_output,     "input");

  for(int i=0;i<layers_cnt-1;i++)
  {
    dt_node_connect_named(graph, id_encoder[i], "output", id_encoder[i+1],            "input");
    fprintf(stderr, "skip connection enc %d to dec %d\n", i, layers_cnt-2-i);
    dt_node_connect_named(graph, id_encoder[i], "output", id_decoder[layers_cnt-2-i], "skip");
    dt_node_connect_named(graph, id_convola[i], "output", id_decoder[i+1],            "input");
  }
  dt_node_connect_named(graph, id_encoder[layers_cnt-1], "output", id_decoder[0],            "input");
  dt_node_connect_named(graph, id_input,                 "output", id_decoder[layers_cnt-1], "skip");
}
#undef layers_cnt
