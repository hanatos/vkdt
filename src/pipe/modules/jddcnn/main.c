#include "modules/api.h"

static inline int round16(const int x)
{ // rounds x to the next multiple of 16
  return 16 * ((x+15)/16);
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
  const int feat[] = {32, 43, 57, 76, 101, 101};
  char shader[10];

  int id_encoder[layers_cnt];   // convolution layer nodes
  int index_weights_buffer = 0; // beginning of the weights of the next convolution

  // starting with bayer planes, i.e. 2x2 downsampled
  int wd[layers_cnt+2] = {module->connector[0].roi.wd, (module->connector[0].roi.wd+1)/2};
  int ht[layers_cnt+2] = {module->connector[0].roi.ht, (module->connector[0].roi.ht+1)/2};

  for(int i=0;i<layers_cnt;i++)
  {
    const int i_cnt = i == 0 ? 5 : feat[i - 1];
    const int o_cnt = feat[i];

    dt_roi_t roi_out = { .wd = wd[i+1] * ht[i+1], .ht = round16(o_cnt) };
    int pc[] = { index_weights_buffer, wd[i+1], ht[i+1], wd[i] };

    snprintf(shader, sizeof(shader), "enc%d", i);
    id_encoder[i] = dt_node_add(
        graph, module, "jddcnn", shader,
        // MIN(256, wd[i+1] / 8 * DT_LOCAL_SIZE_X),
        // MIN(256, ht[i+1] / 8 * DT_LOCAL_SIZE_Y),
        (ht[i+1]+7) / 8 * DT_LOCAL_SIZE_X, (wd[i+1]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 3,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi);

    fprintf(stderr, "encoder conv %d [%d %d %d %d] running on %d x %d\n", i, o_cnt, i_cnt, 3, 3, wd[i+1], ht[i+1]);

    index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
    // XXX unfortunately currently the kernels run on full input resolution here.
    // XXX the max pooling is performed on *input* of the next layer, so there is a 4x memory traffic on the table here.
    wd[i+2] = (wd[i+1]+1)/2;
    ht[i+2] = (ht[i+1]+1)/2;
  }

  int id_decoder[layers_cnt-1];
  for(int i=0;i<layers_cnt-1;i++)
  {
    // const int i_cnt = (i == layers_cnt-1) ? feat[layers_cnt - 1 - i] + 5: feat[layers_cnt - 2 - i] + feat[layers_cnt - 1 - i];
    // const int o_cnt = (i == layers_cnt-1) ? 3                           : feat[layers_cnt - 2 - i];
    const int i_cnt = feat[layers_cnt - 2 - i] + feat[layers_cnt - 1 - i];
    const int o_cnt = (i == layers_cnt-2) ? 12 : feat[layers_cnt - 2 - i];

    // layers upsample their inputs first thing when running. so the resolution to run the kernel here is the larger one,
    // i.e. the input resolution of the corresponding encoder layer.
    dt_roi_t roi_out = { .wd = wd[layers_cnt-1-i] * ht[layers_cnt-1-i], .ht = round16(o_cnt) };
    int pc[] = { index_weights_buffer, wd[layers_cnt-1-i], ht[layers_cnt-1-i], wd[layers_cnt-2-i] };

    // TODO: can this go faster if we parallelise workgroups over feature channels too?
    snprintf(shader, sizeof(shader), "dec%d", i);
    id_decoder[i] = dt_node_add(
        graph, module, "jddcnn", shader,
        // MIN(256, wd[layers_cnt-1-i] / 8 * DT_LOCAL_SIZE_X),
        // MIN(256, ht[layers_cnt-1-i] / 8 * DT_LOCAL_SIZE_Y),
        (ht[layers_cnt-1-i]+7) / 8 * DT_LOCAL_SIZE_X, (wd[layers_cnt-1-i]+7) / 8 * DT_LOCAL_SIZE_Y,
        1, sizeof(pc), pc, 4,
        "weights", "read",  "ssbo", "f16", dt_no_roi,
        "output",  "write", "ssbo", "f16", &roi_out,
        "input",   "read",  "ssbo", "f16", dt_no_roi,  // low res inputs, to be upsampled first
        "skip",    "read",  "ssbo", "f16", dt_no_roi); // these are the skip connections on high res
    fprintf(stderr, "decoder conv %d [%d %d %d %d] running on %d x %d\n", i, o_cnt, i_cnt, 3, 3, wd[layers_cnt-1-i], ht[layers_cnt-1-i]);

    index_weights_buffer += 9 * i_cnt * o_cnt + o_cnt;
  }
  fprintf(stderr, "weights %lu bytes\n", sizeof(uint16_t)*index_weights_buffer);

  // wire weights from file
  dt_roi_t roi_weights = { .wd = index_weights_buffer, .ht = 1, .full_wd = index_weights_buffer, .full_ht = 1, .scale = 1 };
  const int id_lut = dt_node_add(graph, module, "jddcnn", "weights", 1, 1, 1, 0, 0, 1,
      "weights", "source", "ssbo", "f16", &roi_weights);
  graph->module[id_lut].flags = s_module_request_read_source; // read once
  for(int i=0;i<layers_cnt;i++)
    dt_node_connect_named(graph, id_lut, "weights", id_encoder[i], "weights");
  for(int i=0;i<layers_cnt-1;i++)
    dt_node_connect_named(graph, id_lut, "weights", id_decoder[i], "weights");

  dt_roi_t roi_out = { .wd = wd[1] * ht[1], .ht = round16(5) };
  const int id_input = dt_node_add(graph, module, "jddcnn", "input", wd[0], ht[0], 1, 0, 0, 2,
      "input",  "read",  "rggb", "*",   dt_no_roi,
      "output", "write", "ssbo", "f16", &roi_out);
  const int id_output = dt_node_add(graph, module, "jddcnn", "output", wd[0], ht[0], 1, 0, 0, 2,
      "input",  "read",  "ssbo", "f16", dt_no_roi,
      "output", "write", "rgba", "f16", &module->connector[1].roi);
  dt_connector_copy(graph, module, 0, id_input,  0);
  dt_connector_copy(graph, module, 1, id_output, 1);
  dt_node_connect_named(graph, id_input,                 "output", id_encoder[0], "input");
  dt_node_connect_named(graph, id_decoder[layers_cnt-2], "output", id_output,     "input");

  for(int i=0;i<layers_cnt-1;i++)
  {
    dt_node_connect_named(graph, id_encoder[i], "output", id_encoder[i+1],            "input");
    fprintf(stderr, "skip connection enc %d to dec %d\n", i, layers_cnt-2-i);
    dt_node_connect_named(graph, id_encoder[i], "output", id_decoder[layers_cnt-2-i], "skip");
    if(i < layers_cnt-2)
    dt_node_connect_named(graph, id_decoder[i], "output", id_decoder[i+1],            "input");
  }
  dt_node_connect_named(graph, id_encoder[layers_cnt-1], "output", id_decoder[0],            "input");
  // dt_node_connect_named(graph, id_input,                 "output", id_decoder[layers_cnt-1], "skip");
}
#undef layers_cnt
